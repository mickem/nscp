#pragma once

#include <boost/date_time.hpp>
#include <boost/lexical_cast.hpp>

#include <types.hpp>
#include <swap_bytes.hpp>
#include <nsca/nsca_enrypt.hpp>


namespace nsca {
//#define NSCA_MAX_PLUGINOUTPUT_LENGTH	512
//#define NSCA_MAX_PASSWORD_LENGTH     512
	class data {
	public:
		static const short transmitted_iuv_size = 128;
		static const short version3 = 3;

		typedef struct data_packet : public boost::noncopyable {
			int16_t   packet_version;
			u_int32_t crc32_value;
			u_int32_t timestamp;
			int16_t   return_code;
			char      data[];
			/*
			char      host_name[];
			char      svc_description[];
			char      plugin_output[];
			*/
			//data_packet_struct() : packet_version(NSCA_PACKET_VERSION_3) {}

			char* get_host_ptr() {
				return &data[0];
			}
			char* get_desc_ptr(unsigned int host_len) {
				return &data[host_len];
			}
			char* get_result_ptr(unsigned int host_len, unsigned int desc_len) {
				return &data[host_len+desc_len];
			}
		} data_packet;

		/* initialization packet containing IV and timestamp */
		typedef struct iv_packet {
			char      iv[transmitted_iuv_size];
			u_int32_t timestamp;
		} init_packet;

	};

	/* data packet containing service check results */
	class nsca_exception {
		std::wstring msg_;
	public:
		nsca_exception(std::wstring msg) : msg_(msg) {}
		std::wstring what() { return msg_;}
	};

	class length {
	public:
		typedef unsigned int size_type;
		static const short host_length = 64;
		static const short desc_length = 128;
	public:
		class data {
		public:
			static size_type payload_length_;
			static void set_payload_length(size_type length) {
				payload_length_ = length;
			}
			static size_type get_packet_length() {
				return get_packet_length(payload_length_);
			}
			static size_type get_packet_length(size_type output_length) {
				return sizeof(nsca::data::data_packet)+output_length*sizeof(char)+host_length*sizeof(char)+desc_length*sizeof(char);
			}
			static size_type get_payload_length() {
				return payload_length_;
			}
			static size_type get_payload_length(size_type packet_length) {
				return (packet_length- (host_length*sizeof(char)+desc_length*sizeof(char)+sizeof(nsca::data::data_packet)) )/sizeof(char);
			}
		};
		class iv {
		public:
			static const unsigned int payload_length_ = nsca::data::transmitted_iuv_size;
			/*
			static void set_payload_length(size_type length) {
				payload_length_ = length;
			}
			*/
			static size_type get_packet_length() {
				return get_packet_length(payload_length_);
			}
			static size_type get_packet_length(size_type output_length) {
				return sizeof(nsca::data::iv_packet);
			}
			static size_type get_payload_length() {
				return payload_length_;
			}
			static size_type get_payload_length(size_type packet_length) {
				return payload_length_;
			}
		};
	};

	class packet {
	public:
		std::string service;
		std::string result;
		std::string host;
		unsigned int code;
		unsigned int time;
		unsigned int payload_length_;
	public:
		packet(std::string _host, unsigned int payload_length = 512, int time_delta = 0) : host(_host), payload_length_(payload_length) {
			boost::posix_time::ptime now = boost::posix_time::second_clock::local_time()
				 + boost::posix_time::seconds(time_delta);
			boost::posix_time::ptime time_t_epoch(boost::gregorian::date(1970,1,1)); 
			boost::posix_time::time_duration diff = now - time_t_epoch;
			time = diff.total_seconds();
		}
		packet() : payload_length_(nsca::length::data::get_payload_length())
		{
		}
		packet(unsigned char* buffer, unsigned int buffer_len, unsigned int payload_length) : payload_length_(payload_length) {
			// TODO: Parse here
		}

		std::string toString() const {
			return "service: " + service + ", " + 
				"code: " + boost::lexical_cast<std::string>(code) + ", " + 
				"time: " + boost::lexical_cast<std::string>(time) + ", " + 
				"result: " + result;
		}

		void get_buffer(std::string &buffer) const {
			// FIXME: This is crap and needs rewriting. No std::string and beetter zero handling...
			if (service.length() >= nsca::length::desc_length)
				throw nsca::nsca_exception(_T("Description field to long"));
			if (host.length() >= nsca::length::host_length)
				throw nsca::nsca_exception(_T("Host field to long"));
			if (result.length() >= get_payload_length())
				throw nsca::nsca_exception(_T("Result field to long"));
			if (buffer.size() < get_packet_length())
				throw nsca::nsca_exception(_T("Buffer is to short"));

			nsca::data::data_packet *data = reinterpret_cast<nsca::data::data_packet*>(&*buffer.begin());

			data->packet_version=swap_bytes::hton<int16_t>(nsca::data::version3);
			data->timestamp=swap_bytes::hton<u_int32_t>(time);
			data->return_code = swap_bytes::hton<int16_t>(code);
			data->crc32_value= swap_bytes::hton<u_int32_t>(0);

			memset(data->get_host_ptr(), 0, host.size()+1);
			host.copy(data->get_host_ptr(), host.size());
			memset(data->get_desc_ptr(nsca::length::host_length), 0, service.size()+1);
			service.copy(data->get_desc_ptr(nsca::length::host_length), service.size());
			memset(data->get_result_ptr(nsca::length::host_length, nsca::length::desc_length), 0, result.size()+1);
			result.copy(data->get_result_ptr(nsca::length::host_length, nsca::length::desc_length), result.size());

			unsigned int calculated_crc32=calculate_crc32(buffer.c_str(),buffer.size());
			data->crc32_value=swap_bytes::hton<u_int32_t>(calculated_crc32);
		}
		std::string get_buffer() const {
			std::string buffer;
			get_buffer(buffer);
			return buffer;
		}
		unsigned int get_packet_length() const { return nsca::length::data::get_packet_length(payload_length_); }
		unsigned int get_payload_length() const { return payload_length_; }
	};

	class iv_packet {
	public:
		iv_packet() {
		}
		iv_packet(char* buf, unsigned int len) {

		}

	};
}
