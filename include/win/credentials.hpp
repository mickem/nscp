#pragma once

#include <boost/optional.hpp>
#include <string>

void save_credential(const std::string &alias, const std::string &password);
boost::optional<std::string> read_credential(const std::string &alias);