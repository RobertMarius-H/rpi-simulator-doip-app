#ifndef PSI_VERIFICATION_MANAGER_H
#define PSI_VERIFICATION_MANAGER_H

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>

class VerificationManager
{
public:
    enum class Result
    {
        PENDING,
        VERIFIED,
        MISMATCH,
        STORAGE_ERROR
    };

    enum class Size : std::size_t
    {
        SHA256_BYTES = 32
    };

    enum class Limit : std::uint32_t
    {
        MAXIMUM_DEMO_DURATION_MS = 30'000,
        PACE_POLL_MS = 20
    };

    static VerificationManager& getInstance();
    VerificationManager(const VerificationManager&) = delete;
    VerificationManager& operator=(const VerificationManager&) = delete;
    ~VerificationManager();

    bool start(std::uint32_t expectedSize, const std::uint8_t* expected, std::size_t size);
    Result poll();
    void cancel();
    void configureMinimumDuration(std::uint32_t milliseconds) { pMinimumDurationMs = milliseconds; }
    std::uint8_t progressPercent() const;

private:
    VerificationManager() = default;
    void clearDigests();
    bool reportProgress(std::uint32_t bytes, std::uint32_t total,
                        std::chrono::steady_clock::time_point started);

    std::array<std::uint8_t, static_cast<std::size_t>(Size::SHA256_BYTES)> pExpectedDigest{};
    std::array<std::uint8_t, static_cast<std::size_t>(Size::SHA256_BYTES)> pActualDigest{};
    std::future<bool> pVerification;
    std::atomic<std::uint32_t> pProcessedBytes{0};
    std::atomic<bool> pCancelled{false};
    std::uint32_t pExpectedSize = 0;
    std::uint32_t pMinimumDurationMs = 0;
};

#endif
