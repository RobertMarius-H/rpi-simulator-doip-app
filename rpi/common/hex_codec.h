#ifndef PSI_HEX_CODEC_H
#define PSI_HEX_CODEC_H

#include <cstddef>
#include <cstdint>
#include <string_view>

class HexCodec
{
private:
    enum class Layout : std::size_t
    {
        CHARACTERS_PER_BYTE = 2
    };

    HexCodec() = default;
    int nibble(char character) const;

public:
    static HexCodec& getInstance();
    HexCodec(const HexCodec&) = delete;
    HexCodec& operator=(const HexCodec&) = delete;

    bool decode(std::string_view input, std::uint8_t* output,
                std::size_t outputSize) const;
};

#endif
