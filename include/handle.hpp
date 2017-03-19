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

namespace hlp {
	template<class THandle, class TCloser>
	struct handle {
		THandle handle_;
		handle() : handle_(NULL) {}
		handle(THandle handle) : handle_(handle) {}
		~handle() {
			close();
		}
		void close() {
			if (handle_ != NULL)
				TCloser::close(handle_);
			handle_ = NULL;
		}
		THandle get() const {
			return handle_;
		}
		void set(THandle other) {
			close();
			handle_ = other;
		}
		THandle* ref() {
			return &handle_;
		}
		THandle detach() {
			THandle tmp = handle_;
			handle_ = NULL;
			return tmp;
		}
		operator THandle() const {
			return handle_;
		}
		operator bool() const {
			return handle_ != NULL;
		}
		const handle<THandle, TCloser>& operator = (const THandle &other) {
			close();
			handle_ = other;
			return *this;
		}
		const handle<THandle, TCloser>& operator = (handle<THandle, TCloser> &other) {
			close();
			handle_ = other.detach();
			return *this;
		}
	};


}