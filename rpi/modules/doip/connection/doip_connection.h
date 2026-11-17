#ifndef PSI_DOIP_CONNECTION_H
#define PSI_DOIP_CONNECTION_H

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>

#include "doip_protocol.h"

// DoIP connection control: routing activation and diagnostic addressing.
class DoipConnection
{
public:
    enum class AliveStatus
    {
        IDLE,
        REQUESTED,
        WAITING,
        ALIVE,
        STALE
    };

    static DoipConnection& getInstance();
    DoipConnection(const DoipConnection&) = delete;
    DoipConnection& operator=(const DoipConnection&) = delete;
    void serve(int fd, std::uint32_t maxPayload);
    void resetConnection();
    void requestAliveCheck();
    AliveStatus aliveStatus() const { return pAliveStatus.load(); }
    void clearAliveResult();

private:
    enum class GenericNack : std::uint8_t
    {
        INVALID_VERSION = 0x00,
        UNKNOWN_TYPE = 0x01,
        TOO_LARGE = 0x02,
        INVALID_LENGTH = 0x04
    };
    enum class DiagnosticCode : std::uint8_t
    {
        OK = 0x00,
        INVALID_SOURCE = 0x02,
        INVALID_TARGET = 0x03
    };
    enum class PendingTiming : std::uint32_t
    {
        POLL_MS = 25,
        NOTIFICATION_MS = 4'000
    };

    DoipConnection() = default;
    bool serviceAliveState(int fd, std::chrono::steady_clock::time_point& deadline);
    bool acceptAliveResponse(const DoipProtocol::Frame& frame);
    std::atomic<bool> pAdmissionRequested{false};
    std::atomic<AliveStatus> pAliveStatus{AliveStatus::IDLE};
};

#endif
