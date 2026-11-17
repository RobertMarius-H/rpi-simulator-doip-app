#include "dcm_session.h"

DcmSession& DcmSession::getInstance()
{
    static DcmSession instance;
    return instance;
}

void DcmSession::resetConnection()
{
    pCurrentSession = static_cast<std::uint8_t>(SessionType::DEFAULT);
    pSecuritySession.reset();
}

void DcmSession::setSession(std::uint8_t session)
{
    pCurrentSession = session;
    pSecuritySession.reset();
}
