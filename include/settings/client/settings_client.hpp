#pragma once

#include <map>
#include <list>
#include <string>

#include <boost/any.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>

#include <unicode_char.hpp>
#include <strEx.h>

#include <nscapi/nscapi_core_wrapper.hpp>
#include <settings/client/settings_client_interface.hpp>

namespace nscapi {
	namespace settings_helper {

		typedef boost::shared_ptr<settings_impl_interface> settings_impl_interface_ptr;

		class key_interface {
		public:
			virtual NSCAPI::settings_type get_type() const = 0;
			virtual std::wstring get_default_as_string() const = 0;
			virtual void notify(settings_impl_interface_ptr core_, std::wstring path, std::wstring key) const = 0;
			virtual void notify(settings_impl_interface_ptr core_, std::wstring parent, std::wstring path, std::wstring key) const = 0;
		};
		template<class T>
		class typed_key : public key_interface {
		public:
			typed_key(const T& v)  : default_value_(boost::any(v)), default_value_as_text_(boost::lexical_cast<std::wstring>(v)) {}

			virtual typed_key* default_value(const T& v) {
				default_value_ = boost::any(v);
				default_value_as_text_ = boost::lexical_cast<std::wstring>(v);
				return this;
			}

			virtual std::wstring get_default_as_string() const {
				return default_value_as_text_;
			}
			virtual void update_target(T *value) const = 0;
		protected:
			boost::any default_value_;
			std::wstring default_value_as_text_;
		};

		template<class T>
		class typed_string_value : public typed_key<T> {
		public:
			typed_string_value(const T& v) : typed_key<T>(v) {}
			virtual NSCAPI::settings_type get_type() const {
				return NSCAPI::key_string;
			}
			virtual void notify(settings_impl_interface_ptr core_, std::wstring path, std::wstring key) const {
				T value = boost::lexical_cast<T>(core_->get_string(path, key, typed_key<T>::default_value_as_text_));
				update_target(&value);
			}
			virtual void notify(settings_impl_interface_ptr core_, std::wstring parent, std::wstring path, std::wstring key) const {
				std::wstring default_value = core_->get_string(parent, key, typed_key<T>::default_value_as_text_);
				T value = boost::lexical_cast<T>(core_->get_string(path, key, default_value));
				update_target(&value);
			}
		};
		template<class T>
		class typed_path_value : public typed_key<T> {
		public:
			typed_path_value(const T& v) : typed_key<T>(v) {}
			virtual NSCAPI::settings_type get_type() const {
				return NSCAPI::key_string;
			}
			virtual void notify(settings_impl_interface_ptr core_, std::wstring path, std::wstring key) const {
				std::wstring val = core_->get_string(path, key, typed_key<T>::default_value_as_text_);
				T value = boost::lexical_cast<T>(core_->expand_path(val));
				update_target(&value);
			}
			virtual void notify(settings_impl_interface_ptr core_, std::wstring parent, std::wstring path, std::wstring key) const {
				std::wstring def_val = core_->get_string(parent, key, typed_key<T>::default_value_as_text_);
				std::wstring val = core_->get_string(path, key, def_val);
				T value = boost::lexical_cast<T>(core_->expand_path(val));
				update_target(&value);
			}
		};
		template<class T>
		class typed_int_value : public typed_key<T> {
		public:
			typed_int_value(const T& v) : typed_key<T>(v), default_value_as_int_(boost::lexical_cast<int>(v)) {}
			typed_key<T>* default_value(const T& v) {
				typed_key<T>::default_value(v);
				default_value_as_int_ = boost::lexical_cast<int>(v);
				return this;
			}
			virtual NSCAPI::settings_type get_type() const {
				return NSCAPI::key_integer;
			}
			virtual void notify(settings_impl_interface_ptr core_, std::wstring path, std::wstring key) const {
				T value = static_cast<T>(core_->get_int(path, key, default_value_as_int_));
				update_target(&value);
			}
			virtual void notify(settings_impl_interface_ptr core_, std::wstring parent, std::wstring path, std::wstring key) const {
				T default_value = static_cast<T>(core_->get_int(parent, key, default_value_as_int_));
				T value = static_cast<T>(core_->get_int(path, key, default_value));
				update_target(&value);
			}
		protected:
			int default_value_as_int_;
		};
		template<class T>
		class typed_bool_value : public typed_int_value<T> {
		public:
			typed_bool_value(const T& v) : typed_int_value<T>(v) {}
			virtual NSCAPI::settings_type get_type() const {
				return NSCAPI::key_bool;
			}
			virtual void notify(settings_impl_interface_ptr core_, std::wstring path, std::wstring key) const {
				T value = static_cast<T>(core_->get_bool(path, key, typed_int_value<T>::default_value_as_int_==1));
				update_target(&value);
			}
			virtual void notify(settings_impl_interface_ptr core_, std::wstring parent, std::wstring path, std::wstring key) const {
				T default_value = static_cast<T>(core_->get_bool(parent, key, typed_int_value<T>::default_value_as_int_==1));
				T value = static_cast<T>(core_->get_bool(path, key, default_value));
				update_target(&value);
			}
		};

