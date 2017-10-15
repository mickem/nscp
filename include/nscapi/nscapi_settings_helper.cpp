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

#include <nsclient/nsclient_exception.hpp>

#include <utf8.hpp>

#include <boost/any.hpp>
#include <boost/foreach.hpp>
#include <boost/optional.hpp>

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
		namespace s = nscapi::settings;

		inline std::string make_skey(std::string path, std::string key) {
			return path + "." + key;
		}

		struct post_processor {
			virtual s::settings_value process(settings_impl_interface_ptr core, s::settings_value value) = 0;
		};

		struct store_functor {
			virtual void store(s::settings_value value) = 0;
		};
		struct store_bin_functor {
			virtual void store(s::settings_value key, s::settings_value value) = 0;
		};

		//////////////////////////////////////////////////////////////////////////
		//
		// Basic type implementations
		//

		class typed_key : public key_interface {
		public:
			typed_key(boost::shared_ptr<store_functor> store_functor, const s::settings_value &default_value)
				: has_default_(true)
				, default_value_(default_value)
				, store_functor_(store_functor) {}
			typed_key(boost::shared_ptr<store_functor> store_functor)
				: has_default_(false)
				, default_value_(s::settings_value::make_empty())
				, store_functor_(store_functor) {}

			typed_key* default_value(const s::settings_value &v) {
				has_default_ = true;
				default_value_ = v;
				return this;
			}

			s::settings_value get_default() const {
				return default_value_;
			}
			void update_target(s::settings_value &value) const {
				if (store_functor_)
					store_functor_->store(value);
			}
			virtual void notify_path(settings_impl_interface_ptr core_, std::string path) const {
				throw nsclient::nsclient_exception("Not implemented: notify_path");
			}

		protected:
			bool has_default_;
			s::settings_value default_value_;
			boost::shared_ptr<store_functor> store_functor_;
		};

		class typed_string_value : public typed_key {
			boost::shared_ptr<post_processor> post_processor_;
			boost::shared_ptr<store_functor> store_functor_;
		public:
			typed_string_value(boost::shared_ptr<store_functor> store_functor)
				: typed_key(store_functor) {}
			typed_string_value(boost::shared_ptr<store_functor> store_functor, const std::string &v)
				: typed_key(store_functor, s::settings_value::make_string(v)) {}
			typed_string_value(boost::shared_ptr<store_functor> store_functor, boost::shared_ptr<post_processor> post_processor)
				: typed_key(store_functor)
				, post_processor_(post_processor) {}
			typed_string_value(boost::shared_ptr<store_functor> store_functor, const std::string &v, boost::shared_ptr<post_processor> post_processor)
				: typed_key(store_functor, s::settings_value::make_string(v))
				, post_processor_(post_processor) {}
			NSCAPI::settings_type get_type() const {
				return NSCAPI::key_string;
			}
			void notify(settings_impl_interface_ptr core_, std::string path, std::string key) const {
				std::string dummy("$$DUMMY_VALUE_DO_NOT_USE$$");
				if (typed_key::has_default_)
					dummy = typed_key::default_value_.get_string();
				std::string data = core_->get_string(path, key, dummy);
				if (typed_key::has_default_ || data != dummy) {
					try {
						s::settings_value value = s::settings_value::make_string(data);
						if (post_processor_) {
							value = post_processor_->process(core_, value);
						}
						this->update_target(value);
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
						s::settings_value value = s::settings_value::make_string(data);
						this->update_target(value);
					} catch (const std::exception &e) {
						core_->err(__FILE__, __LINE__, "Failed to parse key: " + make_skey(path, key) + ": " + utf8::utf8_from_native(e.what()));
					}
				}
			}
		};

		struct lookup_path_processor : public post_processor {
			virtual s::settings_value process(settings_impl_interface_ptr core_, s::settings_value value) {
				return s::settings_value::make_string(core_->expand_path(value.get_string()));
			}
		};
		class typed_int_value : public typed_key {
		public:
			typed_int_value(boost::shared_ptr<store_functor> store_functor, long long v)
				: typed_key(store_functor, s::settings_value::make_int(v)) {}
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
				s::settings_value value = s::settings_value::make_int(val);
				this->update_target(value);
			}
			virtual void notify(settings_impl_interface_ptr core_, std::string parent, std::string path, std::string key) const {
				if (typed_key::has_default_) {
					int default_value = core_->get_int(parent, key, default_value_.get_int());
					int val = core_->get_int(path, key, default_value);
					s::settings_value value = s::settings_value::make_int(val);
					this->update_target(value);
				} else {
					int dummy = -1;
					int defval = core_->get_int(path, key, dummy);
					if (defval == dummy) {
						dummy = -2;
						defval = core_->get_int(path, key, dummy);
					}
					if (defval != dummy) {
						int val = core_->get_int(path, key, defval);
						s::settings_value value = s::settings_value::make_int(val);
						this->update_target(value);
					}
					dummy = -1;
					int val = core_->get_int(path, key, dummy);
					if (val == dummy) {
						dummy = -2;
						val = core_->get_int(path, key, dummy);
						if (val == dummy)
							return;
					}
					s::settings_value value = s::settings_value::make_int(val);
					this->update_target(value);
				}
			}
		};

		class typed_bool_value : public typed_key {
		public:
			typed_bool_value(boost::shared_ptr<store_functor> store_functor, const bool& v)
				: typed_key(store_functor, s::settings_value::make_bool(v)) {}
			typed_bool_value(boost::shared_ptr<store_functor> store_functor)
				: typed_key(store_functor) {}
			virtual NSCAPI::settings_type get_type() const {
				return NSCAPI::key_bool;
			}

			virtual void notify(settings_impl_interface_ptr core_, std::string path, std::string key) const {
				if (typed_key::has_default_) {
					bool val = core_->get_bool(path, key, default_value_.get_bool());
					s::settings_value value = s::settings_value::make_bool(val);
					this->update_target(value);
				} else {
					bool v1 = core_->get_bool(path, key, true);
					bool v2 = core_->get_bool(path, key, false);
					if (v1 == v2) {
						s::settings_value value = s::settings_value::make_bool(v1);
						this->update_target(value);
					}
				}
			}
			virtual void notify(settings_impl_interface_ptr core_, std::string parent, std::string path, std::string key) const {
				bool default_value = core_->get_bool(path, key, default_value_.get_bool());
				bool val = core_->get_bool(path, key, default_value);
				s::settings_value value = s::settings_value::make_bool(val);
				this->update_target(value);
			}
		};

		class typed_kvp_value : public key_interface {
		public:
			typed_kvp_value(boost::shared_ptr<store_bin_functor> store_functor) : store_functor_(store_functor) {}

			virtual NSCAPI::settings_type get_type() const {
				return NSCAPI::key_string;
			}

			s::settings_value get_default() const {
				return s::settings_value::make_empty();
			}

			virtual void notify(settings_impl_interface_ptr core_, std::string path, std::string key) const {
				throw nsclient::nsclient_exception("Not implemented: notify");
			}
			virtual void notify(settings_impl_interface_ptr core_, std::string parent, std::string path, std::string key) const {
				throw nsclient::nsclient_exception("Not implemented: notify");
			}

			virtual void notify_path(settings_impl_interface_ptr core_, std::string path) const {
				if (store_functor_) {
					BOOST_FOREACH(std::string key, core_->get_keys(path)) {
						std::string val = core_->get_string(path, key, "");
						store_functor_->store(s::settings_value::make_string(key), s::settings_value::make_string(val));
					}
					BOOST_FOREACH(std::string key, core_->get_sections(path)) {
						store_functor_->store(s::settings_value::make_string(key), s::settings_value::make_empty());
					}
				}
			}

		protected:
			boost::shared_ptr<store_bin_functor> store_functor_;
		};


		struct string_storer : public store_functor {
			std::string *store_to_;
			string_storer(std::string *store_to) : store_to_(store_to) {}
			void store(s::settings_value value) {
				if (store_to_)
					*store_to_ = value.get_string();
			}
		};
		template<class T>
		struct int_storer : public store_functor {
			T *store_to_;
			int_storer(T *store_to) : store_to_(store_to) {}
			void store(s::settings_value value) {
				if (store_to_)
					*store_to_ = value.get_int();
			}
		};
		struct bool_storer : public store_functor {
			bool *store_to_;
			bool_storer(bool *store_to) : store_to_(store_to) {}
			void store(s::settings_value value) {
				if (store_to_)
					*store_to_ = value.get_bool();
			}
		};

		struct path_storer : public store_functor {
			boost::filesystem::path *store_to_;
			path_storer(boost::filesystem::path *store_to) : store_to_(store_to) {}
			void store(s::settings_value value) {
				if (store_to_)
					*store_to_ = value.get_string();
			}
		};

		struct string_fun_storer : public store_functor {
			typedef boost::function<void(std::string)> fun_type;
			fun_type callback_;
			string_fun_storer(fun_type callback) : callback_(callback) {}
			void store(s::settings_value value) {
				if (callback_)
					callback_(value.get_string());
			}
		};
		template<class T>
		struct int_fun_storer : public store_functor {
			typedef boost::function<void(T)> fun_type;
			fun_type callback_;
			int_fun_storer(fun_type callback) : callback_(callback) {}
			void store(s::settings_value value) {
				if (callback_)
					callback_(value.get_int());
			}
		};
		struct bool_fun_storer : public store_functor {
			typedef boost::function<void(bool)> fun_type;
			fun_type callback_;
			bool_fun_storer(fun_type callback) : callback_(callback) {}
			void store(s::settings_value value) {
				if (callback_)
					callback_(value.get_bool());
			}
		};


		key_type string_fun_key(boost::function<void(std::string)> fun, std::string def) {
			key_type r(new typed_string_value(STRING_FUN_STORER(fun), def));
			return r;
		}
		key_type string_fun_key(boost::function<void(std::string)> fun) {
			key_type r(new typed_string_value(STRING_FUN_STORER(fun)));
			return r;
		}

		key_type path_fun_key(boost::function<void(std::string)> fun, std::string def) {
			key_type r(new typed_string_value(STRING_FUN_STORER(fun), def, LOOKUP_PATH));
			return r;
		}
		key_type path_fun_key(boost::function<void(std::string)> fun) {
			key_type r(new typed_string_value(STRING_FUN_STORER(fun), LOOKUP_PATH));
			return r;
		}

		key_type bool_fun_key(boost::function<void(bool)> fun, bool def) {
			key_type r(new typed_bool_value(BOOL_FUN_STORER(fun), def));
			return r;
		}
		key_type bool_fun_key(boost::function<void(bool)> fun) {
			key_type r(new typed_bool_value(BOOL_FUN_STORER(fun)));
			return r;
		}

		key_type int_fun_key(boost::function<void(int)> fun, int def) {
			key_type r(new typed_int_value(boost::shared_ptr<store_functor>(new int_fun_storer<int>(fun)), def));
			return r;
		}
		key_type int_fun_key(boost::function<void(int)> fun) {
			key_type r(new typed_int_value(boost::shared_ptr<store_functor>(new int_fun_storer<int>(fun))));
			return r;
		}


		key_type path_key(std::string *val, std::string def) {
			key_type r(new typed_string_value(STRING_STORER(val), def, LOOKUP_PATH));
			return r;
		}
		key_type path_key(std::string *val) {
			key_type r(new typed_string_value(STRING_STORER(val), LOOKUP_PATH));
			return r;
		}
		key_type path_key(boost::filesystem::path *val, std::string def) {
			key_type r(new typed_string_value(PATH_STORER(val), def, LOOKUP_PATH));
			return r;
		}
		key_type path_key(boost::filesystem::path *val) {
			key_type r(new typed_string_value(PATH_STORER(val), LOOKUP_PATH));
			return r;
		}
		key_type string_key(std::string *val, std::string def) {
			key_type r(new typed_string_value(STRING_STORER(val), def));
			return r;
		}
		key_type string_key(std::string *val) {
			key_type r(new typed_string_value(STRING_STORER(val)));
			return r;
		}
		key_type int_key(int *val, int def) {
			key_type r(new typed_int_value(INT_STORER(int, val), def));
			return r;
		}
		key_type size_key(std::size_t *val, std::size_t def) {
			key_type r(new typed_int_value(INT_STORER(std::size_t, val), def));
			return r;
		}
		key_type int_key(int *val) {
			key_type r(new typed_int_value(INT_STORER(int, val)));
			return r;
		}
		key_type uint_key(unsigned int *val, unsigned int def) {
			key_type r(new typed_int_value(INT_STORER(unsigned int, val), def));
			return r;
		}
		key_type uint_key(unsigned int *val) {
			key_type r(new typed_int_value(INT_STORER(unsigned int, val)));
			return r;
		}
		key_type bool_key(bool *val, bool def) {
			key_type r(new typed_bool_value(BOOL_STORER(val), def));
			return r;
		}
		key_type bool_key(bool *val) {
			key_type r(new typed_bool_value(BOOL_STORER(val)));
			return r;
		}

		struct map_storer : public store_bin_functor {
			typedef std::map<std::string, std::string> map_type;
			map_type *store_to_;
			map_storer(map_type *store_to) : store_to_(store_to) {}
			void store(s::settings_value key, s::settings_value value) {
				if (store_to_ && !value.is_empty())
					(*store_to_)[key.get_string()] = value.get_string();
			}
		};
		struct kvp_storer : public store_bin_functor {
			typedef boost::function<void(std::string, std::string)> fun_type;
			fun_type callback_;
			kvp_storer(fun_type callback) : callback_(callback) {}
			void store(s::settings_value key, s::settings_value value) {
				if (callback_)
					callback_(key.get_string(), value.get_string());
			}
		};

		key_type fun_values_path(boost::function<void(std::string, std::string)> fun) {
			key_type r(new typed_kvp_value(KVP_STORER(fun)));
			return r;
		}
		key_type string_map_path(std::map<std::string, std::string> *val) {
			key_type r(new typed_kvp_value(MAP_STORER(val)));
			return r;
		}
		//////////////////////////////////////////////////////////////////////////
		// 
		// Helper classes
		//

		struct path_info {
			std::string path_name;
			key_type path;
			description_container description;
			description_container subkey_description;
			bool is_subkey;
			bool is_sample;

			path_info(std::string path_name, description_container description) : path_name(path_name), description(description), is_subkey(false), is_sample(false) {}
			path_info(std::string path_name, key_type path, description_container description)
				: path_name(path_name)
				, path(path), description(description)
				, is_subkey(false)
				, is_sample(false) {}
			path_info(std::string path_name, key_type path, description_container description, description_container subkey_description)
				: path_name(path_name)
				, path(path), description(description)
				, subkey_description(subkey_description)
				, is_subkey(true)
				, is_sample(false) {}

			path_info(const path_info& obj) : path_name(obj.path_name), path(obj.path), description(obj.description), is_subkey(obj.is_subkey), is_sample(obj.is_sample) {}
			virtual path_info& operator=(const path_info& obj) {
				path_name = obj.path_name;
				path = obj.path;
				description = obj.description;
				subkey_description = obj.subkey_description;
				is_sample = obj.is_sample;
				is_subkey = obj.is_subkey;
				return *this;
			}
		};
		struct tpl_info {
			std::string path_name;
			description_container description;
			std::string fields;

			tpl_info(std::string path_name, description_container description, std::string fields) : path_name(path_name), description(description), fields(fields) {}

			tpl_info(const tpl_info& obj) : path_name(obj.path_name), description(obj.description), fields(obj.fields) {}
			virtual tpl_info& operator=(const tpl_info& obj) {
				path_name = obj.path_name;
				description = obj.description;
				fields = obj.fields;
				return *this;
			}
		};


		struct key_info {
			std::string path;
			std::string key_name;
			key_type key;
			description_container description;
			std::string parent;
			bool is_sample;

			key_info(std::string path_, std::string key_name_, key_type key, description_container description_)
				: path(path_)
				, key_name(key_name_)
				, key(key)
				, description(description_)
				, is_sample(false) {}
			key_info(const key_info& obj) : path(obj.path), key_name(obj.key_name), key(obj.key), description(obj.description), parent(obj.parent), is_sample(obj.is_sample) {}
			virtual key_info& operator=(const key_info& obj) {
				path = obj.path;
				key_name = obj.key_name;
				key = obj.key;
				description = obj.description;
				parent = obj.parent;
				is_sample = obj.is_sample;
				return *this;
			}
			void set_parent(std::string parent_) {
				parent = parent_;
			}
			bool has_parent() const {
				return !parent.empty();
			}
			std::string get_parent() const {
				return parent;
			}
		};

		settings_paths_easy_init& settings_paths_easy_init::operator()(key_type value, std::string title, std::string description, std::string subkeytitle, std::string subkeydescription) {
			boost::shared_ptr<path_info> d(new path_info(path_, value, description_container(title, description), description_container(subkeytitle, subkeydescription)));
			add(d);
			return *this;
		}

		settings_paths_easy_init& settings_paths_easy_init::operator()(std::string title, std::string description) {
			boost::shared_ptr<path_info> d(new path_info(path_, description_container(title, description)));
			add(d);
			return *this;
		}

		settings_paths_easy_init& settings_paths_easy_init::operator()(std::string path, std::string title, std::string description) {
			if (!path_.empty())
				path = path_ + "/" + path;
			boost::shared_ptr<path_info> d(new path_info(path, description_container(title, description)));
			add(d);
			return *this;
		}

		settings_paths_easy_init& settings_paths_easy_init::operator()(std::string path, key_type value, std::string title, std::string description, std::string subkeytitle, std::string subkeydescription) {
			if (!path_.empty())
				path = path_ + "/" + path;
			boost::shared_ptr<path_info> d(new path_info(path, value, description_container(title, description), description_container(subkeytitle, subkeydescription)));
			add(d);
			return *this;
		}

		settings_paths_easy_init& settings_paths_easy_init::operator()(std::string path, key_type value, std::string title, std::string description) {
			if (!path_.empty())
				path = path_ + "/" + path;
			boost::shared_ptr<path_info> d(new path_info(path, value, description_container(title, description)));
			add(d);
			return *this;
		}

		void settings_paths_easy_init::add(boost::shared_ptr<path_info> d) {
			if (is_sample)
				d->is_sample = true;
			owner->add(d);
		}

		settings_tpl_easy_init& settings_tpl_easy_init::operator()(std::string path, std::string icon, std::string title, std::string desc, std::string fields) {
			if (!path_.empty())
				path = path_ + "/" + path;
			boost::shared_ptr<tpl_info> d(new tpl_info(path, description_container(title, desc, icon), fields));
			add(d);
			return *this;
		}

		void settings_tpl_easy_init::add(boost::shared_ptr<tpl_info> d) {
			owner->add(d);
		}

		settings_keys_easy_init& settings_keys_easy_init::operator()(std::string path, std::string key_name, key_type value, std::string title, std::string description, bool advanced /*= false*/) {
			boost::shared_ptr<key_info> d(new key_info(path, key_name, value, description_container(title, description, advanced)));
			if (!parent_.empty())
				d->set_parent(parent_);
			add(d);
			return *this;
		}

		settings_keys_easy_init& settings_keys_easy_init::operator()(std::string key_name, key_type value, std::string title, std::string description, bool advanced /*= false*/) {
			boost::shared_ptr<key_info> d(new key_info(path_, key_name, value, description_container(title, description, advanced)));
			if (!parent_.empty())
				d->set_parent(parent_);
			add(d);
			return *this;
		}

		void settings_keys_easy_init::add(boost::shared_ptr<key_info> d) {
			if (is_sample)
				d->is_sample = true;
			owner->add(d);
		}




		void settings_registry::register_all() const {
			BOOST_FOREACH(key_list::value_type v, keys_) {
				if (v->key) {
					if (v->has_parent()) {
						core_->register_key(v->parent, v->key_name, v->key->get_type(), v->description.title, v->description.description, v->key->get_default(), v->description.advanced, v->is_sample);
						std::string desc = v->description.description + " parent for this key is found under: " + v->parent + " this is marked as advanced in favor of the parent.";
						core_->register_key(v->path, v->key_name, v->key->get_type(), v->description.title, desc, v->key->get_default(), true, false);
					} else {
						core_->register_key(v->path, v->key_name, v->key->get_type(), v->description.title, v->description.description, v->key->get_default(), v->description.advanced, v->is_sample);
					}
				}
			}
			BOOST_FOREACH(path_list::value_type v, paths_) {
				core_->register_path(v->path_name, v->description.title, v->description.description, v->description.advanced, v->is_sample);
				if (v->is_subkey) {
					core_->register_subkey(v->path_name, v->subkey_description.title, v->subkey_description.description, v->subkey_description.advanced, true);
				}
			}
			BOOST_FOREACH(tpl_list_type::value_type v, tpl_) {
				core_->register_tpl(v->path_name, v->description.title, v->description.icon, v->description.description, v->fields);
			}
		}

		void settings_registry::notify() {
			BOOST_FOREACH(key_list::value_type v, keys_) {
				try {
					if (v->key) {
						if (v->has_parent())
							v->key->notify(core_, v->parent, v->path, v->key_name);
						else
							v->key->notify(core_, v->path, v->key_name);
					}
				} catch (const std::exception &e) {
					core_->err(__FILE__, __LINE__, "Failed to notify " + v->key_name + ": " + utf8::utf8_from_native(e.what()));
				} catch (...) {
					core_->err(__FILE__, __LINE__, "Failed to notify " + v->key_name);
				}
			}
			BOOST_FOREACH(path_list::value_type v, paths_) {
				try {
					if (v->path)
						v->path->notify_path(core_, v->path_name);
				} catch (const std::exception &e) {
					core_->err(__FILE__, __LINE__, "Failed to notify " + v->path_name + ": " + utf8::utf8_from_native(e.what()));
				} catch (...) {
					core_->err(__FILE__, __LINE__, "Failed to notify " + v->path_name);
				}
			}
		}

	}
}