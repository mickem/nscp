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
		THandle* ref() {
			return &handle_;
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
	};


}