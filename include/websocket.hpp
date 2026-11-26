#pragma once

#include "configuration.hpp"
#include "token_manager.hpp"

#include <httplib.h>

#include <memory>

class WebSocket {
public:
    WebSocket(TokenManager& mng, const Configuration& cfg);

    bool connect();
    bool reconnect(const std::string& redir);

    httplib::ws::ReadResult read(std::string& str);
    bool subscribe(const std::string& session_id);

    bool is_open() const;
    operator bool() const;
private:
    TokenManager& m_mng;
    const Configuration& m_cfg;
    std::unique_ptr<httplib::ws::WebSocketClient> m_ws;
    bool m_good = false;
};