/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "check_network.hpp"

#include <boost/algorithm/string/replace.hpp>
#include <boost/thread/locks.hpp>

#include <nscapi/nscapi_metrics_helper.hpp>

#include <parsers/where.hpp>
#include <parsers/where/node.hpp>
#include <parsers/where/engine.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/where/filter_handler_impl.hpp>

#include <error/nscp_exception.hpp>


namespace network_check {
	
	std::string helper::nif_query = "select NetConnectionID, MACAddress, Name, NetConnectionStatus, NetEnabled, Speed from Win32_NetworkAdapter where PhysicalAdapter=True and MACAddress <> null";
	std::string helper::prd_query = "select Name, BytesReceivedPersec, BytesSentPersec, BytesTotalPersec from Win32_PerfRawData_Tcpip_NetworkInterface";

		
	std::string helper::parse_nif_name(std::string name) {
		return name;
	}
	std::string helper::parse_prd_name(std::string name) {
		boost::replace_all(name, "[", "(");
		boost::replace_all(name, "]", ")");
		boost::replace_all(name, "#", "_");
		boost::replace_all(name, "/", "_");
		boost::replace_all(name, "\\", "_");
		return name;
	}
	
	void network_interface::read_wna(wmi_impl::row r) {
		name = helper::parse_nif_name(r.get_string("Name"));
		NetConnectionID = r.get_string("NetConnectionID");
		MACAddress = r.get_string("MACAddress");
		NetConnectionStatus = r.get_string("NetConnectionStatus");
		NetEnabled = r.get_int("NetEnabled")==0?"true":"false";
		Speed = r.get_string("Speed");
		has_nif = true;
	}

	void network_interface::read_prd(wmi_impl::row r) {
		BytesReceivedPersec = r.get_int("BytesReceivedPersec");
		BytesSentPersec = r.get_int("BytesSentPersec");
		BytesTotalPersec = r.get_int("BytesTotalPersec");
		has_prd = true;
	}

	void network_interface::build_metrics(Plugin::Common::MetricsBundle *section) const {

		using namespace nscapi::metrics;

		section->set_key("network");
		add_metric(section, name + ".NetConnectionID", NetConnectionID);
		add_metric(section, name + ".MACAddress", MACAddress);
		add_metric(section, name + ".NetConnectionStatus", NetConnectionStatus);
		add_metric(section, name + ".NetEnabled", NetEnabled);
		add_metric(section, name + ".Speed", Speed);
		if (has_prd) {
			add_metric(section, name + ".BytesReceivedPersec", BytesReceivedPersec);
			add_metric(section, name + ".BytesSentPersec", BytesSentPersec);
			add_metric(section, name + ".BytesTotalPersec", BytesTotalPersec);
		}
	}
	



	void network_data::query_nif(netmap_type &netmap) {
		wmi_impl::query wmiQuery1(helper::nif_query, "root\\cimv2", "", "");
		wmi_impl::row_enumerator row1 = wmiQuery1.execute();
		while (row1.has_next()) {
			network_interface nif;
			nif.read_wna(row1.get_next());
			netmap[nif.name] = nif;
		}
		std::string keys;
		BOOST_FOREACH(const netmap_type::value_type &v, netmap) {
			str::format::append_list(keys, v.first);

		}
	}

	void network_data::query_prd(netmap_type &netmap) {
		wmi_impl::query wmiQuery(helper::prd_query, "root\\cimv2", "", "");
		wmi_impl::row_enumerator row = wmiQuery.execute();
		while (row.has_next()) {
			wmi_impl::row r = row.get_next();
			std::string name = helper::parse_prd_name(r.get_string("Name"));
			netmap_type::iterator it = netmap.find(name);
			if (it == netmap.end()) {
				continue;
			}
			it->second.read_prd(r);
		}
	}


	void network_data::fetch() {

		if (!fetch_network_)
			return;

		nics_type tmp;
		try {
			netmap_type netmap;
			query_nif(netmap);
			query_prd(netmap);
				
			BOOST_FOREACH(const netmap_type::value_type &v, netmap) {
				if (!v.second.is_compleate())
					continue;
				tmp.push_back(v.second);
			}
		} catch (const wmi_impl::wmi_exception &e) {
			if (e.get_code() == WBEM_E_INVALID_QUERY) {
				fetch_network_ = false;
				throw error::nscp_exception("Failed to fetch network metrics, disabling...");
			}
			throw error::nscp_exception("Failed to fetch network metrics: " + e.reason());
		}
		{
			boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!writeLock.owns_lock())
				throw error::nscp_exception("Failed to get mutex for writing");
			nics_ = tmp;
		}
	}

	nics_type network_data::get() {
		boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
		if (!readLock.owns_lock()) 
			throw error::nscp_exception("Failed to get mutex for reading");
		return nics_;
	}



	namespace check {


		typedef network_interface filter_obj;

		typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
		struct filter_obj_handler : public native_context {
			filter_obj_handler();
		};
		typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;


		filter_obj_handler::filter_obj_handler() {
			static const parsers::where::value_type type_custom_state = parsers::where::type_custom_int_1;
			static const parsers::where::value_type type_custom_start_type = parsers::where::type_custom_int_2;

			registry_.add_string()
				("name", boost::bind(&filter_obj::get_name, _1), "Network interface name")
				("net_connection_id", boost::bind(&filter_obj::get_NetConnectionID, _1), "Network connection id")
				("MAC", boost::bind(&filter_obj::get_MACAddress, _1), "The MAC address")
				("status", boost::bind(&filter_obj::get_NetConnectionStatus, _1), "Network connection status")
				("enabled", boost::bind(&filter_obj::get_NetEnabled, _1), "True if the network interface is enabled")
				("speed", boost::bind(&filter_obj::get_Speed, _1), "The network interface speed")
				;

			registry_.add_int()
				("received", boost::bind(&filter_obj::getBytesReceivedPersec, _1), "Bytes received per second")
				("sent", boost::bind(&filter_obj::getBytesSentPersec, _1), "Bytes sent per second")
				("total", boost::bind(&filter_obj::getBytesTotalPersec, _1), "Bytes total per second")
				;
		}



		void check_network(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, nics_type nicdata) {
			modern_filter::data_container data;
			modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

			filter_type filter;
			filter_helper.add_options("total > 10000", "total > 100000", "", filter.get_filter_syntax(), "critical");
			filter_helper.add_syntax("${status}: ${list}", filter.get_filter_syntax(), "${name} >${sent} <${received} bps", "${name}", "", "%(status): Network interfaces seem ok.");

			if (!filter_helper.parse_options())
				return;

			if (!filter_helper.build_filter(filter))
				return;
			BOOST_FOREACH(network_check::nics_type::value_type v, nicdata) {
				boost::shared_ptr<filter_obj> record(new filter_obj(v));
				filter.match(record);
			}
			filter_helper.post_process(filter);
		}
	}


}