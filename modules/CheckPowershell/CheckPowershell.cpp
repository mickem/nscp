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

#include "CheckPowershell.h"

#include <NSCAPI.h>

#include <parsers/perfdata.hpp>

#include <nscapi/nscapi_plugin_wrapper.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>

#include <managed/convert.hpp>
#include <managed/core_impl.hpp>

#using <C:\\source\\build\\x64\\dev\\System.Management.Automation.dll>

using namespace System;
using namespace System::Management::Automation;
using namespace System::Management::Automation::Runspaces;
using namespace System::Collections;
using namespace System::Collections::Generic;
using namespace System::Collections::ObjectModel;
using namespace Plugin;
using namespace NSCP::Helpers;


CheckPowershell::CheckPowershell() {
	commands = gcnew System::Collections::Generic::Dictionary<System::String^, System::String^>();

}

bool CheckPowershell::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	String ^command_path = "/modules/powershell/commands";

	SettingsHelper^ settings = gcnew SettingsHelper(gcnew CoreImpl(get_core()), get_id());
	settings->registerPath(command_path, "Powershell scripts", "Native powershell script execution", false);
	RegistryHelper ^reg = gcnew RegistryHelper(gcnew CoreImpl(get_core()), get_id());

	for each (String^ s in settings->getKeys(command_path)) {
		settings->registerKey(command_path, s, 0, "Powershell script", "powershell script", "", false);
		String^ v = settings->getString(command_path, s, "");
		reg->registerCommand(s, "Command: " + v);
		static_cast<Dictionary<String^, String^> ^>(commands)[s] = v;
	}
	return true;
}
bool CheckPowershell::unloadModule() {
	return true;
}

Common::Types::ResultCode parse_returnCode(String ^str) {
	int i = int::Parse(str);
	if (i == 0) {
		return Common::Types::ResultCode::OK;
	}
	if (i == 1) {
		return Common::Types::ResultCode::WARNING;
	}
	if (i == 2) {
		return Common::Types::ResultCode::CRITICAL;
	}
	return Common::Types::ResultCode::UNKNOWN;
}

struct perf_builder : public parsers::perfdata::builder {

	gcroot<QueryResponseMessage::Types::Response::Types::Line::Builder^> payload;
	gcroot<Common::Types::PerformanceData::Builder^> lastPerf = Common::Types::PerformanceData::CreateBuilder();
	gcroot<Common::Types::PerformanceData::Types::FloatValue::Builder^> lastFloat = Common::Types::PerformanceData::Types::FloatValue::CreateBuilder();

	perf_builder(QueryResponseMessage::Types::Response::Types::Line::Builder^ payload) : payload(payload) {}


	void add_string(std::string alias, std::string value) {
		lastPerf = Common::Types::PerformanceData::CreateBuilder();
		lastPerf->Alias = to_mstring(alias);
		payload->AddPerf(lastPerf);
		lastPerf->Alias = to_mstring(alias);
		Common::Types::PerformanceData::Types::StringValue::Builder^ b = Common::Types::PerformanceData::Types::StringValue::CreateBuilder();
		b->Value = to_mstring(value);
		lastPerf->SetStringValue(b->Build());
	}

	void add(std::string alias) {
		lastPerf = Common::Types::PerformanceData::CreateBuilder();
		lastFloat = Common::Types::PerformanceData::Types::FloatValue::CreateBuilder();
		lastPerf->Alias = to_mstring(alias);
	}
	void set_value(float value) {
		lastFloat->Value = value;
	}
	void set_warning(float value) {
		lastFloat->Warning = value;
	}
	void set_critical(float value) {
		lastFloat->Critical = value;
	}
	void set_minimum(float value) {
		lastFloat->Minimum = value;
	}
	void set_maximum(float value) {
		lastFloat->Maximum = value;
	}
	void set_unit(const std::string &value) {
		lastFloat->Unit = to_mstring(value);
	}
	void next() {
		lastPerf->SetFloatValue(lastFloat->Build());
		payload->AddPerf(lastPerf->Build());
	}
};

void CheckPowershell::query_fallback(QueryRequestMessage::Types::Request^ request_payload, QueryResponseMessage::Types::Response::Builder^ query_builder, QueryRequestMessage^ request_message) {
	query_builder->SetResult(Common::Types::ResultCode::UNKNOWN);
	if (!commands->ContainsKey(request_payload->Command)) {
		QueryResponseMessage::Types::Response::Types::Line::Builder ^line_builder = QueryResponseMessage::Types::Response::Types::Line::CreateBuilder();
		line_builder->SetMessage("Failed to find command: " + request_payload->Command);
		return;
	}
	String ^command = static_cast<Dictionary<String^, String^> ^>(commands)[request_payload->Command];

	PowerShell ^ps = PowerShell::Create();
	Runspace ^rs = RunspaceFactory::CreateRunspace();
	ps->Runspace = rs;
	ps->Runspace->Open();
	ps->AddScript("cd " + to_mstring(get_core()->expand_path("${base-path}")));
	NSC_DEBUG_MSG(to_nstring("Running: " + command));
	ps->AddScript(command);
	ps->AddScript("return $LastExitCode");

	Collection<PSObject^>^ results = ps->Invoke();

	for each(InformationRecord ^rec in ps->Streams->Information->ReadAll()) {
		QueryResponseMessage::Types::Response::Types::Line::Builder^ line_builder = QueryResponseMessage::Types::Response::Types::Line::CreateBuilder();
		String ^out = rec->ToString();
		if (out->Contains("|")) {
			int index = out->IndexOf("|");
			line_builder->SetMessage(out->Substring(0, index));
			boost::shared_ptr<parsers::perfdata::builder> builder = boost::shared_ptr<perf_builder>(new perf_builder(line_builder));
			parsers::perfdata::parse(builder, to_nstring(out->Substring(index + 1)));
		} else {
			line_builder->SetMessage(out);
		}
		query_builder->AddLines(line_builder->Build());
	}

	for each(ErrorRecord ^rec in ps->Streams->Error->ReadAll()) {
		NSC_LOG_ERROR(to_nstring(rec->ToString()));
	}

	if (results->Count == 0) {
		NSC_LOG_ERROR("Failed to extract return code from powershell command");
	} else {
		query_builder->SetResult(parse_returnCode(results[0]->ToString()));
	}
}