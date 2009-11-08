/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#pragma once

#include <string>

namespace service_helper_impl {
/**
 * @ingroup NSClient++
 * Helper class to implement a NT service
 *
 * @version 1.0
 * first version
 *
 * @date 02-13-2005
 *
 * @author mickem
 *
 * @par license
 * This code is absolutely free to use and modify. The code is provided "as is" with
 * no expressed or implied warranty. The author accepts no liability if it causes
 * any damage to your computer, causes your pet to fall ill, increases baldness
 * or makes your car start emitting strange noises when you start it up.
 * This code has no bugs, just undocumented features!
 * 
 * @todo 
 *
 * @bug 
 *
 */
template <class TBase>
class unix_service : public TBase
{
public:
private:
public:
	unix_service(std::wstring name) {
	}
	virtual ~unix_service() {
	}

	bool StartServiceCtrlDispatcher() {
		return false;
	}




	void service_main(unsigned int dwArgc, wchar_t *lpszArgv)
	{

	}



	unsigned int service_ctrl_ex(unsigned int dwCtrlCode, unsigned int dwEventType, void* lpEventData, void* lpContext) {
		return 0;
	}

	/**
	* Actual code of the service that does the work.
	*
	* @param dwArgc 
	* @param *lpszArgv 
	*
	* @author mickem
	*
	* @date 03-13-2004
	*
	*/
	void ServiceStart(unsigned int dwArgc, char *lpszArgv) {
	}



	/**
	* Stops the service
	*
	*
	* @author mickem
	*
	* @date 03-13-2004
	*
	*/
	void ServiceStop() {
	}
};
}