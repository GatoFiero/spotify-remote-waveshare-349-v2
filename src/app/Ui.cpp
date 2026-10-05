#include "Ui.h"

#include <string.h>

#include "../fonts/FontBody.h"
#include "../fonts/FontSmall.h"
#include "../fonts/FontTitle.h"
#include "../platform/Screen.h"
#include "AlbumArt.h"
#include "Text.h"
#include "PlaylistShortcuts.h"
#include "PlaylistLayout.h"

namespace ui {
namespace {

using app::NowPlaying;
using app::Status;

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

constexpr uint16_t kBackground   = rgb(0x0B, 0x0B, 0x0D);
constexpr uint16_t kPrimary      = rgb(0xFF, 0xFF, 0xFF);
constexpr uint16_t kSecondary    = rgb(0xB3, 0xB3, 0xB3);
constexpr uint16_t kTertiary     = rgb(0x6E, 0x6E, 0x73);
constexpr uint16_t kAccent       = rgb(0x1D, 0xB9, 0x54);  // Spotify green
constexpr uint16_t kTrack        = rgb(0x3A, 0x3A, 0x3D);
constexpr uint16_t kArtPlacehold = rgb(0x1C, 0x1C, 0x20);

// --- layout (logical 640 x 172) -------------------------------------------
constexpr int16_t kArtX = 11, kArtY = 11, kArtSize = albumart::kSize;

constexpr int16_t kTextX = 176;
constexpr int16_t kTextWidth = 300;
constexpr int16_t kTitleBaseline = 46;
constexpr int16_t kArtistBaseline = 76;
constexpr int16_t kAlbumBaseline = 98;

constexpr int16_t kProgressY = 116, kProgressH = 6;
constexpr int16_t kTimeBaseline = 138;

// Text block region cleared as a unit when the track changes.
constexpr int16_t kMetaTop = 20, kMetaHeight = 86;

constexpr int16_t kButtonCenterY = 76;
constexpr int16_t kButtonRadius = 23;   // drawn chip
constexpr int16_t kPrevCx = 512, kPlayCx = 560, kNextCx = 608;

constexpr int16_t kStatusRight = 628, kStatusBaseline = 156;

struct Rendered {
    bool valid = false;
    char title[128] = {};
    char artist[128] = {};
    char album[128] = {};
    char device_name[64] = {};
    char track_uri[64] = {};
    app::LikeState like_state = app::LikeState::Ready;
    bool has_track = false;
    bool is_playing = false;
    Status status = Status::Booting;
    uint32_t art_generation = 0;
    bool art_matched = false;
    uint32_t elapsed_seconds = 0xFFFFFFFF;
    uint32_t duration_ms = 0;
    Button pressed = Button::None;
    bool battery_present = false;
    uint8_t battery_percent = 0;
};
Rendered last;

const char *statusMessage(Status status) {
    switch (status) {
        case Status::Booting:        return "Starting up";
        case Status::WifiConnecting: return "Connecting to WiFi";
        case Status::SetupRequired:  return "Connect Spotify";
        case Status::Authorizing:    return "Signing in to Spotify";
        case Status::NoActiveDevice: return "Nothing playing";
        case Status::AuthFailed:     return "Sign-in expired";
        case Status::NetworkError:   return "Spotify unreachable";
        case Status::Playing:        return "";
    }
    return "";
}

// Written into a caller-owned buffer because the idle hint names the device.
void statusHint(const NowPlaying &now, char *out, size_t size) {
    switch (now.status) {
        case Status::NoActiveDevice:
            if (now.device_name[0]) {
                snprintf(out, size, "Tap play to resume on %s", now.device_name);
            } else {
                strlcpy(out, "Start a track on any Spotify device", size);
            }
            return;
        case Status::AuthFailed:   strlcpy(out, "Open setup on your computer", size); return;
        case Status::SetupRequired:strlcpy(out, "On your PC: 127.0.0.1:8888", size); return;
        case Status::NetworkError: strlcpy(out, "Retrying", size); return;
        default:                   out[0] = '\0'; return;
    }
}

int16_t buttonCenterX(Button button) {
    switch (button) {
        case Button::Previous:  return kPrevCx;
        case Button::PlayPause: return kPlayCx;
        case Button::Next:      return kNextCx;
        default:                return -1000;
    }
}

void drawArt(bool matches_current_track) {
    const uint16_t *pixels = albumart::pixels();
    if (pixels && matches_current_track) {
        screen::drawBitmapRgb565(kArtX, kArtY, pixels, kArtSize, kArtSize);
        return;
    }
    // No cover yet: a plain tile reads better than a stale one from another track.
    screen::fillRoundRect(kArtX, kArtY, kArtSize, kArtSize, 6, kArtPlacehold);
    screen::drawText(&FontSmall, kArtX + 46, kArtY + kArtSize / 2 + 4, kTertiary, "no art", 80);
}

void drawMeta(const NowPlaying &now) {
    screen::fillRect(kTextX, kMetaTop, kTextWidth, kMetaHeight, kBackground);
    if (now.has_track) {
        screen::drawText(&FontTitle, kTextX, kTitleBaseline, kPrimary, now.title, kTextWidth);
        screen::drawText(&FontBody, kTextX, kArtistBaseline, kSecondary, now.artist, kTextWidth);
        screen::drawText(&FontSmall, kTextX, kAlbumBaseline, kTertiary, now.album, kTextWidth);
    } else {
        char hint[96];
        statusHint(now, hint, sizeof(hint));
        screen::drawText(&FontTitle, kTextX, kTitleBaseline, kPrimary,
                         statusMessage(now.status), kTextWidth);
        screen::drawText(&FontSmall, kTextX, kArtistBaseline, kTertiary, hint, kTextWidth);
    }
}

void drawProgress(const NowPlaying &now, uint32_t elapsed_ms) {
    screen::fillRect(kTextX, kProgressY, kTextWidth, kProgressH, kBackground);
    screen::fillRect(kTextX, kTimeBaseline - FontSmall.yAdvance, kTextWidth,
                     FontSmall.yAdvance + 4, kBackground);
    if (!now.has_track) return;

    screen::fillRoundRect(kTextX, kProgressY, kTextWidth, kProgressH, kProgressH / 2, kTrack);
    if (now.duration_ms > 0) {
        const int16_t filled = static_cast<int16_t>(
            static_cast<uint64_t>(kTextWidth) * elapsed_ms / now.duration_ms);
        if (filled > 1) {
            screen::fillRoundRect(kTextX, kProgressY, filled, kProgressH, kProgressH / 2, kAccent);
        }
    }

    char buffer[16];
    text::formatDuration(elapsed_ms, buffer, sizeof(buffer));
    screen::drawText(&FontSmall, kTextX, kTimeBaseline, kSecondary, buffer, 80);
    text::formatDuration(now.duration_ms, buffer, sizeof(buffer));
    screen::drawTextRight(&FontSmall, kTextX + kTextWidth, kTimeBaseline, kTertiary, buffer);
}

void drawTransportIcon(Button button, int16_t cx, bool is_playing, uint16_t color) {
    Arduino_GFX &g = screen::gfx();
    const int16_t cy = kButtonCenterY;

    switch (button) {
        case Button::Previous:
            g.fillRect(cx - 11, cy - 9, 3, 18, color);
            g.fillTriangle(cx + 11, cy - 10, cx + 11, cy + 10, cx - 6, cy, color);
            break;
        case Button::Next:
            g.fillRect(cx + 8, cy - 9, 3, 18, color);
            g.fillTriangle(cx - 11, cy - 10, cx - 11, cy + 10, cx + 6, cy, color);
            break;
        case Button::PlayPause:
            if (is_playing) {
                g.fillRect(cx - 8, cy - 11, 5, 22, color);
                g.fillRect(cx + 3, cy - 11, 5, 22, color);
            } else {
                g.fillTriangle(cx - 7, cy - 12, cx - 7, cy + 12, cx + 11, cy, color);
            }
            break;
        default:
            break;
    }
}

void drawButton(Button button, bool is_playing, bool pressed, bool enabled) {
    const int16_t cx = buttonCenterX(button);
    const int16_t x = cx - kButtonRadius;
    const int16_t y = kButtonCenterY - kButtonRadius;
    const int16_t size = kButtonRadius * 2;

    screen::fillRect(x, y, size, size, kBackground);
    if (pressed) {
        screen::fillRoundRect(x, y, size, size, kButtonRadius, kAccent);
    }
    const uint16_t color = pressed ? kBackground : (enabled ? kPrimary : kTertiary);
    drawTransportIcon(button, cx, is_playing, color);
    screen::markDirty(x, y, size, size);
}

void drawButtons(const NowPlaying &now, Button pressed) {
    const bool enabled = now.status == Status::Playing || now.status == Status::NoActiveDevice;
    drawButton(Button::Previous, now.is_playing, pressed == Button::Previous, enabled);
    drawButton(Button::PlayPause, now.is_playing, pressed == Button::PlayPause, enabled);
    drawButton(Button::Next, now.is_playing, pressed == Button::Next, enabled);
    const bool saveable = now.track_uri[0] && now.status == Status::Playing;
    const bool saved = now.like_state == app::LikeState::Saved;
    screen::fillRect(490, 130, 54, 42, kBackground);
    const bool failed = now.like_state == app::LikeState::Failed || now.like_state == app::LikeState::NeedsAuthorization;
    const uint16_t heart_color = failed ? rgb(0xFF,0x65,0x65)
        : saved ? kAccent : now.like_state == app::LikeState::Saving ? rgb(0xF0,0xC0,0x50)
        : saveable ? kPrimary : kTertiary;
    if (pressed == Button::Like) screen::fillRoundRect(495,132,42,38,12,kArtPlacehold);
    auto &g = screen::gfx();
    g.fillCircle(508, 143, 7, heart_color);
    g.fillCircle(522, 143, 7, heart_color);
    g.fillTriangle(501, 144, 529, 144, 515, 161, heart_color);
    screen::markDirty(490,130,54,42);
}

void drawStatusLine(const NowPlaying &now, const hardware::Battery &battery) {
    screen::fillRect(546, kStatusBaseline - FontSmall.yAdvance,
                     kStatusRight - 546, FontSmall.yAdvance + 4, kBackground);

    char line[48] = {};
    if (battery.present) {
        snprintf(line, sizeof(line), "%u%%", battery.percent);
    }
    if (now.status == Status::WifiConnecting || now.status == Status::NetworkError) {
        strlcat(line, battery.present ? "  offline" : "offline", sizeof(line));
    }
    if (line[0]) {
        screen::drawTextRight(&FontSmall, kStatusRight, kStatusBaseline, kTertiary, line);
    }
}

}  // namespace

void begin() {
    screen::fillRect(0, 0, screen::kWidth, screen::kHeight, kBackground);
    last = Rendered{};
}

Button hitTest(int16_t x, int16_t y) {
    return playerButtonAt(x,y);
}

void render(const NowPlaying &now, const hardware::Battery &battery, Button pressed) {
    const uint32_t elapsed_ms = now.elapsedMs();
    const uint32_t elapsed_seconds = elapsed_ms / 1000;
    const uint32_t art_generation = albumart::generation();
    // A cover from the previous track is worse than no cover at all.
    const bool art_matched = art_generation != 0 && art_generation == now.track_generation;

    const bool first = !last.valid;
    const bool meta_changed = first || now.has_track != last.has_track ||
                              now.status != last.status ||
                              strcmp(now.title, last.title) != 0 ||
                              strcmp(now.artist, last.artist) != 0 ||
                              strcmp(now.album, last.album) != 0 ||
                              strcmp(now.device_name, last.device_name) != 0;

    if (first) screen::fillRect(0, 0, screen::kWidth, screen::kHeight, kBackground);
    if (first || art_generation != last.art_generation || art_matched != last.art_matched ||
        meta_changed) {
        drawArt(art_matched);
    }
    if (meta_changed) drawMeta(now);
    if (first || meta_changed || elapsed_seconds != last.elapsed_seconds ||
        now.duration_ms != last.duration_ms) {
        drawProgress(now, elapsed_ms);
    }
    if (first || pressed != last.pressed || now.is_playing != last.is_playing ||
        now.status != last.status || now.like_state != last.like_state || strcmp(now.track_uri,last.track_uri) != 0) {
        drawButtons(now, pressed);
    }
    if (first || battery.percent != last.battery_percent ||
        battery.present != last.battery_present || now.status != last.status) {
        drawStatusLine(now, battery);
    }

    last.valid = true;
    strlcpy(last.title, now.title, sizeof(last.title));
    strlcpy(last.artist, now.artist, sizeof(last.artist));
    strlcpy(last.album, now.album, sizeof(last.album));
    strlcpy(last.device_name, now.device_name, sizeof(last.device_name));
    last.has_track = now.has_track;
    last.is_playing = now.is_playing;
    last.status = now.status;
    last.like_state = now.like_state;
    strlcpy(last.track_uri,now.track_uri,sizeof(last.track_uri));
    last.art_generation = art_generation;
    last.art_matched = art_matched;
    last.elapsed_seconds = elapsed_seconds;
    last.duration_ms = now.duration_ms;
    last.pressed = pressed;
    last.battery_present = battery.present;
    last.battery_percent = battery.percent;
}

const char *displayedTrackUri() { return last.track_uri; }

namespace {
void drawMenuClose(bool pressed) {
    screen::fillRoundRect(558,0,82,38,8,pressed ? kAccent : kTrack);
    auto &g = screen::gfx();
    for (int d=0;d<3;++d) {
        g.drawLine(589+d,9,607+d,27,pressed ? kBackground : kPrimary);
        g.drawLine(607+d,9,589+d,27,pressed ? kBackground : kPrimary);
    }
    screen::markDirty(558,0,82,38);
}
}

void drawQuickMenu(bool music, QuickAction pressed, const NowPlaying &now, uint8_t brightness) {
    screen::fillRect(0,0,screen::kWidth,screen::kHeight,kBackground);
    screen::drawText(&FontBody,16,24,kSecondary,music ? "Music controls" : "Device controls",400);
    drawMenuClose(pressed == QuickAction::Close);
    for (int slot = 0; slot < 3; ++slot) screen::fillRoundRect(16 + slot * 202,40,194,90,8,kArtPlacehold);
    auto centered = [](int cx, int baseline, const char *text, uint16_t color) {
        screen::drawText(&FontBody,cx - screen::measure(&FontBody,text)/2,baseline,color,text,190);
    };
    auto stepper = [&](int left, QuickAction minus, QuickAction plus) {
        screen::fillRoundRect(left+4,87,88,38,6,pressed == minus ? kAccent : kTrack);
        screen::fillRoundRect(left+102,87,88,38,6,pressed == plus ? kAccent : kTrack);
        centered(left+48,113,"-",pressed == minus ? kBackground : kPrimary);
        centered(left+146,113,"+",pressed == plus ? kBackground : kPrimary);
    };
    char label[32];
    if (music) {
        centered(113,61,"Volume",kSecondary);
        if (now.supports_volume && now.volume_percent >= 0) snprintf(label,sizeof(label),"%d%%",now.volume_percent);
        else strlcpy(label,"Unavailable",sizeof(label));
        centered(113,80,label,kPrimary);
        stepper(16,QuickAction::VolumeDown,QuickAction::VolumeUp);
        centered(315,65,"Shuffle",kSecondary);
        centered(315,105,now.shuffle ? "On" : "Off",now.shuffle ? kAccent : kPrimary);
        centered(517,65,"Repeat",kSecondary);
        centered(517,105,now.repeat_mode == 2 ? "One song" : now.repeat_mode == 1 ? "All" : "Off",now.repeat_mode ? kAccent : kPrimary);
        const bool error = now.control_result < 0 || now.control_result >= 300;
        const char *hint = now.control_pending ? "Updating..." : error ? "Couldn't apply change. Try again or choose another device."
            : now.control_result >= 200 ? "Updated. Tap X to return to your music." : "Volume changes by 5%. Repeat cycles Off / All / One song.";
        screen::drawText(&FontSmall,16,158,error ? rgb(0xFF,0x65,0x65) : kTertiary,hint,608);
    } else {
        centered(113,78,"Choose",kPrimary);
        centered(113,104,"device",kPrimary);
        centered(315,61,"Brightness",kSecondary);
        snprintf(label,sizeof(label),"%u%%",brightness);
        centered(315,80,label,kPrimary);
        stepper(218,QuickAction::BrightDown,QuickAction::BrightUp);
        centered(517,90,"Shut down",kPrimary);
        screen::drawText(&FontSmall,16,158,kTertiary,"Tap X to return. Swipe top left for music controls.",608);
    }
}

int playlistRowAt(int16_t x, int16_t y) {
    return playlistShortcutAt(app::kPlaylistCount,x,y);
}

void drawPlaylists(int pressed, const NowPlaying &now) {
    screen::fillRect(0,0,screen::kWidth,screen::kHeight,kBackground);
    const bool playlist_result = now.control_kind == static_cast<uint8_t>(spotify::Command::PlayPlaylist);
    const bool error = playlist_result && (now.control_result < 0 || now.control_result >= 300);
    const char *heading = playlist_result && now.control_pending ? "Starting with shuffle..." : error ? "Couldn't start - choose a device"
        : playlist_result && now.control_result >= 200 ? "Your playlists - shuffle on" : "Your playlists";
    screen::drawText(&FontBody,16,24,error ? rgb(0xFF,0x65,0x65) : kSecondary,heading,520);
    drawMenuClose(pressed == -2);
    if (app::kPlaylistCount == 0) {
        screen::drawText(&FontBody,16,80,kPrimary,"No playlists added yet",600);
        screen::drawText(&FontSmall,16,106,kTertiary,"Your chosen playlists will appear here.",600);
    }
    for (int index = 0; index < app::kPlaylistCount && index < 8; ++index) {
        const int y = 40 + (index / 2) * 26;
        const int x = index % 2 ? 330 : 16;
        screen::fillRoundRect(x,y,294,24,4,pressed == index ? kAccent : kArtPlacehold);
        screen::drawText(&FontBody,x+14,y+18,pressed == index ? kBackground : kPrimary,app::kPlaylistShortcuts[index].label,266);
    }
    if (app::kPlaylistCount > 8) {
        screen::fillRoundRect(16,146,608,26,6,pressed == 8 ? kAccent : kArtPlacehold);
        const char *label = app::kPlaylistShortcuts[8].label;
        screen::drawText(&FontBody,(640-screen::measure(&FontBody,label))/2,165,pressed == 8 ? kBackground : kPrimary,label,600);
    }
}

}  // namespace ui

