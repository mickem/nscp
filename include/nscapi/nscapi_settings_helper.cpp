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


namespace nscapi {
	namespace settings_helper {
		namespace s = nscapi::settings;

		inline std::string make_skey(std::string path, std::string key) {
			return path + "." + key;
		}

		struct post_processor {
			virtual std::string process(settings_impl_interface_ptr core, std::string value) = 0;
		};

		struct store_functor {
			virtual void store(std::string value) = 0;
		};
		struct store_bin_functor {
			virtual void store(std::string key, std::string value) = 0;
		};

		typedef boost::shared_ptr<store_functor> store_ptr;
		typedef boost::shared_ptr<post_processor> post_ptr;
		typedef boost::shared_ptr<store_bin_functor> bin_ptr;

		//////////////////////////////////////////////////////////////////////////
		//
		// Basic type implementations
		//

		class typed_key : public key_interface {
			typedef store_ptr store_ptr;
			typedef boost::shared_ptr<post_processor> post_ptr;

			bool has_default_;
			std::string default_value_;

			post_ptr post_functor_;
			store_ptr store_functor_;
		public:
			typed_key(store_functor *store_functor)
				: has_default_(false)
				, store_functor_(store_ptr(store_functor)) {}
			typed_key(store_functor *store_functor, const std::string &default_value)
				: has_default_(true)
				, default_value_(default_value)
				, store_functor_(store_ptr(store_functor)) {}
			typed_key(store_functor *store_functor, post_processor *post_functor)
				: has_default_(false)
				, store_functor_(store_ptr(store_functor))
				, post_functor_(post_ptr(post_functor)) {}
			typed_key(store_functor *store_functor, const std::string &default_value, post_processor *post_functor)
				: has_default_(true)
				, default_value_(default_value)
				, store_functor_(store_ptr(store_functor))
				, post_functor_(post_ptr(post_functor)) {}

			std::string get_default() const {
				return default_value_;
			}
			void update_target(std::string &value) const {
				if (store_functor_)
					store_functor_->store(value);
			}
			virtual void notify_path(settings_impl_interface_ptr core_, std::string path) const {
				throw nsclient::nsclient_exception("Not implemented: notify_path");
			}

			void notify(settings_impl_interface_ptr core_, std::string path, std::string key) const {
				std::string dummy("$$DUMMY_VALUE_DO_NOT_USE$$");
				if (has_default_)
					dummy = default_value_;
				std::string data = core_->get_string(path, key, dummy);
				if (has_default_ || data != dummy) {
					try {
						if (post_functor_) {
							data = post_functor_->process(core_, data);
						}
						this->update_target(data);
					} catch (const std::exception &e) {
						core_->err(__FILE__, __LINE__, "Failed to parse key: " + make_skey(path, key) + ": " + utf8::utf8_from_native(e.what()));
					}
				}
			}
			virtual void notify(settings_impl_interface_ptr core_, std::string parent, std::string path, std::string key) const {
				std::string dummy("$$DUMMY_VALUE_DO_NOT_USE$$");
				if (has_default_)
					dummy = default_value_;
				std::string data = core_->get_string(parent, key, dummy);
				if (has_default_ || data != dummy)
					dummy = data;
				data = core_->get_string(path, key, dummy);
				if (has_default_ || data != "$$DUMMY_VALUE_DO_NOT_USE$$") {
					try {
						this->update_target(data);
					} catch (const std::exception &e) {
						core_->err(__FILE__, __LINE__, "Failed to parse key: " + make_skey(path, key) + ": " + utf8::utf8_from_native(e.what()));
					}
				}
			}
		};

		struct lookup_path_processor : public post_processor {
			virtual std::string process(settings_impl_interface_ptr core_, std::string value) {
				return core_->expand_path(value);
			}
		};

