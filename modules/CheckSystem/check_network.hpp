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

#pragma once

#include <string>
#include <list>

#include <nscapi/nscapi_protobuf.hpp>

#include <wmi/wmi_query.hpp>

#include <boost/unordered_map.hpp>
#include <boost/thread/shared_mutex.hpp>

namespace network_check {
	struct helper {

		static std::string parse_nif_name(std::string name);
		static std::string parse_prd_name(std::string name);

		static std::string nif_query;
		static std::string prd_query;

	};

	struct network_interface {
		std::string name;
		std::string NetConnectionID;
		std::string MACAddress;
		std::string NetConnectionStatus;
		std::string NetEnabled;
		std::string Speed;
		bool has_nif;
		bool has_prd;
		long long oldBytesReceivedPersec;
		long long oldBytesSentPersec;
		long long oldBytesTotalPersec;
		long long BytesReceivedPersec;
		long long BytesSentPersec;
		long long BytesTotalPersec;

		network_interface() 
			: has_nif(false)
			, has_prd(false)
			, oldBytesReceivedPersec(0)
			, oldBytesSentPersec(0)
			, oldBytesTotalPersec(0)
			, BytesReceivedPersec(0)
			, BytesSentPersec(0)
			, BytesTotalPersec(0) {}
		network_interface(const network_interface &other)
			: name(other.name)
			, NetConnectionID(other.NetConnectionID)
			, MACAddress(other.MACAddress)
			, NetConnectionStatus(other.NetConnectionStatus)
			, NetEnabled(other.NetEnabled)
			, Speed(other.Speed)
			, has_nif(other.has_nif)
			, has_prd(other.has_prd)
			, oldBytesReceivedPersec(other.oldBytesReceivedPersec)
			, oldBytesSentPersec(other.oldBytesSentPersec)
			, oldBytesTotalPersec(other.oldBytesTotalPersec)
			, BytesReceivedPersec(other.BytesReceivedPersec)
			, BytesSentPersec(other.BytesSentPersec)
			, BytesTotalPersec(other.BytesTotalPersec) 
		{}

		const network_interface& operator()(const network_interface &other) {
			name = other.name;
			NetConnectionID = other.NetConnectionID;
			MACAddress = other.MACAddress;
			NetConnectionStatus = other.NetConnectionStatus;
			NetEnabled = other.NetEnabled;
			Speed = other.Speed;
			has_nif = other.has_nif;
			has_prd = other.has_prd;
			oldBytesReceivedPersec = other.oldBytesReceivedPersec;
			oldBytesSentPersec = other.oldBytesSentPersec;
			oldBytesTotalPersec = other.oldBytesTotalPersec;
			BytesReceivedPersec = other.BytesReceivedPersec;
			BytesSentPersec = other.BytesSentPersec;
			BytesTotalPersec = other.BytesTotalPersec;
			return *this;
		}


		void read_wna(wmi_impl::row r);
		void read_prd(wmi_impl::row r, long long delta);

		bool is_compleate() const { return has_nif; }
		void build_metrics(Plugin::Common::MetricsBundle *section) const;

		std::string get_name() const { return name; }
		std::string get_NetConnectionID() const { return NetConnectionID; }
		std::string get_MACAddress() const { return MACAddress; }
		std::string get_NetConnectionStatus() const { return NetConnectionStatus; }
		std::string get_NetEnabled() const { return NetEnabled; }
		std::string get_Speed() const { return Speed; }

		long long getBytesReceivedPersec() const { return BytesReceivedPersec; }
		long long getBytesSentPersec() const { return BytesSentPersec; }
		long long getBytesTotalPersec() const { return BytesTotalPersec; }

	};

	typedef std::list<network_interface> nics_type;
	typedef boost::unordered_map<std::string, network_interface> netmap_type;

	class network_data {

		boost::shared_mutex mutex_;
		bool fetch_network_;
		nics_type nics_;
		boost::posix_time::ptime last_;

	public:

		network_data() 
		: fetch_network_(true) 
		, last_(boost::posix_time::second_clock::local_time()) 
		{}

		void fetch();
		nics_type get();

	private:
		void query_nif(netmap_type &netmap);
		void query_prd(netmap_type &netmap, long long delta);
	};


	namespace check {
		void check_network(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, nics_type data);

	}

}