// --- device picker --------------------------------------------------------

namespace ui {
namespace {

constexpr int16_t kPickerHeaderBaseline = 22;
constexpr int16_t kRowTop = 40;
constexpr int16_t kRowHeight = 40;
constexpr int16_t kRowPitch = 42;
constexpr uint8_t kRowsPerPage = 3;
constexpr int16_t kRowLeft = 8, kRowRight = 632;
constexpr int16_t kCloseLeft = 558, kCloseRight = 639, kCloseTop = 0, kCloseBottom = 38;

bool picker_open = false;
uint8_t picker_page = 0;

struct RenderedPicker {
    bool valid = false;
    uint32_t generation = 0;
    uint8_t page = 0;
    uint8_t count = 0;
    bool loading = false;
    int pressed = kPickerNone;
    char preferred[64] = {};
};
RenderedPicker last_picker;

int16_t rowTop(uint8_t slot) { return kRowTop + slot * kRowPitch; }

void drawCloseIcon(uint16_t color) {
    drawMenuClose(color == kAccent);
}

void drawRow(const spotify::Device &device, uint8_t slot, bool pressed, bool preferred) {
    const int16_t y = rowTop(slot);
    const int16_t width = kRowRight - kRowLeft;

    screen::fillRoundRect(kRowLeft, y, width, kRowHeight, 6, pressed ? kAccent : kArtPlacehold);

    // A filled dot marks the device currently playing; a ring marks the one the
    // board wakes by default.
    Arduino_GFX &g = screen::gfx();
    const int16_t dot_x = kRowLeft + 22, dot_y = y + kRowHeight / 2;
    const uint16_t mark = pressed ? kBackground : kAccent;
    if (device.active) {
        g.fillCircle(dot_x, dot_y, 5, mark);
    } else if (preferred) {
        g.drawCircle(dot_x, dot_y, 5, mark);
    }

    const bool selectable = !device.restricted;
    const uint16_t name_color = pressed      ? kBackground
                                : selectable ? kPrimary
                                             : kTertiary;
    screen::drawText(&FontBody, kRowLeft + 38, y + 28, name_color, device.name, 400);

    const char *detail = device.restricted ? "not controllable" : device.type;
    screen::drawTextRight(&FontSmall, kRowRight - 16, y + 28,
                          pressed ? kBackground : kTertiary, detail);
}

}  // namespace

void openPicker() {
    picker_open = true;
    picker_page = 0;
    last_picker = RenderedPicker{};
}

void closePicker() {
    picker_open = false;
    last = Rendered{};  // force a full repaint of the now-playing view
}

bool pickerOpen() { return picker_open; }

void nextPickerPage() {
    ++picker_page;
    last_picker = RenderedPicker{};
}

int pickerRowAt(const spotify::DeviceList &list, int16_t x, int16_t y) {
    if (x >= kCloseLeft && x <= kCloseRight && y >= kCloseTop && y <= kCloseBottom) {
        return kPickerClose;
    }
    if (y >= 2 && y <= 30 && x >= 340 && x <= 445) return kPickerRefresh;
    if (y >= 2 && y <= 30 && x >= 458 && x <= 550 && list.count > kRowsPerPage) return kPickerNextPage;
    if (x < kRowLeft || x > kRowRight) return kPickerNone;

    for (uint8_t slot = 0; slot < kRowsPerPage; ++slot) {
        const int16_t top = rowTop(slot);
        if (y < top || y > top + kRowHeight) continue;
        const int index = picker_page * kRowsPerPage + slot;
        if (index >= list.count) return kPickerNone;
        if (list.items[index].restricted) return kPickerNone;  // cannot be selected
        return index;
    }
    return kPickerNone;
}

void invalidate() {
    last = Rendered{};
    last_picker = RenderedPicker{};
}

void drawPowerPrompt(uint8_t percent) {
    constexpr int16_t kBarWidth = 220, kBarHeight = 6;
    const int16_t bar_x = (screen::kWidth - kBarWidth) / 2;

    screen::fillRect(0, 0, screen::kWidth, screen::kHeight, kBackground);

    const char *title = "Powering off";
    screen::drawText(&FontTitle, (screen::kWidth - screen::measure(&FontTitle, title)) / 2, 78,
                     kPrimary, title, screen::kWidth);

    const char *hint = "Release to cancel";
    screen::drawText(&FontSmall, (screen::kWidth - screen::measure(&FontSmall, hint)) / 2, 102,
                     kTertiary, hint, screen::kWidth);

    screen::fillRoundRect(bar_x, 118, kBarWidth, kBarHeight, kBarHeight / 2, kTrack);
    const int16_t filled = kBarWidth * percent / 100;
    if (filled > 1) {
        screen::fillRoundRect(bar_x, 118, filled, kBarHeight, kBarHeight / 2, kAccent);
    }
}

void renderPicker(const spotify::DeviceList &list, int pressed_index) {
    // Wrap the page here rather than in nextPickerPage(), which does not know
    // how many devices there are.
    const uint8_t pages = list.count == 0 ? 1 : (list.count + kRowsPerPage - 1) / kRowsPerPage;
    if (picker_page >= pages) picker_page = 0;

    const bool changed = !last_picker.valid || last_picker.generation != list.generation ||
                         last_picker.page != picker_page || last_picker.count != list.count ||
                         last_picker.loading != list.loading ||
                         last_picker.pressed != pressed_index ||
                         strcmp(last_picker.preferred, list.preferred_id) != 0;
    if (!changed) return;

    screen::fillRect(0, 0, screen::kWidth, screen::kHeight, kBackground);
    screen::drawText(&FontBody, 16, kPickerHeaderBaseline, kSecondary, "Play on", 300);
    screen::drawText(&FontSmall,350,kPickerHeaderBaseline,kSecondary,"Refresh",90);
    if (pages > 1) {
        char label[16];
        snprintf(label, sizeof(label), "%u/%u", picker_page + 1, pages);
        screen::drawText(&FontSmall,458,kPickerHeaderBaseline,kTertiary,"Next",50);
        screen::drawTextRight(&FontSmall, kCloseLeft - 6, kPickerHeaderBaseline, kTertiary, label);
    }
    drawCloseIcon(pressed_index == kPickerClose ? kAccent : kSecondary);

    if (list.count == 0) {
        const char *message = list.loading ? "Looking for devices..."
                                           : "No devices visible to Spotify";
        screen::drawText(&FontBody, 16, rowTop(0) + 28, kTertiary, message, 600);
        screen::drawText(&FontSmall, 16, rowTop(1) + 20, kTertiary,
                         "Open Spotify on a phone, speaker or computer, then tap Refresh.",
                         608);
    } else {
        for (uint8_t slot = 0; slot < kRowsPerPage; ++slot) {
            const int index = picker_page * kRowsPerPage + slot;
            if (index >= list.count) break;
            drawRow(list.items[index], slot, pressed_index == index,
                    strcmp(list.items[index].id, list.preferred_id) == 0);
        }
    }

    last_picker.valid = true;
    last_picker.generation = list.generation;
    last_picker.page = picker_page;
    last_picker.count = list.count;
    last_picker.loading = list.loading;
    last_picker.pressed = pressed_index;
    strlcpy(last_picker.preferred, list.preferred_id, sizeof(last_picker.preferred));
}

}  // namespace ui
