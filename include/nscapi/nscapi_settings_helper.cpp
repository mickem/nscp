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

#include <nscapi/nscapi_settings_helper.hpp>

#include <error/nscp_exception.hpp>

#define STRING_FUN_STORER(val) boost::shared_ptr<store_functor>(new string_fun_storer(val))
#define BOOL_FUN_STORER(val) boost::shared_ptr<store_functor>(new bool_fun_storer(val))

#define STRING_STORER(val) boost::shared_ptr<store_functor>(new string_storer(val))
#define PATH_STORER(val) boost::shared_ptr<store_functor>(new path_storer(val))
#define BOOL_STORER(val) boost::shared_ptr<store_functor>(new bool_storer(val))
#define INT_STORER(type, val) boost::shared_ptr<store_functor>(new int_storer<type>(val))

#define MAP_STORER(fun) boost::shared_ptr<store_bin_functor>(new map_storer(fun))
#define KVP_STORER(val) boost::shared_ptr<store_bin_functor>(new kvp_storer(val))


#define LOOKUP_PATH boost::shared_ptr<post_processor>(new lookup_path_processor())

namespace nscapi {
	namespace settings_helper {

		struct post_processor {
			virtual nscapi::settings::settings_value process(settings_impl_interface_ptr core, nscapi::settings::settings_value value) = 0;
		};

		struct store_functor {
			virtual void store(nscapi::settings::settings_value value) = 0;
		};
		struct store_bin_functor {
			virtual void store(nscapi::settings::settings_value key, nscapi::settings::settings_value value) = 0;
		};

		//////////////////////////////////////////////////////////////////////////
		//
		// Basic type implementations
		//

		class typed_key : public key_interface {
		public:
			typed_key(boost::shared_ptr<store_functor> store_functor, const nscapi::settings::settings_value &default_value)
				: has_default_(true)
				, default_value_(default_value)
				, store_functor_(store_functor) {}
			typed_key(boost::shared_ptr<store_functor> store_functor)
				: has_default_(false)
				, default_value_(nscapi::settings::settings_value::make_empty())
				, store_functor_(store_functor) {}

			typed_key* default_value(const nscapi::settings::settings_value &v) {
				has_default_ = true;
				default_value_ = default_value_;
				return this;
			}

			nscapi::settings::settings_value get_default() const override {
				return default_value_;
			}
			void update_target(nscapi::settings::settings_value &value) const {
				if (store_functor_)
					store_functor_->store(value);
			}
			virtual void notify_path(settings_impl_interface_ptr core_, std::string path) const {
				throw error::nscp_exception("Not implemented: notify_path");
			}

		protected:
			bool has_default_;
			nscapi::settings::settings_value default_value_;
			boost::shared_ptr<store_functor> store_functor_;
		};

		class typed_string_value : public typed_key {
			boost::shared_ptr<post_processor> post_processor_;
			boost::shared_ptr<store_functor> store_functor_;
		public:
			typed_string_value(boost::shared_ptr<store_functor> store_functor)
				: typed_key(store_functor) {}
			typed_string_value(boost::shared_ptr<store_functor> store_functor, const std::string &v)
				: typed_key(store_functor, nscapi::settings::settings_value::make_string(v)) {}
			typed_string_value(boost::shared_ptr<store_functor> store_functor, boost::shared_ptr<post_processor> post_processor)
				: typed_key(store_functor)
				, post_processor_(post_processor) {}
			typed_string_value(boost::shared_ptr<store_functor> store_functor, const std::string &v, boost::shared_ptr<post_processor> post_processor)
				: typed_key(store_functor, nscapi::settings::settings_value::make_string(v))
				, post_processor_(post_processor) {}
			NSCAPI::settings_type get_type() const override {
				return NSCAPI::key_string;
			}
			void notify(settings_impl_interface_ptr core_, std::string path, std::string key) const {
				std::string dummy("$$DUMMY_VALUE_DO_NOT_USE$$");
				if (typed_key::has_default_)
					dummy = typed_key::default_value_.get_string();
				std::string data = core_->get_string(path, key, dummy);
				if (typed_key::has_default_ || data != dummy) {
					try {
						nscapi::settings::settings_value value = nscapi::settings::settings_value::make_string(data);
						this->update_target(post_processor_ ? post_processor_->process(core_, value) : value);
					} catch (const std::exception &e) {
						core_->err(__FILE__, __LINE__, "Failed to parse key: " + make_skey(path, key) + ": " + utf8::utf8_from_native(e.what()));
					}
				}
			}
			virtual void notify(settings_impl_interface_ptr core_, std::string parent, std::string path, std::string key) const {
				std::string dummy("$$DUMMY_VALUE_DO_NOT_USE$$");
				if (typed_key::has_default_)
					dummy = typed_key::default_value_.get_string();
				std::string data = core_->get_string(parent, key, dummy);
				if (typed_key::has_default_ || data != dummy)
					dummy = data;
				data = core_->get_string(path, key, dummy);
				if (typed_key::has_default_ || data != "$$DUMMY_VALUE_DO_NOT_USE$$") {
					try {
						this->update_target(nscapi::settings::settings_value::make_string(data));
					} catch (const std::exception &e) {
						core_->err(__FILE__, __LINE__, "Failed to parse key: " + make_skey(path, key) + ": " + utf8::utf8_from_native(e.what()));
					}
				}
			}
		};

