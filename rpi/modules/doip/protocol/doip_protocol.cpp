#include "doip_protocol.h"

#include <algorithm>
#include <cstdint>

#include "doip_config.h"
#include "soad.h"

DoipProtocol& DoipProtocol::getInstance()
{
    static DoipProtocol instance;
    return instance;
}

std::uint16_t DoipProtocol::readU16(const std::uint8_t* bytes) const
{
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(bytes[0]) << 8)
        | static_cast<std::uint16_t>(bytes[1]));
}

std::uint32_t DoipProtocol::readU32(const std::uint8_t* bytes) const
{
    return (static_cast<std::uint32_t>(readU16(bytes)) << 16)
           | static_cast<std::uint32_t>(readU16(bytes + 2));
}

void DoipProtocol::appendU16(std::vector<std::uint8_t>& bytes, std::uint16_t value) const
{
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    bytes.push_back(static_cast<std::uint8_t>(value));
}

void DoipProtocol::appendU32(std::vector<std::uint8_t>& bytes, std::uint32_t value) const
{
    appendU16(bytes, static_cast<std::uint16_t>(value >> 16));
    appendU16(bytes, static_cast<std::uint16_t>(value));
}

std::vector<std::uint8_t> DoipProtocol::makeFrame(PayloadType type,
                                                  const std::vector<std::uint8_t>& payload) const
{
    const std::uint32_t size = static_cast<std::uint32_t>(payload.size());
    std::vector<std::uint8_t> frame;
    frame.reserve(static_cast<std::size_t>(Layout::HEADER_SIZE) + payload.size());
    frame.push_back(static_cast<std::uint8_t>(DoipConfig::Defaults::PROTOCOL_VERSION));
    frame.push_back(static_cast<std::uint8_t>(DoipConfig::Defaults::INVERSE_PROTOCOL_VERSION));
    appendU16(frame, static_cast<std::uint16_t>(type));
    appendU32(frame, size);
    frame.insert(frame.end(), payload.begin(), payload.end());
    return frame;
}

bool DoipProtocol::sendFrame(int fd, PayloadType type,
                            const std::vector<std::uint8_t>& payload) const
{
    return SoAd::getInstance().sendAll(
        fd, makeFrame(type, payload),
        static_cast<std::uint32_t>(DoipConfig::Defaults::PARTIAL_FRAME_TIMEOUT_MS));
}

DoipProtocol::ReadResult DoipProtocol::readFrame(int fd, std::uint32_t maxPayload,
                                                Clock::time_point firstByteDeadline,
                                                Frame& frame,
                                                Clock::time_point frameDeadline) const
{
    std::uint8_t header[static_cast<std::size_t>(Layout::HEADER_SIZE)]{};
    ReadResult result{};
    result.transport = SoAd::getInstance().readExact(fd, header, 1, firstByteDeadline);
    if (TransportReadStatus::COMPLETE != result.transport)
    {
        return result;
    }
    result.started = true;

    const auto partialDeadline = Clock::now()
        + std::chrono::milliseconds(static_cast<std::uint32_t>(
            DoipConfig::Defaults::PARTIAL_FRAME_TIMEOUT_MS));
    const auto deadline = std::min(frameDeadline, partialDeadline);
    result.transport = SoAd::getInstance().readExact(fd, header + 1,
        static_cast<std::size_t>(Layout::HEADER_SIZE) - 1, deadline);
    if (TransportReadStatus::COMPLETE != result.transport)
    {
        return result;
    }

    if ((static_cast<std::uint8_t>(DoipConfig::Defaults::PROTOCOL_VERSION) != header[0])
        || (static_cast<std::uint8_t>(DoipConfig::Defaults::INVERSE_PROTOCOL_VERSION) != header[1]))
    {
        result.frameError = FrameError::INVALID_VERSION;
        return result;
    }

    frame.type = static_cast<PayloadType>(readU16(header + 2));
    const std::uint32_t size = readU32(header + 4);
    if (size > maxPayload)
    {
        result.frameError = FrameError::TOO_LARGE;
        return result;
    }
    if (((PayloadType::ROUTING_REQUEST == frame.type)
         && (static_cast<std::size_t>(Layout::ROUTING_REQUEST_SIZE) != size))
        || ((PayloadType::DIAGNOSTIC == frame.type)
            && (size < static_cast<std::size_t>(Layout::MIN_DIAGNOSTIC_SIZE)))
        || ((PayloadType::ALIVE_RESPONSE == frame.type)
            && (static_cast<std::size_t>(Layout::ALIVE_RESPONSE_SIZE) != size)))
    {
        result.frameError = FrameError::INVALID_LENGTH;
        return result;
    }

    frame.payload.resize(size);
    if (0 == size)
    {
        return result;
    }
    result.transport = SoAd::getInstance().readExact(fd, frame.payload.data(), size,
                                                     deadline);
    return result;
}

bool DoipProtocol::sendNack(int fd, std::uint8_t code) const
{
    return sendFrame(fd, PayloadType::GENERIC_NACK, {code});
}

bool DoipProtocol::sendRoutingResponse(int fd, std::uint16_t source,
                                       std::uint8_t code) const
{
    std::vector<std::uint8_t> payload;
    appendU16(payload, source);
    appendU16(payload, static_cast<std::uint16_t>(DoipConfig::Defaults::PI_LOGICAL_ADDRESS));
    payload.push_back(code);
    payload.insert(payload.end(), static_cast<std::size_t>(Layout::ROUTING_RESERVED_BYTES), 0);
    return sendFrame(fd, PayloadType::ROUTING_RESPONSE, payload);
}

bool DoipProtocol::sendAliveRequest(int fd) const
{
    return sendFrame(fd, PayloadType::ALIVE_REQUEST, {});
}

bool DoipProtocol::sendDiagnosticResult(int fd, PayloadType type,
                                       std::uint16_t testerAddress, std::uint8_t code) const
{
    std::vector<std::uint8_t> payload;
    appendU16(payload, static_cast<std::uint16_t>(DoipConfig::Defaults::PI_LOGICAL_ADDRESS));
    appendU16(payload, testerAddress);
    payload.push_back(code);
    return sendFrame(fd, type, payload);
}

bool DoipProtocol::sendUdsResponse(int fd, const std::vector<std::uint8_t>& uds) const
{
    std::vector<std::uint8_t> payload;
    appendU16(payload, static_cast<std::uint16_t>(DoipConfig::Defaults::PI_LOGICAL_ADDRESS));
    appendU16(payload, static_cast<std::uint16_t>(DoipConfig::Defaults::TESTER_LOGICAL_ADDRESS));
    payload.insert(payload.end(), uds.begin(), uds.end());
    return sendFrame(fd, PayloadType::DIAGNOSTIC, payload);
}