		template<class T, class TBase>
		class typed_key_value : public TBase {
		public:
			typed_key_value(T* store_to, const T& v) : TBase(v), store_to_(store_to) {}

			virtual void update_target(T *value) const {
				if (store_to_)
					*store_to_ = *value;
			}
		protected:
			T* store_to_;
		};

		template<class T, class TBase>
		class typed_key_fun : public TBase {
		public:
			typed_key_fun(boost::function<void (T)> callback, const T& v): TBase(v), callback_(callback) {}

			virtual void update_target(T *value) const {
				callback_(*value);
			}
		protected:
			boost::function<void (T)> callback_;
		};


		typedef typed_key_value<std::wstring, typed_string_value<std::wstring> > wstring_key_type;
		typedef typed_key_value<std::string, typed_string_value<std::string> > string_key_type;
		typedef typed_key_value<std::wstring, typed_path_value<std::wstring> > wpath_key_type;
		typedef typed_key_value<unsigned int, typed_int_value<unsigned int> > uint_key_type;
		typedef typed_key_value<int, typed_int_value<int> > int_key_type;
		typedef typed_key_value<bool, typed_bool_value<bool> > bool_key_type;

		wstring_key_type* wstring_key(std::wstring *val, std::wstring def = _T(""));
		string_key_type* string_key(std::string *val, std::string def = "");
		int_key_type* int_key(int *val, int def = 0);
		uint_key_type* uint_key(unsigned int *val, unsigned int def = 0);
		bool_key_type* bool_key(bool *val, bool def = false);
		wpath_key_type* wpath_key(std::wstring *val, std::wstring def = _T(""));

		template<class T>
		typed_key_fun<T, typed_int_value<T> >* int_fun_key(boost::function<void (T)> fun, T def) {
			typed_key_fun<T, typed_int_value<T> >* r = new typed_key_fun<T, typed_int_value<T> >(fun, def);
			return r;
		}
		template<class T>
		typed_key_fun<T, typed_bool_value<T> >* bool_fun_key(boost::function<void (T)> fun, T def) {
			typed_key_fun<T, typed_bool_value<T> >* r = new typed_key_fun<T, typed_bool_value<T> >(fun, def);
			return r;
		}
		template<class T>
		typed_key_fun<T, typed_string_value<T> >* string_fun_key(boost::function<void (T)> fun, T def) {
			typed_key_fun<T, typed_string_value<T> >* r = new typed_key_fun<T, typed_string_value<T> >(fun, def);
			return r;
		}

		class path_interface {
		public:
			virtual void notify(settings_impl_interface_ptr core_, std::wstring path) const = 0;
		};

		template<class T=std::map<std::wstring,std::wstring> >
		class typed_path_map : public path_interface {
		public:
			typed_path_map(T* store_to)  : store_to_(store_to) {}

			virtual void notify(settings_impl_interface_ptr core_, std::wstring path) const {
				if (store_to_) {
					std::list<std::wstring> list = core_->get_keys(path);
					T result;
					BOOST_FOREACH(std::wstring key, list) {
						result[key] = core_->get_string(path, key, _T(""));
					}
					*store_to_ = result;
				}
			}

		protected:
			T* store_to_;
		};

		class typed_path_list : public path_interface {
		public:
			typed_path_list(std::list<std::wstring>* store_to)  : store_to_(store_to) {}

			virtual void notify(settings_impl_interface_ptr core_, std::wstring path) const {
				if (store_to_) {
					*store_to_ = core_->get_keys(path);
				}
			}

		protected:
			std::list<std::wstring>* store_to_;
		};

		class typed_path_fun_value : public path_interface {
		public:
			typed_path_fun_value(boost::function<void (std::wstring, std::wstring)> callback) : callback_(callback) {}

