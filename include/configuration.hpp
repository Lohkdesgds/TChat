#pragma once

#include <string>

constexpr char c_default_cfg_path[] = "config.json";

class Configuration {
public:
    Configuration();
    ~Configuration();

    void save();

    const std::string& get_client_id() const;
    const std::string& get_client_secret() const;
    const std::string& get_channel_name() const;
    const std::string& get_channel_id() const;
    const std::string& get_self_id() const;
    const std::string& get_refresh_token() const;

    void set_client_id(const std::string& id);
    void set_client_secret(const std::string& sec);
    void set_channel_name(const std::string& id);
    void set_channel_id(const std::string& id);
    void set_self_id(const std::string& id);
    void set_refresh_token(const std::string& token);
private:
    std::string m_client_id;
    std::string m_client_secret;
    std::string m_channel_name;
    std::string m_channel_id;
    std::string m_self_id;
    std::string m_refresh_token;
};