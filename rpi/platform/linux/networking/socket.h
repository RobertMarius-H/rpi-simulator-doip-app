#ifndef PSI_SOCKET_H
#define PSI_SOCKET_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "transport_read_status.h"

// Linux socket ownership belongs to the transport boundary.
class Socket
{
public:
    using Clock = std::chrono::steady_clock;
    static Socket& getInstance();
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    ~Socket();

    void setListener(int fd);
    bool listen(std::uint16_t port);
    int acceptClient(std::uint32_t timeoutMs);
    void closeClient(int fd) const;
    void shutdownClient(int fd) const;
    TransportReadStatus readExact(int fd, std::uint8_t* bytes, std::size_t size,
                                  Clock::time_point deadline) const;
    bool sendAll(int fd, const std::vector<std::uint8_t>& bytes,
                 std::uint32_t timeoutMs) const;

private:
    enum class Limit : int
    {
        LISTEN_BACKLOG = 16
    };

    Socket() = default;
    int pListener = -1;
};

#endif
