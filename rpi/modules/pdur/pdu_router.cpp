#include "pdu_router.h"

#include "doip_config.h"
#include "uds_dispatcher.h"
#include "uds_security.h"

PduRouter& PduRouter::getInstance()
{
    static PduRouter instance;
    return instance;
}

PduRouter::RouteStatus PduRouter::validate(std::uint16_t source,
                                            std::uint16_t target) const
{
    if (static_cast<std::uint16_t>(DoipConfig::Defaults::TESTER_LOGICAL_ADDRESS) != source)
    {
        return RouteStatus::INVALID_SOURCE;
    }
    if (static_cast<std::uint16_t>(DoipConfig::Defaults::PI_LOGICAL_ADDRESS) != target)
    {
        return RouteStatus::INVALID_TARGET;
    }
    return RouteStatus::ACCEPTED;
}

std::vector<std::uint8_t> PduRouter::forward(const std::vector<std::uint8_t>& payload)
{
    const std::size_t addressBytes =
        static_cast<std::size_t>(DoipConfig::Layout::LOGICAL_ADDRESS_BYTES);
    if (payload.size() <= addressBytes)
    {
        return {};
    }
    return UdsDispatcher::getInstance().handle(
        payload.data() + addressBytes,
        payload.size() - addressBytes,
        UdsSecurity::getInstance());
}

void PduRouter::resetConnection()
{
    UdsDispatcher::getInstance().resetConnection();
}

bool PduRouter::responsePending() const
{
    return UdsDispatcher::getInstance().responsePending();
}

std::vector<std::uint8_t> PduRouter::pollResponse()
{
    return UdsDispatcher::getInstance().pollResponse();
}

std::vector<std::uint8_t> PduRouter::pendingNotification() const
{
    return UdsDispatcher::getInstance().pendingNotification();
}
