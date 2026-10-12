// ============================================================================
// WifiManager.cpp
// ============================================================================

#include "WifiManager.h"
#include <Arduino.h>
#include <WiFi.h>

WifiManager::WifiManager()
    : ssid(nullptr), password(nullptr),
      state(WifiState::IDLE), stateEnteredMs(0),
      retryCount(0), lastRssi(0), lastRssiPollMs(0) {}

void WifiManager::begin(const char* s, const char* p) {
    ssid = s;
    password = p;
    retryCount = 0;
    startConnecting(millis());
}

void WifiManager::startConnecting(uint32_t nowMs) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password); // non-blocking: kicks off the association, returns immediately
    state = WifiState::CONNECTING;
    stateEnteredMs = nowMs;
}

uint32_t WifiManager::currentBackoffMs() const {
    // Exponential backoff: base * 2^retryCount, capped.
    uint32_t backoff = WIFI_RETRY_BASE_MS;
    for (uint8_t i = 0; i < retryCount && backoff < WIFI_RETRY_MAX_MS; i++) backoff *= 2;
    return (backoff > WIFI_RETRY_MAX_MS) ? WIFI_RETRY_MAX_MS : backoff;
}

void WifiManager::update(uint32_t nowMs) {
    switch (state) {

        case WifiState::IDLE:
            // begin() not called yet, or explicitly idled; nothing to do.
            break;

        case WifiState::CONNECTING: {
            if (WiFi.status() == WL_CONNECTED) {
                state = WifiState::CONNECTED;
                stateEnteredMs = nowMs;
                retryCount = 0; // a successful connect forgives prior retry backoff
                lastRssi = (int8_t)WiFi.RSSI();
                lastRssiPollMs = nowMs;
            } else if (nowMs - stateEnteredMs >= WIFI_CONNECT_TIMEOUT_MS) {
                WiFi.disconnect(true);
                state = WifiState::RETRY_WAIT;
                stateEnteredMs = nowMs;
                if (retryCount < 255) retryCount++;
            }
            break;
        }

        case WifiState::CONNECTED: {
            if (WiFi.status() != WL_CONNECTED) {
                state = WifiState::RETRY_WAIT;
                stateEnteredMs = nowMs;
                if (retryCount < 255) retryCount++;
                break;
            }
            if (nowMs - lastRssiPollMs >= WIFI_RSSI_POLL_MS) {
                lastRssiPollMs = nowMs;
                lastRssi = (int8_t)WiFi.RSSI();
            }
            break;
        }

        case WifiState::RETRY_WAIT: {
            if (nowMs - stateEnteredMs >= currentBackoffMs()) {
                startConnecting(nowMs);
            }
            break;
        }
    }
}

WifiState WifiManager::getState()      const { return state; }
bool      WifiManager::isConnected()   const { return state == WifiState::CONNECTED; }
int8_t    WifiManager::getRssi()       const { return lastRssi; }
uint8_t   WifiManager::getRetryCount() const { return retryCount; }
