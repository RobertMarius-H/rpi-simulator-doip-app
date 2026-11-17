#include "firmware_storage.h"

#include <algorithm>
#include <cerrno>
#include <array>
#include <cstdio>
#include <fcntl.h>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>

#include <openssl/evp.h>

FirmwareStorage& FirmwareStorage::getInstance()
{
    static FirmwareStorage instance;
    return instance;
}

FirmwareStorage::~FirmwareStorage()
{
    closeIncoming();
    if (0 <= pDirectory)
    {
        ::close(pDirectory);
    }
}

void FirmwareStorage::closeIncoming()
{
    if (0 <= pIncoming)
    {
        ::close(pIncoming);
        pIncoming = -1;
    }
    pUnsyncedBytes = 0;
}

bool FirmwareStorage::removeFile(const char* name) const
{
    return (0 == ::unlinkat(pDirectory, name, 0)) || (ENOENT == errno);
}

bool FirmwareStorage::initialize(const std::string& directory)
{
    closeIncoming();
    if (0 <= pDirectory)
    {
        ::close(pDirectory);
        pDirectory = -1;
    }
    pDirectory = ::open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (0 > pDirectory)
    {
        return false;
    }
    if ((!removeFile(INCOMING_NAME))
        || (!removeFile(VERIFIED_NAME))
        || (!removeFile(PROBE_NAME)))
    {
        return false;
    }
    const int probe = ::openat(pDirectory, PROBE_NAME,
                               O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (0 > probe)
    {
        return false;
    }
    const bool closed = (0 == ::close(probe));
    return removeFile(PROBE_NAME) && closed;
}

bool FirmwareStorage::prepare()
{
    if ((0 > pDirectory) || (0 <= pIncoming))
    {
        return false;
    }
    pIncoming = ::openat(pDirectory, INCOMING_NAME,
                         O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    pUnsyncedBytes = 0;
    return 0 <= pIncoming;
}

bool FirmwareStorage::append(const std::uint8_t* bytes, std::size_t size)
{
    if ((0 > pIncoming) || (nullptr == bytes) || (0 == size))
    {
        return false;
    }
    std::size_t written = 0;
    while (written < size)
    {
        const ssize_t count = ::write(pIncoming, bytes + written, size - written);
        if (0 < count)
        {
            written += static_cast<std::size_t>(count);
        }
        else if ((0 > count) && (EINTR == errno))
        {
            continue;
        }
        else
        {
            return false;
        }
    }
    pUnsyncedBytes += written;
    if (static_cast<std::size_t>(Limit::SYNC_INTERVAL_BYTES) <= pUnsyncedBytes)
    {
        if (0 != ::fdatasync(pIncoming))
        {
            return false;
        }
        pUnsyncedBytes = 0;
    }
    return true;
}

bool FirmwareStorage::finalize()
{
    if (0 > pIncoming)
    {
        return false;
    }
    const bool synced = (0 == ::fsync(pIncoming));
    const bool closed = (0 == ::close(pIncoming));
    pIncoming = -1;
    pUnsyncedBytes = 0;
    return synced && closed;
}

bool FirmwareStorage::discard()
{
    closeIncoming();
    return (0 <= pDirectory) && removeFile(INCOMING_NAME);
}

bool FirmwareStorage::removeVerified()
{
    return (0 <= pDirectory) && removeFile(VERIFIED_NAME);
}

bool FirmwareStorage::hashIncoming(
    std::uint32_t expectedSize, Digest& digest,
    const std::function<bool(std::uint32_t)>& progress) const
{
    if (0 > pDirectory)
    {
        return false;
    }
    const int fd = ::openat(pDirectory, INCOMING_NAME, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (0 > fd)
    {
        return false;
    }
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(
        EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    struct stat information{};
    bool valid = (nullptr != context)
        && (0 == ::fstat(fd, &information))
        && (S_ISREG(information.st_mode))
        && (static_cast<off_t>(expectedSize) == information.st_size)
        && (1 == EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr));
    std::array<std::uint8_t, static_cast<std::size_t>(Limit::HASH_CHUNK_BYTES)> buffer{};
    std::uint32_t readBytes = 0;
    while (valid && (readBytes < expectedSize))
    {
        const std::size_t remaining = static_cast<std::size_t>(expectedSize - readBytes);
        const std::size_t wanted = std::min(buffer.size(), remaining);
        const ssize_t count = ::read(fd, buffer.data(), wanted);
        if ((0 > count) && (EINTR == errno))
        {
            continue;
        }
        valid = (0 < count)
            && (1 == EVP_DigestUpdate(context.get(), buffer.data(),
                                      static_cast<std::size_t>(count)));
        if (valid)
        {
            readBytes += static_cast<std::uint32_t>(count);
            valid = progress(readBytes);
        }
    }
    unsigned int digestSize = 0;
    valid = valid
        && (readBytes == expectedSize)
        && (1 == EVP_DigestFinal_ex(context.get(), digest.data(), &digestSize))
        && (digest.size() == digestSize);
    const bool closed = (0 == ::close(fd));
    return valid && closed;
}

bool FirmwareStorage::markVerified()
{
    if ((0 > pDirectory)
        || (0 != ::renameat(pDirectory, INCOMING_NAME, pDirectory, VERIFIED_NAME)))
    {
        return false;
    }
    if (0 == ::fsync(pDirectory))
    {
        return true;
    }
    removeFile(VERIFIED_NAME);
    return false;
}
