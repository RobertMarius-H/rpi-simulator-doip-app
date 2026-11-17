#include "socket.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <limits>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

Socket& Socket::getInstance()
{
    static Socket instance;
    return instance;
}

Socket::~Socket()
{
    if (0 <= pListener)
    {
        ::close(pListener);
    }
}

void Socket::setListener(int fd)
{
    if (0 <= pListener)
    {
        ::close(pListener);
    }
    pListener = fd;
}

void Socket::closeClient(int fd) const
{
    if (0 <= fd)
    {
        ::close(fd);
    }
}

void Socket::shutdownClient(int fd) const
{
    if (0 <= fd)
    {
        ::shutdown(fd, SHUT_RDWR);
    }
}

bool Socket::listen(std::uint16_t port)
{
    setListener(::socket(AF_INET, SOCK_STREAM, 0));
    if (0 > pListener)
    {
        return false;
    }
    const int reuse = 1;
    if (0 > ::setsockopt(pListener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)))
    {
        return false;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    return (0 == ::bind(pListener, reinterpret_cast<sockaddr*>(&address),
                        sizeof(address)))
           && (0 == ::listen(pListener, static_cast<int>(Limit::LISTEN_BACKLOG)));
}

int Socket::acceptClient(std::uint32_t timeoutMs)
{
    pollfd listener{pListener, POLLIN, 0};
    const int ready = ::poll(&listener, 1, static_cast<int>(timeoutMs));
    if (0 == ready)
    {
        errno = EAGAIN;
        return -1;
    }
    if (0 > ready)
    {
        return -1;
    }
    const int fd = ::accept(pListener, nullptr, nullptr);
    if (0 <= fd)
    {
        const int enabled = 1;
        if (0 != ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled)))
        {
            ::close(fd);
            return -1;
        }
    }
    return fd;
}

TransportReadStatus Socket::readExact(int fd, std::uint8_t* bytes, std::size_t size,
                                      Clock::time_point deadline) const
{
    std::size_t received = 0;
    while (received < size)
    {
        int waitMs = -1;
        if (Clock::time_point::max() != deadline)
        {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - Clock::now()).count();
            if (0 >= remaining)
            {
                return TransportReadStatus::TIMED_OUT;
            }
            waitMs = static_cast<int>(std::min<std::int64_t>(remaining,
                std::numeric_limits<int>::max()));
        }
        pollfd descriptor{fd, POLLIN, 0};
        const int ready = ::poll(&descriptor, 1, waitMs);
        if (0 == ready)
        {
            return TransportReadStatus::TIMED_OUT;
        }
        if (0 > ready)
        {
            if (EINTR == errno)
            {
                continue;
            }
            return TransportReadStatus::ERROR;
        }
        const ssize_t count = ::recv(fd, bytes + received, size - received, 0);
        if (0 == count)
        {
            return TransportReadStatus::CLOSED;
        }
        if (0 > count)
        {
            if ((EINTR == errno) || (EAGAIN == errno) || (EWOULDBLOCK == errno))
            {
                continue;
            }
            return TransportReadStatus::ERROR;
        }
        received += static_cast<std::size_t>(count);
    }
    return TransportReadStatus::COMPLETE;
}

bool Socket::sendAll(int fd, const std::vector<std::uint8_t>& bytes,
                     std::uint32_t timeoutMs) const
{
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
    std::size_t sent = 0;
    while (sent < bytes.size())
    {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - Clock::now()).count();
        if (0 >= remaining)
        {
            return false;
        }
        pollfd descriptor{fd, POLLOUT, 0};
        const int waitMs = static_cast<int>(std::min<std::int64_t>(remaining,
            std::numeric_limits<int>::max()));
        const int ready = ::poll(&descriptor, 1, waitMs);
        if (0 == ready)
        {
            return false;
        }
        if (0 > ready)
        {
            if (EINTR == errno)
            {
                continue;
            }
            return false;
        }
        const ssize_t count = ::send(fd, bytes.data() + sent, bytes.size() - sent,
                                     MSG_NOSIGNAL | MSG_DONTWAIT);
        if (0 < count)
        {
            sent += static_cast<std::size_t>(count);
        }
        else if ((0 > count)
                 && ((EINTR == errno) || (EAGAIN == errno) || (EWOULDBLOCK == errno)))
        {
            continue;
        }
        else
        {
            return false;
        }
    }
    return true;
}
