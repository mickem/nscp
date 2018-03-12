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

#include <nscapi/nscapi_core_helper.hpp>
#include <file_helpers.hpp>
#include <zip/miniz.hpp>
#include <str/xtos.hpp>
#include <str/nscp_string.hpp>

#include <json_spirit.h>


struct zip_archive {

	mz_zip_archive handle_;
	zip_archive() {
		memset(&handle_, 0, sizeof(handle_));
	}
	zip_archive(std::string file) {
		memset(&handle_, 0, sizeof(handle_));
		read(file);
	}
	~zip_archive() {
		mz_zip_reader_end(&handle_);
	}

	bool read(std::string file) {
		return mz_zip_reader_init_file(&handle_, file.c_str(), 0);
	}
	unsigned int get_numfiles() {
		return mz_zip_reader_get_num_files(&handle_);
	}
	bool file_stat(unsigned int id, mz_zip_archive_file_stat &file_stat) {
		return mz_zip_reader_file_stat(&handle_, id, &file_stat);
	}
	const char* extract_file_to_heap(const char* filename, std::size_t &size) {
		return reinterpret_cast<char*>(mz_zip_reader_extract_file_to_heap(&handle_, filename, &size, 0));
	}
	bool extract_file_to_file(const char* filename, const char* dst_file) {
		return mz_zip_reader_extract_file_to_file(&handle_, filename, dst_file, 0);
	}

	
};

/**
 * Default c-tor
 * Initializes the plug in name but does not load the actual plug in.<br>
 * To load the plug in use function load() that loads an initializes the plug in.
 *
 * @param file The file (DLL) to load as a NSC plug in.
 */
nsclient::core::zip_plugin::zip_plugin(const unsigned int id, const boost::filesystem::path file, std::string alias, nsclient::core::path_instance paths, nsclient::core::plugin_mgr_instance plugins, nsclient::logging::logger_instance logger)
	: plugin_interface(id, alias)
	, file_(file)
	, paths_(paths)
	, plugins_(plugins)
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


nsclient::core::script_def read_script_def(const json_spirit::Value & s) {
	nsclient::core::script_def def;
	if (s.isString()) {
		def.script = s.getString();
		std::string name = file_helpers::meta::get_filename(boost::filesystem::path(def.script));
		if (boost::algorithm::ends_with(name, ".py")) {
			def.provider = "PythonScript";
			def.alias = name.substr(0, name.length()-3);
			def.command = name;
		} else {
			def.provider = "CheckExternalScripts";
			def.alias = def.script;
			def.command = def.script;
		}
	} else {
		def.provider = s.getString("provider");
		def.script = s.getString("script");
		def.alias = s.getString("alias");
		def.command = s.getString("command");
	}
	return def;
}


