#pragma once

#include "configuration.hpp"

#include <httplib.h>

#include <memory>

constexpr char c_twitch_ws[] = "wss://eventsub.wss.twitch.tv/ws";

class WebSocket {
public:
    WebSocket(const Configuration& cfg);

    bool connect();
    bool reconnect(const std::string& redir);

    httplib::ws::ReadResult read(std::string& str);
    bool subscribe(const std::string& session_id);

    bool is_open() const;
    operator bool() const;
private:
    const Configuration& m_conf;
    std::unique_ptr<httplib::ws::WebSocketClient> m_ws;
    bool m_good = false;
};