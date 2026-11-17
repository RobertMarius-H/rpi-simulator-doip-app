#ifndef PSI_UDS_DOWNLOAD_SERVICE_H
#define PSI_UDS_DOWNLOAD_SERVICE_H

#include <cstddef>
#include <cstdint>
#include <vector>

class UdsDownloadService
{
private:
    enum class Service : std::uint8_t
    {
        REQUEST_DOWNLOAD = 0x34,
        TRANSFER_DATA = 0x36,
        TRANSFER_EXIT = 0x37,
        DOWNLOAD_RESPONSE = 0x74,
        TRANSFER_RESPONSE = 0x76,
        EXIT_RESPONSE = 0x77,
        NEGATIVE_RESPONSE = 0x7F
    };

    enum class NegativeCode : std::uint8_t
    {
        INVALID_LENGTH = 0x13,
        SEQUENCE_ERROR = 0x24,
        OUT_OF_RANGE = 0x31,
        SECURITY_DENIED = 0x33,
        UPLOAD_DOWNLOAD_NOT_ACCEPTED = 0x70,
        TRANSFER_SUSPENDED = 0x71,
        GENERAL_PROGRAMMING_FAILURE = 0x72,
        WRONG_BLOCK_SEQUENCE = 0x73,
        SESSION_NOT_SUPPORTED = 0x7F
    };

    enum class Layout : std::size_t
    {
        DOWNLOAD_LENGTH = 11,
        DOWNLOAD_FORMAT_OFFSET = 1,
        ADDRESS_LENGTH_OFFSET = 2,
        ADDRESS_OFFSET = 3,
        SIZE_OFFSET = 7,
        SIZE_BYTES = 4,
        TRANSFER_MINIMUM_LENGTH = 3,
        TRANSFER_COUNTER_OFFSET = 1,
        TRANSFER_DATA_OFFSET = 2,
        EXIT_LENGTH = 1
    };

    enum class Format : std::uint8_t
    {
        UNCOMPRESSED = 0x00,
        FOUR_BYTE_ADDRESS_AND_SIZE = 0x44
    };

    enum class DownloadResponse : std::uint8_t
    {
        LENGTH_FORMAT = 0x20
    };

    UdsDownloadService() = default;
    std::vector<std::uint8_t> negative(std::uint8_t sid, NegativeCode code) const;
    std::vector<std::uint8_t> requestDownload(const std::uint8_t* request,
                                              std::size_t size,
                                              bool programmingSession,
                                              bool unlocked) const;
    std::vector<std::uint8_t> transferData(const std::uint8_t* request,
                                           std::size_t size,
                                           bool programmingSession,
                                           bool unlocked) const;
    std::vector<std::uint8_t> transferExit(std::size_t size,
                                           bool programmingSession,
                                           bool unlocked) const;

public:
    static UdsDownloadService& getInstance();
    UdsDownloadService(const UdsDownloadService&) = delete;
    UdsDownloadService& operator=(const UdsDownloadService&) = delete;

    std::vector<std::uint8_t> handle(const std::uint8_t* request,
                                     std::size_t size,
                                     bool programmingSession,
                                     bool unlocked);
};

#endif