void nsclient::core::zip_plugin::read_metadata() {
	zip_archive archive;
	if (!archive.read(file_.string())) {
		throw plugin_exception(get_alias_or_name(), "Failed to read:" + file_.string());
	}

	for (unsigned int i = 0; i < archive.get_numfiles(); i++) {
		mz_zip_archive_file_stat file_stat;
		if (!archive.file_stat(i, file_stat)) {
			throw plugin_exception(get_alias_or_name(), "Failed to read:" + file_.string());
		}

		if (std::string(file_stat.m_filename) == "module.json") {
			std::size_t uncomp_size;
			const char *p = archive.extract_file_to_heap(file_stat.m_filename, uncomp_size);
			if (!p) {
				throw plugin_exception(get_alias_or_name(), "Failed to read:" + file_.string());
			}
			read_metadata(std::string(p));
			return;
		}
	}
	throw plugin_exception(get_alias_or_name(), "Failed to find module.json in " + file_.string());
}
void nsclient::core::zip_plugin::read_metadata(std::string data) {
	try {
		json_spirit::Value root;
		json_spirit::read_or_throw(data, root);
		name_ = root.getString("name");
		description_ = root.getString("description");

		if (root.contains("scripts")) {
			BOOST_FOREACH(const json_spirit::Value &s, root.getArray("scripts")) {
				script_def def = read_script_def(s);
				if (modules_.find(def.provider) == modules_.end()) {
					modules_.insert(def.provider);
				}
				scripts_.push_back(def);
			}
		}
		if (root.contains("modules")) {
			BOOST_FOREACH(const json_spirit::Value &s, root.getArray("modules")) {
				std::string module = s.getString();
				if (modules_.find(module) == modules_.end()) {
					modules_.insert(module);
				}
			}
		}
		if (root.contains("on_start")) {
			BOOST_FOREACH(const json_spirit::Value &s, root.getArray("on_start")) {
				on_start_.push_back(s.getString());
			}
		}
	} catch (const json_spirit::PathError &e) {
			throw plugin_exception(get_alias_or_name(), "Failed to parse module.json " + e.reason + " for " + e.path);
	} catch (const json_spirit::ParseError &e) {
		throw plugin_exception(get_alias_or_name(), "Failed to parse module.json " + e.reason_ + " at line " + str::xtos(e.line_));
	}
}

bool nsclient::core::zip_plugin::load_plugin(NSCAPI::moduleLoadMode mode) {
	boost::filesystem::path scripts_folder = boost::filesystem::path(paths_->expand_path("${scripts}")) / "tmp";
	boost::filesystem::path target_path = scripts_folder / getModule();
	boost::filesystem::create_directory(scripts_folder);
	boost::filesystem::create_directory(target_path);
	BOOST_FOREACH(const std::string &plugin, modules_) {
		plugins_->load_single_plugin(plugin, "", true);
	}
	zip_archive archive(file_.string());

	BOOST_FOREACH(const script_def &script, scripts_) {
		boost::filesystem::path target = target_path / file_helpers::meta::get_filename(boost::filesystem::path(script.script));
		if (!archive.extract_file_to_file(script.script.c_str(), target.string().c_str())) {
			LOG_ERROR_CORE("Failed to add script " + script.script);
			continue;
		}
		std::list<std::string> ret;
		std::vector<std::string> args;
		args.push_back("--script");
		args.push_back(target.string());
		args.push_back("--alias");
		args.push_back(script.alias);
		args.push_back("--no-config");
		plugins_->simple_exec(script.provider + ".add", args, ret);
		BOOST_FOREACH(const std::string &s, ret) {
			LOG_DEBUG_CORE(" : " + s);
		}
	}
	BOOST_FOREACH(const std::string &cmd, on_start_) {
		std::list<std::string> ret;
		std::vector<std::string> args;
		try {
			strEx::s::parse_command(cmd, args);
		} catch (const std::exception &e) {
			LOG_ERROR_CORE("Failed to parse \"" + cmd + "\": " + utf8::utf8_from_native(e.what()));
			continue;
		}

		std::string command = args.front();
		args.erase(args.begin());
		plugins_->simple_exec(command, args, ret);
		BOOST_FOREACH(const std::string &s, ret) {
			LOG_DEBUG_CORE(" : " + s);
		}
	}

	return true;
}


/**
* Unload the plug in
* @throws NSPluginException if the module is not loaded and/or cannot be unloaded (plug in remains loaded if so).
*/
void nsclient::core::zip_plugin::unload_plugin() {
	boost::filesystem::path scripts_folder = boost::filesystem::path(paths_->expand_path("${scripts}")) / "tmp";
	boost::filesystem::path target_path = scripts_folder / getModule();
	BOOST_FOREACH(const script_def &script, scripts_) {
		boost::filesystem::path target = target_path / file_helpers::meta::get_filename(boost::filesystem::path(script.script));
		boost::filesystem::remove(target);
	}
	boost::filesystem::remove(target_path);
	boost::filesystem::remove(scripts_folder);
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

