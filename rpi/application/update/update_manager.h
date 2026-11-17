#ifndef PSI_UPDATE_MANAGER_H
#define PSI_UPDATE_MANAGER_H

#include <cstddef>
#include <cstdint>

#include "rte_update.h"

class DownloadManager;
class VerificationManager;

// Coordinates update phases without interpreting UDS or owning storage descriptors.
// TODO(OTA): Add origin and transaction metadata when an authenticated OTA source exists.
// TODO(OTA): Publish typed completion events to source-specific reporters.
class UpdateManager
{
public:
    using State = RteUpdate::State;
    using Result = RteUpdate::Result;

    static UpdateManager& getInstance();
    UpdateManager(const UpdateManager&) = delete;
    UpdateManager& operator=(const UpdateManager&) = delete;
    ~UpdateManager();

    Result begin(std::uint32_t declaredSize);
    Result receive(std::uint8_t counter, const std::uint8_t* bytes, std::size_t size);
    Result finish();
    Result startVerification(const std::uint8_t* expected, std::size_t size);
    Result pollVerification();
    std::uint8_t verificationProgress() const;
    Result activate();
    void abortUnverified();
    State state() const { return pState; }

private:
    UpdateManager();
    void discardTransfer();

    DownloadManager& pDownload;
    VerificationManager& pVerification;
    State pState = State::IDLE;
};

#endif
