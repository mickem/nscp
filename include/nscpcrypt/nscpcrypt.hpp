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

#include <sstream>

#define TRANSMITTED_IV_SIZE     128     /* size of IV to transmit - must be as big as largest IV needed for any crypto algorithm */

namespace nscp {
	namespace encryption {
		class encryption_exception : public std::exception {
			std::string msg_;
		public:
			encryption_exception() {}
			~encryption_exception() throw () {}
			encryption_exception(std::string msg) : msg_(msg) {}
			const char* what() const throw () { return msg_.c_str(); }
		};
		struct helpers {
			static std::string get_crypto_string(std::string sep = ", ");
			static int encryption_to_int(std::string encryption_raw);
			static std::string encryption_to_string(int encryption);
		};
		class any_encryption {
		public:
			virtual ~any_encryption() {}
			virtual void init(std::string password, std::string iv) = 0;
			virtual void encrypt(std::string &buffer) = 0;
			virtual void decrypt(std::string &buffer) = 0;
			virtual std::string getName() = 0;
			virtual int get_keySize() = 0;
			virtual std::size_t get_blockSize() = 0;
		};

		class engine {
		public:

		private:
			any_encryption *core_;
		public:

			engine() : core_(NULL) {}
			~engine() {
				delete core_;
			}

			static bool hasEncryption(int encryption_method);
			static any_encryption* get_encryption_core(int encryption_method);
			static std::string generate_transmitted_iv(unsigned int len = TRANSMITTED_IV_SIZE);
			/* initializes encryption routines */
			void encrypt_init(std::string password, int encryption_method, std::string received_iv);

			/* encrypt a buffer */
			void encrypt_buffer(std::string &buffer);
			/* encrypt a buffer */
			void decrypt_buffer(std::string &buffer);
			std::string get_rand_buffer(int length);
			std::string to_string() const {
				if (core_ == NULL)
					return "<NULL>";
				std::stringstream ss;
				ss << "Name: " << core_->getName()
					<< ", block size: " << core_->get_blockSize()
					<< ", key size: " << core_->get_keySize();
				return ss.str();
			}
		};
	}
}