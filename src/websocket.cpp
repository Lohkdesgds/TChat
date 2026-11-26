#include "websocket.hpp"

#include <nlohmann/json.hpp>

extern bool check_response(const httplib::Result& response, int expected_status, const std::string& operation);

WebSocket::WebSocket(TokenManager& mng, const Configuration& cfg) :
    m_mng(mng),
    m_cfg(cfg),
    m_ws(std::make_unique<httplib::ws::WebSocketClient>("wss://eventsub.wss.twitch.tv/ws"))
{}

bool WebSocket::connect() {
    if (const auto res = m_ws->connect(); !res) {
        std::cout << "ERROR: Cannot connect to WebSocket: " << httplib::to_string(res.error()) << std::endl;
        m_good = false;
        return false;
    }
    m_good = true;
    return m_good;
}

bool WebSocket::reconnect(const std::string& redir) {
    m_ws = std::make_unique<httplib::ws::WebSocketClient>(redir);
    return connect();
}

httplib::ws::ReadResult WebSocket::read(std::string& str) {
    return m_good ? m_ws->read(str) : httplib::ws::Fail;
}

bool WebSocket::subscribe(const std::string& session_id) {
    httplib::Client client("https://api.twitch.tv");

    httplib::Headers headers = {
        {"Client-ID", m_cfg.get_client_id()},
        {"Authorization", "Bearer " + m_mng.get_token()},
        {"Content-Type", "application/json"}
    };

    nlohmann::json subscription = {
        {"type", "channel.chat.message"},
        {"version", "1"},
        {"condition", {
            {"broadcaster_user_id", m_cfg.get_channel_id()},
            {"user_id", m_cfg.get_self_id()}
        }},
        {"transport", {
            {"method", "websocket"},
            {"session_id", session_id}
        }}
    };

    auto response = client.Post(
        "/helix/eventsub/subscriptions",
        headers,
        subscription.dump(),
        "application/json");

    if (!check_response(response, 202, "Subscribe to Twitch chat")) {
        std::cout << "Failed to properly subscribe to chat events" << std::endl;
        m_good = false;
        return false;
    }

    return true;
}


bool WebSocket::is_open() const {
    return m_good && m_ws->is_open();
}

WebSocket::operator bool() const {
    return m_good && m_ws->is_open();
}