			virtual void notify(settings_impl_interface_ptr core_, std::wstring path) const {
				if (callback_) {
					std::list<std::wstring> list = core_->get_keys(path);
					BOOST_FOREACH(std::wstring key, list) {
						std::wstring val = core_->get_string(path, key, _T(""));
						callback_(key, val);
					}
				}
			}

		protected:
			boost::function<void (std::wstring, std::wstring)> callback_;
		};

		class typed_path_fun : public path_interface {
		public:
			typed_path_fun(boost::function<void (std::wstring)> callback) : callback_(callback) {}

			virtual void notify(settings_impl_interface_ptr core_, std::wstring path) const {
				if (callback_) {
					std::list<std::wstring> list = core_->get_keys(path);
					BOOST_FOREACH(std::wstring key, list) {
						callback_(key);
					}
				}
			}

		protected:
			boost::function<void (std::wstring)> callback_;
		};


		typed_path_fun* fun_path(boost::function<void (std::wstring)> fun);
		typed_path_fun_value* fun_values_path(boost::function<void (std::wstring,std::wstring)> fun);
		typed_path_map<>* wstring_map_path(std::map<std::wstring,std::wstring> *val);
		typed_path_list* wstring_list_path(std::list<std::wstring> *val);

		struct description_container {
			std::wstring title;
			std::wstring description;
			bool advanced;

			description_container(std::wstring title, std::wstring description, bool advanced) 
				: title(title)
				, description(description)
				, advanced(advanced)
			{}
			description_container(std::wstring title, std::wstring description) 
				: title(title)
				, description(description)
				, advanced(false)
			{}

			description_container(const description_container& obj) {
				title = obj.title;
				description = obj.description;
				advanced = obj.advanced;
			}
			description_container& operator=(const description_container& obj) {
				title = obj.title;
				description = obj.description;
				advanced = obj.advanced;
				return *this;
			}

		};

		struct key_info {
			std::wstring path;
			std::wstring key_name;
			std::wstring parent;

			boost::shared_ptr<key_interface> key;
			description_container description;

			key_info(std::wstring path_, std::wstring key_name_, key_interface* key, description_container description_) 
				: path(path_)
				, key_name(key_name_)
				, key(key)
				, description(description_)
			{}
			key_info(const key_info& obj) : path(obj.path), key_name(obj.key_name), key(obj.key), description(obj.description), parent(obj.parent) {}
			virtual key_info& operator=(const key_info& obj) {
				path = obj.path;
				key_name = obj.key_name;
				key = obj.key;
				description = obj.description;
				parent = obj.parent;
				return *this;
			}
			void set_parent(std::wstring parent_) {
				parent = parent_;
			}
			bool has_parent() const {
				return !parent.empty();
			}
			std::wstring get_parent() const {
				return parent;
			}
		};
		struct path_info {
			std::wstring path_name;
			description_container description;
			boost::shared_ptr<path_interface> path;

			path_info(std::wstring path_name, description_container description_) : path_name(path_name), description(description_) {}
			path_info(std::wstring path_name, path_interface* path, description_container description_) : path_name(path_name), path(path), description(description_) {}

			path_info(const path_info& obj) : path_name(obj.path_name), description(obj.description), path(obj.path) {}
			virtual path_info& operator=(const path_info& obj) {
				path_name = obj.path_name;
				path = obj.path;
				description = obj.description;
				return *this;
			}

		};

		class settings_registry;
		class settings_paths_easy_init {
		public:
			settings_paths_easy_init(settings_registry* owner) : owner(owner) {}
			settings_paths_easy_init(std::wstring path, settings_registry* owner) : path_(path), owner(owner) {}

			settings_paths_easy_init& operator()(path_interface *value, std::wstring title, std::wstring description) {
				boost::shared_ptr<path_info> d(new path_info(path_, value, description_container(title, description)));
				add(d);
				return *this;
			}
			settings_paths_easy_init& operator()(std::wstring title, std::wstring description) {
				boost::shared_ptr<path_info> d(new path_info(path_, description_container(title, description)));
				add(d);
				return *this;
			}
			settings_paths_easy_init& operator()(std::wstring path, std::wstring title, std::wstring description) {
				if (!path_.empty())
					path = path_ + _T("/") + path;
				boost::shared_ptr<path_info> d(new path_info(path, description_container(title, description)));
				add(d);
				return *this;
			}
			settings_paths_easy_init& operator()(std::wstring path, path_interface *value, std::wstring title, std::wstring description) {
				if (!path_.empty())
					path = path_ + _T("/") + path;
				boost::shared_ptr<path_info> d(new path_info(path, value, description_container(title, description)));
				add(d);
				return *this;
			}

