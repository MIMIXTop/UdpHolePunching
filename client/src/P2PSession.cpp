//
// Created by mimixtop on 19.06.2026.
//

#include "P2PSession.hpp"
#include <msquic.h>

unsigned int P2PSession::ConnectionCallback(MsQuicConnection *connection, void *context, QUIC_CONNECTION_EVENT *event) {

    switch (event->Type) {
        case QUIC_CONNECTION_EVENT_CONNECTED:
            MsQuicApi->ConnectionSendResumptionTicket(connection)
            break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
            break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
            break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
            break;
        case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
            break;
        case QUIC_CONNECTION_EVENT_RESUMED:
            break;
        default:
            break;
    }

}
