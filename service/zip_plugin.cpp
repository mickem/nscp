/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "zip_plugin.h"

#include "core_api.h"
#include "NSCAPI.h"

#include <file_helpers.hpp>
#include <zip/miniz.hpp>
#include <str/xtos.hpp>

#include <json_spirit.h>

/**
 * Default c-tor
 * Initializes the plug in name but does not load the actual plug in.<br>
 * To load the plug in use function load() that loads an initializes the plug in.
 *
 * @param file The file (DLL) to load as a NSC plug in.
 */
nsclient::core::zip_plugin::zip_plugin(const unsigned int id, const boost::filesystem::path file, std::string alias, nsclient::logging::logger_instance logger)
	: plugin_interface(id, alias)
	, file_(file)
	, logger_(logger)
{
	read_metadata();
}
/**
 * Default d-tor
 */
nsclient::core::zip_plugin::~zip_plugin() {
}
/**
 * Returns the name of the plug in.
 *
 * @return Name of the plug in.
 *
 * @throws NSPluginException if the module is not loaded.
 */
std::string nsclient::core::zip_plugin::getName() {
	return name_;
}
std::string nsclient::core::zip_plugin::getDescription() {
	return description_;
}

void nsclient::core::zip_plugin::read_metadata() {
	mz_zip_archive zip_archive;
	memset(&zip_archive, 0, sizeof(zip_archive));
	mz_bool status = mz_zip_reader_init_file(&zip_archive, file_.string().c_str(), 0);
	if (!status) {
		throw plugin_exception(get_alias_or_name(), "Failed to read:" + file_.string());
	}

	for (int i = 0; i < (int)mz_zip_reader_get_num_files(&zip_archive); i++) {
		LOG_ERROR_CORE("...");
		mz_zip_archive_file_stat file_stat;
		if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
			mz_zip_reader_end(&zip_archive);
			throw plugin_exception(get_alias_or_name(), "Failed to read:" + file_.string());
		}

		if (std::string(file_stat.m_filename) == "module.json") {
			size_t uncomp_size;
			void *p = mz_zip_reader_extract_file_to_heap(&zip_archive, file_stat.m_filename, &uncomp_size, 0);
			if (!p) {
				mz_zip_reader_end(&zip_archive);
				throw plugin_exception(get_alias_or_name(), "Failed to read:" + file_.string());
			}
			read_metadata(std::string((const char *)p));
			return;
		}
	}
	throw plugin_exception(get_alias_or_name(), "Failed to find module.json in " + file_.string());
}
void nsclient::core::zip_plugin::read_metadata(std::string data) {
	json_spirit::Value root;
	json_spirit::read_or_throw(data, root);
	name_ = root.getString("name");
	description_ = root.getString("description");
}

bool nsclient::core::zip_plugin::load_plugin(NSCAPI::moduleLoadMode mode) {
	return true;
}

NSCAPI::nagiosReturn nsclient::core::zip_plugin::handleCommand(const std::string request, std::string &reply) {
	throw plugin_exception(get_alias_or_name(), "cannot handle commands");
}

NSCAPI::nagiosReturn nsclient::core::zip_plugin::handle_schedule(const std::string &request) {
	throw plugin_exception(get_alias_or_name(), "cannot handle schedule");
}

NSCAPI::nagiosReturn nsclient::core::zip_plugin::handleNotification(const char *channel, std::string &request, std::string &reply) {
	throw plugin_exception(get_alias_or_name(), "cannot handle commands");
}

NSCAPI::nagiosReturn nsclient::core::zip_plugin::on_event(const std::string &request) {
	throw plugin_exception(get_alias_or_name(), "cannot handle commands");
}

NSCAPI::nagiosReturn nsclient::core::zip_plugin::fetchMetrics(std::string &request) {
	throw plugin_exception(get_alias_or_name(), "cannot handle commands");
}

NSCAPI::nagiosReturn nsclient::core::zip_plugin::submitMetrics(const std::string &request) {
	throw plugin_exception(get_alias_or_name(), "cannot handle commands");
}

void nsclient::core::zip_plugin::handleMessage(const char * data, unsigned int len) {
	throw plugin_exception(get_alias_or_name(), "cannot handle commands");
}

/**
 * Unload the plug in
 * @throws NSPluginException if the module is not loaded and/or cannot be unloaded (plug in remains loaded if so).
 */
void nsclient::core::zip_plugin::unload_plugin() {
}

int nsclient::core::zip_plugin::commandLineExec(bool targeted, std::string &request, std::string &reply) {
	throw plugin_exception(get_alias_or_name(), "cannot handle commands");
}
bool nsclient::core::zip_plugin::is_duplicate(boost::filesystem::path file, std::string alias) {
	return false;
}

std::string nsclient::core::zip_plugin::get_version() {
	return "1.0.0";
}

bool nsclient::core::zip_plugin::route_message(const char *channel, const char* buffer, unsigned int buffer_len, char **new_channel_buffer, char **new_buffer, unsigned int *new_buffer_len) {
	throw plugin_exception(get_alias_or_name(), "cannot handle commands");

}

std::string nsclient::core::zip_plugin::getModule() {
	std::string tmp = file_helpers::meta::get_filename(file_);
	if (boost::algorithm::ends_with(tmp, ".zip")) {
		return tmp.substr(0, tmp.length() - 4);
	}
	return tmp;
}

