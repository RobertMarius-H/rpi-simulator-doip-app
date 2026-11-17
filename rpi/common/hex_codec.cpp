#include "hex_codec.h"

HexCodec& HexCodec::getInstance()
{
    static HexCodec instance;
    return instance;
}

int HexCodec::nibble(char character) const
{
    int result = -1;
    if ('0' <= character && character <= '9')
    {
        result = character - '0';
    }
    else if ('a' <= character && character <= 'f')
    {
        result = character - 'a' + 10;
    }
    else if ('A' <= character && character <= 'F')
    {
        result = character - 'A' + 10;
    }
    return result;
}

bool HexCodec::decode(std::string_view input, std::uint8_t* output,
                      std::size_t outputSize) const
{
    const std::size_t charactersPerByte =
        static_cast<std::size_t>(Layout::CHARACTERS_PER_BYTE);
    if ((nullptr == output)
        || (0U != (input.size() % charactersPerByte))
        || (outputSize != (input.size() / charactersPerByte)))
    {
        return false;
    }
    for (std::size_t index = 0; index < outputSize; ++index)
    {
        const int high = nibble(input[index * charactersPerByte]);
        const int low = nibble(input[index * charactersPerByte + 1U]);
        if (0 > high || 0 > low)
        {
            return false;
        }
        output[index] = static_cast<std::uint8_t>((high << 4) | low);
    }
    return true;
}
