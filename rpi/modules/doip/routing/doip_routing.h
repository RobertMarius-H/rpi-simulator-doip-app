#ifndef PSI_DOIP_ROUTING_H
#define PSI_DOIP_ROUTING_H

#include "doip_protocol.h"

// DoIP routing activation policy for the configured tester and ECU addresses.
class DoipRouting
{
private:
    enum class ResponseCode : std::uint8_t
    {
        ACTIVATED = 0x10,
        UNKNOWN_SOURCE = 0x00,
        CAPACITY_FULL = 0x01,
        UNSUPPORTED_ACTIVATION = 0x06
    };
    enum class Activation : std::uint8_t
    {
        DEFAULT_TYPE = 0x00
    };
    enum class Layout : std::size_t
    {
        ACTIVATION_TYPE_OFFSET = 2
    };

    DoipRouting() = default;
    ResponseCode responseCode(const DoipProtocol::Frame& frame,
                              bool capacityAvailable) const;

public:
    static DoipRouting& getInstance();
    DoipRouting(const DoipRouting&) = delete;
    DoipRouting& operator=(const DoipRouting&) = delete;

    bool activate(int fd, const DoipProtocol::Frame& frame,
                  bool capacityAvailable = true) const;
    bool validRequest(const DoipProtocol::Frame& frame) const;
};

#endif
