#ifndef PSI_RTE_UPDATE_H
#define PSI_RTE_UPDATE_H

#include <cstddef>
#include <cstdint>

// ECU-specific RTE-lite contract between Dcm and the update application.
class RteUpdate
{
public:
    enum class State
    {
        IDLE,
        RECEIVING,
        RECEIVED,
        VERIFYING,
        VERIFIED,
        CLEANUP_PENDING
    };

    enum class Result
    {
        OK,
        INVALID_STATE,
        INVALID_SIZE,
        INVALID_SEQUENCE,
        TOO_MUCH_DATA,
        STORAGE_ERROR,
        VERIFICATION_FAILED,
        PENDING
    };

    static RteUpdate& getInstance();
    RteUpdate(const RteUpdate&) = delete;
    RteUpdate& operator=(const RteUpdate&) = delete;

    Result beginDownload(std::uint32_t declaredSize);
    Result writeBlock(std::uint8_t counter, const std::uint8_t* bytes,
                      std::size_t size);
    Result finishDownload();
    Result startVerification(const std::uint8_t* digest, std::size_t size);
    Result pollVerification();
    std::uint8_t verificationProgress() const;
    Result activate();
    void abortUnverified();
    State state() const;

private:
    RteUpdate() = default;
};

#endif
