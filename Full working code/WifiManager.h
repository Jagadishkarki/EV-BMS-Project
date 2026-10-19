#pragma once
// ============================================================================
// WifiManager.h
// Task 5: non-blocking Wi-Fi connection state machine. No delay(), no
// WiFi.waitForConnectResult() blocking spins — only WiFi.status() polling
// against millis() timers, with increasing backoff between retries.
// ============================================================================

#include <stdint.h>
#include "TelemetryConfig.h"

enum class WifiState : uint8_t {
    IDLE = 0,
    CONNECTING,
    CONNECTED,
    RETRY_WAIT
};

class WifiManager {
public:
    WifiManager();

    void begin(const char* ssid, const char* password);

    // Call every loop(). Non-blocking.
    void update(uint32_t nowMs);

    WifiState getState()     const;
    bool      isConnected()  const;
    int8_t    getRssi()      const; // dBm; 0 if not currently known
    uint8_t   getRetryCount() const;

private:
    const char* ssid;
    const char* password;

    WifiState state;
    uint32_t  stateEnteredMs;
    uint8_t   retryCount;
    int8_t    lastRssi;
    uint32_t  lastRssiPollMs;

    uint32_t currentBackoffMs() const;
    void     startConnecting(uint32_t nowMs);
};
