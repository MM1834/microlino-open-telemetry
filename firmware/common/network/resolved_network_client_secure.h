#pragma once

#include <Network.h>
#include <WiFiClientSecure.h>

// NetworkManager::hostByName() in Arduino-ESP32 3.3.8 mutates lwIP's DNS
// state.  That operation must run on the Arduino loop task, not on an
// application worker.  This client keeps hostname verification/SNI intact but
// uses an address resolved beforehand by the loop task.
class ResolvedNetworkClientSecure final : public NetworkClientSecure {
public:
    void configureServerIdentity(const String &hostname, const String &rootCa,
                                 const String &certificate = String(),
                                 const String &privateKey = String())
    {
        hostname_ = hostname;
        rootCa_ = rootCa;
        certificate_ = certificate;
        privateKey_ = privateKey;
        resolved_ = false;
    }

    bool resolveOnLoopTask()
    {
        if (resolved_) return true;
        if (hostname_.isEmpty()) return false;
        IPAddress address;
        if (!Network.hostByName(hostname_.c_str(), address)) {
            resolved_ = false;
            return false;
        }
        address_ = address;
        resolved_ = true;
        return true;
    }

    void clearResolution() { resolved_ = false; }
    bool resolutionReady() const { return resolved_; }

    int connect(const char *host, uint16_t port) override
    {
        if (!resolved_ || hostname_ != host) return 0;
        return NetworkClientSecure::connect(
            address_, port, hostname_.c_str(), rootCa_.c_str(),
            certificate_.isEmpty() ? nullptr : certificate_.c_str(),
            privateKey_.isEmpty() ? nullptr : privateKey_.c_str());
    }

    int connect(const char *host, uint16_t port, int32_t timeout) override
    {
        setTimeout(timeout);
        return connect(host, port);
    }

private:
    String hostname_;
    String rootCa_;
    String certificate_;
    String privateKey_;
    IPAddress address_;
    bool resolved_ = false;
};
