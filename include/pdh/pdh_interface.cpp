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

#include <str/utils.hpp>
#include <str/format.hpp>
#include <utf8.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

#include <pdh/pdh_interface.hpp>

#include <pdh/basic_impl.hpp>
#include <pdh/thread_Safe_impl.hpp>

#include <pdh/pdh_collectors.hpp>
#include <pdh/pdh_enumerations.hpp>

namespace PDH {
	boost::shared_ptr<impl_interface> factory::instance;

	void factory::set_thread_safe() {
		instance = boost::make_shared<ThreadedSafePDH>();
	}
	void factory::set_native() {
		instance = boost::make_shared<NativeExternalPDH>();
	}

	boost::shared_ptr<impl_interface> factory::get_impl() {
		if (!instance) {
			//instance = boost::make_shared<NativeExternalPDH>();
			//instance = new PDH::NativeExternalPDH();
			instance = boost::make_shared<ThreadedSafePDH>();
		}
		return instance;
	}

	counter_info::counter_info(BYTE *lpBuffer, DWORD, BOOL explainText) {
		PDH_COUNTER_INFO *info = (PDH_COUNTER_INFO*)lpBuffer;
		dwType = info->dwType;
		CVersion = info->CVersion;
		CStatus = info->CStatus;
		lScale = info->lScale;
		lDefaultScale = info->lDefaultScale;
		dwUserData = info->dwUserData;
		dwQueryUserData = info->dwQueryUserData;
		szFullPath = info->szFullPath;
		if (info->szMachineName)
			szMachineName = info->szMachineName;
		if (info->szObjectName)
			szObjectName = info->szObjectName;
		if (info->szInstanceName)
			szInstanceName = info->szInstanceName;
		if (info->szParentInstance)
			szParentInstance = info->szParentInstance;
		dwInstanceIndex = info->dwInstanceIndex;
		if (info->szCounterName)
			szCounterName = info->szCounterName;
		if (explainText) {
			if (info->szExplainText)
				szExplainText = info->szExplainText;
		}
	}

	void pdh_object::set_default_buffer_size(std::string buffer_size_) {
		if (buffer_size == 0)
			set_buffer_size(buffer_size_);
	}

	void pdh_object::set_buffer_size(std::string buffer_size_) {
		if (buffer_size_.empty())
			return;
		try {
			buffer_size = str::format::stox_as_time_sec<long>(buffer_size_, "s");
		} catch (...) {
			buffer_size = 0;
		}
	}
	unsigned long pdh_object::get_flags() {
		return flags_;
	}

	pdh_object::data_types pdh_object::get_type() {
		if ((flags_&format_large) == format_large)
			return type_large;
		if ((flags_&format_long) == format_long)
			return type_long;
		if ((flags_&format_double) == format_double)
			return type_double;
		throw pdh_exception("No type specified");
	}
	void pdh_object::set_type(const data_types type) {
		flags_ &= ~(format_double | format_long | format_large);
		if (type == type_double)
			flags_ |= format_double;
		else if (type == type_long)
			flags_ |= format_long;
		else if (type == type_large)
			flags_ |= format_large;
		else
			throw pdh_exception("Invalid type specified");
	}
	void pdh_object::set_type(const std::string &type) {
		flags_ &= ~(format_double | format_long | format_large);
		if (type == "double")
			flags_ |= format_double;
		else if (type == "long")
			flags_ |= format_long;
		else if (type == "large" || type == "long long")
			flags_ |= format_large;
		else
			throw pdh_exception("Invalid type specified: " + type);
	}

	void pdh_object::set_flags(const std::string &value) {
		BOOST_FOREACH(const std::string f, str::utils::split_lst(value, std::string(","))) {
			if (f == "nocap100")
				flags_ |= PDH_FMT_NOCAP100;
			else if (f == "1000")
				flags_ |= PDH_FMT_1000;
			else if (f == "noscale")
				flags_ |= PDH_FMT_NOSCALE;
			else
				throw pdh_exception("Invalid format specified: " + f);
		}
	}
	void pdh_object::add_flags(const std::string &value) {
		BOOST_FOREACH(const std::string f, str::utils::split_lst(value, std::string(","))) {
			if (f == "nocap100")
				flags_ |= PDH_FMT_NOCAP100;
			else if (f == "1000")
				flags_ |= PDH_FMT_1000;
			else if (f == "noscale")
				flags_ |= PDH_FMT_NOSCALE;
			else
				throw pdh_exception("Invalid format specified: " + f);
		}
	}

	bool pdh_object::has_instances() {
		if (instances_.empty() && path.find("$INSTANCE$") != std::string::npos)
			return true;
		if (instances_ == "auto" && path.find("$INSTANCE$") != std::string::npos)
			return true;
		return instances_ == "true";
	}

	std::list<std::string> helpers::build_list(TCHAR *buffer, DWORD bufferSize) {
		std::list<std::string> ret;
		if (bufferSize == 0)
			return ret;
		DWORD prevPos = 0;
		for (unsigned int i = 0; i < bufferSize - 1; i++) {
			if (buffer[i] == 0) {
				std::wstring str = &buffer[prevPos];
				ret.push_back(utf8::cvt<std::string>(str));
				prevPos = i + 1;
			}
		}
		return ret;
	}

	pdh_instance factory::create(pdh_object object) {
		if (object.has_instances()) {
			std::string path = object.path;

			str::utils::replace(path, "$INSTANCE$", "*");
			std::string alias = object.alias;
			std::string err;
			std::list<pdh_object> sub_counters;
			BOOST_FOREACH(const std::string &s, PDH::Enumerations::expand_wild_card_path(path, err)) {
				std::string::size_type pos1 = s.find('(');
				std::string tag = s;
				if (pos1 != std::string::npos) {
					std::string::size_type pos2 = s.find(')', pos1);
					if (pos2 != std::string::npos)
						tag = s.substr(pos1 + 1, pos2 - pos1 - 1);
				}
				pdh_object sub = object;
				sub.set_instances("");
				sub.alias = alias + "_" + tag;
				sub.path = s;
				sub_counters.push_back(sub);
			}
			if (!err.empty())
				throw pdh_exception("Failed to expand path: " + err);
			return boost::make_shared<instance_providers::container>(object, sub_counters);
		} else {
			if (object.is_rrd()) {
				if (object.get_type() == pdh_object::type_double)
					return boost::make_shared<instance_providers::rrd_collector<double> >(object);
				else if (object.get_type() == pdh_object::type_long)
					return boost::make_shared<instance_providers::rrd_collector<long> >(object);
				else if (object.get_type() == pdh_object::type_large)
					return boost::make_shared<instance_providers::rrd_collector<long long> >(object);
				else
					throw pdh_exception("Invalid type specified");
			} else if (object.is_static()) {
				if (object.get_type() == pdh_object::type_double)
					return boost::make_shared<instance_providers::value_collector<double> >(object);
				else if (object.get_type() == pdh_object::type_long)
					return boost::make_shared<instance_providers::value_collector<long> >(object);
				else if (object.get_type() == pdh_object::type_large)
					return boost::make_shared<instance_providers::value_collector<long long> >(object);
				else
					throw pdh_exception("Invalid type specified");
			} else {
				throw pdh_exception("Invalid strategy specified");
			}
		}
	}
	pdh_instance factory::create(std::string counter) {
		pdh_object object;
		object.set_counter(counter);
		object.set_type(pdh_object::type_double);
		return create(object);
	}
	pdh_instance factory::create(std::string counter, pdh_object object) {
		pdh_object copy = object;
		copy.set_counter(counter);
		return create(copy);
	}
}