		struct lookup_path_processor : public post_processor {
			virtual nscapi::settings::settings_value process(settings_impl_interface_ptr core_, nscapi::settings::settings_value value) override {
				return nscapi::settings::settings_value::make_string(core_->expand_path(value.get_string()));
			}
		};
		class typed_int_value : public typed_key {
		public:
			typed_int_value(boost::shared_ptr<store_functor> store_functor, long long v)
				: typed_key(store_functor, nscapi::settings::settings_value::make_int(v)) {}
			typed_int_value(boost::shared_ptr<store_functor> store_functor)
				: typed_key(store_functor) {}
			virtual NSCAPI::settings_type get_type() const {
				return NSCAPI::key_integer;
			}
			virtual void notify(settings_impl_interface_ptr core_, std::string path, std::string key) const {
				int dummy = -1;
				if (typed_key::has_default_)
					dummy = typed_key::default_value_.get_int();
				int val = core_->get_int(path, key, dummy);
				if (!typed_key::has_default_ && val == dummy) {
					dummy = -2;
					val = core_->get_int(path, key, dummy);
					if (val == dummy)
						return;
				}
				this->update_target(nscapi::settings::settings_value::make_int(val));
			}
			virtual void notify(settings_impl_interface_ptr core_, std::string parent, std::string path, std::string key) const {
				if (typed_key::has_default_) {
					int default_value = core_->get_int(parent, key, default_value_.get_int());
					int value = core_->get_int(path, key, default_value_.get_int());
					this->update_target(nscapi::settings::settings_value::make_int(value));
				} else {
					int dummy = -1;
					int defval = core_->get_int(path, key, dummy);
					if (defval == dummy) {
						dummy = -2;
						defval = core_->get_int(path, key, dummy);
					}
					if (defval != dummy) {
						int value = core_->get_int(path, key, defval);
						this->update_target(nscapi::settings::settings_value::make_int(value));
					}
					dummy = -1;
					int val = core_->get_int(path, key, dummy);
					if (val == dummy) {
						dummy = -2;
						val = core_->get_int(path, key, dummy);
						if (val == dummy)
							return;
					}
					this->update_target(nscapi::settings::settings_value::make_int(val));
				}
			}
		};

		class typed_bool_value : public typed_key {
		public:
			typed_bool_value(boost::shared_ptr<store_functor> store_functor, const bool& v)
				: typed_key(store_functor, nscapi::settings::settings_value::make_bool(v)) {}
			typed_bool_value(boost::shared_ptr<store_functor> store_functor)
				: typed_key(store_functor) {}
			virtual NSCAPI::settings_type get_type() const {
				return NSCAPI::key_bool;
			}

			virtual void notify(settings_impl_interface_ptr core_, std::string path, std::string key) const {
				if (typed_key::has_default_) {
					bool value = core_->get_bool(path, key, default_value_.get_bool());
					this->update_target(nscapi::settings::settings_value::make_bool(value));
				} else {
					bool v1 = core_->get_bool(path, key, true);
					bool v2 = core_->get_bool(path, key, false);
					if (v1 == v2)
						this->update_target(nscapi::settings::settings_value::make_bool(v1));
				}
			}
			virtual void notify(settings_impl_interface_ptr core_, std::string parent, std::string path, std::string key) const {
				bool default_value = core_->get_bool(path, key, default_value_.get_bool());
				bool value = core_->get_bool(path, key, default_value);
				this->update_target(nscapi::settings::settings_value::make_bool(value));
			}
		};

		class typed_kvp_value : public key_interface {
		public:
			typed_kvp_value(boost::shared_ptr<store_bin_functor> store_functor) : store_functor_(store_functor) {}

			virtual NSCAPI::settings_type get_type() const override {
				return NSCAPI::key_string;
			}

			nscapi::settings::settings_value get_default() const override {
				return nscapi::settings::settings_value::make_empty();
			}

