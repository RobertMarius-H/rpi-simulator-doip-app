#include "doip_routing.h"

#include "doip_config.h"

DoipRouting& DoipRouting::getInstance()
{
    static DoipRouting instance;
    return instance;
}

bool DoipRouting::validRequest(const DoipProtocol::Frame& frame) const
{
    return ResponseCode::ACTIVATED == responseCode(frame, true);
}

DoipRouting::ResponseCode DoipRouting::responseCode(
    const DoipProtocol::Frame& frame, bool capacityAvailable) const
{
    if (static_cast<std::size_t>(DoipProtocol::Layout::ROUTING_REQUEST_SIZE)
        != frame.payload.size())
    {
        return ResponseCode::UNKNOWN_SOURCE;
    }
    const std::uint16_t source = DoipProtocol::getInstance().readU16(frame.payload.data());
    ResponseCode code = ResponseCode::ACTIVATED;
    if (static_cast<std::uint16_t>(DoipConfig::Defaults::TESTER_LOGICAL_ADDRESS) != source)
    {
        code = ResponseCode::UNKNOWN_SOURCE;
    }
    else if (static_cast<std::uint8_t>(Activation::DEFAULT_TYPE)
             != frame.payload[static_cast<std::size_t>(Layout::ACTIVATION_TYPE_OFFSET)])
    {
        code = ResponseCode::UNSUPPORTED_ACTIVATION;
    }
    else if (!capacityAvailable)
    {
        code = ResponseCode::CAPACITY_FULL;
    }
    return code;
}

bool DoipRouting::activate(int fd, const DoipProtocol::Frame& frame,
                           bool capacityAvailable) const
{
    if (static_cast<std::size_t>(DoipProtocol::Layout::ROUTING_REQUEST_SIZE)
        != frame.payload.size())
    {
        return false;
    }
    auto& protocol = DoipProtocol::getInstance();
    const std::uint16_t source = protocol.readU16(frame.payload.data());
    const ResponseCode code = responseCode(frame, capacityAvailable);
    return protocol.sendRoutingResponse(fd, source, static_cast<std::uint8_t>(code))
           && (ResponseCode::ACTIVATED == code);
}
