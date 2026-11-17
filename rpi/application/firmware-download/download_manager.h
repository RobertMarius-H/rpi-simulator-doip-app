#ifndef PSI_DOWNLOAD_MANAGER_H
#define PSI_DOWNLOAD_MANAGER_H

#include <cstddef>
#include <cstdint>
#include <vector>

class DownloadManager
{
public:
    enum class Result
    {
        OK,
        INVALID_STATE,
        INVALID_SIZE,
        INVALID_SEQUENCE,
        TOO_MUCH_DATA,
        STORAGE_ERROR
    };

    enum class Limit : std::uint32_t
    {
        MAXIMUM_IMAGE_BYTES = 500'000'000,
        MAXIMUM_BLOCK_BYTES = 16'384
    };

    static DownloadManager& getInstance();
    DownloadManager(const DownloadManager&) = delete;
    DownloadManager& operator=(const DownloadManager&) = delete;

    Result begin(std::uint32_t declaredSize);
    // TODO(OTA): Move the UDS block counter/retry policy to the Dcm adapter.
    Result receive(std::uint8_t counter, const std::uint8_t* bytes, std::size_t size);
    Result finish();
    void reset();
    std::uint32_t declaredSize() const { return pDeclaredSize; }

private:
    DownloadManager() = default;

    std::uint32_t pDeclaredSize = 0;
    std::uint32_t pAcceptedSize = 0;
    std::uint8_t pNextCounter = 1;
    std::uint8_t pLastCounter = 0;
    bool pHasLastBlock = false;
    bool pReceiving = false;
    std::vector<std::uint8_t> pLastBlock;
};

#endif
