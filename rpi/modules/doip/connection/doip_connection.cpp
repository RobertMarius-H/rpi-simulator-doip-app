#include "doip_connection.h"

#include <chrono>
#include <thread>
#include <vector>

#include <openssl/crypto.h>

#include "doip_config.h"
#include "doip_protocol.h"
#include "pdu_router.h"

DoipConnection& DoipConnection::getInstance()
{
    static DoipConnection instance;
    return instance;
}

void DoipConnection::resetConnection()
{
    pAdmissionRequested.store(false);
    pAliveStatus.store(AliveStatus::IDLE);
}

void DoipConnection::requestAliveCheck()
{
    pAdmissionRequested.store(true);
    AliveStatus expected = AliveStatus::IDLE;
    pAliveStatus.compare_exchange_strong(expected, AliveStatus::REQUESTED);
}

void DoipConnection::clearAliveResult()
{
    pAliveStatus.store(AliveStatus::IDLE);
    pAdmissionRequested.store(false);
}

bool DoipConnection::serviceAliveState(
    int fd, std::chrono::steady_clock::time_point& deadline)
{
    if (AliveStatus::REQUESTED == pAliveStatus.load())
    {
        if (!DoipProtocol::getInstance().sendAliveRequest(fd))
        {
            pAliveStatus.store(AliveStatus::STALE);
            return false;
        }
        deadline = DoipProtocol::Clock::now()
            + std::chrono::milliseconds(static_cast<std::uint32_t>(
                DoipConfig::Defaults::ALIVE_RESPONSE_MS));
        pAliveStatus.store(AliveStatus::WAITING);
    }
    if ((AliveStatus::WAITING == pAliveStatus.load())
        && (deadline <= DoipProtocol::Clock::now()))
    {
        pAliveStatus.store(AliveStatus::STALE);
        return false;
    }
    return AliveStatus::STALE != pAliveStatus.load();
}

bool DoipConnection::acceptAliveResponse(const DoipProtocol::Frame& frame)
{
    const bool valid = (static_cast<std::size_t>(DoipProtocol::Layout::ALIVE_RESPONSE_SIZE)
            == frame.payload.size())
        && (static_cast<std::uint16_t>(DoipConfig::Defaults::TESTER_LOGICAL_ADDRESS)
            == DoipProtocol::getInstance().readU16(frame.payload.data()));
    if (valid && (AliveStatus::WAITING == pAliveStatus.load()))
    {
        pAliveStatus.store(AliveStatus::ALIVE);
    }
    else if (!valid)
    {
        pAliveStatus.store(AliveStatus::STALE);
    }
    return valid;
}

