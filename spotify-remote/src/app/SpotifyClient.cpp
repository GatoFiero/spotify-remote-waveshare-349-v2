#include "SpotifyClient.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "../Secrets.h"
#include "AlbumArt.h"
#include "Net.h"
#include "Text.h"

namespace spotify {
namespace {

using app::NowPlaying;
using app::Status;

constexpr char kTokenUrl[] = "https://accounts.spotify.com/api/token";
constexpr char kPlayerUrl[] =
    "https://api.spotify.com/v1/me/player?additional_types=track,episode";
constexpr char kApiBase[] = "https://api.spotify.com/v1/me/player/";
constexpr char kTransferUrl[] = "https://api.spotify.com/v1/me/player";
constexpr char kDevicesUrl[] = "https://api.spotify.com/v1/me/player/devices";

constexpr uint32_t kPollIntervalMs = 3000;
// Spotify needs a moment to settle after a transport command, and the first
// state it reports back is often still the old track.
constexpr uint32_t kSettleDelayMs = 600;
// A woken device needs a moment before it will accept a skip.
constexpr uint32_t kWakeSettleMs = 900;
constexpr uint32_t kErrorBackoffMs = 5000;

NowPlaying state;
SemaphoreHandle_t state_mutex = nullptr;
QueueHandle_t command_queue = nullptr;

Preferences prefs;
String access_token;
String refresh_token;
uint32_t token_expires_at_ms = 0;

// The last device we saw playing. Persisted, so the board can wake it after a
// power cycle without waiting to observe a session first.
String last_device_id;
String last_device_name;

// Kept open between polls: a TLS handshake costs well over a second, and we poll
// every three.
WiFiClientSecure api_client;

char pending_art_url[192] = {};
uint32_t pending_art_generation = 0;
uint8_t pending_art_attempts = 0;

Status currentStatus() {
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    const Status status = state.status;
    xSemaphoreGive(state_mutex);
    return status;
}

// True only when a response actually carries a body worth reading. Asking
// HTTPClient for the body of a 204 (or of a failed request) makes it block for
// the full TCP timeout waiting on bytes the server is never going to send.
bool hasBody(HTTPClient &http, int code) {
    return code > 0 && code != HTTP_CODE_NO_CONTENT && http.getSize() != 0;
}

void setStatus(Status status) {
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    state.status = status;
    xSemaphoreGive(state_mutex);
}

// --- authorisation --------------------------------------------------------

void loadRefreshToken() {
    prefs.begin("spotify", /*readOnly=*/false);
    refresh_token = prefs.getString("refresh", "");
    last_device_id = prefs.getString("device", "");
    last_device_name = prefs.getString("devname", "");
    if (refresh_token.isEmpty()) {
        refresh_token = SPOTIFY_REFRESH_TOKEN;  // first boot: seed from Secrets.h
        log_i("seeding refresh token from Secrets.h");
    }
}

void rememberDevice(const String &id, const String &name) {
    if (id.isEmpty() || id == last_device_id) return;  // only write NVS on a real change
    last_device_id = id;
    last_device_name = name;
    prefs.putString("device", id);
    prefs.putString("devname", name);
    log_i("remembering device \"%s\"", name.c_str());
}

void forgetDevice() {
    last_device_id = "";
    prefs.remove("device");
}

void storeRefreshToken(const String &token) {
    if (token.isEmpty() || token == refresh_token) return;
    refresh_token = token;
    prefs.putString("refresh", token);
    log_i("stored rotated refresh token");
}

bool refreshAccessToken() {
    setStatus(Status::Authorizing);

    WiFiClientSecure client;
    net::secure(client);
    HTTPClient http;
    http.setTimeout(15000);
    if (!http.begin(client, kTokenUrl)) {
        setStatus(Status::NetworkError);
        return false;
    }
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String body = "grant_type=refresh_token&refresh_token=";
    body += refresh_token;
    body += "&client_id=" SPOTIFY_CLIENT_ID;

    const int code = http.POST(body);
    const String response = hasBody(http, code) ? http.getString() : String();
    http.end();
    if (code != HTTP_CODE_OK) {
        log_e("token refresh HTTP %d: %s", code, response.c_str());
        // 400 means the token itself was rejected -- re-running the auth tool is
        // the only fix, so say so rather than retrying forever in silence.
        setStatus(code == HTTP_CODE_BAD_REQUEST ? Status::AuthFailed : Status::NetworkError);
        return false;
    }

    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, response);
    if (error) {
        log_e("token response parse: %s", error.c_str());
        setStatus(Status::NetworkError);
        return false;
    }