		class typed_kvp_value : public key_interface {
		private:
			bin_ptr store_functor_;
		public:
			typed_kvp_value(store_bin_functor *store_functor) 
				: store_functor_(bin_ptr(store_functor)) {}

			std::string get_default() const {
				return "";
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
						store_functor_->store(key, val);
					}
					BOOST_FOREACH(std::string key, core_->get_sections(path)) {
						store_functor_->store(key, "");
					}
				}
			}
		};


		struct string_storer : public store_functor {
			std::string *store_to_;
			string_storer(std::string *store_to) : store_to_(store_to) {}
			void store(std::string value) {
				if (store_to_)
					*store_to_ = value;
			}
		};
		template<class T>
		struct int_storer : public store_functor {
			T *store_to_;
			int_storer(T *store_to) : store_to_(store_to) {}
			void store(std::string value) {
				if (store_to_)
					*store_to_ = str::stox<T>(value, 0);
			}
		};
		struct bool_storer : public store_functor {
			bool *store_to_;
			bool_storer(bool *store_to) : store_to_(store_to) {}
			void store(std::string value) {
				if (store_to_)
					*store_to_ = s::settings_value::to_bool(value);
			}
		};

		struct path_storer : public store_functor {
			boost::filesystem::path *store_to_;
			path_storer(boost::filesystem::path *store_to) : store_to_(store_to) {}
			void store(std::string value) {
				if (store_to_)
					*store_to_ = value;
			}
		};

		struct string_fun_storer : public store_functor {
			typedef boost::function<void(std::string)> fun_type;
			fun_type callback_;
			string_fun_storer(fun_type callback) : callback_(callback) {}
			void store(std::string value) {
				if (callback_)
					callback_(value);
			}
		};
		struct cstring_fun_storer : public store_functor {
			typedef boost::function<void(const char*)> fun_type;
			fun_type callback_;
			cstring_fun_storer(fun_type callback) : callback_(callback) {}
			void store(std::string value) {
				if (callback_)
					callback_(value.c_str());
			}
		};
		template<class T>
		struct int_fun_storer : public store_functor {
			typedef boost::function<void(T)> fun_type;
			fun_type callback_;
			int_fun_storer(fun_type callback) : callback_(callback) {}
			void store(std::string value) {
				if (callback_)
					callback_(str::stox<T>(value, -1));
			}
		};
		struct bool_fun_storer : public store_functor {
			typedef boost::function<void(bool)> fun_type;
			fun_type callback_;
			bool_fun_storer(fun_type callback) : callback_(callback) {}
			void store(std::string value) {
				if (callback_)
					callback_(s::settings_value::to_bool(value));
			}
		};


		key_type string_fun_key(boost::function<void(std::string)> fun, std::string def) {
			key_type r(new typed_key(new string_fun_storer(fun), def));
			return r;
		}
		key_type string_fun_key(boost::function<void(std::string)> fun) {
			key_type r(new typed_key(new string_fun_storer(fun)));
			return r;
		}
		key_type cstring_fun_key(boost::function<void(const char*)> fun, std::string def) {
			key_type r(new typed_key(new cstring_fun_storer(fun), def));
			return r;
		}
		key_type cstring_fun_key(boost::function<void(const char*)> fun) {
			key_type r(new typed_key(new cstring_fun_storer(fun)));
			return r;
		}

		key_type path_fun_key(boost::function<void(std::string)> fun, std::string def) {
			key_type r(new typed_key(new string_fun_storer(fun), def, new lookup_path_processor()));
			return r;
		}
		key_type path_fun_key(boost::function<void(std::string)> fun) {
			key_type r(new typed_key(new string_fun_storer(fun), new lookup_path_processor()));
			return r;
		}

		key_type bool_fun_key(boost::function<void(bool)> fun, bool def) {
			key_type r(new typed_key(new bool_fun_storer(fun), s::settings_value::from_bool(def)));
			return r;
		}
		key_type bool_fun_key(boost::function<void(bool)> fun) {
			key_type r(new typed_key(new bool_fun_storer(fun)));
			return r;
		}

		key_type int_fun_key(boost::function<void(int)> fun, int def) {
			key_type r(new typed_key(new int_fun_storer<int>(fun), s::settings_value::from_int(def)));
			return r;
		}
		key_type int_fun_key(boost::function<void(int)> fun) {
			key_type r(new typed_key(new int_fun_storer<int>(fun)));
			return r;
		}


		key_type path_key(std::string *val, std::string def) {
			key_type r(new typed_key(new string_storer(val), def, new lookup_path_processor()));
			return r;
		}
		key_type path_key(std::string *val) {
			key_type r(new typed_key(new string_storer(val), new lookup_path_processor()));
			return r;
		}
		key_type path_key(boost::filesystem::path *val, std::string def) {
			key_type r(new typed_key(new path_storer(val), def, new lookup_path_processor()));
			return r;
		}
		key_type path_key(boost::filesystem::path *val) {
			key_type r(new typed_key(new path_storer(val), new lookup_path_processor()));
			return r;
		}
		key_type string_key(std::string *val, std::string def) {
			key_type r(new typed_key(new string_storer(val), def));
			return r;
		}
		key_type string_key(std::string *val) {
			key_type r(new typed_key(new string_storer(val)));
			return r;
		}
		key_type int_key(int *val, int def) {
			key_type r(new typed_key(new int_storer<int>(val), s::settings_value::from_int(def)));
			return r;
		}
		key_type size_key(std::size_t *val, std::size_t def) {
			key_type r(new typed_key(new int_storer<std::size_t>(val), str::xtos<std::size_t>(def)));
			return r;
		}
		key_type int_key(int *val) {
			key_type r(new typed_key(new int_storer<int>(val)));
			return r;
		}
		key_type uint_key(unsigned int *val, unsigned int def) {
			key_type r(new typed_key(new int_storer<unsigned int>(val), s::settings_value::from_int(def)));
			return r;
		}
		key_type uint_key(unsigned int *val) {
			key_type r(new typed_key(new int_storer<unsigned int>(val)));
			return r;
		}
		key_type bool_key(bool *val, bool def) {
			key_type r(new typed_key(new bool_storer(val), s::settings_value::from_bool(def)));
			return r;
		}
		key_type bool_key(bool *val) {
			key_type r(new typed_key(new bool_storer(val)));
			return r;
		}

		struct map_storer : public store_bin_functor {
			typedef std::map<std::string, std::string> map_type;
			map_type *store_to_;
			map_storer(map_type *store_to) : store_to_(store_to) {}
			void store(std::string key, std::string value) {
				if (store_to_ && !value.empty())
					(*store_to_)[key] = value;
			}
		};
		struct kvp_storer : public store_bin_functor {
			typedef boost::function<void(std::string, std::string)> fun_type;
			fun_type callback_;
			kvp_storer(fun_type callback) : callback_(callback) {}
			void store(std::string key, std::string value) {
				if (callback_)
					callback_(key, value);
			}
		};

		key_type fun_values_path(boost::function<void(std::string, std::string)> fun) {
			key_type r(new typed_kvp_value(new kvp_storer(fun)));
			return r;
		}
		key_type string_map_path(std::map<std::string, std::string> *val) {
			key_type r(new typed_kvp_value(new map_storer(val)));
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
						core_->register_key(v->parent, v->key_name, v->description.title, v->description.description, v->key->get_default(), v->description.advanced, v->is_sample);
						std::string desc = v->description.description + " parent for this key is found under: " + v->parent + " this is marked as advanced in favor of the parent.";
						core_->register_key(v->path, v->key_name, v->description.title, desc, v->key->get_default(), true, false);
					} else {
						core_->register_key(v->path, v->key_name, v->description.title, v->description.description, v->key->get_default(), v->description.advanced, v->is_sample);
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