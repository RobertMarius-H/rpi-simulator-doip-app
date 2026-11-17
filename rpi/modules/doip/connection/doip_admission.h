#ifndef PSI_DOIP_ADMISSION_H
#define PSI_DOIP_ADMISSION_H

#include <cstdint>

#include "doip_protocol.h"

// Validates a newly accepted TCP connection before it becomes a DoIP route.
class DoipAdmission
{
public:
    static DoipAdmission& getInstance();
    DoipAdmission(const DoipAdmission&) = delete;
    DoipAdmission& operator=(const DoipAdmission&) = delete;
    bool readRequest(int fd, std::uint32_t maxPayload,
                     DoipProtocol::Frame& frame) const;
    bool reply(int fd, const DoipProtocol::Frame& frame,
               bool capacityAvailable) const;
    bool eligible(const DoipProtocol::Frame& frame) const;

private:
    enum class Nack : std::uint8_t
    {
        INVALID_VERSION = 0x00,
        TOO_LARGE = 0x02,
        INVALID_LENGTH = 0x04
    };

    DoipAdmission() = default;
};

#endif
