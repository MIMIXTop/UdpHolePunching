#pragma once

#include <memory>
#include <msquic.hpp>

#include "Connection.hpp"
#include "Stream.hpp"

class P2PSession : public P2P::Connection {
public:
    P2PSession(MsQuicRegistration& ms_reg);
    ~P2PSession();
private:
   std::shared_ptr<P2P::Stream> message_stream_;
};
