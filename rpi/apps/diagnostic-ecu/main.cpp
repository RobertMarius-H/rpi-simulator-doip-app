#include <array>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include <openssl/crypto.h>

#include "doip_config.h"
#include "doip_server.h"
#include "firmware_storage.h"
#include "hex_codec.h"
#include "uds_security.h"
#include "verification_manager.h"

int main(int argc, char* argv[])
{
    auto& doipConfig = DoipConfig::getInstance();
    auto& security = UdsSecurity::getInstance();
    std::size_t maxConnections = 0;
    std::uint32_t minimumVerificationMs = 0;
    const char* homeDirectory = std::getenv("HOME");
    std::string updateDirectory;
    if ((nullptr != homeDirectory) && ('\0' != homeDirectory[0]))
    {
        updateDirectory = std::string(homeDirectory)
            + FirmwareStorage::DEFAULT_DIRECTORY_FROM_HOME;
    }
    for (int index = 1; index < argc; index += 2)
    {
        if (index + 1 >= argc)
        {
            std::cerr << "Usage: rpi_doip_server [--port 1..65535] "
                         "[--connections N] [--update-dir path] "
                         "[--verification-min-ms 0..30000] "
                         "[--alive-idle-ms 100..60000]\n";
            return 2;
        }
        const std::string_view option(argv[index]);
        const std::string_view value(argv[index + 1]);
        if ("--port" == option)
        {
            unsigned int port = 0;
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), port);
            if ((std::errc{} != parsed.ec)
                || (value.data() + value.size() != parsed.ptr)
                || (0 == port)
                || (static_cast<std::uint32_t>(DoipConfig::Limit::MAXIMUM_TCP_PORT) < port))
            {
                std::cerr << "Invalid TCP port\n";
                return 2;
            }
            doipConfig.setTcpPort(static_cast<std::uint16_t>(port));
        }
        else if ("--connections" == option)
        {
            const auto parsedCount = std::from_chars(value.data(), value.data() + value.size(),
                                                     maxConnections);
            if ((std::errc{} != parsedCount.ec)
                || (value.data() + value.size() != parsedCount.ptr)
                || (0U == maxConnections))
            {
                std::cerr << "Invalid connection count\n";
                return 2;
            }
        }
        else if (("--update-dir" == option) && (!value.empty()))
        {
            updateDirectory = value;
        }
        else if ("--verification-min-ms" == option)
        {
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(),
                                                minimumVerificationMs);
            if ((std::errc{} != parsed.ec)
                || (value.data() + value.size() != parsed.ptr)
                || (static_cast<std::uint32_t>(
                        VerificationManager::Limit::MAXIMUM_DEMO_DURATION_MS)
                    < minimumVerificationMs))
            {
                std::cerr << "Invalid verification minimum duration\n";
                return 2;
            }
        }
        else if ("--alive-idle-ms" == option)
        {
            std::uint32_t idleMs = 0;
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(),
                                                idleMs);
            if ((std::errc{} != parsed.ec)
                || (value.data() + value.size() != parsed.ptr)
                || (100U > idleMs)
                || (static_cast<std::uint32_t>(DoipConfig::Defaults::ALIVE_IDLE_MS)
                    < idleMs))
            {
                std::cerr << "Invalid alive idle timeout\n";
                return 2;
            }
            doipConfig.setAliveIdleMs(idleMs);
        }
        else
        {
            std::cerr << "Unknown option or empty update directory\n";
            return 2;
        }
    }

    if (updateDirectory.empty())
    {
        std::cerr << "HOME is unavailable; provide --update-dir.\n";
        return 2;
    }
    if (!FirmwareStorage::getInstance().initialize(updateDirectory))
    {
        std::cerr << "Update storage is unavailable: " << updateDirectory << '\n';
        return 1;
    }

    std::array<std::uint8_t, static_cast<std::size_t>(UdsSecurity::Size::SECRET)> secret{};
    const char* configuredSecret = std::getenv("DOIP_DEMO_SECRET_HEX");
    const bool validSecret = (nullptr == configuredSecret)
        ? security.loadTestSecret(secret)
        : HexCodec::getInstance().decode(configuredSecret, secret.data(), secret.size());
    if (!validSecret)
    {
        OPENSSL_cleanse(secret.data(), secret.size());
        std::cerr << "Invalid DOIP_DEMO_SECRET_HEX: expected 64 hexadecimal characters.\n";
        return 1;
    }
    security.initialize(secret, static_cast<std::uint32_t>(UdsSecurity::Policy::LOCKOUT_MS));
    OPENSSL_cleanse(secret.data(), secret.size());
    if (0 != minimumVerificationMs)
    {
        VerificationManager::getInstance().configureMinimumDuration(minimumVerificationMs);
    }
    return DoipServer::getInstance().run(maxConnections);
}