			void add(boost::shared_ptr<path_info> d);

		private:
			settings_registry* owner;
			std::wstring path_;
		};	

		class settings_keys_easy_init {
		public:
			settings_keys_easy_init(settings_registry* owner_) : owner(owner_) {}
			settings_keys_easy_init(std::wstring path, settings_registry* owner_) : owner(owner_), path_(path) {}
			settings_keys_easy_init(std::wstring path, std::wstring parent, settings_registry* owner_) : owner(owner_), path_(path), parent_(parent) {}

			settings_keys_easy_init& operator()(std::wstring path, std::wstring key_name, key_interface *value, std::wstring title, std::wstring description) {
				boost::shared_ptr<key_info> d(new key_info(path, key_name, value, description_container(title, description)));
				if (!parent_.empty())
					d->set_parent(parent_);
				add(d);
				return *this;
			}

			settings_keys_easy_init& operator()(std::wstring key_name, key_interface* value, std::wstring title, std::wstring description) {
				boost::shared_ptr<key_info> d(new key_info(path_, key_name, value, description_container(title, description)));
				if (!parent_.empty())
					d->set_parent(parent_);
				add(d);
				return *this;
			}

			void add(boost::shared_ptr<key_info> d);

		private:
			settings_registry* owner;
			std::wstring path_;
			std::wstring parent_;
		};	


		class path_extension {
		public:
			path_extension(settings_registry * owner, std::wstring path) : owner_(owner), path_(path) {}

			settings_keys_easy_init add_key_to_path(std::wstring path) {
				return settings_keys_easy_init(get_path(path), owner_);
			}
			settings_keys_easy_init add_key() {
				return settings_keys_easy_init(path_, owner_);
			}
			settings_paths_easy_init add_path(std::wstring path = _T("")) {
				return settings_paths_easy_init(get_path(path), owner_);
			}
			inline std::wstring get_path(std::wstring path) {
				if (!path.empty())
					return path_ + _T("/") + path;
				return path_;
			}

		private:
			std::wstring path_;
			settings_registry * owner_;
		};
		class alias_extension {
		public:
			alias_extension(settings_registry * owner, std::wstring alias) : owner_(owner), alias_(alias) {}
			alias_extension(const alias_extension &other) : owner_(other.owner_), alias_(other.alias_), parent_(other.parent_) {}
			alias_extension& operator = (const alias_extension& other) {
				owner_ = other.owner_;
				alias_ = other.alias_;
				parent_ = other.parent_;
				return *this;
			}

			settings_keys_easy_init add_key_to_path(std::wstring path) {
				return settings_keys_easy_init(get_path(path), parent_, owner_);
			}
			settings_paths_easy_init add_path(std::wstring path) {
				return settings_paths_easy_init(get_path(path), owner_);
			}
			inline std::wstring get_path(std::wstring path) {
				if (path.empty())
					return _T("/") + alias_;
				return path + _T("/") + alias_;
			}


			settings_keys_easy_init add_key_to_settings(std::wstring path = _T("")) {
				return settings_keys_easy_init(get_settings_path(path), parent_, owner_);
			}
			settings_paths_easy_init add_path_to_settings(std::wstring path = _T("")) {
				return settings_paths_easy_init(get_settings_path(path), owner_);
			}
			inline std::wstring get_settings_path(std::wstring path) {
				if (path.empty())
					return _T("/settings/") + alias_;
				return _T("/settings/") + alias_ + _T("/") + path;
			}

			alias_extension add_parent(std::wstring parent_path) {
				set_parent_path(parent_path);
				return *this;
			}


			static std::wstring get_alias(std::wstring cur, std::wstring def) {
				if (cur.empty())
					return def;
				else
					return cur;
			}
			static std::wstring get_alias(std::wstring prefix, std::wstring cur, std::wstring def) {
				if (!prefix.empty())
					prefix += _T("/");
				if (cur.empty())
					return prefix + def;
				else
					return prefix + cur;
			}
			void set_alias(std::wstring cur, std::wstring def) {
				alias_ = get_alias(cur, def);
			}
			void set_alias(std::wstring prefix, std::wstring cur, std::wstring def) {
				alias_ = get_alias(prefix, cur, def);
			}
			void set_parent_path(std::wstring parent) {
				parent_ = parent;
			}

