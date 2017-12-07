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

#include "IcingaClient.h"


/**
 * Default c-tor
 * @return
 */
IcingaClient::IcingaClient() {}

/**
 * Default d-tor
 * @return
 */
IcingaClient::~IcingaClient() {}

bool IcingaClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
	return true;
}

//////////////////////////////////////////////////////////////////////////
// Settings helpers
//


/**
 * Unload (terminate) module.
 * Attempt to stop the background processing thread.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool IcingaClient::unloadModule() {
	return true;
}

