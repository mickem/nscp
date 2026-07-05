// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/nscapi_thin_plugin_impl.hpp>

#include <vcclr.h>

class CheckPowershell : public nscapi::impl::thin_plugin {
private:

	gcroot<System::Collections::Generic::Dictionary<System::String^, System::String^>^ > commands;

public:

	CheckPowershell();

	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();
	void query_fallback(PB::Commands::QueryRequestMessage::Types::Request^ request_payload, PB::Commands::QueryResponseMessage::Types::Response^ query, PB::Commands::QueryRequestMessage^ request_message);
};