		private:
			std::wstring alias_;
			settings_registry * owner_;
			std::wstring parent_;
		};

		class settings_registry {
			typedef std::list<boost::shared_ptr<key_info> > key_list;
			typedef std::list<boost::shared_ptr<path_info> > path_list;
			key_list keys_;
			path_list paths_;
			settings_impl_interface_ptr core_;
			std::wstring alias_;
		public:
			settings_registry(settings_impl_interface_ptr core) : core_(core) {}
			void add(boost::shared_ptr<key_info> info) {
				keys_.push_back(info);
			}
			void add(boost::shared_ptr<path_info> info) {
				paths_.push_back(info);
			}

			settings_keys_easy_init add_key() {
				return settings_keys_easy_init(this);
			} 
			settings_keys_easy_init add_key_to_path(std::wstring path) {
				return settings_keys_easy_init(path, this);
			}
			settings_keys_easy_init add_key_to_settings(std::wstring path) {
				return settings_keys_easy_init(_T("/settings/") + path, this);
			}
			settings_paths_easy_init add_path() {
				return settings_paths_easy_init(this);
			}
			settings_paths_easy_init add_path_to_settings() {
				return settings_paths_easy_init(_T("/settings"), this);
			}

			void set_alias(std::wstring cur, std::wstring def) {
				alias_ = alias_extension::get_alias(cur, def);
			}
			void set_alias(std::wstring prefix, std::wstring cur, std::wstring def) {
				alias_ = alias_extension::get_alias(prefix, cur, def);
			}
			void set_alias(std::wstring alias) {
				alias_ = alias;
			}
			alias_extension alias() {
				return alias_extension(this, alias_);
			}
			alias_extension alias(std::wstring alias) {
				return alias_extension(this, alias);
			}
			alias_extension alias(std::wstring cur, std::wstring def) {
				return alias_extension(this, alias_extension::get_alias(cur, def));
			}
			alias_extension alias(std::wstring prefix, std::wstring cur, std::wstring def) {
				return alias_extension(this, alias_extension::get_alias(prefix, cur, def));
			}

			path_extension path(std::wstring path) {
				return path_extension(this, path);
			}
			

			void register_all() {
				BOOST_FOREACH(key_list::value_type v, keys_) {
					if (v->key) {
						//std::wcout << _T("Setting: ") << v->key_name << _T(" ===> ") << v->parent << std::endl;
						std::wstring desc = v->description.description;
						if (v->has_parent()) {
							desc += _T(" Parent element can be found under: ") + v->parent;
							core_->register_key(v->parent, v->key_name, v->key->get_type(), v->description.title, desc, v->key->get_default_as_string(), v->description.advanced);
						}
						core_->register_key(v->path, v->key_name, v->key->get_type(), v->description.title, desc, v->key->get_default_as_string(), v->description.advanced);
					}
				}
				BOOST_FOREACH(path_list::value_type v, paths_) {
					core_->register_path(v->path_name, v->description.title, v->description.description, v->description.advanced);
				}
			}

			void notify() {
				BOOST_FOREACH(key_list::value_type v, keys_) {
					try {
						if (v->key) {
							if (v->has_parent())
								v->key->notify(core_, v->parent, v->path, v->key_name);
							else
								v->key->notify(core_, v->path, v->key_name);
						}
					} catch (const nscapi::nscapi_exception &e) {
						core_->err(__FILE__, __LINE__, _T("Failed to notify ") + v->key_name + _T(": ") + e.msg_);
					} catch (const std::exception &e) {
						core_->err(__FILE__, __LINE__, _T("Failed to notify ") + v->key_name + _T(": ") + utf8::cvt<std::wstring>(e.what()));
					} catch (...) {
						core_->err(__FILE__, __LINE__, _T("Failed to notify: ") + v->key_name);
					}
				}
				BOOST_FOREACH(path_list::value_type v, paths_) {
					try {
						if (v->path)
							v->path->notify(core_, v->path_name);
					} catch (const nscapi::nscapi_exception &e) {
						core_->err(__FILE__, __LINE__, _T("Failed to notify ") + v->path_name + _T(": ") + e.msg_);
					} catch (const std::exception &e) {
						core_->err(__FILE__, __LINE__, _T("Failed to notify ") + v->path_name + _T(": ") + utf8::cvt<std::wstring>(e.what()));
					} catch (...) {
						core_->err(__FILE__, __LINE__, _T("Failed to notify: ") + v->path_name);
					}
				}

			}
		};
	}
}