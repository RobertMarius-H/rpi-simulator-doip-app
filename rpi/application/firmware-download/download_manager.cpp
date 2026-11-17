#include "download_manager.h"

#include <algorithm>

#include "rte_storage.h"

DownloadManager& DownloadManager::getInstance()
{
    static DownloadManager instance;
    return instance;
}

void DownloadManager::reset()
{
    pDeclaredSize = 0;
    pAcceptedSize = 0;
    pNextCounter = 1;
    pLastCounter = 0;
    pHasLastBlock = false;
    pReceiving = false;
    std::fill(pLastBlock.begin(), pLastBlock.end(), 0);
    pLastBlock.clear();
}

DownloadManager::Result DownloadManager::begin(std::uint32_t declaredSize)
{
    if ((0 == declaredSize)
        || (static_cast<std::uint32_t>(Limit::MAXIMUM_IMAGE_BYTES) < declaredSize))
    {
        return Result::INVALID_SIZE;
    }
    if (pReceiving)
    {
        return Result::INVALID_STATE;
    }
    reset();
    pDeclaredSize = declaredSize;
    pReceiving = true;
    return Result::OK;
}

DownloadManager::Result DownloadManager::receive(
    std::uint8_t counter, const std::uint8_t* bytes, std::size_t size)
{
    if (!pReceiving)
    {
        return Result::INVALID_STATE;
    }
    if ((nullptr == bytes) || (0 == size)
        || (static_cast<std::size_t>(Limit::MAXIMUM_BLOCK_BYTES) < size))
    {
        return Result::INVALID_SIZE;
    }
    if ((pHasLastBlock) && (pLastCounter == counter))
    {
        if ((pLastBlock.size() == size)
            && (std::equal(pLastBlock.begin(), pLastBlock.end(), bytes)))
        {
            return Result::OK;
        }
        return Result::INVALID_SEQUENCE;
    }
    if (pNextCounter != counter)
    {
        return Result::INVALID_SEQUENCE;
    }
    if (size > static_cast<std::size_t>(pDeclaredSize - pAcceptedSize))
    {
        return Result::TOO_MUCH_DATA;
    }
    if (!RteStorage::getInstance().append(bytes, size))
    {
        return Result::STORAGE_ERROR;
    }
    pAcceptedSize += static_cast<std::uint32_t>(size);
    pLastCounter = counter;
    pNextCounter = static_cast<std::uint8_t>(counter + 1U);
    pHasLastBlock = true;
    pLastBlock.assign(bytes, bytes + size);
    return Result::OK;
}

DownloadManager::Result DownloadManager::finish()
{
    if ((!pReceiving) || (pAcceptedSize != pDeclaredSize))
    {
        return Result::INVALID_STATE;
    }
    if (!RteStorage::getInstance().finalize())
    {
        return Result::STORAGE_ERROR;
    }
    pReceiving = false;
    return Result::OK;
}
