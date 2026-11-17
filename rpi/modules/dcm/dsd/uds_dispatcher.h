#ifndef PSI_UDS_DISPATCHER_H
#define PSI_UDS_DISPATCHER_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "dcm_session.h"
#include "uds_security.h"

// DCM-like boundary: session selection and UDS service dispatch.
class UdsDispatcher
{
private:
    enum class Service : std::uint8_t
    {
        SESSION_CONTROL = 0x10,
        SECURITY_ACCESS = 0x27,
        ROUTINE_CONTROL = 0x31,
        REQUEST_DOWNLOAD = 0x34,
        TRANSFER_DATA = 0x36,
        TRANSFER_EXIT = 0x37,
        SESSION_RESPONSE = 0x50,
        NEGATIVE_RESPONSE = 0x7F
    };
    enum class NegativeCode : std::uint8_t
    {
        SERVICE_NOT_SUPPORTED = 0x11,
        SUBFUNCTION_NOT_SUPPORTED = 0x12,
        INVALID_LENGTH = 0x13
    };
    enum class Timing : std::uint16_t
    {
        P2_SERVER_MAX_MS = 500,
        P2_STAR_SERVER_MAX_IN_10MS = 500
    };

    UdsDispatcher() = default;

public:
    static UdsDispatcher& getInstance();
    UdsDispatcher(const UdsDispatcher&) = delete;
    UdsDispatcher& operator=(const UdsDispatcher&) = delete;

    void resetConnection();
    std::vector<std::uint8_t> handle(const std::uint8_t* uds, std::size_t size,
                                     UdsSecurity& security);
    bool responsePending() const;
    std::vector<std::uint8_t> pollResponse();
    std::vector<std::uint8_t> pendingNotification() const;
};

#endif
