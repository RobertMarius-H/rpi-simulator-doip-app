#include "doip_admission.h"

#include <chrono>

#include "doip_config.h"
#include "doip_routing.h"

DoipAdmission& DoipAdmission::getInstance()
{
    static DoipAdmission instance;
    return instance;
}

bool DoipAdmission::readRequest(int fd, std::uint32_t maxPayload,
                                DoipProtocol::Frame& frame) const
{
    auto& protocol = DoipProtocol::getInstance();
    const auto deadline = DoipProtocol::Clock::now()
        + std::chrono::milliseconds(static_cast<std::uint32_t>(
            DoipConfig::Defaults::ROUTING_ACTIVATION_TIMEOUT_MS));
    const auto result = protocol.readFrame(fd, maxPayload, deadline, frame, deadline);
    switch (result.frameError)
    {
        case DoipProtocol::FrameError::INVALID_VERSION:
            protocol.sendNack(fd, static_cast<std::uint8_t>(Nack::INVALID_VERSION));
            break;
        case DoipProtocol::FrameError::TOO_LARGE:
            protocol.sendNack(fd, static_cast<std::uint8_t>(Nack::TOO_LARGE));
            break;
        case DoipProtocol::FrameError::INVALID_LENGTH:
            protocol.sendNack(fd, static_cast<std::uint8_t>(Nack::INVALID_LENGTH));
            break;
        case DoipProtocol::FrameError::NONE:
        default:
            break;
    }
    return (TransportReadStatus::COMPLETE == result.transport)
        && (DoipProtocol::FrameError::NONE == result.frameError)
        && (DoipProtocol::PayloadType::ROUTING_REQUEST == frame.type);
}

bool DoipAdmission::reply(int fd, const DoipProtocol::Frame& frame,
                          bool capacityAvailable) const
{
    return DoipRouting::getInstance().activate(fd, frame, capacityAvailable);
}

bool DoipAdmission::eligible(const DoipProtocol::Frame& frame) const
{
    return DoipRouting::getInstance().validRequest(frame);
}
