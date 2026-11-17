#ifndef PSI_SOAD_H
#define PSI_SOAD_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "transport_read_status.h"

// Socket adapter exposed to the diagnostic transport; protocol agnostic.
class SoAd
{
public:
    using Clock = std::chrono::steady_clock;

    static SoAd& getInstance();
    SoAd(const SoAd&) = delete;
    SoAd& operator=(const SoAd&) = delete;

    bool listen(std::uint16_t port);
    int acceptClient(std::uint32_t timeoutMs);
    void closeClient(int fd) const;
    void shutdownClient(int fd) const;
    TransportReadStatus readExact(int fd, std::uint8_t* bytes, std::size_t size,
                                  Clock::time_point deadline) const;
    bool sendAll(int fd, const std::vector<std::uint8_t>& bytes,
                 std::uint32_t timeoutMs) const;

private:
    SoAd() = default;
};

#endif
