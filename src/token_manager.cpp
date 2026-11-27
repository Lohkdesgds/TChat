#include "token_manager.hpp"

#include <random>

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <SDL3/SDL.h>

constexpr const char* c_redirect_uri = "http://localhost:3000";
constexpr const char* c_redirect_host = "127.0.0.1";
constexpr int c_redirect_port = 3000;

static std::string generate_state();
bool check_response(const httplib::Result& response, int expected_status, const std::string& operation);


TokenManager::TokenManager(Configuration& conf) :
    m_conf(conf) 
{}

bool TokenManager::connect() {
    if (!(m_good = try_refresh())) {
        std::cout << "Cached token could not be used. Trying fresh..." << std::endl;
        m_good = authenticate();
    }
    return m_good;
}

bool TokenManager::good() const {
    return m_good;
}

TokenManager::operator bool() const {
    return m_good;
}

bool TokenManager::fetch_users() {
    if (!m_good) {
        std::cout << "Not yet initialized. Do connect() first." << std::endl;
        return false;
    }

    if (const auto res = fetch_user(false); !res.empty()) {
        std::cout << "Got target channel ID: " << res << std::endl;
        m_conf.write_cache().channel_id = res;
    }
    else {
        std::cout << "Failed to fetch target channel ID." << std::endl;
        m_conf.write_cache().channel_id.clear();
        m_conf.write_cache().self_id.clear();
        return false;
    }

    if (const auto res = fetch_user(true); !res.empty()) {
        std::cout << "Got self ID: " << res << std::endl;
        m_conf.write_cache().self_id = res;
    }
    else {
        std::cout << "Failed to fetch self ID." << std::endl;
        m_conf.write_cache().channel_id.clear();
        m_conf.write_cache().self_id.clear();
        return false;
    }

    return true;
}

std::string TokenManager::fetch_user(const bool self) {
    const std::string username = self ? "" : m_conf.read_cache().channel_name;

    httplib::Client client("https://api.twitch.tv");

    httplib::Headers headers = {
        {"Client-ID", m_conf.read_conf().client_id},
        {"Authorization", "Bearer " + m_conf.read_cache().access_token}
    };

    const std::string endpoint =
        username.empty()
            ? "/helix/users"
            : "/helix/users?login=" + username;

    auto response = client.Get(endpoint, headers);

    if (!check_response(response, 200, "Get Twitch user")) {
        std::cout << "Trying to save auth..." << std::endl;
        if (try_refresh()) {
            std::cout << "Refresh good. Retrying..." << std::endl;
            return fetch_user(self);
        }
        std::cout << "Token expired or something else is going on. Please relaunch the app and redo the auth." << std::endl;
        m_good = false;
        return {};
    }

    const auto data = nlohmann::json::parse(response->body);

    if (data.at("data").empty()) {
        std::cout << "Twitch user not found: " << username << std::endl;
        return {};
    }

    if (const auto id = data["data"][0]["id"].get<std::string>(); !id.empty()) {
        return id;
    }

    std::cout << "Failed to fetch user " << username << std::endl;
    return {};
}

bool TokenManager::authenticate() {
    const std::string state = generate_state();

    std::string authorization_code;
    std::mutex mutex;
    std::condition_variable cv;
    bool listener_failed = false;

    httplib::Server server;

    server.Get("/", [&](const httplib::Request& req,
                        httplib::Response& res) {
        if (!req.has_param("code") ||
            !req.has_param("state")) {
            res.status = 400;
            res.set_content(
                "Missing authorization code or state",
                "text/plain");
            return;
        }

        if (req.get_param_value("state") != state) {
            res.status = 400;
            res.set_content(
                "Invalid OAuth state",
                "text/plain");
            return;
        }

        {
            std::lock_guard lock(mutex);
            authorization_code = req.get_param_value("code");
        }

        res.set_content(
            "Twitch authentication successful! "
            "You can close this window.",
            "text/plain");

        cv.notify_one();
    });

    std::thread server_thread([&]() {
        if (!server.listen(c_redirect_host, c_redirect_port)) {
            {
                std::lock_guard lock(mutex);
                listener_failed = true;
            }
            cv.notify_one();
        }
    });

    // Give the local HTTP server time to start.
    std::this_thread::sleep_for(
        std::chrono::milliseconds(300));

    const std::string authorization_url =
        std::string("https://id.twitch.tv/oauth2/authorize") +
        "?client_id=" + m_conf.read_conf().client_id +
        "&redirect_uri=http%3A%2F%2Flocalhost%3A3000" +
        "&response_type=code" +
        "&scope=user%3Aread%3Achat" +
        "&state=" + state;

    std::cout << "Opening this on the browser for auth:\n\n"
              << authorization_url << std::endl;

    SDL_OpenURL(authorization_url.c_str());

    {
        std::unique_lock lock(mutex);

        cv.wait(lock, [&]() {
            return !authorization_code.empty() || listener_failed;
        });
    }

    std::cout << "Working..." << std::endl;
    server.stop();
    server_thread.join();

    if (listener_failed) {
        std::cout << "Could not start OAuth callback server. Is port 3000 already in use?" << std::endl;
        return false;
    }

    // Exchange authorization code for access token.

    httplib::Client client("https://id.twitch.tv");

    httplib::Params params = {
        {"client_id", m_conf.read_conf().client_id },
        {"client_secret", m_conf.read_conf().client_secret },
        {"code", authorization_code},
        {"grant_type", "authorization_code"},
        {"redirect_uri", c_redirect_uri}
    };

    auto response = client.Post("/oauth2/token", params);

    if (!check_response(response, 200, "Token exchange"))
        return false;

    const auto data = nlohmann::json::parse(response->body);

    std::cout << "Authentication successful!" << std::endl;

    m_conf.write_cache().access_token = data.at("access_token").get<std::string>();
    m_conf.write_cache().refresh_token = data.at("refresh_token").get<std::string>();
    //m_expires_in = data.at("expires_in").get<int>();

    return true;
}

bool TokenManager::try_refresh() {
    httplib::Client client("https://id.twitch.tv");

    httplib::Params params = {
        {"client_id", m_conf.read_conf().client_id },
        {"client_secret", m_conf.read_conf().client_secret },
        {"grant_type", "refresh_token"},
        {"refresh_token", m_conf.read_cache().refresh_token }
    };

    auto response = client.Post("/oauth2/token", params);

    if (!response)
        return false;

    if (response->status != 200)
        return false;

    const auto data = nlohmann::json::parse(response->body);

    m_conf.write_cache().access_token = data.at("access_token").get<std::string>();
    m_conf.write_cache().refresh_token = data.at("refresh_token").get<std::string>();
    //m_expires_in = data.at("expires_in").get<int>();

    return true;
}

std::string generate_state() {
    static constexpr char chars[] =
        "0123456789abcdef";

    std::random_device rd;
    std::string result;

    for (int i = 0; i < 32; ++i)
        result += chars[rd() % 16];

    return result;
}

bool check_response(const httplib::Result& response, int expected_status, const std::string& operation) {
    if (!response) {
        std::cout << "Error: " << operation << ": HTTP request failed" << std::endl;
        return false;
    }

    if (response->status != expected_status) {
        std::cout << "Error: "
            << operation << ": HTTP "
            << std::to_string(response->status)
            << "\n" << response->body << std::endl;
        return false;
    }

    return true;
}