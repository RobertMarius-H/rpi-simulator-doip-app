#include "rte_update.h"

#include "update_manager.h"

RteUpdate& RteUpdate::getInstance()
{
    static RteUpdate instance;
    return instance;
}

RteUpdate::Result RteUpdate::beginDownload(std::uint32_t declaredSize)
{
    return UpdateManager::getInstance().begin(declaredSize);
}

RteUpdate::Result RteUpdate::writeBlock(
    std::uint8_t counter, const std::uint8_t* bytes, std::size_t size)
{
    return UpdateManager::getInstance().receive(counter, bytes, size);
}

RteUpdate::Result RteUpdate::finishDownload()
{
    return UpdateManager::getInstance().finish();
}

RteUpdate::Result RteUpdate::startVerification(
    const std::uint8_t* digest, std::size_t size)
{
    return UpdateManager::getInstance().startVerification(digest, size);
}

RteUpdate::Result RteUpdate::pollVerification()
{
    return UpdateManager::getInstance().pollVerification();
}

std::uint8_t RteUpdate::verificationProgress() const
{
    return UpdateManager::getInstance().verificationProgress();
}

RteUpdate::Result RteUpdate::activate()
{
    return UpdateManager::getInstance().activate();
}

void RteUpdate::abortUnverified()
{
    UpdateManager::getInstance().abortUnverified();
}

RteUpdate::State RteUpdate::state() const
{
    return UpdateManager::getInstance().state();
}
