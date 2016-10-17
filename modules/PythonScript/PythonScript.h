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

#include <strEx.h>
#include <utils.h>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <boost/python.hpp>

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>

#include <scripts/functions.hpp>

struct python_script : public boost::noncopyable {
	std::string alias;
	std::string base_path;
	unsigned int plugin_id;
	boost::python::dict localDict;
	python_script(unsigned int plugin_id, const std::string base_path, const std::string alias, const script_container& script);
	~python_script();
	bool callFunction(const std::string& functionName);
	bool callFunction(const std::string& functionName, unsigned int i1, const std::string &s1, const std::string &s2);
	bool callFunction(const std::string& functionName, const std::list<std::string> &args);
	void _exec(const std::string &scriptfile);
};

class PythonScript : public nscapi::impl::simple_plugin {
private:
	boost::filesystem::path root_;
	typedef script_container::list_type script_type;
	script_type scripts_;
	typedef std::list<boost::shared_ptr<python_script> > instance_list_type;
	instance_list_type instances_;
	std::string alias_;

public:
	PythonScript() {}
	virtual ~PythonScript() {}
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();
	std::string get_alias() {
		return alias_;
	}
	void query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message);
	void handleNotification(const std::string &channel, const Plugin::QueryResponseMessage::Response &request, Plugin::SubmitResponseMessage::Response *response, const Plugin::SubmitRequestMessage &request_message);
	bool commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message);
	void submitMetrics(const Plugin::MetricsMessage &response);
	void fetchMetrics(Plugin::MetricsMessage::Response *response);

	void list(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response);
	void execute_script(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response);

private:
	bool loadScript(std::string alias, std::string script);
	NSCAPI::nagiosReturn execute_and_load_python(std::list<std::wstring> args, std::wstring &message);
	boost::optional<boost::filesystem::path> find_file(std::string file);
};