void DoipConnection::serve(int fd, std::uint32_t maxPayload)
{
    auto& protocol = DoipProtocol::getInstance();
    auto lastActivity = DoipProtocol::Clock::now();
    auto aliveDeadline = DoipProtocol::Clock::time_point::max();

    while (true)
    {
        const bool idle = DoipProtocol::Clock::now() - lastActivity
            >= std::chrono::milliseconds(DoipConfig::getInstance().aliveIdleMs());
        const bool ownCheck = idle && (AliveStatus::IDLE == pAliveStatus.load());
        if (ownCheck)
        {
            AliveStatus expected = AliveStatus::IDLE;
            pAliveStatus.compare_exchange_strong(expected, AliveStatus::REQUESTED);
        }
        if (!serviceAliveState(fd, aliveDeadline))
        {
            return;
        }
        if (AliveStatus::ALIVE == pAliveStatus.load())
        {
            lastActivity = DoipProtocol::Clock::now();
            if (!pAdmissionRequested.load())
            {
                clearAliveResult();
            }
        }
        DoipProtocol::Frame frame;
        const auto readDeadline = DoipProtocol::Clock::now()
            + std::chrono::milliseconds(static_cast<std::uint32_t>(
                DoipConfig::Defaults::READ_POLL_MS));
        const DoipProtocol::ReadResult result = protocol.readFrame(fd, maxPayload,
            readDeadline, frame);
        if (DoipProtocol::FrameError::INVALID_VERSION == result.frameError)
        {
            protocol.sendNack(fd, static_cast<std::uint8_t>(GenericNack::INVALID_VERSION));
            return;
        }
        if (DoipProtocol::FrameError::TOO_LARGE == result.frameError)
        {
            protocol.sendNack(fd, static_cast<std::uint8_t>(GenericNack::TOO_LARGE));
            return;
        }
        if (DoipProtocol::FrameError::INVALID_LENGTH == result.frameError)
        {
            protocol.sendNack(fd, static_cast<std::uint8_t>(GenericNack::INVALID_LENGTH));
            return;
        }
        if (TransportReadStatus::COMPLETE != result.transport)
        {
            if ((TransportReadStatus::TIMED_OUT == result.transport)
                && (!result.started))
            {
                continue;
            }
            return;
        }
        lastActivity = DoipProtocol::Clock::now();

        if (DoipProtocol::PayloadType::ALIVE_RESPONSE == frame.type)
        {
            if (!acceptAliveResponse(frame))
            {
                return;
            }
            continue;
        }

        if (DoipProtocol::PayloadType::DIAGNOSTIC != frame.type)
        {
            if (!protocol.sendNack(fd, static_cast<std::uint8_t>(GenericNack::UNKNOWN_TYPE)))
            {
                return;
            }
            continue;
        }

        const std::uint16_t source = protocol.readU16(frame.payload.data());
        const std::uint16_t target = protocol.readU16(
            frame.payload.data()
            + static_cast<std::size_t>(DoipConfig::Layout::SOURCE_ADDRESS_BYTES));
        const auto route = PduRouter::getInstance().validate(source, target);
        if (PduRouter::RouteStatus::INVALID_SOURCE == route)
        {
            protocol.sendDiagnosticResult(
                fd, DoipProtocol::PayloadType::DIAGNOSTIC_NACK,
                source, static_cast<std::uint8_t>(DiagnosticCode::INVALID_SOURCE));
            OPENSSL_cleanse(frame.payload.data(), frame.payload.size());
            return;
        }
        if (PduRouter::RouteStatus::INVALID_TARGET == route)
        {
            if (!protocol.sendDiagnosticResult(
                    fd, DoipProtocol::PayloadType::DIAGNOSTIC_NACK,
                    source, static_cast<std::uint8_t>(DiagnosticCode::INVALID_TARGET)))
            {
                OPENSSL_cleanse(frame.payload.data(), frame.payload.size());
                return;
            }
            OPENSSL_cleanse(frame.payload.data(), frame.payload.size());
            continue;
        }
        const bool acknowledged = protocol.sendDiagnosticResult(
            fd, DoipProtocol::PayloadType::DIAGNOSTIC_ACK,
            source, static_cast<std::uint8_t>(DiagnosticCode::OK));
        std::vector<std::uint8_t> udsResponse;
        if (acknowledged)
        {
            udsResponse = PduRouter::getInstance().forward(frame.payload);
        }
        const bool sent = (acknowledged)
            && (!udsResponse.empty())
            && (protocol.sendUdsResponse(fd, udsResponse));
        OPENSSL_cleanse(frame.payload.data(), frame.payload.size());
        if (!sent)
        {
            return;
        }
        auto nextPending = DoipProtocol::Clock::now()
            + std::chrono::milliseconds(static_cast<std::uint32_t>(
                PendingTiming::NOTIFICATION_MS));
        while (PduRouter::getInstance().responsePending())
        {
            if (!serviceAliveState(fd, aliveDeadline))
            {
                return;
            }
            if (AliveStatus::WAITING == pAliveStatus.load())
            {
                DoipProtocol::Frame control;
                const auto pollDeadline = DoipProtocol::Clock::now()
                    + std::chrono::milliseconds(static_cast<std::uint32_t>(
                        PendingTiming::POLL_MS));
                const auto controlResult = protocol.readFrame(fd, maxPayload,
                    pollDeadline, control, aliveDeadline);
                if (TransportReadStatus::COMPLETE == controlResult.transport)
                {
                    if ((DoipProtocol::FrameError::NONE != controlResult.frameError)
                        || (DoipProtocol::PayloadType::ALIVE_RESPONSE != control.type)
                        || (!acceptAliveResponse(control)))
                    {
                        return;
                    }
                }
                else if ((TransportReadStatus::TIMED_OUT != controlResult.transport)
                         || (controlResult.started))
                {
                    return;
                }
            }
            if (AliveStatus::ALIVE == pAliveStatus.load())
            {
                lastActivity = DoipProtocol::Clock::now();
            }
            const auto pendingResponse = PduRouter::getInstance().pollResponse();
            if (!pendingResponse.empty())
            {
                if (!protocol.sendUdsResponse(fd, pendingResponse))
                {
                    return;
                }
                break;
            }
            if (nextPending <= DoipProtocol::Clock::now())
            {
                if (!protocol.sendUdsResponse(
                        fd, PduRouter::getInstance().pendingNotification()))
                {
                    return;
                }
                nextPending = DoipProtocol::Clock::now()
                    + std::chrono::milliseconds(static_cast<std::uint32_t>(
                        PendingTiming::NOTIFICATION_MS));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(
                static_cast<std::uint32_t>(PendingTiming::POLL_MS)));
        }
    }
}
