#include "rte_storage.h"

#include "firmware_storage.h"

RteStorage& RteStorage::getInstance()
{
    static RteStorage instance;
    return instance;
}

bool RteStorage::prepare()
{
    return FirmwareStorage::getInstance().prepare();
}

bool RteStorage::append(const std::uint8_t* bytes, std::size_t size)
{
    return FirmwareStorage::getInstance().append(bytes, size);
}

bool RteStorage::finalize()
{
    return FirmwareStorage::getInstance().finalize();
}

bool RteStorage::discard()
{
    return FirmwareStorage::getInstance().discard();
}

bool RteStorage::removeVerified()
{
    return FirmwareStorage::getInstance().removeVerified();
}

bool RteStorage::hashIncoming(
    std::uint32_t expectedSize, Digest& digest,
    const std::function<bool(std::uint32_t)>& progress) const
{
    return FirmwareStorage::getInstance().hashIncoming(expectedSize, digest, progress);
}

bool RteStorage::markVerified()
{
    return FirmwareStorage::getInstance().markVerified();
}
