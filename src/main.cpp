

#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "configuration.hpp"
#include "token_manager.hpp"
#include "websocket.hpp"
#include "display.hpp"

#include <nlohmann/json.hpp>

#include <atomic>
#include <thread>
#include <memory>

void ws_async(Display& disp);

struct Core {
    std::unique_ptr<Display> disp;
    std::thread ws_thread;
};

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    Core* core = new Core();
    *appstate = (void*)core;

    core->disp = std::make_unique<Display>();
    core->ws_thread = std::thread([core]{ ws_async(*core->disp); });
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    Core& core = *((Core*)appstate);
    return core.disp->handle(*event);
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    Core& core = *((Core*)appstate);
    return core.disp->draw();
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    (void)result;
    Core& core = *((Core*)appstate);
    core.disp->destroy(); // be sure
    core.ws_thread.join();

    delete ((Core*)appstate);
}


void ws_async(Display& disp) {

    while(!disp.is_closed()) {
        disp.push(Display::message{"SYSTEM", "Starting...", "#0dff00"});

        Configuration conf;
        
        if (conf.read_conf().client_id.empty() || conf.read_conf().client_secret.empty()) {
            SDL_Log("Captured: Client ID and/or secret are empty. Requesting user.");
            conf.write_conf().reset();

            while(conf.read_conf().client_id.empty()) {
                conf.write_conf().client_id = disp.put_request("Give an Twitch App Client ID:");
                if (disp.is_closed()) return;
            }
            while(conf.read_conf().client_secret.empty()) {
                conf.write_conf().client_secret = disp.put_request("Give an Twitch App Client Secret:");
                if (disp.is_closed()) return;
            }
        }

        disp.push(Display::message{"SYSTEM", "Config ready. Loading...", "#0dff00"});

        /* for now */
        //conf.write_cache().channel_name.clear();
        //conf.write_cache().channel_id.clear();
        //conf.write_cache().access_token.clear();
        
        while(conf.read_cache().channel_name.empty()) {
            conf.write_cache().channel_name = disp.put_request("What channel to connect?");
            if (disp.is_closed()) return;
        }

        disp.push(Display::message{"SYSTEM", "Authenticating and fetching...", "#0dff00"});

        TokenManager token_manager(conf);

        if (!token_manager.connect()) {
            disp.push(Display::message{"SYSTEM", "Token manager failed!", "#ff0000"});
            std::this_thread::sleep_for(std::chrono::seconds(3));

            std::string in = disp.put_request("Do you want to reset the config? [y/N]:");

            if (in[0] == 'Y' || in[0] == 'y') {
                disp.push(Display::message{"SYSTEM", "Resetting config!", "#ffff00"});
                conf.write_conf().reset();
                conf.write_cache().reset();
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            disp.push(Display::message{"SYSTEM", "Trying again...", "#0dff00"});
            continue;
        }

        if (!token_manager.fetch_users()) {
            disp.push(Display::message{"SYSTEM", "IDs were not correctly loaded.", "#ff0000"});
            std::this_thread::sleep_for(std::chrono::seconds(3));

            std::string in = disp.put_request("Do you want to reset the config? [y/N]:");

            if (in[0] == 'Y' || in[0] == 'y') {
                disp.push(Display::message{"SYSTEM", "Resetting config!", "#ffff00"});
                conf.write_conf().reset();
                conf.write_cache().reset();
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            disp.push(Display::message{"SYSTEM", "Trying again...", "#0dff00"});
            continue;
        }

        conf.save();
        disp.push(Display::message{"SYSTEM", "Connecting to WebSocket...", "#0dff00"});

        WebSocket ws(conf);

        if (!ws.connect()) {
            disp.push(Display::message{"SYSTEM", "WebSocket could not connect properly.", "#ff0000"});
            std::this_thread::sleep_for(std::chrono::seconds(3));

            std::string in = disp.put_request("Do you want to reset the config? [y/N]:");

            if (in[0] == 'Y' || in[0] == 'y') {
                disp.push(Display::message{"SYSTEM", "Resetting config!", "#ffff00"});
                conf.write_conf().reset();
                conf.write_cache().reset();
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            disp.push(Display::message{"SYSTEM", "Trying again...", "#0dff00"});
            continue;
        }

        conf.save();
        std::string buf;

        for (bool keep_ws = true; keep_ws && ws.is_open() && !disp.is_closed();) {
            if (disp.has_touched()) {
                std::string new_channel = disp.put_request("Connect to something else?", conf.read_cache().channel_name);
                if (disp.is_closed()) return;
                if (new_channel.size() > 0 && new_channel != conf.read_cache().channel_name) {
                    conf.write_cache().channel_name = new_channel;
                    disp.push(Display::message{"SYSTEM", "Restarting session...", "#0dff00"});
                    keep_ws = false;
                    continue;
                }
            }

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
                    //std::cout << "EventSub session: " << session_id << '\n';
                    if (ws.subscribe(session_id)) {
                        disp.push(Display::message{"SYSTEM",
                            "Connected.","#0dff00"});
                    } else {
                        disp.push(Display::message{"SYSTEM",
                            "Failed to \"subscribe\" to the events.", "#ff0000"});

                        std::this_thread::sleep_for(std::chrono::seconds(3));
                        keep_ws = false;
                    }
                    continue;
                }

                // Chat messages.
                if (message_type == "notification" && metadata.value("subscription_type", "") == "channel.chat.message") {
                    const auto& event = data["payload"]["event"];

                    const std::string username = event["chatter_user_name"];
                    const std::string text = event["message"]["text"];
                    const std::string color = event["color"];

                    //std::cout << "[" << color << "] @" << username << ": " << text << std::endl;

                    disp.push(Display::message{username, text, color});

                    //std::cout << event.dump(2) << std::endl;
                    continue;
                }

                if (message_type == "session_reconnect") {
                    const std::string new_url = data["payload"]["session"]["reconnect_url"];
                    std::cout << "Reconnect: " << new_url;

                    if (!ws.reconnect(new_url)) {
                        disp.push(Display::message{"SYSTEM",
                            "Chat disconnected on session reconnect. Restarting...","#ffff00"});
                        
                        keep_ws = false;
                    }
                    continue;
                }
            }
            catch (const std::exception& e) {
                disp.push(Display::message{"SYSTEM",
                    std::string("Event error: ") + e.what(),"#ffff00"});

                std::cerr << "Event error: " << e.what() << '\n';
            }
        }        
    }
}
