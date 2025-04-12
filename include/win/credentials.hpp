#pragma once

#include <string>
#include <boost/optional.hpp>

void save_credential(const std::string &alias, const std::string &password);
boost::optional<std::string> read_credential(const std::string &alias);