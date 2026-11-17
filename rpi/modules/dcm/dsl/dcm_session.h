#ifndef PSI_DCM_SESSION_H
#define PSI_DCM_SESSION_H

#include <cstdint>

#include "uds_security.h"

// DCM session context for the current single tester connection.
class DcmSession
{
public:
    enum class SessionType : std::uint8_t
    {
        DEFAULT = 0x01,
        PROGRAMMING = 0x02
    };

    static DcmSession& getInstance();
    DcmSession(const DcmSession&) = delete;
    DcmSession& operator=(const DcmSession&) = delete;

    void resetConnection();
    void setSession(std::uint8_t session);
    std::uint8_t currentSession() const { return pCurrentSession; }
    UdsSecurity::Session& securitySession() { return pSecuritySession; }
    const UdsSecurity::Session& securitySession() const { return pSecuritySession; }

private:
    DcmSession() = default;
    std::uint8_t pCurrentSession = static_cast<std::uint8_t>(SessionType::DEFAULT);
    UdsSecurity::Session pSecuritySession;
};

#endif
