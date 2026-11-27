#include "configuration.hpp"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <battery/embed.hpp>

#include <fstream>
#include <iostream>

static std::string get_app_files_path(std::string for_file);

void Configuration::conf::load(const nlohmann::json& j) {
    if (j.contains("client_id")) client_id = j["client_id"];
    if (j.contains("client_secret")) client_secret = j["client_secret"];
}

nlohmann::json Configuration::conf::store() const {
    nlohmann::json j;
    j["client_id"] = client_id;
    j["client_secret"] = client_secret;
    return j;
}

void Configuration::conf::reset() {
    *this = {};
}

void Configuration::cache::load(const nlohmann::json& j) {
    if (j.contains("channel_name")) channel_name = j["channel_name"];
    if (j.contains("channel_id")) channel_id = j["channel_id"];
    if (j.contains("self_id")) self_id = j["self_id"];
    if (j.contains("refresh_token")) refresh_token = j["refresh_token"];
    if (j.contains("access_token")) access_token = j["access_token"];
}

nlohmann::json Configuration::cache::store() const {
    nlohmann::json j;
    j["channel_name"] = channel_name;
    j["channel_id"] = channel_id;
    j["self_id"] = self_id;
    j["refresh_token"] = refresh_token;
    j["access_token"] = access_token;
    return j;
}

void Configuration::cache::reset() {
    *this = {};
}


Configuration::Configuration() {
    const auto conf_path = get_app_files_path(c_default_cfg_path);
    const auto cache_path = get_app_files_path(c_default_cache_path);

    SDL_Log("Loading config from %s", conf_path.c_str());
    SDL_Log("Loading cache from %s", cache_path.c_str());

    std::fstream conf_in(conf_path, std::ios::in | std::ios::binary);
    std::fstream cache_in(cache_path, std::ios::in | std::ios::binary);

    auto conf_j  = nlohmann::json::parse(conf_in,  nullptr, false);
    auto cache_j = nlohmann::json::parse(cache_in, nullptr, false);

    if (!conf_j.is_discarded()) m_conf.load(conf_j);
    if (!cache_j.is_discarded()) m_cache.load(cache_j);
}

Configuration::~Configuration() {
    save();
}

void Configuration::save() {
    if (m_save_conf) {
        const auto conf_path = get_app_files_path(c_default_cfg_path);
        std::fstream conf_out(conf_path, std::ios::out | std::ios::binary);
        conf_out << m_conf.store();
        m_save_conf = false;
    }
    if (m_save_cache) {
        const auto cache_path = get_app_files_path(c_default_cache_path);
        std::fstream cache_out(cache_path, std::ios::out | std::ios::binary);    
        cache_out << m_cache.store();
    }
}

const Configuration::conf& Configuration::read_conf() const {
    return m_conf;
}

const Configuration::cache& Configuration::read_cache() const {
    return m_cache;
}

Configuration::conf& Configuration::write_conf() {
    m_save_conf = true;
    return m_conf;
}

Configuration::cache& Configuration::write_cache() {
    m_save_cache = true;
    return m_cache;
}


std::string get_app_files_path(std::string for_file) {
    char* pref_path = SDL_GetPrefPath("LohksStudiosWorks", "TChatApp");
    if (!pref_path) {
        SDL_Log("Failed to get preference path: %s", SDL_GetError());
        return for_file;
    }

    std::string path = std::string(pref_path) + for_file;
    SDL_free(pref_path);
    return path;
}