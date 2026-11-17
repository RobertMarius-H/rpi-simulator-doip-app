#ifndef PSI_UDS_ROUTINE_SERVICE_H
#define PSI_UDS_ROUTINE_SERVICE_H

#include <cstddef>
#include <cstdint>
#include <vector>

class UdsRoutineService
{
private:
    enum class Service : std::uint8_t
    {
        ROUTINE_CONTROL = 0x31,
        POSITIVE_RESPONSE = 0x71,
        NEGATIVE_RESPONSE = 0x7F
    };
    enum class Subfunction : std::uint8_t
    {
        START = 0x01,
        REQUEST_RESULTS = 0x03
    };
    enum class Routine : std::uint16_t
    {
        VERIFY = 0xF001,
        ACTIVATE = 0xF002,
        VERIFY_WITH_PROGRESS = 0xF003
    };
    enum class ProgressState : std::uint8_t
    {
        VERIFYING = 0x01,
        VERIFIED = 0x02
    };
    enum class NegativeCode : std::uint8_t
    {
        SUBFUNCTION_NOT_SUPPORTED = 0x12,
        INVALID_LENGTH = 0x13,
        SEQUENCE_ERROR = 0x24,
        OUT_OF_RANGE = 0x31,
        SECURITY_DENIED = 0x33,
        PROGRAMMING_FAILURE = 0x72,
        RESPONSE_PENDING = 0x78,
        SESSION_NOT_SUPPORTED = 0x7F
    };
    enum class Layout : std::size_t
    {
        HEADER_BYTES = 4,
        VERIFY_BYTES = 36,
        SUBFUNCTION_OFFSET = 1,
        ROUTINE_HIGH_OFFSET = 2,
        ROUTINE_LOW_OFFSET = 3
    };

    UdsRoutineService() = default;
    std::vector<std::uint8_t> negative(NegativeCode code) const;
    std::vector<std::uint8_t> positive(Routine routine) const;
    std::vector<std::uint8_t> progress(ProgressState state, std::uint8_t percent) const;
    bool pLegacyPending = false;

public:
    static UdsRoutineService& getInstance();
    UdsRoutineService(const UdsRoutineService&) = delete;
    UdsRoutineService& operator=(const UdsRoutineService&) = delete;

    std::vector<std::uint8_t> handle(const std::uint8_t* request, std::size_t size,
                                     bool programmingSession, bool unlocked);
    bool responsePending() const;
    std::vector<std::uint8_t> pollResponse();
    std::vector<std::uint8_t> pendingNotification() const;
    void resetConnection() { pLegacyPending = false; }
};

#endif
