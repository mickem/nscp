// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

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
