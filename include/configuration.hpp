#pragma once

#include <nlohmann/json.hpp>

#include <string>

constexpr char c_default_cfg_path[] = "config.json";
constexpr char c_default_cache_path[] = "cache.json";

class Configuration {
public:
    struct conf {
        std::string
            client_id,
            client_secret;
        
        void load(const nlohmann::json& j);
        nlohmann::json store() const;
        void reset();
    };

    struct cache {
        std::string
            channel_name,
            channel_id,
            self_id,
            refresh_token,
            access_token;

        void load(const nlohmann::json& j);
        nlohmann::json store() const;
        void reset();
    };

    Configuration();
    ~Configuration();

    void save();

    const conf& read_conf() const;
    const cache& read_cache() const;

    conf& write_conf();
    cache& write_cache();
private:
    conf m_conf;
    cache m_cache;
    bool m_save_conf = false;
    bool m_save_cache = false;
};