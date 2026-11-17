#ifndef PSI_UDS_SECURITY_H
#define PSI_UDS_SECURITY_H

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

class UdsSecurity
{
public:
    enum class Size : std::size_t
    {
        SECRET = 32,
        SEED = 16,
        KEY = 32
    };
    enum class Policy : std::uint32_t
    {
        LOCKOUT_MS = 60'000,
        MAX_WRONG_KEYS = 3
    };

    struct Session
    {
        std::array<std::uint8_t, static_cast<std::size_t>(Size::SEED)> seed{};
        bool hasSeed = false;
        bool unlocked = false;

        ~Session();
        void reset();
    };

    static UdsSecurity& getInstance();
    void initialize(const std::array<std::uint8_t, static_cast<std::size_t>(Size::SECRET)>& secret,
                    std::uint32_t lockoutMs);
    ~UdsSecurity();

    UdsSecurity(const UdsSecurity&) = delete;
    UdsSecurity& operator=(const UdsSecurity&) = delete;

    bool loadTestSecret(std::array<std::uint8_t, static_cast<std::size_t>(Size::SECRET)>& secret) const;

    std::vector<std::uint8_t> handle(const std::uint8_t* request,
                                     std::size_t size,
                                     bool programmingSession,
                                     Session& session);

private:
    enum class Service : std::uint8_t
    {
        SECURITY_ACCESS = 0x27,
        REQUEST_SEED = 0x01,
        SEND_KEY = 0x02,
        POSITIVE_RESPONSE = 0x67,
        NEGATIVE_RESPONSE = 0x7F
    };

    enum class NegativeCode : std::uint8_t
    {
        INVALID_LENGTH = 0x13,
        UNSUPPORTED_SUBFUNCTION = 0x12,
        SESSION_NOT_SUPPORTED = 0x7F,
        REQUIRED_DELAY = 0x37,
        GENERAL_PROGRAMMING_FAILURE = 0x72,
        REQUEST_SEQUENCE_ERROR = 0x24,
        INVALID_KEY = 0x35,
        EXCEEDED_ATTEMPTS = 0x36
    };

    using Clock = std::chrono::steady_clock;
    // Development-only shared value; replace with external secret provisioning.
    static constexpr char TEST_SECRET_HEX[] =
        "DCD74632290E94C7A5E9B70F940C38F163144A15AA9C4D9C08959C279AE8700D";
    static_assert(sizeof(TEST_SECRET_HEX) == static_cast<std::size_t>(Size::SECRET) * 2 + 1,
                  "The test secret must contain exactly 64 hex characters.");

    UdsSecurity() = default;
    static std::vector<std::uint8_t> negativeResponse(std::uint8_t code);

    std::array<std::uint8_t, static_cast<std::size_t>(Size::SECRET)> pSecret{};
    std::chrono::milliseconds pLockoutDuration{};
    std::uint8_t pWrongKeys = 0;
    Clock::time_point pLockoutUntil{};
};

#endif // PSI_UDS_SECURITY_H