			virtual void notify(settings_impl_interface_ptr core_, std::string path, std::string key) const override {
				throw error::nscp_exception("Not implemented: notify");
			}
			virtual void notify(settings_impl_interface_ptr core_, std::string parent, std::string path, std::string key) const override {
				throw error::nscp_exception("Not implemented: notify");
			}

			virtual void notify_path(settings_impl_interface_ptr core_, std::string path) const override {
				if (store_functor_) {
					BOOST_FOREACH(std::string key, core_->get_keys(path)) {
						std::string val = core_->get_string(path, key, "");
						store_functor_->store(nscapi::settings::settings_value::make_string(key), nscapi::settings::settings_value::make_string(val));
					}
					BOOST_FOREACH(std::string key, core_->get_sections(path)) {
						store_functor_->store(nscapi::settings::settings_value::make_string(key), nscapi::settings::settings_value::make_empty());
					}
				}
			}

		protected:
			boost::shared_ptr<store_bin_functor> store_functor_;
		};


		struct string_storer : public store_functor {
			std::string *store_to_;
			string_storer(std::string *store_to) : store_to_(store_to) {}
			void store(nscapi::settings::settings_value value) override {
				if (store_to_)
					*store_to_ = value.get_string();
			}
		};
		template<class T>
		struct int_storer : public store_functor {
			T *store_to_;
			int_storer(T *store_to) : store_to_(store_to) {}
			void store(nscapi::settings::settings_value value) override {
				if (store_to_)
					*store_to_ = value.get_int();
			}
		};
		struct bool_storer : public store_functor {
			bool *store_to_;
			bool_storer(bool *store_to) : store_to_(store_to) {}
			void store(nscapi::settings::settings_value value) override {
				if (store_to_)
					*store_to_ = value.get_bool();
			}
		};

		struct path_storer : public store_functor {
			boost::filesystem::path *store_to_;
			path_storer(boost::filesystem::path *store_to) : store_to_(store_to) {}
			void store(nscapi::settings::settings_value value) override {
				if (store_to_)
					*store_to_ = value.get_string();
			}
		};

		struct string_fun_storer : public store_functor {
			typedef boost::function<void(std::string)> fun_type;
			fun_type callback_;
			string_fun_storer(fun_type callback) : callback_(callback) {}
			void store(nscapi::settings::settings_value value) override {
				if (callback_)
					callback_(value.get_string());
			}
		};
		template<class T>
		struct int_fun_storer : public store_functor {
			typedef boost::function<void(T)> fun_type;
			fun_type callback_;
			int_fun_storer(fun_type callback) : callback_(callback) {}
			void store(nscapi::settings::settings_value value) override {
				if (callback_)
					callback_(value.get_int());
			}
		};
		struct bool_fun_storer : public store_functor {
			typedef boost::function<void(bool)> fun_type;
			fun_type callback_;
			bool_fun_storer(fun_type callback) : callback_(callback) {}
			void store(nscapi::settings::settings_value value) override {
				if (callback_)
					callback_(value.get_bool());
			}
		};

		boost::shared_ptr<key_interface> string_fun_key(boost::function<void(std::string)> fun, std::string def) {
			boost::shared_ptr<key_interface> r(new typed_string_value(STRING_FUN_STORER(fun), def));
			return r;
		}
		boost::shared_ptr<key_interface> string_fun_key(boost::function<void(std::string)> fun) {
			boost::shared_ptr<key_interface> r(new typed_string_value(STRING_FUN_STORER(fun)));
			return r;
		}

		boost::shared_ptr<key_interface> path_fun_key(boost::function<void(std::string)> fun, std::string def) {
			boost::shared_ptr<key_interface> r(new typed_string_value(STRING_FUN_STORER(fun), def, LOOKUP_PATH));
			return r;
		}
		boost::shared_ptr<key_interface> path_fun_key(boost::function<void(std::string)> fun) {
			boost::shared_ptr<key_interface> r(new typed_string_value(STRING_FUN_STORER(fun), LOOKUP_PATH));
			return r;
		}

		boost::shared_ptr<key_interface> bool_fun_key(boost::function<void(bool)> fun, bool def) {
			boost::shared_ptr<key_interface> r(new typed_bool_value(BOOL_FUN_STORER(fun), def));
			return r;
		}
		boost::shared_ptr<key_interface> bool_fun_key(boost::function<void(bool)> fun) {
			boost::shared_ptr<key_interface> r(new typed_bool_value(BOOL_FUN_STORER(fun)));
			return r;
		}

		boost::shared_ptr<key_interface> int_fun_key(boost::function<void(int)> fun, int def) {
			boost::shared_ptr<key_interface> r(new typed_int_value(boost::shared_ptr<store_functor>(new int_fun_storer<int>(fun)), def));
			return r;
		}
		boost::shared_ptr<key_interface> int_fun_key(boost::function<void(int)> fun) {
			boost::shared_ptr<key_interface> r(new typed_int_value(boost::shared_ptr<store_functor>(new int_fun_storer<int>(fun))));
			return r;
		}


