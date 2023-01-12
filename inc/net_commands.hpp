#ifndef NET_COMMANDS_HPP
#define NET_COMMANDS_HPP

#include <functional>
#include <string>

#include "json.hpp"
nlohmann::json cmd_execute(std::string command, nlohmann::json par);

#endif  // _NET_COMMANDS_HPP
