/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#pragma once

#include <boost/thread.hpp>

#include <pdh.hpp>
#include <boost/unordered_map.hpp>
#include <boost/shared_ptr.hpp>


#define PDH_SYSTEM_KEY_CPU "cpu"
#define PDH_SYSTEM_KEY_MCB "memory commit bytes"
#define PDH_SYSTEM_KEY_MCL "memory commit limit"
#define PDH_SYSTEM_KEY_UPT "uptime"


/**
 * @ingroup NSClientCompat
 * PDH collector thread (gathers performance data and allows for clients to retrieve it)
 *
 * @version 1.0
 * first version
 *
 * @date 02-13-2005
 *
 * @author mickem
 *
 * @par license
 * This code is absolutely free to use and modify. The code is provided "as is" with
 * no expressed or implied warranty. The author accepts no liability if it causes
 * any damage to your computer, causes your pet to fall ill, increases baldness
 * or makes your car start emitting strange noises when you start it up.
 * This code has no bugs, just undocumented features!
 * 
 */
class PDHCollector {
public:

	struct system_counter_data {

		struct counter {
			typedef enum data_type_struct {
				type_int64, type_uint64
			};
			typedef enum data_format_struct {
				format_large
			};
			typedef enum collection_strategy_struct {
				rrd, value
			};

			counter(std::string alias, std::wstring path, data_type_struct data_type, data_format_struct data_format, collection_strategy_struct collection_strategy)
				: alias(alias)
				, path(path)
				, data_type(data_type)
				, data_format(data_format)
				, collection_strategy(collection_strategy)
			{}
			counter(std::string alias, std::wstring path, data_type_struct data_type, data_format_struct data_format, collection_strategy_struct collection_strategy, std::string buffer_size)
				: alias(alias)
				, path(path)
				, data_type(data_type)
				, data_format(data_format)
				, collection_strategy(collection_strategy)
				, buffer_size(buffer_size)
			{}
			data_type_struct data_type;
			data_format_struct data_format;
			std::string alias;
			std::wstring path;
			std::string buffer_size;
			collection_strategy_struct collection_strategy;

			boost::shared_ptr<PDHCollectors::PDHCollector> create(int check_intervall);
			void set_default_buffer_size(std::string buffer_size_) {
				if (buffer_size.empty())
					buffer_size = buffer_size_;
			}

			int get_buffer_length(int check_intervall) {
				try {
					unsigned int i = strEx::stoui_as_time(buffer_size, check_intervall*100);
					if (check_intervall == 0)
						return 100; // TODO fix this!
					return i/(check_intervall*100)+10;
				} catch (...) {
					return 100; // TODO fix this!
				}

			}


		};

		unsigned int check_intervall;
		std::string buffer_length;
		std::wstring subsystem;

		std::list<counter> counters;
	};

private:

	boost::shared_mutex mutex_;
	HANDLE stop_event_;
	typedef boost::shared_ptr<PDHCollectors::PDHCollector> collector_ptr;
	typedef boost::unordered_map<std::string,collector_ptr > counter_map;
	counter_map counters_;
	int check_intervall_;
	boost::shared_ptr<boost::thread> thread_;
	boost::shared_ptr<system_counter_data> thread_data_;

public:
	PDHCollector();
	virtual ~PDHCollector();
	void thread_proc();
	void exitThread(void);

	// Retrieve values
	int getCPUAvrage(std::string time);
	long long getUptime();
	unsigned long long getMemCommitLimit();
	unsigned long long getMemCommit();
	bool loadCounter(PDH::PDHQuery &pdh);

	__int64 get_int_value(std::string counter);
	double get_avg_value(std::string counter, unsigned int delta);
	double get_double(std::string counter);
	void start(boost::shared_ptr<system_counter_data> data);
	bool stop();
};
