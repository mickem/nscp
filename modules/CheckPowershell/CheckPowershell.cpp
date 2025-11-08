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

#using <C:\\src\\build\\wtf\\System.Management.Automation.dll>

using namespace System;
//using namespace System::Management::Automation;
using namespace System::Management::Automation::Runspaces;
using namespace System::Collections;
using namespace System::Collections::Generic;
using namespace System::Collections::ObjectModel;
using namespace PB::Commands;
using namespace PB::Common;
using namespace NSCP::Helpers;


CheckPowershell::CheckPowershell() {
	commands = gcnew System::Collections::Generic::Dictionary<System::String^, System::String^>();

}

bool CheckPowershell::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
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

ResultCode parse_returnCode(String ^str) {
	int i = int::Parse(str);
	if (i == 0) {
		return ResultCode::Ok;
	}
	if (i == 1) {
		return ResultCode::Warning;
	}
	if (i == 2) {
		return ResultCode::Critical;
	}
	return ResultCode::Unknown;
}

struct perf_builder : public parsers::perfdata::builder {

	gcroot<QueryResponseMessage::Types::Response::Types::Line^> payload;
	gcroot<PerformanceData^> lastPerf = gcnew PerformanceData();
	gcroot<PerformanceData::Types::FloatValue^> lastFloat = gcnew PerformanceData::Types::FloatValue();

	perf_builder(QueryResponseMessage::Types::Response::Types::Line^ payload) : payload(payload) {}


	void add_string(std::string alias, std::string value) {
		lastPerf = gcnew PerformanceData();
		lastPerf->Alias = to_mstring(alias);
		payload->Perf->Add(lastPerf);
		lastPerf->Alias = to_mstring(alias);
		PerformanceData::Types::StringValue^ b = gcnew PerformanceData::Types::StringValue();
		b->Value = to_mstring(value);
		lastPerf->StringValue = b;
	}

	void add(std::string alias) {
		lastPerf = gcnew PerformanceData();
		lastFloat = gcnew PerformanceData::Types::FloatValue();
		lastPerf->Alias = to_mstring(alias);
	}
	void set_value(double value) {
		lastFloat->Value = value;
	}
	void set_warning(double value) {
		lastFloat->Warning->Value = value;
	}
	void set_critical(double value) {
		lastFloat->Critical->Value = value;
	}
	void set_minimum(double value) {
		lastFloat->Minimum->Value = value;
	}
	void set_maximum(double value) {
		lastFloat->Maximum->Value = value;
	}
	void set_unit(const std::string &value) {
		lastFloat->Unit = to_mstring(value);
	}
	void next() {
		lastPerf->FloatValue = lastFloat;
		payload->Perf->Add(lastPerf);
	}
};

void CheckPowershell::query_fallback(QueryRequestMessage::Types::Request^ request_payload, QueryResponseMessage::Types::Response^ query, QueryRequestMessage^) {
	query->Result = ResultCode::Unknown;
	if (!commands->ContainsKey(request_payload->Command)) {
		QueryResponseMessage::Types::Response::Types::Line ^line_builder = gcnew QueryResponseMessage::Types::Response::Types::Line();
		line_builder->Message = "Failed to find command: " + request_payload->Command;
		return;
	}
	String ^command = static_cast<Dictionary<String^, String^> ^>(commands)[request_payload->Command];

	System::Management::Automation::PowerShell ^ps = System::Management::Automation::PowerShell::Create();
	Runspace ^rs = RunspaceFactory::CreateRunspace();
	ps->Runspace = rs;
	ps->Runspace->Open();
	ps->AddScript("cd " + to_mstring(get_core()->expand_path("${base-path}")));
	NSC_DEBUG_MSG(to_nstring("Running: " + command));
	ps->AddScript(command);
	ps->AddScript("return $LastExitCode");

	Collection<System::Management::Automation::PSObject^>^ results = ps->Invoke();

	for each(System::Management::Automation::InformationRecord ^rec in ps->Streams->Information->ReadAll()) {
		QueryResponseMessage::Types::Response::Types::Line^ line_builder = gcnew QueryResponseMessage::Types::Response::Types::Line();
		String ^out = rec->ToString();
		if (out->Contains("|")) {
			int index = out->IndexOf("|");
			line_builder->Message = out->Substring(0, index);
			boost::shared_ptr<parsers::perfdata::builder> builder = boost::shared_ptr<perf_builder>(new perf_builder(line_builder));
			parsers::perfdata::parse(builder, to_nstring(out->Substring(index + 1)));
		} else {
			line_builder->Message = out;
		}
		query->Lines->Add(line_builder);
	}

	for each(System::Management::Automation::ErrorRecord ^rec in ps->Streams->Error->ReadAll()) {
		NSC_LOG_ERROR(to_nstring(rec->ToString()));
	}

	if (results->Count == 0) {
		NSC_LOG_ERROR("Failed to extract return code from powershell command");
	} else {
		query->Result = parse_returnCode(results[0]->ToString());
	}
}