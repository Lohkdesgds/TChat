#pragma once

#include "configuration.hpp"

#include <string>
#include <thread>

class TokenManager {
public:
    TokenManager(Configuration& conf);

    bool connect();

    bool good() const;
    operator bool() const;

    bool fetch_users();

    const std::string& get_token() const;
private:
    std::string fetch_user(const bool self);
    bool authenticate();
    bool try_refresh();

    Configuration& m_conf;

    std::string m_access_token;
    std::string m_refresh_token;
    int m_expires_in;

    bool m_good = false;
};