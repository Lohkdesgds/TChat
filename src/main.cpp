#include "configuration.hpp"
#include "token_manager.hpp"
#include "websocket.hpp"

#include <nlohmann/json.hpp>

std::string get_str() {
    std::string str;
    std::getline(std::cin, str);
    if (str.size() > 0 && str.back() == '\n') str.pop_back();
    return str;
}

int main()
{
    Configuration conf;
    std::string buf;

    do {
        std::cout << "What channel name to watch? ";

        if (!conf.get_channel_name().empty())
            std::cout << "(last one: '" << conf.get_channel_name() << "') ";

        if (!(buf = get_str()).empty()) {
            if (buf != conf.get_channel_name())
                conf.set_channel_id("");
            conf.set_channel_name(buf);
        }
    } while (conf.get_channel_name().empty());

    std::cout << "Authentication started..." << std::endl;

    TokenManager token_manager(conf);

    if (!token_manager.connect()) {
        std::cout << "Token manager failed. Retry later?" << std::endl;
        return 1;
    }

    if (!token_manager.fetch_users()) {
        std::cout << "IDs were not correctly loaded. Try cleaning up the config file?" << std::endl;
        return 1;
    }

    std::cout << "Everything good. Connecting to Twitch..." << std::endl;

    WebSocket ws(token_manager, conf);
    if (!ws.connect()) return 3;
    
    while(1) {
        while (ws.is_open()) {
            switch(ws.read(buf)) {
            case httplib::ws::Text:
                break;
            default:
                continue;
            }

            const auto data = nlohmann::json::parse(buf, nullptr, false);
            if (data.is_discarded()) continue;

            try {
                const auto& metadata = data.at("metadata");
                const std::string message_type = metadata.value("message_type", "");

                if (message_type == "session_welcome") {
                    const std::string session_id = data["payload"]["session"]["id"];
                    std::cout << "EventSub session: " << session_id << '\n';
                    ws.subscribe(session_id);
                    conf.save();
                    continue;
                }

                // Chat messages.
                if (message_type == "notification" && metadata.value("subscription_type", "") == "channel.chat.message") {
                    const auto& event = data["payload"]["event"];

                    const std::string username = event["chatter_user_name"];
                    const std::string text = event["message"]["text"];
                    const std::string color = event["color"];

                    std::cout << "[" << color << "] @" << username << ": " << text << std::endl;

                    //std::cout << event.dump(2) << std::endl;
                    continue;
                }

                if (message_type == "session_reconnect") {
                    const std::string new_url = data["payload"]["session"]["reconnect_url"];
                    std::cout << "Reconnect: " << new_url;
                    if (!ws.reconnect(new_url)) {
                        std::cout << "Failed to reconnect!" << std::endl;
                        return 4;
                    }
                    continue;
                }
            }
            catch (const std::exception& e) {
                std::cerr << "Event error: " << e.what() << '\n';
            }
        }

    }

    return 0;
}
