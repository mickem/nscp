#include "storage_manager.hpp"

#include <file_helpers.hpp>
#include <str/xtos.hpp>

#include <boost/thread/locks.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>

#include <fstream>

std::string mk_key(const std::string &plugin_name, const std::string &context, const std::string key = "") {
	return plugin_name + "." + context + "." + key;
}

template<typename T>
bool read_chunk(::google::protobuf::io::CodedInputStream &stream, T &obj) {
	uint32_t size = 0;
	stream.ReadVarint32(&size);
	std::string tmp;
	stream.ReadString(&tmp, size);
	obj.ParseFromString(tmp);
	return true;
}
void nsclient::core::storage_manager::load() {
	std::string file = get_filename();

	std::ifstream in(file.c_str(), std::ios::in | std::ios::binary);

	typedef boost::shared_ptr<google::protobuf::io::ZeroCopyInputStream> istr_type;
	typedef boost::shared_ptr<google::protobuf::io::CodedInputStream> codedstr_type;
	istr_type raw_in = istr_type(new ::google::protobuf::io::IstreamInputStream(&in));
	codedstr_type coded_in = codedstr_type(new ::google::protobuf::io::CodedInputStream(raw_in.get()));

	::Plugin::Storage::File header;
	if (!read_chunk(*coded_in, header)) {
		LOG_ERROR_CORE("Failed to read storage.");
		return;
	}
	for (long long i = 0; i < header.entries(); i++) {
		::Plugin::Storage::Block block;
		if (!read_chunk(*coded_in, block)) {
			LOG_ERROR_CORE("Failed to read block " + str::xtos(i) + " from storage.");
			continue;
		}
		storage_[mk_key(block.owner(), block.entry().context(), block.entry().key())] = storage_item(block.owner(), block.entry());
	}
}

void nsclient::core::storage_manager::put(std::string plugin_name, const ::Plugin::Storage_Entry& entry) {
	boost::unique_lock<boost::shared_mutex> writeLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!writeLock.owns_lock()) {
		LOG_ERROR_CORE("FATAL ERROR: Could not get write-mutex.");
		return;
	}
	std::string key = mk_key(plugin_name, entry.context(), entry.key());
	storage_[key] = storage_item(plugin_name, entry);
}

nsclient::core::storage_manager::entry_list nsclient::core::storage_manager::get(std::string plugin_name, std::string context) {
	entry_list ret;
	boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!readLock.owns_lock()) {
		LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
		return ret;
	}
	std::string key = mk_key(plugin_name, context);
	BOOST_FOREACH(const storage_type::value_type &v, storage_) {
		if (boost::algorithm::starts_with(v.first, key)) {
			ret.push_back(v.second.entry);
		}
	}
	return ret;
}

template<typename T> 
bool write_chunk(::google::protobuf::io::CodedOutputStream &stream, const T &obj) {
	std::string tmp;
	obj.SerializeToString(&tmp);
	stream.WriteVarint32(tmp.size());
	stream.WriteString(tmp);
	return true;
}

void nsclient::core::storage_manager::save() {
	try {
		{
			std::string file = get_tmpname();
			std::string path = file_helpers::meta::get_path(file);
			if (!file_helpers::checks::is_file(path)) {
				boost::filesystem::create_directories(path);
			}
			std::ofstream out(file.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);

			typedef boost::shared_ptr<google::protobuf::io::ZeroCopyOutputStream> istr_type;
			typedef boost::shared_ptr<google::protobuf::io::CodedOutputStream> codedstr_type;
			istr_type raw_out = istr_type(new ::google::protobuf::io::OstreamOutputStream(&out));
			codedstr_type coded_out = codedstr_type(new ::google::protobuf::io::CodedOutputStream(raw_out.get()));

			::Plugin::Storage::File header;
			header.set_version(1);
			header.set_entries(storage_.size());
			if (!write_chunk<>(*coded_out, header)) {
				LOG_ERROR_CORE("Failed to write header to storage.");
				return;
			}

			BOOST_FOREACH(const storage_type::value_type &v, storage_) {
				::Plugin::Storage::Block block;
				block.set_owner(v.second.owner);
				block.mutable_entry()->CopyFrom(v.second.entry);
				if (!write_chunk(*coded_out, block)) {
					LOG_ERROR_CORE("Failed to write block to storage.");
					return;
				}
			}
		}
		boost::filesystem::rename(get_tmpname(), get_filename());
	} catch (const std::exception &e) {
		LOG_ERROR_CORE("Failed to save settings: " + utf8::utf8_from_native(e.what()));
	} catch (...) {
		LOG_ERROR_CORE("Failed to save settings: UNKNOWN EXCEPTION");
	}

}

std::string nsclient::core::storage_manager::get_filename() {
	return path_->expand_path("${data-path}/nsclient.db");
}
std::string nsclient::core::storage_manager::get_tmpname() {
	return path_->expand_path("${data-path}/nsclient.tmp");
}
