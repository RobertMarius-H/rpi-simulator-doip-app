#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <cstdlib>
#include <gtest/gtest.h>
#include <openssl/hmac.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "hex_codec.h"

class VerticalSliceTest : public ::testing::Test
{
protected:
    using Bytes = std::vector<std::uint8_t>;

    void SetUp() override
    {
        char directory[] = "/tmp/psi-update-XXXXXX";
        const char* created = ::mkdtemp(directory);
        ASSERT_NE(created, nullptr);
        pDirectory = created;
        const int probe = ::socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_GE(probe, 0);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;
        ASSERT_EQ(::bind(probe, reinterpret_cast<sockaddr*>(&address), sizeof(address)), 0);
        socklen_t length = sizeof(address);
        ASSERT_EQ(::getsockname(probe, reinterpret_cast<sockaddr*>(&address), &length), 0);
        pPort = ntohs(address.sin_port);
        ::close(probe);
        pServer = ::fork();
        ASSERT_GE(pServer, 0);
        if (0 == pServer)
        {
            const std::string port = std::to_string(pPort);
            const std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
            const char* connections = "2";
            if (("RejectsMalformedHeaders" == testName)
                || ("RoutesOnlyValidDiagnosticAddresses" == testName))
            {
                connections = "4";
            }
            else if ("SecurityAccessSequenceAndLockout" == testName)
            {
                connections = "3";
            }
            else if ("VerifiesAndActivatesStagedImage" == testName)
            {
                connections = "3";
            }
            else if ("RejectsSecondResponsiveTester" == testName)
            {
                connections = "23";
            }
            else if ("ReplacesStaleTester" == testName)
            {
                connections = "3";
            }
            else if ("KeepsDiagnosticRequestDuringAliveCheck" == testName)
            {
                connections = "3";
            }
            if (("ChecksIdleTester" == testName)
                || ("ClosesIdleTesterWithoutAliveResponse" == testName))
            {
                ::execl(PSI_SERVER_PATH, PSI_SERVER_PATH, "--port", port.c_str(),
                        "--connections", connections, "--update-dir", pDirectory.c_str(),
                        "--alive-idle-ms", "200", nullptr);
            }
            else
            {
                ::execl(PSI_SERVER_PATH, PSI_SERVER_PATH, "--port", port.c_str(),
                        "--connections", connections, "--update-dir", pDirectory.c_str(),
                        nullptr);
            }
            ::_exit(127);
        }
        for (int attempt = 0; attempt < 100; ++attempt)
        {
            int status = 0;
            if (pServer == ::waitpid(pServer, &status, WNOHANG))
            {
                FAIL() << "server exited before accepting connections";
            }
            pClient = connectClient();
            if (0 <= pClient)
            {
                ::close(pClient);
                pClient = -1;
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        FAIL() << "server did not listen";
    }

    void TearDown() override
    {
        closeClient();
        if (0 < pServer)
        {
            int status = 0;
            for (int attempt = 0; attempt < 100; ++attempt)
            {
                if (pServer == ::waitpid(pServer, &status, WNOHANG))
                {
                    EXPECT_TRUE(WIFEXITED(status));
                    EXPECT_EQ(WEXITSTATUS(status), 0);
                    EXPECT_EQ(::rmdir(pDirectory.c_str()), 0);
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            ::kill(pServer, SIGTERM);
            ::waitpid(pServer, nullptr, 0);
            ADD_FAILURE() << "server did not exit after the expected number of connections";
        }
        EXPECT_EQ(::rmdir(pDirectory.c_str()), 0);
    }

    int connectClient() const
    {
        const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (0 > fd)
        {
            return -1;
        }
        timeval timeout{2, 0};
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(pPort);
        if (0 != ::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)))
        {
            ::close(fd);
            return -1;
        }
        return fd;
    }

    void openClient()
    {
        pClient = connectClient();
        ASSERT_GE(pClient, 0);
    }

    void closeClient()
    {
        if (0 <= pClient)
        {
            ::close(pClient);
            pClient = -1;
        }
    }

    Bytes hex(const char* text) const
    {
        const std::string source(text);
        Bytes result(source.size() / 2U);
        EXPECT_TRUE(HexCodec::getInstance().decode(source, result.data(), result.size()));
        return result;
    }

    void sendBytes(const Bytes& bytes)
    {
        sendBytesTo(pClient, bytes);
    }

    void sendBytesTo(int fd, const Bytes& bytes)
    {
        std::size_t sent = 0;
        while (sent < bytes.size())
        {
            const ssize_t count = ::send(
                fd, bytes.data() + sent, bytes.size() - sent, MSG_NOSIGNAL);
            ASSERT_GT(count, 0);
            sent += static_cast<std::size_t>(count);
        }
    }

    Bytes readExact(std::size_t size)
    {
        return readExactFrom(pClient, size);
    }

    Bytes readExactFrom(int fd, std::size_t size)
    {
        Bytes result(size);
        std::size_t received = 0;
        while (received < size)
        {
            const ssize_t count = ::recv(fd, result.data() + received, size - received, 0);
            if (0 >= count)
            {
                ADD_FAILURE() << "connection closed or timed out";
                return {};
            }
            received += static_cast<std::size_t>(count);
        }
        return result;
    }

    Bytes readFrame()
    {
        return readFrameFrom(pClient);
    }

    Bytes readFrameFrom(int fd)
    {
        Bytes header = readExactFrom(fd, 8);
        if (8U != header.size())
        {
            return {};
        }
        const std::size_t size = (static_cast<std::size_t>(header[4]) << 24U) |
            (static_cast<std::size_t>(header[5]) << 16U) |
            (static_cast<std::size_t>(header[6]) << 8U) | header[7];
        if (16390U < size)
        {
            ADD_FAILURE() << "oversized response";
            return {};
        }
        Bytes payload = readExactFrom(fd, size);
        header.insert(header.end(), payload.begin(), payload.end());
        return header;
    }

    void expectFrame(const char* expected)
    {
        EXPECT_EQ(readFrame(), hex(expected));
    }

    void expectFrameFrom(int fd, const char* expected)
    {
        EXPECT_EQ(readFrameFrom(fd), hex(expected));
    }

    void activate()
    {
        sendBytes(hex("03FC0005000000070E800000000000"));
        expectFrame("03FC0006000000090E8010001000000000");
    }

    Bytes readUdsResponse()
    {
        Bytes response = readFrame();
        if (12U > response.size())
        {
            return {};
        }
        EXPECT_TRUE(std::equal(response.begin(), response.begin() + 4U,
                               hex("03FC8001").begin()));
        EXPECT_TRUE(std::equal(response.begin() + 8U, response.begin() + 12U,
                               hex("10000E80").begin()));
        return Bytes(response.begin() + 12U, response.end());
    }

    Bytes sendUds(const Bytes& request)
    {
        Bytes frame = hex("03FC8001000000000E801000");
        const std::size_t length = 4U + request.size();
        frame[4] = static_cast<std::uint8_t>(length >> 24U);
        frame[5] = static_cast<std::uint8_t>(length >> 16U);
        frame[6] = static_cast<std::uint8_t>(length >> 8U);
        frame[7] = static_cast<std::uint8_t>(length);
        frame.insert(frame.end(), request.begin(), request.end());
        sendBytes(frame);
        expectFrame("03FC80020000000510000E8000");
        return readUdsResponse();
    }

    void expectClosed()
    {
        std::uint8_t byte = 0;
        EXPECT_EQ(::recv(pClient, &byte, 1, 0), 0);
    }

    void unlockProgramming()
    {
        EXPECT_EQ(sendUds(hex("1002")), hex("500201F401F4"));
        const Bytes seed = sendUds(hex("2701"));
        ASSERT_EQ(seed.size(), 18U);
        const Bytes secret = hex(
            "DCD74632290E94C7A5E9B70F940C38F163144A15AA9C4D9C08959C279AE8700D");
        std::array<std::uint8_t, 32> digest{};
        unsigned int digestSize = 0;
        ASSERT_NE(HMAC(EVP_sha256(), secret.data(), static_cast<int>(secret.size()),
                       seed.data() + 2U, 16U, digest.data(), &digestSize), nullptr);
        Bytes key = hex("2702");
        key.insert(key.end(), digest.begin(), digest.end());
        EXPECT_EQ(sendUds(key), hex("6702"));
    }

    pid_t pServer = -1;
    int pClient = -1;
    std::uint16_t pPort = 0;
    std::string pDirectory;
};

TEST_F(VerticalSliceTest, ReferenceTraceAndFragmentedFrames)
{
    openClient();
    const Bytes route = hex("03FC0005000000070E800000000000");
    sendBytes(Bytes(route.begin(), route.begin() + 4U));
    sendBytes(Bytes(route.begin() + 4U, route.end()));
    expectFrame("03FC0006000000090E8010001000000000");
    Bytes session = hex("03FC8001000000060E8010001002");
    const Bytes secondSession = session;
    session.insert(session.end(), secondSession.begin(), secondSession.end());
    sendBytes(session);
    for (int index = 0; index < 2; ++index)
    {
        expectFrame("03FC80020000000510000E8000");
        expectFrame("03FC80010000000A10000E80500201F401F4");
    }
}

TEST_F(VerticalSliceTest, RejectsMalformedHeaders)
{
    for (const auto* request : {"02FC000500000007", "03FC800100004007", "03FC000500000006"})
    {
        openClient();
        sendBytes(hex(request));
        const char* response = (0 == std::strcmp(request, "02FC000500000007"))
            ? "03FC00000000000100" : ((0 == std::strcmp(request, "03FC800100004007"))
            ? "03FC00000000000102" : "03FC00000000000104");
        expectFrame(response);
        expectClosed();
        closeClient();
    }
}

TEST_F(VerticalSliceTest, RoutesOnlyValidDiagnosticAddresses)
{
    openClient();
    activate();
    sendBytes(hex("03FC8001000000060E8010011002"));
    expectFrame("03FC80030000000510000E8003");
    EXPECT_EQ(sendUds(hex("22")), hex("7F2211"));
    closeClient();

    openClient();
    sendBytes(hex("03FC0005000000070E810000000000"));
    expectFrame("03FC0006000000090E8110000000000000");
    expectClosed();
    closeClient();

    openClient();
    activate();
    sendBytes(hex("03FC8001000000060E8110001002"));
    expectFrame("03FC80030000000510000E8102");
    expectClosed();
}

TEST_F(VerticalSliceTest, RejectsSecondResponsiveTester)
{
    openClient();
    activate();
    const int invalid = connectClient();
    ASSERT_GE(invalid, 0);
    sendBytesTo(invalid, hex("03FC0005000000070E810000000000"));
    expectFrameFrom(invalid, "03FC0006000000090E8110000000000000");
    ::close(invalid);
    for (int attempt = 0; attempt < 20; ++attempt)
    {
        const int second = connectClient();
        ASSERT_GE(second, 0);
        sendBytesTo(second, hex("03FC0005000000070E800000000000"));
        expectFrame("03FC000700000000");
        sendBytes(hex("03FC0008000000020E80"));
        expectFrameFrom(second, "03FC0006000000090E8010000100000000");
        ::close(second);
    }
    EXPECT_EQ(sendUds(hex("22")), hex("7F2211"));
}

TEST_F(VerticalSliceTest, ReplacesStaleTester)
{
    openClient();
    activate();
    const int second = connectClient();
    ASSERT_GE(second, 0);
    timeval timeout{5, 0};
    ASSERT_EQ(::setsockopt(second, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                           sizeof(timeout)), 0);
    sendBytesTo(second, hex("03FC0005000000070E800000000000"));
    expectFrame("03FC000700000000");
    expectFrameFrom(second, "03FC0006000000090E8010001000000000");
    expectClosed();
    closeClient();
    pClient = second;
    EXPECT_EQ(sendUds(hex("22")), hex("7F2211"));
}

TEST_F(VerticalSliceTest, KeepsDiagnosticRequestDuringAliveCheck)
{
    openClient();
    activate();
    const int second = connectClient();
    ASSERT_GE(second, 0);
    sendBytesTo(second, hex("03FC0005000000070E800000000000"));
    expectFrame("03FC000700000000");
    sendBytes(hex("03FC8001000000060E8010001001"));
    sendBytes(hex("03FC0008000000020E80"));
    expectFrame("03FC80020000000510000E8000");
    expectFrame("03FC80010000000A10000E80500101F401F4");
    expectFrameFrom(second, "03FC0006000000090E8010000100000000");
    ::close(second);
}

TEST_F(VerticalSliceTest, ChecksIdleTester)
{
    openClient();
    activate();
    expectFrame("03FC000700000000");
    sendBytes(hex("03FC0008000000020E80"));
    EXPECT_EQ(sendUds(hex("22")), hex("7F2211"));
    expectFrame("03FC000700000000");
    sendBytes(hex("03FC0008000000020E81"));
    expectClosed();
}

TEST_F(VerticalSliceTest, ClosesIdleTesterWithoutAliveResponse)
{
    openClient();
    activate();
    expectFrame("03FC000700000000");
    timeval timeout{5, 0};
    ASSERT_EQ(::setsockopt(pClient, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                           sizeof(timeout)), 0);
    expectClosed();
}

TEST_F(VerticalSliceTest, SecurityAccessSequenceAndLockout)
{
    openClient();
    activate();
    EXPECT_EQ(sendUds(hex("2701")), hex("7F277F"));
    EXPECT_EQ(sendUds(hex("27")), hex("7F2713"));
    EXPECT_EQ(sendUds(hex("2703")), hex("7F2712"));
    EXPECT_EQ(sendUds(hex("1002")), hex("500201F401F4"));
    EXPECT_EQ(sendUds(hex("2702")), hex("7F2713"));
    Bytes emptyKey = hex("2702");
    emptyKey.insert(emptyKey.end(), 32U, 0);
    EXPECT_EQ(sendUds(emptyKey), hex("7F2724"));

    const Bytes first = sendUds(hex("2701"));
    const Bytes second = sendUds(hex("2701"));
    ASSERT_EQ(first.size(), 18U);
    ASSERT_EQ(second.size(), 18U);
    EXPECT_NE(first, second);
    Bytes secret = hex("DCD74632290E94C7A5E9B70F940C38F163144A15AA9C4D9C08959C279AE8700D");
    std::array<std::uint8_t, 32> digest{};
    unsigned int digestSize = 0;
    ASSERT_NE(HMAC(EVP_sha256(), secret.data(), static_cast<int>(secret.size()),
                   first.data() + 2U, 16U, digest.data(), &digestSize), nullptr);
    Bytes oldKey = hex("2702");
    oldKey.insert(oldKey.end(), digest.begin(), digest.end());
    EXPECT_EQ(sendUds(oldKey), hex("7F2735"));

    const Bytes third = sendUds(hex("2701"));
    ASSERT_EQ(third.size(), 18U);
    ASSERT_NE(HMAC(EVP_sha256(), secret.data(), static_cast<int>(secret.size()),
                   third.data() + 2U, 16U, digest.data(), &digestSize), nullptr);
    Bytes key = hex("2702");
    key.insert(key.end(), digest.begin(), digest.end());
    EXPECT_EQ(sendUds(key), hex("6702"));
    Bytes unlocked = hex("6701");
    unlocked.insert(unlocked.end(), 16U, 0);
    EXPECT_EQ(sendUds(hex("2701")), unlocked);
    EXPECT_EQ(sendUds(hex("1002")), hex("500201F401F4"));
    EXPECT_EQ(sendUds(hex("1001")), hex("500101F401F4"));
    EXPECT_EQ(sendUds(hex("2701")), hex("7F277F"));
    closeClient();

    openClient();
    activate();
    EXPECT_EQ(sendUds(hex("1002")), hex("500201F401F4"));
    for (const auto* expected : {"7F2735", "7F2735", "7F2736"})
    {
        const Bytes seed = sendUds(hex("2701"));
        ASSERT_EQ(seed.size(), 18U);
        EXPECT_EQ(sendUds(emptyKey), hex(expected));
    }
    EXPECT_EQ(sendUds(hex("2701")), hex("7F2737"));
}

TEST_F(VerticalSliceTest, StagesDownloadAndRejectsInvalidBlocks)
{
    openClient();
    activate();
    EXPECT_EQ(sendUds(hex("3400440000000000000004")), hex("7F347F"));
    EXPECT_EQ(sendUds(hex("1002")), hex("500201F401F4"));
    EXPECT_EQ(sendUds(hex("3400440000000000000004")), hex("7F3433"));
    unlockProgramming();
    EXPECT_EQ(sendUds(hex("3400440000000000000000")), hex("7F3431"));
    EXPECT_EQ(sendUds(hex("3400440000000000000004")), hex("74204002"));
    EXPECT_EQ(sendUds(hex("37")), hex("7F3724"));
    EXPECT_EQ(sendUds(hex("360241424344")), hex("7F3673"));
    EXPECT_EQ(sendUds(hex("360141424344")), hex("7601"));
    EXPECT_EQ(sendUds(hex("360141424344")), hex("7601"));
    EXPECT_EQ(sendUds(hex("360141424345")), hex("7F3673"));
    EXPECT_EQ(sendUds(hex("37")), hex("77"));

    std::ifstream staged(pDirectory + "/incoming.img", std::ios::binary);
    ASSERT_TRUE(staged.is_open());
    const std::string contents((std::istreambuf_iterator<char>(staged)),
                               std::istreambuf_iterator<char>());
    EXPECT_EQ(contents, "ABCD");
    staged.close();

    EXPECT_EQ(sendUds(hex("1002")), hex("500201F401F4"));
    EXPECT_FALSE(std::ifstream(pDirectory + "/incoming.img").good());
    unlockProgramming();
    EXPECT_EQ(sendUds(hex("3400440000000000000001")), hex("74204002"));
    EXPECT_EQ(sendUds(hex("36014142")), hex("7F3624"));
    EXPECT_FALSE(std::ifstream(pDirectory + "/incoming.img").good());
    EXPECT_EQ(sendUds(hex("3400440000000000000001")), hex("74204002"));
}

TEST_F(VerticalSliceTest, StagesAcrossSyncIntervalAndCounterWrap)
{
    openClient();
    activate();
    unlockProgramming();
    EXPECT_EQ(sendUds(hex("3400440000000000400001")), hex("74204002"));

    Bytes request(16'386U, 0xA5);
    request[0] = 0x36;
    for (std::size_t index = 0; index < 256U; ++index)
    {
        const auto counter = static_cast<std::uint8_t>(index + 1U);
        request[1] = counter;
        ASSERT_EQ(sendUds(request), (Bytes{0x76, counter}));
    }
    EXPECT_EQ(sendUds(hex("3601A5")), hex("7601"));
    EXPECT_EQ(sendUds(hex("37")), hex("77"));

    std::ifstream staged(pDirectory + "/incoming.img", std::ios::binary);
    ASSERT_TRUE(staged.is_open());
    const std::string contents((std::istreambuf_iterator<char>(staged)),
                               std::istreambuf_iterator<char>());
    ASSERT_EQ(contents.size(), 4'194'305U);
    EXPECT_TRUE(std::all_of(contents.begin(), contents.end(), [](char value) {
        return 0xA5 == static_cast<unsigned char>(value);
    }));
}

TEST_F(VerticalSliceTest, VerifiesAndActivatesStagedImage)
{
    openClient();
    activate();
    unlockProgramming();
    EXPECT_EQ(sendUds(hex("3101F0")), hex("7F3113"));
    EXPECT_EQ(sendUds(hex("3102F001")), hex("7F3112"));
    EXPECT_EQ(sendUds(hex("3101F004")), hex("7F3131"));
    EXPECT_EQ(sendUds(hex("3101F00100")), hex("7F3113"));
    EXPECT_EQ(sendUds(hex("3101F00200")), hex("7F3113"));
    Bytes wrong = hex("3101F001");
    wrong.insert(wrong.end(), 32U, 0);
    EXPECT_EQ(sendUds(wrong), hex("7F3124"));
    EXPECT_EQ(sendUds(hex("3101F002")), hex("7F3124"));

    EXPECT_EQ(sendUds(hex("3400440000000000000004")), hex("74204002"));
    EXPECT_EQ(sendUds(hex("360141424344")), hex("7601"));
    EXPECT_EQ(sendUds(hex("37")), hex("77"));
    EXPECT_EQ(sendUds(wrong), hex("7F3178"));
    EXPECT_EQ(readUdsResponse(), hex("7F3172"));
    EXPECT_FALSE(std::ifstream(pDirectory + "/incoming.img").good());

    EXPECT_EQ(sendUds(hex("3400440000000000000004")), hex("74204002"));
    EXPECT_EQ(sendUds(hex("360141424344")), hex("7601"));
    EXPECT_EQ(sendUds(hex("37")), hex("77"));
    std::array<std::uint8_t, 32> digest{};
    unsigned int digestSize = 0;
    ASSERT_EQ(EVP_Digest("ABCD", 4U, digest.data(), &digestSize,
                         EVP_sha256(), nullptr), 1);
    ASSERT_EQ(digestSize, digest.size());
    Bytes verify = hex("3101F003");
    verify.insert(verify.end(), digest.begin(), digest.end());
    EXPECT_EQ(sendUds(verify), hex("7101F003"));
    Bytes status;
    for (int attempt = 0; attempt < 20; ++attempt)
    {
        status = sendUds(hex("3103F003"));
        ASSERT_EQ(status.size(), 6U);
        EXPECT_TRUE(std::equal(status.begin(), status.begin() + 4U,
                               hex("7103F003").begin()));
        EXPECT_LE(status[5], 100U);
        if (2U == status[4])
        {
            break;
        }
        EXPECT_EQ(status[4], 1U);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    EXPECT_EQ(status, hex("7103F0030264"));
    EXPECT_FALSE(std::ifstream(pDirectory + "/incoming.img").good());
    EXPECT_TRUE(std::ifstream(pDirectory + "/verified.img").good());
    closeClient();

    openClient();
    activate();
    unlockProgramming();
    EXPECT_EQ(sendUds(hex("3101F002")), hex("7101F002"));
    EXPECT_EQ(sendUds(hex("2701")), hex("7F277F"));
    EXPECT_EQ(sendUds(hex("3101F002")), hex("7F317F"));
    EXPECT_EQ(sendUds(hex("22")), hex("7F2211"));
    unlockProgramming();
    EXPECT_EQ(sendUds(hex("3101F002")), hex("7F3124"));
    closeClient();
    EXPECT_FALSE(std::ifstream(pDirectory + "/verified.img").good());
}
