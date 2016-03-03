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
		long long BytesReceivedPersec;
		long long BytesSentPersec;
		long long BytesTotalPersec;

		network_interface() : has_nif(false), has_prd(false) {}
		network_interface(const network_interface &other)
			: name(other.name)
			, NetConnectionID(other.NetConnectionID)
			, MACAddress(other.MACAddress)
			, NetConnectionStatus(other.NetConnectionStatus)
			, NetEnabled(other.NetEnabled)
			, Speed(other.Speed)
			, has_nif(other.has_nif)
			, has_prd(other.has_prd)
			, BytesReceivedPersec(other.BytesReceivedPersec)
			, BytesSentPersec(other.BytesSentPersec)
			, BytesTotalPersec(other.BytesTotalPersec) {}

		const network_interface* operator()(const network_interface &other) {
			name = other.name;
			NetConnectionID = other.NetConnectionID;
			MACAddress = other.MACAddress;
			NetConnectionStatus = other.NetConnectionStatus;
			NetEnabled = other.NetEnabled;
			Speed = other.Speed;
			has_nif = other.has_nif;
			has_prd = other.has_prd;
			BytesReceivedPersec = other.BytesReceivedPersec;
			BytesSentPersec = other.BytesSentPersec;
			BytesTotalPersec = other.BytesTotalPersec;
		}


		void read_wna(wmi_impl::row r);
		void read_prd(wmi_impl::row r);

		bool is_compleate() const { return has_nif; }
		void build_metrics(Plugin::Common::MetricsBundle *section) const;

		std::string get_name() const { return name; }
		std::string get_NetConnectionID() const { return NetConnectionID; }
		std::string get_MACAddress() const { return MACAddress; }
		std::string get_NetConnectionStatus() const { return NetConnectionStatus; }
		std::string get_NetEnabled() const { return NetEnabled; }
		std::string get_Speed() const { return Speed; }

		long long getBytesReceivedPersec() const { return BytesReceivedPersec; }
		long long getBytesSentPersec() const { return BytesReceivedPersec; }
		long long getBytesTotalPersec() const { return BytesReceivedPersec; }

	};

	typedef std::list<network_interface> nics_type;
	typedef boost::unordered_map<std::string, network_interface> netmap_type;

	class network_data {

		boost::shared_mutex mutex_;
		bool fetch_network_;
		nics_type nics_;
	public:

		network_data() : fetch_network_(true) {}

		void fetch();
		nics_type get();

	private:
		void query_nif(netmap_type &netmap);
		void query_prd(netmap_type &netmap);
	};


	namespace check {
		void check_network(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, nics_type data);

	}

}

