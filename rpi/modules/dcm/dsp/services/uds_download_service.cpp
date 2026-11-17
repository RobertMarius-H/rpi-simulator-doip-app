#include "uds_download_service.h"

#include "download_manager.h"
#include "rte_update.h"

UdsDownloadService& UdsDownloadService::getInstance()
{
    static UdsDownloadService instance;
    return instance;
}

std::vector<std::uint8_t> UdsDownloadService::negative(std::uint8_t sid,
                                                       NegativeCode code) const
{
    return {static_cast<std::uint8_t>(Service::NEGATIVE_RESPONSE),
            sid,
            static_cast<std::uint8_t>(code)};
}

std::vector<std::uint8_t> UdsDownloadService::handle(const std::uint8_t* request,
                                                      std::size_t size,
                                                      bool programmingSession,
                                                      bool unlocked)
{
    if ((nullptr == request) || (0 == size))
    {
        return {};
    }
    switch (static_cast<Service>(request[0]))
    {
        case Service::REQUEST_DOWNLOAD:
            return requestDownload(request, size, programmingSession, unlocked);
        case Service::TRANSFER_DATA:
            return transferData(request, size, programmingSession, unlocked);
        case Service::TRANSFER_EXIT:
            return transferExit(size, programmingSession, unlocked);
        default:
            return {};
    }
}

std::vector<std::uint8_t> UdsDownloadService::requestDownload(
    const std::uint8_t* request, std::size_t size,
    bool programmingSession, bool unlocked) const
{
    const auto sid = static_cast<std::uint8_t>(Service::REQUEST_DOWNLOAD);
    if (static_cast<std::size_t>(Layout::DOWNLOAD_LENGTH) != size)
    {
        return negative(sid, NegativeCode::INVALID_LENGTH);
    }
    if ((static_cast<std::uint8_t>(Format::UNCOMPRESSED)
         != request[static_cast<std::size_t>(Layout::DOWNLOAD_FORMAT_OFFSET)])
        || (static_cast<std::uint8_t>(Format::FOUR_BYTE_ADDRESS_AND_SIZE)
            != request[static_cast<std::size_t>(Layout::ADDRESS_LENGTH_OFFSET)]))
    {
        return negative(sid, NegativeCode::OUT_OF_RANGE);
    }
    for (std::size_t index = static_cast<std::size_t>(Layout::ADDRESS_OFFSET);
         index < static_cast<std::size_t>(Layout::SIZE_OFFSET); ++index)
    {
        if (0 != request[index])
        {
            return negative(sid, NegativeCode::OUT_OF_RANGE);
        }
    }
    std::uint32_t declaredSize = 0;
    for (std::size_t index = static_cast<std::size_t>(Layout::SIZE_OFFSET);
         index < (static_cast<std::size_t>(Layout::SIZE_OFFSET)
                  + static_cast<std::size_t>(Layout::SIZE_BYTES)); ++index)
    {
        declaredSize = (declaredSize << 8U) | request[index];
    }
    if ((0 == declaredSize)
        || (static_cast<std::uint32_t>(DownloadManager::Limit::MAXIMUM_IMAGE_BYTES)
            < declaredSize))
    {
        return negative(sid, NegativeCode::OUT_OF_RANGE);
    }
    if (!programmingSession)
    {
        return negative(sid, NegativeCode::SESSION_NOT_SUPPORTED);
    }
    if (!unlocked)
    {
        return negative(sid, NegativeCode::SECURITY_DENIED);
    }
    const auto result = RteUpdate::getInstance().beginDownload(declaredSize);
    if (RteUpdate::Result::INVALID_STATE == result)
    {
        return negative(sid, NegativeCode::SEQUENCE_ERROR);
    }
    if (RteUpdate::Result::OK != result)
    {
        return negative(sid, NegativeCode::UPLOAD_DOWNLOAD_NOT_ACCEPTED);
    }
    const std::uint32_t maximumRequest =
        static_cast<std::uint32_t>(DownloadManager::Limit::MAXIMUM_BLOCK_BYTES)
        + static_cast<std::uint32_t>(Layout::TRANSFER_DATA_OFFSET);
    return {static_cast<std::uint8_t>(Service::DOWNLOAD_RESPONSE),
            static_cast<std::uint8_t>(DownloadResponse::LENGTH_FORMAT),
            static_cast<std::uint8_t>(maximumRequest >> 8U),
            static_cast<std::uint8_t>(maximumRequest)};
}

std::vector<std::uint8_t> UdsDownloadService::transferData(
    const std::uint8_t* request, std::size_t size,
    bool programmingSession, bool unlocked) const
{
    const auto sid = static_cast<std::uint8_t>(Service::TRANSFER_DATA);
    if ((static_cast<std::size_t>(Layout::TRANSFER_MINIMUM_LENGTH) > size)
        || (static_cast<std::size_t>(DownloadManager::Limit::MAXIMUM_BLOCK_BYTES)
            < (size - static_cast<std::size_t>(Layout::TRANSFER_DATA_OFFSET))))
    {
        return negative(sid, NegativeCode::INVALID_LENGTH);
    }
    if (!programmingSession)
    {
        return negative(sid, NegativeCode::SESSION_NOT_SUPPORTED);
    }
    if (!unlocked)
    {
        return negative(sid, NegativeCode::SECURITY_DENIED);
    }
    const auto result = RteUpdate::getInstance().writeBlock(
        request[static_cast<std::size_t>(Layout::TRANSFER_COUNTER_OFFSET)],
        request + static_cast<std::size_t>(Layout::TRANSFER_DATA_OFFSET),
        size - static_cast<std::size_t>(Layout::TRANSFER_DATA_OFFSET));
    if (RteUpdate::Result::INVALID_SEQUENCE == result)
    {
        return negative(sid, NegativeCode::WRONG_BLOCK_SEQUENCE);
    }
    if (RteUpdate::Result::STORAGE_ERROR == result)
    {
        return negative(sid, NegativeCode::TRANSFER_SUSPENDED);
    }
    if (RteUpdate::Result::OK != result)
    {
        return negative(sid, NegativeCode::SEQUENCE_ERROR);
    }
    return {static_cast<std::uint8_t>(Service::TRANSFER_RESPONSE),
            request[static_cast<std::size_t>(Layout::TRANSFER_COUNTER_OFFSET)]};
}

std::vector<std::uint8_t> UdsDownloadService::transferExit(
    std::size_t size, bool programmingSession, bool unlocked) const
{
    const auto sid = static_cast<std::uint8_t>(Service::TRANSFER_EXIT);
    if (static_cast<std::size_t>(Layout::EXIT_LENGTH) != size)
    {
        return negative(sid, NegativeCode::INVALID_LENGTH);
    }
    if (!programmingSession)
    {
        return negative(sid, NegativeCode::SESSION_NOT_SUPPORTED);
    }
    if (!unlocked)
    {
        return negative(sid, NegativeCode::SECURITY_DENIED);
    }
    const auto result = RteUpdate::getInstance().finishDownload();
    if (RteUpdate::Result::STORAGE_ERROR == result)
    {
        return negative(sid, NegativeCode::GENERAL_PROGRAMMING_FAILURE);
    }
    if (RteUpdate::Result::OK != result)
    {
        return negative(sid, NegativeCode::SEQUENCE_ERROR);
    }
    return {static_cast<std::uint8_t>(Service::EXIT_RESPONSE)};
}
