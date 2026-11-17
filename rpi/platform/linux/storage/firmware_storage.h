#ifndef PSI_FIRMWARE_STORAGE_H
#define PSI_FIRMWARE_STORAGE_H

#include <cstddef>
#include <cstdint>
#include <array>
#include <functional>
#include <string>

// Owns the update directory and the currently open staging file.
class FirmwareStorage
{
private:
    enum class Limit : std::size_t
    {
        SYNC_INTERVAL_BYTES = 4'194'304,
        HASH_CHUNK_BYTES = 65'536
    };

    static constexpr char INCOMING_NAME[] = "incoming.img";
    static constexpr char VERIFIED_NAME[] = "verified.img";
    static constexpr char PROBE_NAME[] = ".write-probe";

    FirmwareStorage() = default;
    bool removeFile(const char* name) const;
    void closeIncoming();

    int pDirectory = -1;
    int pIncoming = -1;
    std::size_t pUnsyncedBytes = 0;

public:
    enum class Size : std::size_t
    {
        SHA256_BYTES = 32
    };
    using Digest = std::array<std::uint8_t, static_cast<std::size_t>(Size::SHA256_BYTES)>;

    static constexpr char DEFAULT_DIRECTORY_FROM_HOME[] = "/UpdMgr/image";
    static FirmwareStorage& getInstance();
    FirmwareStorage(const FirmwareStorage&) = delete;
    FirmwareStorage& operator=(const FirmwareStorage&) = delete;
    ~FirmwareStorage();

    bool initialize(const std::string& directory);
    bool prepare();
    bool append(const std::uint8_t* bytes, std::size_t size);
    bool finalize();
    bool discard();
    bool removeVerified();
    bool hashIncoming(std::uint32_t expectedSize, Digest& digest,
                      const std::function<bool(std::uint32_t)>& progress) const;
    bool markVerified();
};

#endif
