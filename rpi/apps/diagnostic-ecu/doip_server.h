#ifndef PSI_DOIP_SERVER_H
#define PSI_DOIP_SERVER_H

#include <cstddef>

class DoipServer
{
public:
    static DoipServer& getInstance();
    DoipServer(const DoipServer&) = delete;
    DoipServer& operator=(const DoipServer&) = delete;
    int run(std::size_t maxConnections = 0);

private:
    DoipServer() = default;
};

#endif // PSI_DOIP_SERVER_H
