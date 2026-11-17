#include "doip_config.h"

DoipConfig& DoipConfig::getInstance()
{
    static DoipConfig instance;
    return instance;
}

