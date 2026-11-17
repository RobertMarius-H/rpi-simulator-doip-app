#include "uds_dispatcher.h"

#include <cstddef>

#include "rte_update.h"
#include "uds_download_service.h"
#include "uds_routine_service.h"

UdsDispatcher& UdsDispatcher::getInstance()
{
    static UdsDispatcher instance;
    return instance;
}

void UdsDispatcher::resetConnection()
{
    RteUpdate::getInstance().abortUnverified();
    UdsRoutineService::getInstance().resetConnection();
    DcmSession::getInstance().resetConnection();
}

bool UdsDispatcher::responsePending() const
{
    return UdsRoutineService::getInstance().responsePending();
}

std::vector<std::uint8_t> UdsDispatcher::pollResponse()
{
    return UdsRoutineService::getInstance().pollResponse();
}

std::vector<std::uint8_t> UdsDispatcher::pendingNotification() const
{
    return UdsRoutineService::getInstance().pendingNotification();
}

std::vector<std::uint8_t> UdsDispatcher::handle(const std::uint8_t* uds,
                                                std::size_t size,
                                                UdsSecurity& security)
{
    if ((nullptr == uds) || (0 == size))
    {
        return {};
    }
    if (static_cast<std::uint8_t>(Service::SECURITY_ACCESS) == uds[0])
    {
        auto& session = DcmSession::getInstance();
        return security.handle(uds, size,
                               static_cast<std::uint8_t>(DcmSession::SessionType::PROGRAMMING)
                                   == session.currentSession(),
                               session.securitySession());
    }
    if ((static_cast<std::uint8_t>(Service::REQUEST_DOWNLOAD) == uds[0])
        || (static_cast<std::uint8_t>(Service::TRANSFER_DATA) == uds[0])
        || (static_cast<std::uint8_t>(Service::TRANSFER_EXIT) == uds[0]))
    {
        const auto& session = DcmSession::getInstance();
        return UdsDownloadService::getInstance().handle(
            uds, size,
            static_cast<std::uint8_t>(DcmSession::SessionType::PROGRAMMING)
                == session.currentSession(),
            session.securitySession().unlocked);
    }
    if (static_cast<std::uint8_t>(Service::ROUTINE_CONTROL) == uds[0])
    {
        const auto& session = DcmSession::getInstance();
        return UdsRoutineService::getInstance().handle(
            uds, size,
            static_cast<std::uint8_t>(DcmSession::SessionType::PROGRAMMING)
                == session.currentSession(),
            session.securitySession().unlocked);
    }
    if (static_cast<std::uint8_t>(Service::SESSION_CONTROL) != uds[0])
    {
        return {static_cast<std::uint8_t>(Service::NEGATIVE_RESPONSE),
                uds[0],
                static_cast<std::uint8_t>(NegativeCode::SERVICE_NOT_SUPPORTED)};
    }
    if (2 != size)
    {
        return {static_cast<std::uint8_t>(Service::NEGATIVE_RESPONSE),
                static_cast<std::uint8_t>(Service::SESSION_CONTROL),
                static_cast<std::uint8_t>(NegativeCode::INVALID_LENGTH)};
    }
    if ((static_cast<std::uint8_t>(DcmSession::SessionType::DEFAULT) != uds[1])
        && (static_cast<std::uint8_t>(DcmSession::SessionType::PROGRAMMING) != uds[1]))
    {
        return {static_cast<std::uint8_t>(Service::NEGATIVE_RESPONSE),
                static_cast<std::uint8_t>(Service::SESSION_CONTROL),
                static_cast<std::uint8_t>(NegativeCode::SUBFUNCTION_NOT_SUPPORTED)};
    }

    RteUpdate::getInstance().abortUnverified();
    DcmSession::getInstance().setSession(uds[1]);

    const std::uint16_t p2 = static_cast<std::uint16_t>(Timing::P2_SERVER_MAX_MS);
    const std::uint16_t p2StarUnits =
        static_cast<std::uint16_t>(Timing::P2_STAR_SERVER_MAX_IN_10MS);
    return {static_cast<std::uint8_t>(Service::SESSION_RESPONSE),
            uds[1],
            static_cast<std::uint8_t>(p2 >> 8),
            static_cast<std::uint8_t>(p2),
            static_cast<std::uint8_t>(p2StarUnits >> 8),
            static_cast<std::uint8_t>(p2StarUnits)};
}

