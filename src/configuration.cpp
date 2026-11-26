#include "configuration.hpp"

#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>
#include <battery/embed.hpp>

Configuration::Configuration() {
    const auto embed = b::embed<"resources/config.json">();
    const auto j = nlohmann::json::parse(embed.str());

    m_client_id = j["client_id"];
    m_client_secret = j["client_secret"];

    std::fstream in(c_default_cfg_path, std::ios::in | std::ios::binary);

    try {
        const auto j2 = nlohmann::json::parse(in);
        m_channel_name = j2["channel_name"];
        m_channel_id = j2["channel_id"];
        m_self_id = j2["self_id"];
        m_refresh_token = j2["cached_token"];
    } catch(...) {
        std::cout << "User cfg empty or invalid." << std::endl;
    }
}

Configuration::~Configuration() {
    save();
}

void Configuration::save() {
    std::fstream out(c_default_cfg_path, std::ios::out | std::ios::binary);
    nlohmann::json j;
    j["channel_name"] = m_channel_name;
    j["channel_id"] = m_channel_id;
    j["self_id"] = m_self_id;
    j["cached_token"] = m_refresh_token;
    out << j;
}

const std::string& Configuration::get_client_id() const {
    return m_client_id;
}

const std::string& Configuration::get_client_secret() const {
    return m_client_secret;
}

const std::string& Configuration::get_channel_name() const {
    return m_channel_name;
}

const std::string& Configuration::get_channel_id() const {
    return m_channel_id;
}

const std::string& Configuration::get_self_id() const {
    return m_self_id;
}

const std::string& Configuration::get_refresh_token() const {
    return m_refresh_token;
}

void Configuration::set_client_id(const std::string& id) {
    m_client_id = id;
}

void Configuration::set_client_secret(const std::string& sec) {
    m_client_secret = sec;
}

void Configuration::set_channel_name(const std::string& id) {
    m_channel_name = id;
}

void Configuration::set_channel_id(const std::string& id) {
    m_channel_id = id;
}

void Configuration::set_self_id(const std::string& id) {
    m_self_id = id;
}

void Configuration::set_refresh_token(const std::string& token) {
    m_refresh_token = token;
}