		boost::shared_ptr<key_interface> path_key(std::string *val, std::string def) {
			boost::shared_ptr<key_interface> r(new typed_string_value(STRING_STORER(val), def, LOOKUP_PATH));
			return r;
		}
		boost::shared_ptr<key_interface> path_key(std::string *val) {
			boost::shared_ptr<key_interface> r(new typed_string_value(STRING_STORER(val), LOOKUP_PATH));
			return r;
		}
		boost::shared_ptr<key_interface> path_key(boost::filesystem::path *val, std::string def) {
			boost::shared_ptr<key_interface> r(new typed_string_value(PATH_STORER(val), def, LOOKUP_PATH));
			return r;
		}
		boost::shared_ptr<key_interface> path_key(boost::filesystem::path *val) {
			boost::shared_ptr<key_interface> r(new typed_string_value(PATH_STORER(val), LOOKUP_PATH));
			return r;
		}
		boost::shared_ptr<key_interface> string_key(std::string *val, std::string def) {
			boost::shared_ptr<key_interface> r(new typed_string_value(STRING_STORER(val), def));
			return r;
		}
		boost::shared_ptr<key_interface> string_key(std::string *val) {
			boost::shared_ptr<key_interface> r(new typed_string_value(STRING_STORER(val)));
			return r;
		}
		boost::shared_ptr<key_interface> int_key(int *val, int def) {
			boost::shared_ptr<key_interface> r(new typed_int_value(INT_STORER(int, val), def));
			return r;
		}
		boost::shared_ptr<key_interface> size_key(std::size_t *val, std::size_t def) {
			boost::shared_ptr<key_interface> r(new typed_int_value(INT_STORER(std::size_t, val), def));
			return r;
		}
		boost::shared_ptr<key_interface> int_key(int *val) {
			boost::shared_ptr<key_interface> r(new typed_int_value(INT_STORER(int, val)));
			return r;
		}
		boost::shared_ptr<key_interface> uint_key(unsigned int *val, unsigned int def) {
			boost::shared_ptr<key_interface> r(new typed_int_value(INT_STORER(unsigned int, val), def));
			return r;
		}
		boost::shared_ptr<key_interface> uint_key(unsigned int *val) {
			boost::shared_ptr<key_interface> r(new typed_int_value(INT_STORER(unsigned int, val)));
			return r;
		}
		boost::shared_ptr<key_interface> bool_key(bool *val, bool def) {
			boost::shared_ptr<key_interface> r(new typed_bool_value(BOOL_STORER(val), def));
			return r;
		}
		boost::shared_ptr<key_interface> bool_key(bool *val) {
			boost::shared_ptr<key_interface> r(new typed_bool_value(BOOL_STORER(val)));
			return r;
		}

		struct map_storer : public store_bin_functor {
			typedef std::map<std::string, std::string> map_type;
			map_type *store_to_;
			map_storer(map_type *store_to) : store_to_(store_to) {}
			void store(nscapi::settings::settings_value key, nscapi::settings::settings_value value) override {
				if (store_to_ && !value.is_empty())
					(*store_to_)[key.get_string()] = value.get_string();
			}
		};
		struct kvp_storer : public store_bin_functor {
			typedef boost::function<void(std::string, std::string)> fun_type;
			fun_type callback_;
			kvp_storer(fun_type callback) : callback_(callback) {}
			void store(nscapi::settings::settings_value key, nscapi::settings::settings_value value) override {
				if (callback_)
					callback_(key.get_string(), value.get_string());
			}
		};

		boost::shared_ptr<key_interface> fun_values_path(boost::function<void(std::string, std::string)> fun) {
			boost::shared_ptr<key_interface> r(new typed_kvp_value(KVP_STORER(fun)));
			return r;
		}
		boost::shared_ptr<key_interface> string_map_path(std::map<std::string, std::string> *val) {
			boost::shared_ptr<key_interface> r(new typed_kvp_value(MAP_STORER(val)));
			return r;
		}

		void settings_paths_easy_init::add(boost::shared_ptr<path_info> d) {
			if (is_sample)
				d->is_sample = true;
			owner->add(d);
		}
		void settings_tpl_easy_init::add(boost::shared_ptr<tpl_info> d) {
			owner->add(d);
		}
		void settings_keys_easy_init::add(boost::shared_ptr<key_info> d) {
			if (is_sample)
				d->is_sample = true;
			owner->add(d);
		}

	}
}