    access_token = doc["access_token"].as<String>();
    const uint32_t expires_in = doc["expires_in"] | 3600;
    token_expires_at_ms = millis() + (expires_in - 120) * 1000;  // renew two minutes early

    // Under PKCE, Spotify rotates the refresh token on most refreshes.
    if (doc["refresh_token"].is<const char *>()) {
        storeRefreshToken(doc["refresh_token"].as<String>());
    }
    if (access_token.isEmpty()) {
        log_e("token response carried no access_token");
        setStatus(Status::NetworkError);
        return false;
    }
    log_i("access token refreshed, valid for %lu s", expires_in);
    return true;
}

bool ensureAccessToken() {
    if (!access_token.isEmpty() && millis() < token_expires_at_ms) return true;
    return refreshAccessToken();
}

// --- player state ---------------------------------------------------------

// The /me/player response is enormous, mostly available_markets. Filtering keeps
// the parsed document in the hundreds of bytes instead of over 100 KB.
void buildFilter(JsonDocument &filter) {
    filter["is_playing"] = true;
    filter["progress_ms"] = true;
    filter["device"]["id"] = true;
    filter["device"]["name"] = true;

    JsonObject item = filter["item"].to<JsonObject>();
    item["name"] = true;
    item["duration_ms"] = true;
    item["type"] = true;
    item["artists"][0]["name"] = true;

    JsonObject album = item["album"].to<JsonObject>();
    album["name"] = true;
    album["images"][0]["url"] = true;
    album["images"][0]["width"] = true;

    // Podcast episodes carry the show and images at the item level instead.
    item["show"]["name"] = true;
    item["images"][0]["url"] = true;
    item["images"][0]["width"] = true;
}

// Prefers the cover nearest 300 px: big enough to look sharp at 150, small
// enough to download and decode quickly.
const char *pickImageUrl(JsonArrayConst images) {
    const char *best = nullptr;
    int best_distance = INT32_MAX;
    for (JsonObjectConst image : images) {
        const char *url = image["url"];
        if (!url) continue;
        const int width = image["width"] | 300;
        const int distance = abs(width - 300);
        if (distance < best_distance) {
            best_distance = distance;
            best = url;
        }
    }
    return best;
}

void applyStopped(Status status) {
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    state.status = status;
    state.has_track = false;
    state.is_playing = false;
    state.title[0] = state.artist[0] = state.album[0] = '\0';
    // Keep the device name: the UI uses it to offer to wake that device.
    strlcpy(state.device_name, last_device_name.c_str(), sizeof(state.device_name));
    xSemaphoreGive(state_mutex);
    albumart::clear();
}

bool pollPlayerState() {
    HTTPClient http;
    http.setTimeout(12000);
    http.setReuse(true);
    if (!http.begin(api_client, kPlayerUrl)) return false;
    http.addHeader("Authorization", "Bearer " + access_token);
    static const char *kCollected[] = {"Retry-After"};
    http.collectHeaders(kCollected, 1);

    const int code = http.GET();

    // Read the body before branching, always, and via getString(): it decodes
    // Transfer-Encoding: chunked, which Spotify uses here and which getStream()
    // hands over raw (the hex chunk lengths land mid-JSON and the parse fails
    // with IncompleteInput once a response spans more than one chunk). It also
    // drains the socket, which matters because this connection is reused --
    // bytes left behind by one response are read as the start of the next.
    const String retry_after = http.header("Retry-After");
    const String body = hasBody(http, code) ? http.getString() : String();
    http.end();

    if (code == HTTP_CODE_NO_CONTENT) {  // Spotify is reachable, nothing is active
        applyStopped(Status::NoActiveDevice);
        return true;
    }
    if (code == HTTP_CODE_UNAUTHORIZED) {
        access_token = "";  // force a refresh on the next pass
        return false;
    }
    if (code == HTTP_CODE_TOO_MANY_REQUESTS) {
        const uint32_t wait_s = retry_after.isEmpty() ? 5 : retry_after.toInt();
        log_w("rate limited, backing off %lu s", wait_s);
        vTaskDelay(pdMS_TO_TICKS(wait_s * 1000));
        return true;
    }
    if (code != HTTP_CODE_OK) {
        log_w("player state HTTP %d: %s", code, body.c_str());
        return false;
    }

    JsonDocument filter;
    buildFilter(filter);
    JsonDocument doc;
    const DeserializationError error =
        deserializeJson(doc, body, DeserializationOption::Filter(filter));
    if (error) {
        log_e("player state parse: %s", error.c_str());
        return false;
    }

    JsonObjectConst item = doc["item"];
    if (item.isNull()) {
        applyStopped(Status::NoActiveDevice);
        return true;
    }

    const bool is_episode = strcmp(item["type"] | "track", "episode") == 0;

    char title[128] = {};
    char artist[128] = {};
    char album[128] = {};
    text::toLatin1(item["name"] | "", title, sizeof(title));

    if (is_episode) {
        text::toLatin1(item["show"]["name"] | "", artist, sizeof(artist));
    } else {
        // Join every credited artist, the way the Spotify client does.
        String joined;
        for (JsonObjectConst credit : item["artists"].as<JsonArrayConst>()) {
            const char *name = credit["name"];
            if (!name) continue;
            if (!joined.isEmpty()) joined += ", ";
            joined += name;
        }
        text::toLatin1(joined.c_str(), artist, sizeof(artist));
        text::toLatin1(item["album"]["name"] | "", album, sizeof(album));
    }

    const char *art_url =
        pickImageUrl(is_episode ? item["images"].as<JsonArrayConst>()
                                : item["album"]["images"].as<JsonArrayConst>());

    xSemaphoreTake(state_mutex, portMAX_DELAY);
    const bool track_changed = strcmp(state.title, title) != 0 ||
                               strcmp(state.artist, artist) != 0 ||
                               strcmp(state.album, album) != 0;
    state.status = Status::Playing;
    state.has_track = true;
    state.is_playing = doc["is_playing"] | false;
    strlcpy(state.title, title, sizeof(state.title));
    strlcpy(state.artist, artist, sizeof(state.artist));
    strlcpy(state.album, album, sizeof(state.album));
    strlcpy(state.device_name, doc["device"]["name"] | "", sizeof(state.device_name));
    state.duration_ms = item["duration_ms"] | 0;
    state.progress_ms = doc["progress_ms"] | 0;
    state.progress_at_ms = millis();
    if (track_changed) ++state.track_generation;
    const uint32_t generation = state.track_generation;
    xSemaphoreGive(state_mutex);

    rememberDevice(doc["device"]["id"] | "", doc["device"]["name"] | "");

    if (track_changed) {
        log_i("now playing: %s - %s%s on %s", title, artist,
              (doc["is_playing"] | false) ? "" : " (paused)",
              doc["device"]["name"] | "?");
        albumart::clear();  // never leave the previous track's cover on screen
        if (art_url) {
            strlcpy(pending_art_url, art_url, sizeof(pending_art_url));
            pending_art_generation = generation;
            pending_art_attempts = 0;
        }
    }
    return true;
}

// --- waking an idle device ------------------------------------------------

// Picks something controllable from the account's visible devices. Restricted
// devices appear in the list but reject Web API control, so they are skipped.
bool resolveDevice(String &out_id, String &out_name) {
    HTTPClient http;
    http.setTimeout(10000);
    http.setReuse(true);
    if (!http.begin(api_client, kDevicesUrl)) return false;
    http.addHeader("Authorization", "Bearer " + access_token);

    const int code = http.GET();
    const String body = hasBody(http, code) ? http.getString() : String();
    http.end();
    if (code != HTTP_CODE_OK) {
        log_w("device list HTTP %d: %s", code, body.c_str());
        return false;
    }

    JsonDocument filter;
    JsonObject device = filter["devices"][0].to<JsonObject>();
    device["id"] = true;
    device["name"] = true;
    device["is_active"] = true;
    device["is_restricted"] = true;

    JsonDocument doc;
    if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return false;

    // Prefer one that is already active, then the one we remember by name, then
    // anything controllable at all.
    JsonObjectConst best;
    int best_rank = 4;
    for (JsonObjectConst candidate : doc["devices"].as<JsonArrayConst>()) {
        if (candidate["is_restricted"] | false) continue;
        const char *id = candidate["id"];
        if (!id || !*id) continue;
        const char *name = candidate["name"] | "";
        const int rank = (candidate["is_active"] | false) ? 1
                         : (last_device_name == name)     ? 2
                                                          : 3;
        if (rank < best_rank) {
            best_rank = rank;
            best = candidate;
        }
    }
    if (best.isNull()) {
        log_w("no controllable device is visible to Spotify");
        return false;
    }
    out_id = best["id"].as<const char *>();
    out_name = best["name"] | "";
    return true;
}

// PUT /me/player moves playback to a device, and with play=true starts it.
bool transferTo(const String &device_id, bool start_playing) {
    HTTPClient http;
    http.setTimeout(10000);
    http.setReuse(true);
    if (!http.begin(api_client, kTransferUrl)) return false;
    http.addHeader("Authorization", "Bearer " + access_token);
    http.addHeader("Content-Type", "application/json");

    String body = "{\"device_ids\":[\"";
    body += device_id;
    body += start_playing ? "\"],\"play\":true}" : "\"],\"play\":false}";

    const int code = http.PUT(body);
    const String response = hasBody(http, code) ? http.getString() : String();
    http.end();

    if (code < 200 || code >= 300) {
        log_w("transfer HTTP %d: %s", code, response.c_str());
        return false;
    }
    return true;
}

bool wakeDevice(bool start_playing) {
    if (!last_device_id.isEmpty()) {
        if (transferTo(last_device_id, start_playing)) return true;
        log_i("remembered device did not answer; asking Spotify what is available");
        forgetDevice();
    }

    String id, name;
    if (!resolveDevice(id, name)) return false;
    if (!transferTo(id, start_playing)) return false;
    rememberDevice(id, name);
    return true;
}

// --- transport ------------------------------------------------------------

bool runCommand(Command command) {
    if (!ensureAccessToken()) return false;

    if (currentStatus() == Status::NoActiveDevice) {
        // Transferring playback with play=true *is* the play action, so a
        // play/pause press needs nothing after it.
        if (!wakeDevice(/*start_playing=*/true)) return false;
        if (command == Command::TogglePlayback) return true;
        vTaskDelay(pdMS_TO_TICKS(kWakeSettleMs));
    }

    bool snapshot_playing;
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    snapshot_playing = state.is_playing;
    xSemaphoreGive(state_mutex);

    String url = kApiBase;
    bool use_put = false;
    switch (command) {
        case Command::Next:     url += "next"; break;
        case Command::Previous: url += "previous"; break;
        case Command::TogglePlayback:
            url += snapshot_playing ? "pause" : "play";
            use_put = true;
            break;
    }

    HTTPClient http;
    http.setTimeout(10000);
    http.setReuse(true);
    if (!http.begin(api_client, url)) return false;
    http.addHeader("Authorization", "Bearer " + access_token);
    // HTTPClient only emits Content-Length when the payload is non-empty
    // (`if (payload && size > 0)`), and Spotify answers a bodyless POST/PUT that
    // carries no Content-Length with 411 Length Required.
    http.addHeader("Content-Length", "0");

    const int code = use_put ? http.PUT("") : http.POST("");
    // 204 is the success reply here and has no body to drain.
    const String response = hasBody(http, code) ? http.getString() : String();
    http.end();

    if (code == HTTP_CODE_UNAUTHORIZED) {
        access_token = "";
        return false;
    }
    // 403 with no active device, 404 with no device at all -- both are "nothing
    // to control", not a failure of ours.
    if (code < 200 || code >= 300) {
        log_w("command HTTP %d: %s", code, response.c_str());
        return false;
    }

    // Reflect the toggle immediately so the button feels instant; the next poll
    // corrects it if Spotify disagreed.
    if (command == Command::TogglePlayback) {
        xSemaphoreTake(state_mutex, portMAX_DELAY);
        state.is_playing = !snapshot_playing;
        state.progress_ms = state.elapsedMs();
        state.progress_at_ms = millis();
        xSemaphoreGive(state_mutex);
    }
    return true;
}

// --- task -----------------------------------------------------------------

void task(void *) {
    loadRefreshToken();
    if (refresh_token.startsWith("paste-the-") || refresh_token.isEmpty()) {
        log_e("no refresh token: run tools/spotify_auth.py and fill in src/Secrets.h");
        setStatus(Status::AuthFailed);
        vTaskDelete(nullptr);
    }
    uint32_t next_poll_ms = 0;

    for (;;) {
        if (!net::ensureWifi()) {
            setStatus(Status::WifiConnecting);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        Command command;
        if (xQueueReceive(command_queue, &command, 0) == pdTRUE) {
            if (runCommand(command)) {
                next_poll_ms = millis() + kSettleDelayMs;
            }
            continue;
        }

        if (static_cast<int32_t>(millis() - next_poll_ms) < 0) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        if (!ensureAccessToken()) {
            // Authorizing is a transient state; leaving it on screen tells the
            // user nothing about what actually failed.
            if (currentStatus() == Status::Authorizing) setStatus(Status::NetworkError);
            next_poll_ms = millis() + kErrorBackoffMs;
            continue;
        }

        if (pollPlayerState()) {
            next_poll_ms = millis() + kPollIntervalMs;
        } else {
            if (currentStatus() != Status::AuthFailed) setStatus(Status::NetworkError);
            next_poll_ms = millis() + kErrorBackoffMs;
        }

        // Album art needs its own TLS session. Close the API connection first so
        // only one handshake's worth of buffers is live at a time.
        if (pending_art_generation != 0) {
            api_client.stop();
            const bool loaded = albumart::load(pending_art_url, pending_art_generation);
            if (loaded || ++pending_art_attempts >= 2) pending_art_generation = 0;
        }
    }
}

}  // namespace

bool begin() {
    state_mutex = xSemaphoreCreateMutex();
    command_queue = xQueueCreate(8, sizeof(Command));
    if (!state_mutex || !command_queue) return false;

    net::secure(api_client);

    // Core 0 alongside the WiFi stack, leaving core 1 entirely to the UI.
    return xTaskCreatePinnedToCore(task, "spotify", 16384, nullptr, 4, nullptr, 0) == pdPASS;
}

void snapshot(app::NowPlaying &out) {
    if (!state_mutex) return;
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    out = state;
    xSemaphoreGive(state_mutex);
}

bool send(Command command) {
    if (!command_queue) return false;
    return xQueueSend(command_queue, &command, 0) == pdTRUE;
}

}  // namespace spotify
