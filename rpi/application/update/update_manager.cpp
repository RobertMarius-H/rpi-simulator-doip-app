#include "update_manager.h"

#include "download_manager.h"
#include "rte_storage.h"
#include "verification_manager.h"

UpdateManager& UpdateManager::getInstance()
{
    static UpdateManager instance;
    return instance;
}

UpdateManager::UpdateManager()
    : pDownload(DownloadManager::getInstance()),
      pVerification(VerificationManager::getInstance())
{
}

UpdateManager::~UpdateManager()
{
    abortUnverified();
}

void UpdateManager::discardTransfer()
{
    pVerification.cancel();
    pState = RteStorage::getInstance().discard()
        ? State::IDLE : State::CLEANUP_PENDING;
    pDownload.reset();
}

UpdateManager::Result UpdateManager::begin(std::uint32_t declaredSize)
{
    if ((0 == declaredSize)
        || (static_cast<std::uint32_t>(DownloadManager::Limit::MAXIMUM_IMAGE_BYTES)
            < declaredSize))
    {
        return Result::INVALID_SIZE;
    }
    if ((State::RECEIVING == pState)
        || (State::RECEIVED == pState)
        || (State::VERIFYING == pState))
    {
        return Result::INVALID_STATE;
    }
    auto& storage = RteStorage::getInstance();
    if ((State::CLEANUP_PENDING == pState) && (!storage.discard()))
    {
        return Result::STORAGE_ERROR;
    }
    const State previousState = (State::CLEANUP_PENDING == pState) ? State::IDLE : pState;
    pState = State::IDLE;
    if (!storage.prepare())
    {
        pState = previousState;
        return Result::STORAGE_ERROR;
    }
    if (!storage.removeVerified())
    {
        discardTransfer();
        if (State::CLEANUP_PENDING != pState)
        {
            pState = previousState;
        }
        return Result::STORAGE_ERROR;
    }
    if (DownloadManager::Result::OK != pDownload.begin(declaredSize))
    {
        discardTransfer();
        return Result::STORAGE_ERROR;
    }
    pState = State::RECEIVING;
    return Result::OK;
}

UpdateManager::Result UpdateManager::receive(
    std::uint8_t counter, const std::uint8_t* bytes, std::size_t size)
{
    if (State::RECEIVING != pState)
    {
        return Result::INVALID_STATE;
    }
    const auto result = pDownload.receive(counter, bytes, size);
    switch (result)
    {
        case DownloadManager::Result::OK:
            return Result::OK;
        case DownloadManager::Result::INVALID_STATE:
            return Result::INVALID_STATE;
        case DownloadManager::Result::INVALID_SIZE:
            return Result::INVALID_SIZE;
        case DownloadManager::Result::INVALID_SEQUENCE:
            return Result::INVALID_SEQUENCE;
        case DownloadManager::Result::TOO_MUCH_DATA:
            discardTransfer();
            return Result::TOO_MUCH_DATA;
        case DownloadManager::Result::STORAGE_ERROR:
            discardTransfer();
            return Result::STORAGE_ERROR;
        default:
            discardTransfer();
            return Result::STORAGE_ERROR;
    }
}

UpdateManager::Result UpdateManager::finish()
{
    if (State::RECEIVING != pState)
    {
        return Result::INVALID_STATE;
    }
    const auto result = pDownload.finish();
    if (DownloadManager::Result::STORAGE_ERROR == result)
    {
        discardTransfer();
        return Result::STORAGE_ERROR;
    }
    if (DownloadManager::Result::OK != result)
    {
        return Result::INVALID_STATE;
    }
    pState = State::RECEIVED;
    return Result::OK;
}

UpdateManager::Result UpdateManager::startVerification(
    const std::uint8_t* expected, std::size_t size)
{
    if (State::RECEIVED != pState)
    {
        return Result::INVALID_STATE;
    }
    if ((nullptr == expected)
        || (static_cast<std::size_t>(VerificationManager::Size::SHA256_BYTES) != size))
    {
        return Result::INVALID_SIZE;
    }
    if (!pVerification.start(pDownload.declaredSize(), expected, size))
    {
        discardTransfer();
        return Result::STORAGE_ERROR;
    }
    pState = State::VERIFYING;
    return Result::PENDING;
}

UpdateManager::Result UpdateManager::pollVerification()
{
    if (State::VERIFYING != pState)
    {
        return Result::INVALID_STATE;
    }
    const auto result = pVerification.poll();
    if (VerificationManager::Result::PENDING == result)
    {
        return Result::PENDING;
    }
    if (VerificationManager::Result::VERIFIED != result)
    {
        discardTransfer();
        return VerificationManager::Result::MISMATCH == result
            ? Result::VERIFICATION_FAILED : Result::STORAGE_ERROR;
    }
    if (!RteStorage::getInstance().markVerified())
    {
        discardTransfer();
        return Result::STORAGE_ERROR;
    }
    pDownload.reset();
    pState = State::VERIFIED;
    return Result::OK;
}

std::uint8_t UpdateManager::verificationProgress() const
{
    return State::VERIFYING == pState ? pVerification.progressPercent() : 0;
}

UpdateManager::Result UpdateManager::activate()
{
    if (State::VERIFIED != pState)
    {
        return Result::INVALID_STATE;
    }
    if (!RteStorage::getInstance().removeVerified())
    {
        return Result::STORAGE_ERROR;
    }
    pState = State::IDLE;
    return Result::OK;
}

void UpdateManager::abortUnverified()
{
    if ((State::RECEIVING == pState)
        || (State::RECEIVED == pState)
        || (State::VERIFYING == pState)
        || (State::CLEANUP_PENDING == pState))
    {
        discardTransfer();
    }
}
