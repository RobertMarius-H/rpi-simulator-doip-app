#ifndef PSI_CONFIG_DOIP_CONFIG_H
#define PSI_CONFIG_DOIP_CONFIG_H

#include <cstdint>
#include <optional>

class DoipConfig
{
public:
    enum class Defaults : std::uint32_t
    {
        TCP_PORT = 13400,
        TESTER_LOGICAL_ADDRESS = 0x0E80,
        PI_LOGICAL_ADDRESS = 0x1000,
        PROTOCOL_VERSION = 0x03,
        INVERSE_PROTOCOL_VERSION = 0xFC,
        ROUTING_ACTIVATION_TIMEOUT_MS = 5'000,
        PARTIAL_FRAME_TIMEOUT_MS = 10'000,
        ALIVE_IDLE_MS = 60'000,
        ALIVE_RESPONSE_MS = 3'000,
        ACCEPT_POLL_MS = 100,
        READ_POLL_MS = 100,
        ADMISSION_POLL_MS = 25
    };

    enum class Layout : std::uint32_t
    {
        SOURCE_ADDRESS_BYTES = 2,
        LOGICAL_ADDRESS_BYTES = 4
    };

    enum class Limit : std::uint32_t
    {
        MAXIMUM_DIAGNOSTIC_PAYLOAD_BYTES = 16'390,
        MAXIMUM_TCP_PORT = 65'535
    };

    static DoipConfig& getInstance();
    DoipConfig(const DoipConfig&) = delete;
    DoipConfig& operator=(const DoipConfig&) = delete;

    // Portul este singura valoare suprascrisă la pornire prin --port.
    void setTcpPort(std::uint16_t port) { pTcpPort = port; }
    std::uint16_t tcpPort() const { return pTcpPort; }
    void setAliveIdleMs(std::uint32_t milliseconds) { pAliveIdleOverrideMs = milliseconds; }
    std::uint32_t aliveIdleMs() const
    {
        return pAliveIdleOverrideMs.value_or(static_cast<std::uint32_t>(Defaults::ALIVE_IDLE_MS));
    }

private:
    DoipConfig() = default;
    std::uint16_t pTcpPort = static_cast<std::uint16_t>(Defaults::TCP_PORT);
    std::optional<std::uint32_t> pAliveIdleOverrideMs;
};

#endif // PSI_CONFIG_DOIP_CONFIG_H
