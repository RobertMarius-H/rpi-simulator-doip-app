#ifndef PSI_TRANSPORT_READ_STATUS_H
#define PSI_TRANSPORT_READ_STATUS_H

enum class TransportReadStatus
{
    COMPLETE,
    CLOSED,
    TIMED_OUT,
    ERROR
};

#endif
