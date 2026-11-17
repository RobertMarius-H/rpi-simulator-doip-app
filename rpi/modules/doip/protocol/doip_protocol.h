#ifndef PSI_DOIP_PROTOCOL_H
#define PSI_DOIP_PROTOCOL_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "transport_read_status.h"

// BSW communication boundary: DoIP wire framing and socket read/write.
class DoipProtocol
{
public:
    enum class FrameError
    {
        NONE,
        INVALID_VERSION,
        TOO_LARGE,
        INVALID_LENGTH
    };

    enum class Layout : std::size_t
    {
        HEADER_SIZE = 8,
        ROUTING_REQUEST_SIZE = 7,
        ALIVE_RESPONSE_SIZE = 2,
        MIN_DIAGNOSTIC_SIZE = 5,
        ROUTING_RESERVED_BYTES = 4
    };

    enum class PayloadType : std::uint16_t
    {
        GENERIC_NACK = 0x0000,
        ROUTING_REQUEST = 0x0005,
        ROUTING_RESPONSE = 0x0006,
        ALIVE_REQUEST = 0x0007,
        ALIVE_RESPONSE = 0x0008,
        DIAGNOSTIC = 0x8001,
        DIAGNOSTIC_ACK = 0x8002,
        DIAGNOSTIC_NACK = 0x8003
    };

    struct ReadResult
    {
        TransportReadStatus transport = TransportReadStatus::COMPLETE;
        FrameError frameError = FrameError::NONE;
        bool started = false;
    };

    struct Frame
    {
        PayloadType type = PayloadType::GENERIC_NACK;
        std::vector<std::uint8_t> payload;
    };

    using Clock = std::chrono::steady_clock;
    static DoipProtocol& getInstance();
    DoipProtocol(const DoipProtocol&) = delete;
    DoipProtocol& operator=(const DoipProtocol&) = delete;

    ReadResult readFrame(int fd, std::uint32_t maxPayload,
                         Clock::time_point firstByteDeadline, Frame& frame,
                         Clock::time_point frameDeadline = Clock::time_point::max()) const;
    bool sendNack(int fd, std::uint8_t code) const;
    bool sendRoutingResponse(int fd, std::uint16_t source, std::uint8_t code) const;
    bool sendAliveRequest(int fd) const;
    bool sendDiagnosticResult(int fd, PayloadType type,
                              std::uint16_t testerAddress, std::uint8_t code) const;
    bool sendUdsResponse(int fd, const std::vector<std::uint8_t>& uds) const;
    std::uint16_t readU16(const std::uint8_t* bytes) const;

private:
    DoipProtocol() = default;
    std::uint32_t readU32(const std::uint8_t* bytes) const;
    void appendU16(std::vector<std::uint8_t>& bytes, std::uint16_t value) const;
    void appendU32(std::vector<std::uint8_t>& bytes, std::uint32_t value) const;
    std::vector<std::uint8_t> makeFrame(PayloadType type,
                                        const std::vector<std::uint8_t>& payload) const;
    bool sendFrame(int fd, PayloadType type,
                   const std::vector<std::uint8_t>& payload) const;
};

#endif
