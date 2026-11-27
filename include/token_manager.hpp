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
private:
    std::string fetch_user(const bool self);
    bool authenticate();
    bool try_refresh();

    Configuration& m_conf;

    bool m_good = false;
};