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

#include <nscapi/nscapi_thin_plugin_impl.hpp>
using namespace PB::Commands;

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
