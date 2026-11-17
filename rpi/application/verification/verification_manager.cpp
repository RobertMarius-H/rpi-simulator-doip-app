#include "verification_manager.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <thread>

#include <openssl/crypto.h>

#include "rte_storage.h"

VerificationManager& VerificationManager::getInstance()
{
    static VerificationManager instance;
    return instance;
}

VerificationManager::~VerificationManager()
{
    cancel();
}

void VerificationManager::clearDigests()
{
    OPENSSL_cleanse(pExpectedDigest.data(), pExpectedDigest.size());
    OPENSSL_cleanse(pActualDigest.data(), pActualDigest.size());
}

bool VerificationManager::start(std::uint32_t expectedSize,
                                const std::uint8_t* expected, std::size_t size)
{
    if ((pVerification.valid()) || (nullptr == expected)
        || (pExpectedDigest.size() != size))
    {
        return false;
    }
    std::copy(expected, expected + size, pExpectedDigest.begin());
    pExpectedSize = expectedSize;
    pProcessedBytes.store(0);
    pCancelled.store(false);
    try
    {
        pVerification = std::async(std::launch::async, [this, expectedSize]() {
            const auto started = std::chrono::steady_clock::now();
            return RteStorage::getInstance().hashIncoming(
                expectedSize, pActualDigest,
                [this, expectedSize, started](std::uint32_t bytes) {
                    return reportProgress(bytes, expectedSize, started);
                });
        });
    }
    catch (const std::exception&)
    {
        clearDigests();
        return false;
    }
    return true;
}

bool VerificationManager::reportProgress(
    std::uint32_t bytes, std::uint32_t total,
    std::chrono::steady_clock::time_point started)
{
    if ((0 != pMinimumDurationMs) && (0 != total))
    {
        const auto elapsedTarget = std::chrono::milliseconds(
            (static_cast<std::uint64_t>(pMinimumDurationMs) * bytes) / total);
        const auto target = started + elapsedTarget;
        while ((!pCancelled.load()) && (std::chrono::steady_clock::now() < target))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(
                static_cast<std::uint32_t>(Limit::PACE_POLL_MS)));
        }
    }
    if (pCancelled.load())
    {
        return false;
    }
    pProcessedBytes.store(bytes);
    return true;
}

std::uint8_t VerificationManager::progressPercent() const
{
    if (0 == pExpectedSize)
    {
        return 0;
    }
    const std::uint64_t percentage =
        (static_cast<std::uint64_t>(pProcessedBytes.load()) * 100U) / pExpectedSize;
    return static_cast<std::uint8_t>(std::min<std::uint64_t>(percentage, 100U));
}

VerificationManager::Result VerificationManager::poll()
{
    if (!pVerification.valid())
    {
        return Result::STORAGE_ERROR;
    }
    if (std::future_status::ready != pVerification.wait_for(std::chrono::milliseconds(0)))
    {
        return Result::PENDING;
    }
    bool hashed = false;
    try
    {
        hashed = pVerification.get();
    }
    catch (const std::exception&)
    {
        hashed = false;
    }
    const bool matched = hashed
        && (0 == CRYPTO_memcmp(pActualDigest.data(), pExpectedDigest.data(),
                               pExpectedDigest.size()));
    clearDigests();
    if (!hashed)
    {
        return Result::STORAGE_ERROR;
    }
    return matched ? Result::VERIFIED : Result::MISMATCH;
}

void VerificationManager::cancel()
{
    if (pVerification.valid())
    {
        pCancelled.store(true);
        pVerification.wait();
        pVerification = std::future<bool>{};
    }
    pExpectedSize = 0;
    pProcessedBytes.store(0);
    pCancelled.store(false);
    clearDigests();
}
