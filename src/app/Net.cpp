#include "Net.h"

#include <WiFi.h>
#include <time.h>

#include "../Secrets.h"

extern const uint8_t kCertBundleStart[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t kCertBundleEnd[] asm("_binary_x509_crt_bundle_end");

namespace net {
namespace {
uint32_t last_attempt_ms = 0;
constexpr uint32_t kRetryIntervalMs = 5000;
}  // namespace

void secure(WiFiClientSecure &client) {
    client.setCACertBundle(kCertBundleStart,
                           static_cast<size_t>(kCertBundleEnd - kCertBundleStart));
    client.setHandshakeTimeout(15);
}

bool ensureWifi() {
    if (WiFi.status() == WL_CONNECTED) return true;

    const uint32_t now = millis();
    if (last_attempt_ms != 0 && now - last_attempt_ms < kRetryIntervalMs) return false;
    last_attempt_ms = now;

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(true);  // the UI is idle most of the time; let the radio doze
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    const uint32_t deadline = millis() + 12000;
    while (WiFi.status() != WL_CONNECTED && millis() < deadline) delay(100);

    if (WiFi.status() == WL_CONNECTED) {
        log_i("wifi up, ip %s, rssi %d", WiFi.localIP().toString().c_str(), WiFi.RSSI());
        // Validated TLS requires a real clock after a cold boot.
        configTime(0, 0, "pool.ntp.org", "time.google.com");
        const uint32_t clock_deadline = millis() + 15000;
        while (time(nullptr) < 1700000000 && millis() < clock_deadline) delay(100);
        if (time(nullptr) < 1700000000) {
            log_w("waiting for clock synchronization before Spotify TLS");
            WiFi.disconnect();
            return false;
        }
        return true;
    }
    log_w("wifi connect failed");
    // Stop the in-flight retry before the next WiFi.begin reconfigures STA.
    WiFi.disconnect();
    return false;
}

}  // namespace net
