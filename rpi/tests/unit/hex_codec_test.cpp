#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

#include <gtest/gtest.h>

#include "hex_codec.h"

class HexCodecTest : public ::testing::Test
{
protected:
    HexCodec& pCodec = HexCodec::getInstance();
};

TEST_F(HexCodecTest, DecodesMixedCaseBytes)
{
    std::array<std::uint8_t, 3> decoded{};
    ASSERT_TRUE(pCodec.decode("00aF7b", decoded.data(), decoded.size()));
    EXPECT_EQ(decoded, (std::array<std::uint8_t, 3>{0x00, 0xAF, 0x7B}));
}

TEST_F(HexCodecTest, RejectsMalformedInput)
{
    std::array<std::uint8_t, 2> decoded{};
    EXPECT_FALSE(pCodec.decode("0", decoded.data(), decoded.size()));
    EXPECT_FALSE(pCodec.decode("000000", decoded.data(), decoded.size()));
    EXPECT_FALSE(pCodec.decode("00X0", decoded.data(), decoded.size()));
    EXPECT_FALSE(pCodec.decode("0000", nullptr, decoded.size()));
    EXPECT_FALSE(pCodec.decode("00", decoded.data(),
                               std::numeric_limits<std::size_t>::max()));
}
