#include "doip_server.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <system_error>
#include <thread>

#include "doip_admission.h"
#include "doip_connection.h"
#include "doip_config.h"
#include "doip_protocol.h"
#include "pdu_router.h"
#include "soad.h"

DoipServer& DoipServer::getInstance()
{
    static DoipServer instance;
    return instance;
}

int DoipServer::run(std::size_t maxConnections)
{
    auto& sockets = SoAd::getInstance();
    const auto& config = DoipConfig::getInstance();
    auto& admission = DoipAdmission::getInstance();
    auto& connection = DoipConnection::getInstance();
    if (!sockets.listen(config.tcpPort()))
    {
        std::cerr << "listen: " << std::strerror(errno) << '\n';
        return 1;
    }

    const std::uint32_t maxPayload = static_cast<std::uint32_t>(
        DoipConfig::Limit::MAXIMUM_DIAGNOSTIC_PAYLOAD_BYTES);
    std::cout << "DoIP slice listening on TCP port " << config.tcpPort() << '\n';
    std::thread activeWorker;
    std::atomic<bool> activeDone{false};
    int activeFd = -1;
    std::size_t served = 0;
    const auto finishActive = [&]() {
        if (activeWorker.joinable())
        {
            activeWorker.join();
        }
        connection.resetConnection();
        PduRouter::getInstance().resetConnection();
        sockets.closeClient(activeFd);
        activeFd = -1;
        ++served;
    };

    while (true)
    {
        if ((0 <= activeFd) && activeDone.load())
        {
            finishActive();
            continue;
        }
        if ((0U != maxConnections) && (served >= maxConnections))
        {
            if (0 > activeFd)
            {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(
                static_cast<std::uint32_t>(DoipConfig::Defaults::ACCEPT_POLL_MS)));
            continue;
        }
        const int candidate = sockets.acceptClient(static_cast<std::uint32_t>(
            DoipConfig::Defaults::ACCEPT_POLL_MS));
        if (0 > candidate)
        {
            if ((EAGAIN == errno) || (EINTR == errno))
            {
                continue;
            }
            std::cerr << "accept: " << std::strerror(errno) << '\n';
            if (0 <= activeFd)
            {
                sockets.shutdownClient(activeFd);
                finishActive();
            }
            return 1;
        }

        DoipProtocol::Frame frame;
        if (!admission.readRequest(candidate, maxPayload, frame))
        {
            sockets.closeClient(candidate);
            ++served;
            continue;
        }
        if (!admission.eligible(frame))
        {
            admission.reply(candidate, frame, true);
            sockets.closeClient(candidate);
            ++served;
            continue;
        }

        if (0 <= activeFd)
        {
            connection.requestAliveCheck();
            const auto aliveDeadline = DoipProtocol::Clock::now()
                + std::chrono::milliseconds(static_cast<std::uint32_t>(
                    DoipConfig::Defaults::ALIVE_RESPONSE_MS));
            while (((DoipConnection::AliveStatus::REQUESTED == connection.aliveStatus())
                    || (DoipConnection::AliveStatus::WAITING == connection.aliveStatus()))
                   && (!activeDone.load())
                   && (DoipProtocol::Clock::now() < aliveDeadline))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(
                    static_cast<std::uint32_t>(
                        DoipConfig::Defaults::ADMISSION_POLL_MS)));
            }
            if ((DoipConnection::AliveStatus::ALIVE == connection.aliveStatus())
                && (!activeDone.load()))
            {
                connection.clearAliveResult();
                admission.reply(candidate, frame, false);
                sockets.closeClient(candidate);
                ++served;
                continue;
            }
            sockets.shutdownClient(activeFd);
            finishActive();
            if ((0U != maxConnections) && (served >= maxConnections))
            {
                sockets.closeClient(candidate);
                ++served;
                break;
            }
        }

        if (!admission.reply(candidate, frame, true))
        {
            sockets.closeClient(candidate);
            ++served;
            continue;
        }
        activeFd = candidate;
        activeDone.store(false);
        try
        {
            activeWorker = std::thread([&connection, &activeDone, candidate, maxPayload]() {
                connection.serve(candidate, maxPayload);
                activeDone.store(true);
            });
        }
        catch (const std::system_error& error)
        {
            std::cerr << "worker: " << error.what() << '\n';
            sockets.closeClient(activeFd);
            activeFd = -1;
            return 1;
        }
    }
    return 0;
}
