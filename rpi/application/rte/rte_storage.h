#ifndef PSI_RTE_STORAGE_H
#define PSI_RTE_STORAGE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>

// ECU-specific RTE-lite port from the update application to Linux storage.
class RteStorage
{
public:
    enum class Size : std::size_t
    {
        SHA256_BYTES = 32
    };

    using Digest = std::array<std::uint8_t,
        static_cast<std::size_t>(Size::SHA256_BYTES)>;

    static RteStorage& getInstance();
    RteStorage(const RteStorage&) = delete;
    RteStorage& operator=(const RteStorage&) = delete;

    bool prepare();
    bool append(const std::uint8_t* bytes, std::size_t size);
    bool finalize();
    bool discard();
    bool removeVerified();
    bool hashIncoming(std::uint32_t expectedSize, Digest& digest,
                      const std::function<bool(std::uint32_t)>& progress) const;
    bool markVerified();

private:
    RteStorage() = default;
};

#endif
