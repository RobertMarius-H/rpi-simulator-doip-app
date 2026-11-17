#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

#include <gtest/gtest.h>

#include "firmware_storage.h"
#include "rte_storage.h"
#include "rte_update.h"
#include "uds_download_service.h"
#include "uds_routine_service.h"

TEST(RteStorageTest, RoutesStagingAndVerificationToLinuxStorage)
{
    const auto directory = std::filesystem::temp_directory_path()
        / ("psi-l5-storage-" + std::to_string(::getpid()) + "-"
           + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
    ASSERT_TRUE(FirmwareStorage::getInstance().initialize(directory.string()));
    RteStorage& port = RteStorage::getInstance();

    ASSERT_TRUE(port.prepare());
    const std::array<std::uint8_t, 4> bytes{0x41, 0x42, 0x43, 0x44};
    ASSERT_TRUE(port.append(bytes.data(), bytes.size()));
    ASSERT_TRUE(port.finalize());
    RteStorage::Digest actual{};
    std::uint32_t progress = 0;
    ASSERT_TRUE(port.hashIncoming(4, actual,
                                  [&progress](std::uint32_t count) {
                                      progress = count;
                                      return true;
                                  }));
    const RteStorage::Digest expected{
        0xE1, 0x2E, 0x11, 0x5A, 0xCF, 0x45, 0x52, 0xB2,
        0x56, 0x8B, 0x55, 0xE9, 0x3C, 0xBD, 0x39, 0x39,
        0x4C, 0x4E, 0xF8, 0x1C, 0x82, 0x44, 0x7F, 0xAF,
        0xC9, 0x97, 0x88, 0x2A, 0x02, 0xD2, 0x36, 0x77};
    EXPECT_EQ(expected, actual);
    EXPECT_EQ(4U, progress);
    EXPECT_FALSE(port.hashIncoming(5, actual, [](std::uint32_t) { return true; }));
    ASSERT_TRUE(port.markVerified());
    EXPECT_TRUE(std::filesystem::exists(directory / "verified.img"));
    EXPECT_TRUE(port.removeVerified());
    EXPECT_FALSE(std::filesystem::exists(directory / "verified.img"));
    ASSERT_TRUE(port.prepare());
    ASSERT_TRUE(port.append(bytes.data(), bytes.size()));
    EXPECT_TRUE(port.discard());
    EXPECT_FALSE(std::filesystem::exists(directory / "incoming.img"));
    std::filesystem::remove_all(directory);
}

TEST(RteUpdateTest, RoutesCompleteUpdateAndAbort)
{
    const auto directory = std::filesystem::temp_directory_path()
        / ("psi-l5-update-" + std::to_string(::getpid()) + "-"
           + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
    ASSERT_TRUE(FirmwareStorage::getInstance().initialize(directory.string()));
    RteUpdate& port = RteUpdate::getInstance();
    port.abortUnverified();
    EXPECT_EQ(RteUpdate::State::IDLE, port.state());
    EXPECT_EQ(RteUpdate::Result::INVALID_SIZE, port.beginDownload(0));
    EXPECT_EQ(RteUpdate::Result::OK, port.beginDownload(4));
    EXPECT_EQ(RteUpdate::State::RECEIVING, port.state());

    const std::array<std::uint8_t, 4> bytes{0x41, 0x42, 0x43, 0x44};
    EXPECT_EQ(RteUpdate::Result::INVALID_SEQUENCE,
              port.writeBlock(2, bytes.data(), bytes.size()));
    EXPECT_EQ(RteUpdate::Result::OK,
              port.writeBlock(1, bytes.data(), bytes.size()));
    EXPECT_EQ(RteUpdate::Result::OK, port.finishDownload());
    EXPECT_EQ(RteUpdate::State::RECEIVED, port.state());
    EXPECT_EQ(RteUpdate::Result::INVALID_SIZE,
              port.startVerification(bytes.data(), bytes.size()));

    const std::array<std::uint8_t, 32> digest{
        0xE1, 0x2E, 0x11, 0x5A, 0xCF, 0x45, 0x52, 0xB2,
        0x56, 0x8B, 0x55, 0xE9, 0x3C, 0xBD, 0x39, 0x39,
        0x4C, 0x4E, 0xF8, 0x1C, 0x82, 0x44, 0x7F, 0xAF,
        0xC9, 0x97, 0x88, 0x2A, 0x02, 0xD2, 0x36, 0x77};
    ASSERT_EQ(RteUpdate::Result::PENDING,
              port.startVerification(digest.data(), digest.size()));
    EXPECT_EQ(RteUpdate::State::VERIFYING, port.state());
    EXPECT_LE(port.verificationProgress(), 100U);
    RteUpdate::Result result = RteUpdate::Result::PENDING;
    for (int attempt = 0; attempt < 1000; ++attempt)
    {
        result = port.pollVerification();
        if (RteUpdate::Result::PENDING != result)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_EQ(RteUpdate::Result::OK, result);
    EXPECT_EQ(RteUpdate::State::VERIFIED, port.state());
    EXPECT_EQ(RteUpdate::Result::OK, port.activate());
    EXPECT_EQ(RteUpdate::State::IDLE, port.state());
    EXPECT_EQ(RteUpdate::Result::INVALID_STATE, port.activate());
    EXPECT_FALSE(std::filesystem::exists(directory / "verified.img"));

    ASSERT_EQ(RteUpdate::Result::OK, port.beginDownload(4));
    ASSERT_EQ(RteUpdate::Result::OK,
              port.writeBlock(1, bytes.data(), bytes.size()));
    port.abortUnverified();
    EXPECT_EQ(RteUpdate::State::IDLE, port.state());
    EXPECT_FALSE(std::filesystem::exists(directory / "incoming.img"));
    std::filesystem::remove_all(directory);
}

TEST(DcmRteIntegrationTest, DiagnosticServicesUseTypedUpdatePort)
{
    const auto directory = std::filesystem::temp_directory_path()
        / ("psi-l5-dcm-" + std::to_string(::getpid()) + "-"
           + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
    ASSERT_TRUE(FirmwareStorage::getInstance().initialize(directory.string()));
    RteUpdate::getInstance().abortUnverified();

    const std::array<std::uint8_t, 11> download{
        0x34, 0x00, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04};
    auto& dcm = UdsDownloadService::getInstance();
    EXPECT_EQ((std::vector<std::uint8_t>{0x74, 0x20, 0x40, 0x02}),
              dcm.handle(download.data(), download.size(), true, true));
    const std::array<std::uint8_t, 6> block{0x36, 0x01, 0x41, 0x42, 0x43, 0x44};
    EXPECT_EQ((std::vector<std::uint8_t>{0x76, 0x01}),
              dcm.handle(block.data(), block.size(), true, true));
    const std::array<std::uint8_t, 1> finish{0x37};
    EXPECT_EQ((std::vector<std::uint8_t>{0x77}),
              dcm.handle(finish.data(), finish.size(), true, true));

    const std::array<std::uint8_t, 32> digest{
        0xE1, 0x2E, 0x11, 0x5A, 0xCF, 0x45, 0x52, 0xB2,
        0x56, 0x8B, 0x55, 0xE9, 0x3C, 0xBD, 0x39, 0x39,
        0x4C, 0x4E, 0xF8, 0x1C, 0x82, 0x44, 0x7F, 0xAF,
        0xC9, 0x97, 0x88, 0x2A, 0x02, 0xD2, 0x36, 0x77};
    std::array<std::uint8_t, 36> verify{};
    verify[0] = 0x31;
    verify[1] = 0x01;
    verify[2] = 0xF0;
    verify[3] = 0x03;
    std::copy(digest.begin(), digest.end(), verify.begin() + 4);
    auto& routine = UdsRoutineService::getInstance();
    EXPECT_EQ((std::vector<std::uint8_t>{0x71, 0x01, 0xF0, 0x03}),
              routine.handle(verify.data(), verify.size(), true, true));
    const std::array<std::uint8_t, 4> status{0x31, 0x03, 0xF0, 0x03};
    std::vector<std::uint8_t> answer;
    for (int attempt = 0; attempt < 1000; ++attempt)
    {
        answer = routine.handle(status.data(), status.size(), true, true);
        if ((6 == answer.size()) && (0x02 == answer[4]))
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    EXPECT_EQ((std::vector<std::uint8_t>{0x71, 0x03, 0xF0, 0x03, 0x02, 0x64}),
              answer);
    const std::array<std::uint8_t, 4> activate{0x31, 0x01, 0xF0, 0x02};
    EXPECT_EQ((std::vector<std::uint8_t>{0x71, 0x01, 0xF0, 0x02}),
              routine.handle(activate.data(), activate.size(), true, true));
    EXPECT_EQ(RteUpdate::State::IDLE, RteUpdate::getInstance().state());
    std::filesystem::remove_all(directory);
}
