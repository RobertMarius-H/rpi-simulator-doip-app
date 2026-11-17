#ifndef PSI_PDU_ROUTER_H
#define PSI_PDU_ROUTER_H

#include <cstddef>
#include <cstdint>
#include <vector>

// PduR-like single-route boundary between DoIP diagnostic PDUs and Dcm.
class PduRouter
{
public:
    enum class RouteStatus
    {
        ACCEPTED,
        INVALID_SOURCE,
        INVALID_TARGET
    };

    static PduRouter& getInstance();
    PduRouter(const PduRouter&) = delete;
    PduRouter& operator=(const PduRouter&) = delete;

    RouteStatus validate(std::uint16_t source, std::uint16_t target) const;
    std::vector<std::uint8_t> forward(const std::vector<std::uint8_t>& payload);
    bool responsePending() const;
    std::vector<std::uint8_t> pollResponse();
    std::vector<std::uint8_t> pendingNotification() const;
    void resetConnection();

private:
    PduRouter() = default;
};

#endif
