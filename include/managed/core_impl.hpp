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

#include <nscapi/nscapi_core_wrapper.hpp>

#include "Vcclr.h"

ref class CoreImpl : public NSCP::Core::ICore {
private:
	typedef cli::array<System::Byte> protobuf_data;
	nscapi::core_wrapper* core;

	nscapi::core_wrapper* get_core();

public:
	CoreImpl(nscapi::core_wrapper* core);

	virtual NSCP::Core::Result^ query(protobuf_data^ request);
	virtual NSCP::Core::Result^ exec(System::String^ target, protobuf_data^ request);
	virtual NSCP::Core::Result^ submit(System::String^ channel, protobuf_data^ request);
	virtual bool reload(System::String^ module);
	virtual NSCP::Core::Result^ settings(protobuf_data^ request);
	virtual NSCP::Core::Result^ registry(protobuf_data^ request);
	virtual void log(protobuf_data^ request);

};
