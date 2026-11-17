#include "uds_security.h"
#include "hex_codec.h"

#include <algorithm>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

std::vector<std::uint8_t> UdsSecurity::negativeResponse(std::uint8_t code)
{
    return {static_cast<std::uint8_t>(Service::NEGATIVE_RESPONSE),
            static_cast<std::uint8_t>(Service::SECURITY_ACCESS), code};
}

UdsSecurity& UdsSecurity::getInstance()
{
    static UdsSecurity instance;
    return instance;
}

UdsSecurity::Session::~Session()
{
    reset();
}

void UdsSecurity::Session::reset()
{
    OPENSSL_cleanse(seed.data(), seed.size());
    hasSeed = false;
    unlocked = false;
}

void UdsSecurity::initialize(
    const std::array<std::uint8_t, static_cast<std::size_t>(Size::SECRET)>& secret,
    std::uint32_t lockoutMs)
{
    OPENSSL_cleanse(pSecret.data(), pSecret.size());
    pSecret = secret;
    pLockoutDuration = std::chrono::milliseconds(lockoutMs);
    pWrongKeys = 0;
    pLockoutUntil = Clock::time_point{};
}

UdsSecurity::~UdsSecurity()
{
    OPENSSL_cleanse(pSecret.data(), pSecret.size());
}

bool UdsSecurity::loadTestSecret(
    std::array<std::uint8_t, static_cast<std::size_t>(Size::SECRET)>& secret) const
{
    const bool decoded = HexCodec::getInstance().decode(TEST_SECRET_HEX,
                                                        secret.data(), secret.size());
    if (!decoded)
    {
        OPENSSL_cleanse(secret.data(), secret.size());
    }
    return decoded;
}

std::vector<std::uint8_t> UdsSecurity::handle(const std::uint8_t* request,
                                              std::size_t size,
                                              bool programmingSession,
                                              Session& session)
{
    if (2 > size)
    {
        return negativeResponse(static_cast<std::uint8_t>(NegativeCode::INVALID_LENGTH));
    }
    const std::uint8_t accessType = request[1];
    if ((static_cast<std::uint8_t>(Service::REQUEST_SEED) != accessType)
        && (static_cast<std::uint8_t>(Service::SEND_KEY) != accessType))
    {
        return negativeResponse(static_cast<std::uint8_t>(NegativeCode::UNSUPPORTED_SUBFUNCTION));
    }
    const std::size_t expectedSize =
        (static_cast<std::uint8_t>(Service::REQUEST_SEED) == accessType)
            ? 2U
            : 2U + static_cast<std::size_t>(Size::KEY);
    if (expectedSize != size)
    {
        return negativeResponse(static_cast<std::uint8_t>(NegativeCode::INVALID_LENGTH));
    }
    if (!programmingSession)
    {
        return negativeResponse(static_cast<std::uint8_t>(NegativeCode::SESSION_NOT_SUPPORTED));
    }

    const auto now = Clock::now();
    if ((Clock::time_point{} != pLockoutUntil) && (now >= pLockoutUntil))
    {
        pLockoutUntil = Clock::time_point{};
        pWrongKeys = 0;
    }
    if (Clock::time_point{} != pLockoutUntil)
    {
        return negativeResponse(static_cast<std::uint8_t>(NegativeCode::REQUIRED_DELAY));
    }

    if (static_cast<std::uint8_t>(Service::REQUEST_SEED) == accessType)
    {
        session.hasSeed = false;
        OPENSSL_cleanse(session.seed.data(), session.seed.size());
        if (session.unlocked)
        {
            std::vector<std::uint8_t> response{
                static_cast<std::uint8_t>(Service::POSITIVE_RESPONSE),
                static_cast<std::uint8_t>(Service::REQUEST_SEED)};
            response.insert(response.end(), static_cast<std::size_t>(Size::SEED), 0x00);
            return response;
        }
        if (1 != RAND_bytes(session.seed.data(), static_cast<int>(session.seed.size())))
        {
            OPENSSL_cleanse(session.seed.data(), session.seed.size());
            return negativeResponse(
                static_cast<std::uint8_t>(NegativeCode::GENERAL_PROGRAMMING_FAILURE));
        }
        if (std::all_of(session.seed.begin(), session.seed.end(),
                        [](std::uint8_t value) { return 0 == value; }))
        {
            OPENSSL_cleanse(session.seed.data(), session.seed.size());
            return negativeResponse(
                static_cast<std::uint8_t>(NegativeCode::GENERAL_PROGRAMMING_FAILURE));
        }
        session.hasSeed = true;
        std::vector<std::uint8_t> response{
            static_cast<std::uint8_t>(Service::POSITIVE_RESPONSE),
            static_cast<std::uint8_t>(Service::REQUEST_SEED)};
        response.insert(response.end(), session.seed.begin(), session.seed.end());
        return response;
    }

    if (!session.hasSeed)
    {
        return negativeResponse(static_cast<std::uint8_t>(NegativeCode::REQUEST_SEQUENCE_ERROR));
    }

    std::array<std::uint8_t, EVP_MAX_MD_SIZE> digest{};
    unsigned int digestSize = 0;
    const auto* result = HMAC(EVP_sha256(), pSecret.data(),
                              static_cast<int>(pSecret.size()),
                              session.seed.data(), session.seed.size(),
                              digest.data(), &digestSize);
    session.hasSeed = false;
    OPENSSL_cleanse(session.seed.data(), session.seed.size());
    if ((nullptr == result) || (static_cast<std::size_t>(Size::KEY) != digestSize))
    {
        OPENSSL_cleanse(digest.data(), digest.size());
        return negativeResponse(static_cast<std::uint8_t>(NegativeCode::GENERAL_PROGRAMMING_FAILURE));
    }

    const bool matches = 0 == CRYPTO_memcmp(
        digest.data(), request + 2, static_cast<std::size_t>(Size::KEY));
    OPENSSL_cleanse(digest.data(), digest.size());
    if (matches)
    {
        session.unlocked = true;
        pWrongKeys = 0;
        pLockoutUntil = Clock::time_point{};
        return {static_cast<std::uint8_t>(Service::POSITIVE_RESPONSE),
                static_cast<std::uint8_t>(Service::SEND_KEY)};
    }

    ++pWrongKeys;
    if (static_cast<std::uint8_t>(Policy::MAX_WRONG_KEYS) <= pWrongKeys)
    {
        pLockoutUntil = now + pLockoutDuration;
        return negativeResponse(static_cast<std::uint8_t>(NegativeCode::EXCEEDED_ATTEMPTS));
    }
    return negativeResponse(static_cast<std::uint8_t>(NegativeCode::INVALID_KEY));
}
