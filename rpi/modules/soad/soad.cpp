#include "soad.h"

#include "socket.h"

SoAd& SoAd::getInstance()
{
    static SoAd instance;
    return instance;
}

bool SoAd::listen(std::uint16_t port)
{
    return Socket::getInstance().listen(port);
}

int SoAd::acceptClient(std::uint32_t timeoutMs)
{
    return Socket::getInstance().acceptClient(timeoutMs);
}

void SoAd::closeClient(int fd) const
{
    Socket::getInstance().closeClient(fd);
}

void SoAd::shutdownClient(int fd) const
{
    Socket::getInstance().shutdownClient(fd);
}

TransportReadStatus SoAd::readExact(int fd, std::uint8_t* bytes, std::size_t size,
                                    Clock::time_point deadline) const
{
    return Socket::getInstance().readExact(fd, bytes, size, deadline);
}

bool SoAd::sendAll(int fd, const std::vector<std::uint8_t>& bytes,
                   std::uint32_t timeoutMs) const
{
    return Socket::getInstance().sendAll(fd, bytes, timeoutMs);
}
