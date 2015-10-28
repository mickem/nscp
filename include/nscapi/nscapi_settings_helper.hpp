#pragma once

#include <map>
#include <list>
#include <string>

#include <boost/any.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/lexical_cast.hpp>

#include <utf8.hpp>

#include <nscapi/nscapi_core_wrapper.hpp>
#include <settings/client/settings_client_interface.hpp>
#include <nscapi/dll_defines.hpp>

#ifdef WIN32
#pragma warning( disable : 4800 )
#endif

namespace boost {
	template<>
	inline std::string lexical_cast<std::string, boost::filesystem::path>(const boost::filesystem::path& arg) {
		return utf8::cvt<std::string>(arg.string());
	}
}

namespace nscapi {
	namespace settings_helper {
		typedef boost::shared_ptr<settings_impl_interface> settings_impl_interface_ptr;
		inline std::string make_skey(std::string path, std::string key) {
			return path + "." + key;
		}
		class key_interface {
		public:
			virtual NSCAPI::settings_type get_type() const = 0;
			virtual std::string get_default_as_string() const = 0;
			virtual void notify(settings_impl_interface_ptr core_, std::string path, std::string key) const = 0;
			virtual void notify(settings_impl_interface_ptr core_, std::string parent, std::string path, std::string key) const = 0;
		};
		template<class T>
		class typed_key : public key_interface {
		public:
			typed_key(const T& v, bool has_default) : has_default_(has_default), default_value_(boost::any(v)), default_value_as_text_(boost::lexical_cast<std::string>(v)) {}
			typed_key(bool has_default) : has_default_(has_default) {}

			virtual typed_key* default_value(const T& v) {
				default_value_ = boost::any(v);
				default_value_as_text_ = boost::lexical_cast<std::string>(v);
				return this;
			}

			virtual std::string get_default_as_string() const {
				return default_value_as_text_;
			}
			virtual void update_target(T *value) const = 0;
		protected:
			bool has_default_;
			boost::any default_value_;
			std::string default_value_as_text_;
		};

		template<class T>
		class typed_string_value : public typed_key<T> {
		public:
			typed_string_value(const T& v, bool has_default) : typed_key<T>(v, has_default) {}
			//typed_string_value() : typed_key<T>() {}
			virtual NSCAPI::settings_type get_type() const {
				return NSCAPI::key_string;
			}
			virtual void notify(settings_impl_interface_ptr core_, std::string path, std::string key) const {
				std::string dummy("$$DUMMY_VALUE_DO_NOT_USE$$");
				if (typed_key<T>::has_default_)
					dummy = typed_key<T>::default_value_as_text_;
				std::string data = core_->get_string(path, key, dummy);
				if (typed_key<T>::has_default_ || data != dummy) {
					try {
						T value = boost::lexical_cast<T>(data);
						this->update_target(&value);
					} catch (const std::exception &e) {
						core_->err(__FILE__, __LINE__, "Failed to parse key: " + make_skey(path, key) + ": " + utf8::utf8_from_native(e.what()));
					}
				}
			}
			virtual void notify(settings_impl_interface_ptr core_, std::string parent, std::string path, std::string key) const {
				std::string dummy("$$DUMMY_VALUE_DO_NOT_USE$$");
				if (typed_key<T>::has_default_)
					dummy = typed_key<T>::default_value_as_text_;
				std::string data = core_->get_string(parent, key, dummy);
				if (typed_key<T>::has_default_ || data != dummy)
					dummy = data;
				data = core_->get_string(path, key, dummy);
				if (typed_key<T>::has_default_ || data != "$$DUMMY_VALUE_DO_NOT_USE$$") {
					try {
						T value = boost::lexical_cast<T>(data);
						this->update_target(&value);
					} catch (const std::exception &e) {
						core_->err(__FILE__, __LINE__, "Failed to parse key: " + make_skey(path, key) + ": " + utf8::utf8_from_native(e.what()));
					}
				}
			}
		};
		template<class T, class TString>
		class typed_xpath_value : public typed_key<T> {
		public:
			typed_xpath_value(const T& v, bool has_default) : typed_key<T>(v, has_default) {}
			virtual NSCAPI::settings_type get_type() const {
				return NSCAPI::key_string;
			}
			virtual void notify(settings_impl_interface_ptr core_, std::string path, std::string key) const {
				TString dummy(utf8::cvt<TString>("$$DUMMY_VALUE_DO_NOT_USE$$"));
				if (typed_key<T>::has_default_)
					dummy = utf8::cvt<TString>(typed_key<T>::default_value_as_text_);
				TString data = utf8::cvt<TString>(core_->get_string(path, key, utf8::cvt<std::string>(dummy)));
				if (typed_key<T>::has_default_ || data != dummy) {
					try {
						T value = utf8::cvt<TString>(core_->expand_path(utf8::cvt<std::string>(data)));
						this->update_target(&value);
					} catch (const std::exception &e) {
						core_->err(__FILE__, __LINE__, "Failed to parse key: " + make_skey(path, key) + ": " + utf8::utf8_from_native(e.what()));
					}
				}
			}
			virtual void notify(settings_impl_interface_ptr core_, std::string parent, std::string path, std::string key) const {
				TString tag(utf8::cvt<TString>("$$DUMMY_VALUE_DO_NOT_USE$$"));
				TString dummy = tag;
				if (typed_key<T>::has_default_)
					dummy = utf8::cvt<TString>(typed_key<T>::default_value_as_text_);
				TString data = utf8::cvt<TString>(core_->get_string(parent, key, utf8::cvt<std::string>(dummy)));
				if (typed_key<T>::has_default_ || data != dummy)
					dummy = data;
				data = utf8::cvt<TString>(core_->get_string(path, key, utf8::cvt<std::string>(dummy)));
				if (typed_key<T>::has_default_ || data != tag) {
					try {
						T value = utf8::cvt<TString>(core_->expand_path(utf8::cvt<std::string>(data)));
						this->update_target(&value);
					} catch (const std::exception &e) {
						core_->err(__FILE__, __LINE__, "Failed to parse key: " + make_skey(path, key) + ": " + utf8::utf8_from_native(e.what()));
					}
				}
			}
		};

		template<class T>
		class typed_int_value : public typed_key<T> {
		public:
			typed_int_value(const T& v, bool has_default) : typed_key<T>(v, has_default), default_value_as_int_(boost::lexical_cast<int>(v)) {}
			typed_key<T>* default_value(const T& v) {
				typed_key<T>::default_value(v);
				default_value_as_int_ = boost::lexical_cast<int>(v);
				return this;
			}
			virtual NSCAPI::settings_type get_type() const {
				return NSCAPI::key_integer;
			}
			virtual void notify(settings_impl_interface_ptr core_, std::string path, std::string key) const {
				int dummy = -1;
				if (typed_key<T>::has_default_)
					dummy = default_value_as_int_;
				int val = core_->get_int(path, key, dummy);
				if (!typed_key<T>::has_default_ && val == dummy) {
					dummy = -2;
					val = core_->get_int(path, key, dummy);
					if (val == dummy)
						return;
				}
				T value = static_cast<T>(val);
				this->update_target(&value);
			}
			virtual void notify(settings_impl_interface_ptr core_, std::string parent, std::string path, std::string key) const {
				if (typed_key<T>::has_default_) {
					T default_value = static_cast<T>(core_->get_int(parent, key, default_value_as_int_));
					T value = static_cast<T>(core_->get_int(path, key, default_value));
					this->update_target(&value);
				} else {
					int dummy = -1;
					int defval = core_->get_int(path, key, dummy);
					if (defval == dummy) {
						dummy = -2;
						defval = core_->get_int(path, key, dummy);
					}
					if (defval != dummy) {
						T value = static_cast<T>(core_->get_int(path, key, defval));
						this->update_target(&value);
					}
					dummy = -1;
					int val = core_->get_int(path, key, dummy);
					if (val == dummy) {
						dummy = -2;
						val = core_->get_int(path, key, dummy);
						if (val == dummy)
							return;
					}
					T value = static_cast<T>(val);
					this->update_target(&value);
				}
			}
		protected:
			int default_value_as_int_;
		};
		template<class T>
		class typed_bool_value : public typed_int_value<T> {
		public:
			typed_bool_value(const T& v, bool has_default) : typed_int_value<T>(v, has_default) {}
			virtual NSCAPI::settings_type get_type() const {
				return NSCAPI::key_bool;
			}
			// TODO: FIXME: Add support for has_default
			virtual void notify(settings_impl_interface_ptr core_, std::string path, std::string key) const {
				if (typed_key<T>::has_default_) {
					T value = static_cast<T>(core_->get_bool(path, key, typed_int_value<T>::default_value_as_int_ == 1));
					this->update_target(&value);
				} else {
					T v1 = static_cast<T>(core_->get_bool(path, key, true));
					T v2 = static_cast<T>(core_->get_bool(path, key, false));
					if (v1 == v2)
						this->update_target(&v1);
				}
			}
			virtual void notify(settings_impl_interface_ptr core_, std::string parent, std::string path, std::string key) const {
				T default_value = static_cast<T>(core_->get_bool(parent, key, typed_int_value<T>::default_value_as_int_ == 1));
				T value = static_cast<T>(core_->get_bool(path, key, default_value));
				this->update_target(&value);
			}
		};

		template<class T, class TBase, class TDefaultT = T>
		class typed_key_value : public TBase {
		public:
			typed_key_value(T* store_to, const TDefaultT& v, bool has_default) : TBase(v, has_default), store_to_(store_to) {}

			virtual void update_target(T *value) const {
				if (store_to_)
					*store_to_ = *value;
			}
		protected:
			T* store_to_;
		};

		template<class T, class V, class TBase>
		class typed_key_entry_in_vector : public TBase {
		public:
			typed_key_entry_in_vector(V* store_to, typename V::key_type key, const T& v, bool has_default) : TBase(v, has_default), store_to_(store_to), key_(key) {}

			virtual void update_target(T *value) const {
				if (store_to_)
					(*store_to_)[key_] = *value;
			}
		protected:
			V* store_to_;
			typename V::key_type key_;
		};

		template<class T, class TBase>
		class typed_key_fun : public TBase {
		public:
			typed_key_fun(boost::function<void(T)> callback, const T& v, bool has_default) : TBase(v, has_default), callback_(callback) {}

			virtual void update_target(T *value) const {
				callback_(*value);
			}
		protected:
			boost::function<void(T)> callback_;
		};

		template<typename T>
		boost::shared_ptr<typed_key_entry_in_vector<std::string, T, typed_string_value<std::string> > > string_vector_key(T *val, typename T::key_type key, std::string def) {
			boost::shared_ptr<typed_key_entry_in_vector<std::string, T, typed_string_value<std::string> > > r(new typed_key_entry_in_vector<std::string, T, typed_string_value<std::string> >(val, key, def, true));
			return r;
		}

		typedef typed_key_value<std::string, typed_string_value<std::string> > string_key_type;
		typedef typed_key_value<std::string, typed_xpath_value<std::string, std::string> > path_key_type;
		typedef typed_key_value<boost::filesystem::path, typed_xpath_value<boost::filesystem::path, std::string> > real_path_key_type;
		typedef typed_key_value<unsigned int, typed_int_value<unsigned int> > uint_key_type;
		typedef typed_key_value<int, typed_int_value<int> > int_key_type;
		typedef typed_key_value<std::size_t, typed_int_value<std::size_t> > size_key_type;
		typedef typed_key_value<bool, typed_bool_value<bool> > bool_key_type;

		NSCAPI_EXPORT boost::shared_ptr<string_key_type> string_key(std::string *val, std::string def);
		NSCAPI_EXPORT boost::shared_ptr<string_key_type> string_key(std::string *val);
		NSCAPI_EXPORT boost::shared_ptr<int_key_type> int_key(int *val, int def = 0);
		NSCAPI_EXPORT boost::shared_ptr<size_key_type> size_key(std::size_t *val, std::size_t def = 0);
		NSCAPI_EXPORT boost::shared_ptr<uint_key_type> uint_key(unsigned int *val, unsigned int def);
		NSCAPI_EXPORT boost::shared_ptr<uint_key_type> uint_key(unsigned int *val);
		NSCAPI_EXPORT boost::shared_ptr<bool_key_type> bool_key(bool *val, bool def);
		NSCAPI_EXPORT boost::shared_ptr<bool_key_type> bool_key(bool *val);
		NSCAPI_EXPORT boost::shared_ptr<path_key_type> path_key(std::string *val, std::string def);
		NSCAPI_EXPORT boost::shared_ptr<path_key_type> path_key(std::string *val);
		NSCAPI_EXPORT boost::shared_ptr<real_path_key_type> path_key(boost::filesystem::path *val, std::string def);
		NSCAPI_EXPORT boost::shared_ptr<real_path_key_type> path_key(boost::filesystem::path *val);

		template<class T>
		boost::shared_ptr<typed_key_fun<T, typed_int_value<T> > > int_fun_key(boost::function<void(T)> fun, T def) {
			boost::shared_ptr<typed_key_fun<T, typed_int_value<T> > > r(new typed_key_fun<T, typed_int_value<T> >(fun, def, true));
			return r;
		}
		template<class T>
		boost::shared_ptr<typed_key_fun<T, typed_int_value<T> > > int_fun_key(boost::function<void(T)> fun) {
			boost::shared_ptr<typed_key_fun<T, typed_int_value<T> > > r(new typed_key_fun<T, typed_int_value<T> >(fun, 0, false));
			return r;
		}
		template<class T>
		boost::shared_ptr<typed_key_fun<T, typed_bool_value<T> > > bool_fun_key(boost::function<void(T)> fun, T def) {
			boost::shared_ptr<typed_key_fun<T, typed_bool_value<T> > > r(new typed_key_fun<T, typed_bool_value<T> >(fun, def, true));
			return r;
		}
		template<class T>
		boost::shared_ptr<typed_key_fun<T, typed_bool_value<T> > > bool_fun_key(boost::function<void(T)> fun) {
			boost::shared_ptr<typed_key_fun<T, typed_bool_value<T> > > r(new typed_key_fun<T, typed_bool_value<T> >(fun, 0, false));
			return r;
		}
		template<class T>
		boost::shared_ptr<typed_key_fun<T, typed_string_value<T> > > string_fun_key(boost::function<void(T)> fun, T def) {
			boost::shared_ptr<typed_key_fun<T, typed_string_value<T> > > r(new typed_key_fun<T, typed_string_value<T> >(fun, def, true));
			return r;
		}
		template<class T>
		boost::shared_ptr<typed_key_fun<T, typed_string_value<T> > > string_fun_key(boost::function<void(T)> fun) {
			boost::shared_ptr<typed_key_fun<T, typed_string_value<T> > > r(new typed_key_fun<T, typed_string_value<T> >(fun, T(), false));
			return r;
		}
		template<class T>
		boost::shared_ptr<typed_key_fun<T, typed_xpath_value<T, std::string> > > path_fun_key(boost::function<void(T)> fun, T def) {
			boost::shared_ptr<typed_key_fun<T, typed_xpath_value<T, std::string> > > r(new typed_key_fun<T, typed_xpath_value<T, std::string> >(fun, def, true));
			return r;
		}
		template<class T>
		boost::shared_ptr<typed_key_fun<T, typed_xpath_value<T, std::string> > > path_fun_key(boost::function<void(T)> fun) {
			boost::shared_ptr<typed_key_fun<T, typed_xpath_value<T, std::string> > > r(new typed_key_fun<T, typed_xpath_value<T, std::string> >(fun, T(), false));
			return r;
		}

		class path_interface {
		public:
			virtual void notify(settings_impl_interface_ptr core_, std::string path) const = 0;
		};

		template<class T = std::string>
		class typed_path_map : public path_interface {
		public:
			typedef std::map<T, T> map_type;
			typed_path_map(map_type* store_to) : store_to_(store_to) {}

			virtual void notify(settings_impl_interface_ptr core_, std::string path) const {
				if (store_to_) {
					std::list<std::string> list = core_->get_keys(path);
					map_type result;
					BOOST_FOREACH(std::string key, list) {
						result[utf8::cvt<T>(key)] = utf8::cvt<T>(core_->get_string(path, key, ""));
					}
					*store_to_ = result;
				}
			}

		protected:
			map_type* store_to_;
		};

		template<class T>
		class typed_path_fun_value : public path_interface {
		public:
			typed_path_fun_value(boost::function<void(T, T)> callback) : callback_(callback) {}

			virtual void notify(settings_impl_interface_ptr core_, std::string path) const {
				if (callback_) {
					std::list<std::string> list = core_->get_keys(path);
					BOOST_FOREACH(std::string key, list) {
						std::string val = core_->get_string(path, key, "");
						callback_(utf8::cvt<T>(key), utf8::cvt<T>(val));
					}
					list = core_->get_sections(path);
					BOOST_FOREACH(std::string key, list) {
						callback_(utf8::cvt<T>(key), T());
					}
				}
			}

		protected:
			boost::function<void(T, T)> callback_;
		};

		NSCAPI_EXPORT boost::shared_ptr<typed_path_fun_value<std::string> > fun_values_path(boost::function<void(std::string, std::string)> fun);
		NSCAPI_EXPORT boost::shared_ptr<typed_path_map<std::string> > string_map_path(std::map<std::string, std::string> *val);

		struct description_container {
			std::string title;
			std::string description;
			bool advanced;
			description_container() : advanced(false) {}

			description_container(std::string title, std::string description, bool advanced)
				: title(title)
				, description(description)
				, advanced(advanced) {}
			description_container(std::string title, std::string description)
				: title(title)
				, description(description)
				, advanced(false) {}

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
			std::string path;
			std::string key_name;
			boost::shared_ptr<key_interface> key;
			description_container description;
			std::string parent;
			bool is_sample;

			key_info(std::string path_, std::string key_name_, boost::shared_ptr<key_interface> key, description_container description_)
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
		struct path_info {
			std::string path_name;
			boost::shared_ptr<path_interface> path;
			description_container description;
			description_container subkey_description;
			bool is_sample;

			path_info(std::string path_name, description_container description) : path_name(path_name), description(description), is_sample(false) {}
			//			path_info(std::string path_name, description_container description) : path_name(path_name), description(description), subkey_description(subkey_description), is_sample(false) {}
			//			path_info(std::string path_name, boost::shared_ptr<path_interface> path, description_container description) : path_name(path_name), path(path), description(description), subkey_description(subkey_description), is_sample(false) {}
			path_info(std::string path_name, boost::shared_ptr<path_interface> path, description_container description, description_container subkey_description) : path_name(path_name), path(path), description(description), subkey_description(subkey_description), is_sample(false) {}

			path_info(const path_info& obj) : path_name(obj.path_name), path(obj.path), description(obj.description), is_sample(obj.is_sample) {}
			virtual path_info& operator=(const path_info& obj) {
				path_name = obj.path_name;
				path = obj.path;
				description = obj.description;
				subkey_description = obj.subkey_description;
				is_sample = obj.is_sample;
				return *this;
			}
		};

		class settings_registry;
		class NSCAPI_EXPORT settings_paths_easy_init {
		public:
			settings_paths_easy_init(settings_registry* owner) : owner(owner), is_sample(false) {}
			settings_paths_easy_init(std::string path, settings_registry* owner) : path_(path), owner(owner), is_sample(false) {}
			settings_paths_easy_init(std::string path, settings_registry* owner, bool is_sample) : path_(path), owner(owner), is_sample(is_sample) {}

			settings_paths_easy_init& operator()(boost::shared_ptr<path_interface> value, std::string title, std::string description, std::string subkeytitle, std::string subkeydescription) {
				boost::shared_ptr<path_info> d(new path_info(path_, value, description_container(title, description), description_container(subkeytitle, subkeydescription)));
				add(d);
				return *this;
			}
			settings_paths_easy_init& operator()(std::string title, std::string description) {
				boost::shared_ptr<path_info> d(new path_info(path_, description_container(title, description)));
				add(d);
				return *this;
			}
			settings_paths_easy_init& operator()(std::string path, std::string title, std::string description) {
				if (!path_.empty())
					path = path_ + "/" + path;
				boost::shared_ptr<path_info> d(new path_info(path, description_container(title, description)));
				add(d);
				return *this;
			}
			settings_paths_easy_init& operator()(std::string path, boost::shared_ptr<path_interface> value, std::string title, std::string description, std::string subkeytitle, std::string subkeydescription) {
				if (!path_.empty())
					path = path_ + "/" + path;
				boost::shared_ptr<path_info> d(new path_info(path, value, description_container(title, description), description_container(subkeytitle, subkeydescription)));
				add(d);
				return *this;
			}

			void add(boost::shared_ptr<path_info> d);

		private:
			std::string path_;
			settings_registry* owner;
			bool is_sample;
		};

		class NSCAPI_EXPORT settings_keys_easy_init {
		public:
			settings_keys_easy_init(settings_registry* owner_) : owner(owner_), is_sample(false) {}
			settings_keys_easy_init(std::string path, settings_registry* owner_) : owner(owner_), path_(path), is_sample(false) {}
			settings_keys_easy_init(std::string path, settings_registry* owner_, bool is_sample) : owner(owner_), path_(path), is_sample(is_sample) {}
			settings_keys_easy_init(std::string path, std::string parent, settings_registry* owner_) : owner(owner_), path_(path), parent_(parent), is_sample(false) {}
			settings_keys_easy_init(std::string path, std::string parent, settings_registry* owner_, bool is_sample) : owner(owner_), path_(path), parent_(parent), is_sample(is_sample) {}

			virtual ~settings_keys_easy_init() {}

			settings_keys_easy_init& operator()(std::string path, std::string key_name, boost::shared_ptr<key_interface> value, std::string title, std::string description, bool advanced = false) {
				boost::shared_ptr<key_info> d(new key_info(path, key_name, value, description_container(title, description, advanced)));
				if (!parent_.empty())
					d->set_parent(parent_);
				add(d);
				return *this;
			}

			settings_keys_easy_init& operator()(std::string key_name, boost::shared_ptr<key_interface> value, std::string title, std::string description, bool advanced = false) {
				boost::shared_ptr<key_info> d(new key_info(path_, key_name, value, description_container(title, description, advanced)));
				if (!parent_.empty())
					d->set_parent(parent_);
				add(d);
				return *this;
			}

			void add(boost::shared_ptr<key_info> d);

		private:
			settings_registry* owner;
			std::string path_;
			std::string parent_;
			bool is_sample;
		};

		class path_extension {
		public:
			path_extension(settings_registry * owner, std::string path) : owner_(owner), path_(path), is_sample(false) {}

			settings_keys_easy_init add_key_to_path(std::string path) {
				return settings_keys_easy_init(get_path(path), owner_, is_sample);
			}
			settings_keys_easy_init add_key() {
				return settings_keys_easy_init(path_, owner_, is_sample);
			}
			settings_paths_easy_init add_path(std::string path = "") {
				return settings_paths_easy_init(get_path(path), owner_, is_sample);
			}
			inline std::string get_path(std::string path) {
				if (!path.empty())
					return path_ + "/" + path;
				return path_;
			}
			void set_sample() {
				is_sample = true;
			}
		private:
			settings_registry * owner_;
			std::string path_;
			bool is_sample;
		};
		class alias_extension {
		public:
			alias_extension(settings_registry * owner, std::string alias) : owner_(owner), alias_(alias) {}
			alias_extension(const alias_extension &other) : owner_(other.owner_), alias_(other.alias_), parent_(other.parent_) {}
			alias_extension& operator = (const alias_extension& other) {
				owner_ = other.owner_;
				alias_ = other.alias_;
				parent_ = other.parent_;
				return *this;
			}

			settings_keys_easy_init add_key_to_path(std::string path) {
				return settings_keys_easy_init(get_path(path), parent_, owner_);
			}
			settings_paths_easy_init add_path(std::string path) {
				return settings_paths_easy_init(get_path(path), owner_);
			}
			inline std::string get_path(std::string path = "") {
				if (path.empty())
					return "/" + alias_;
				return path + "/" + alias_;
			}

			settings_keys_easy_init add_key_to_settings(std::string path = "") {
				return settings_keys_easy_init(get_settings_path(path), parent_, owner_);
			}
			settings_paths_easy_init add_path_to_settings(std::string path = "") {
				return settings_paths_easy_init(get_settings_path(path), owner_);
			}
			inline std::string get_settings_path(std::string path) {
				if (path.empty())
					return "/settings/" + alias_;
				return "/settings/" + alias_ + "/" + path;
			}

			alias_extension add_parent(std::string parent_path) {
				set_parent_path(parent_path);
				return *this;
			}

			static std::string get_alias(std::string cur, std::string def) {
				if (cur.empty())
					return def;
				else
					return cur;
			}
			static std::string get_alias(std::string prefix, std::string cur, std::string def) {
				if (!prefix.empty())
					prefix += "/";
				if (cur.empty())
					return prefix + def;
				else
					return prefix + cur;
			}
			void set_alias(std::string cur, std::string def) {
				alias_ = get_alias(cur, def);
			}
			void set_alias(std::string prefix, std::string cur, std::string def) {
				alias_ = get_alias(prefix, cur, def);
			}
			void set_parent_path(std::string parent) {
				parent_ = parent;
			}

		private:
			settings_registry * owner_;
			std::string alias_;
			std::string parent_;
		};

		class settings_registry {
			typedef std::list<boost::shared_ptr<key_info> > key_list;
			typedef std::list<boost::shared_ptr<path_info> > path_list;
			key_list keys_;
			path_list paths_;
			settings_impl_interface_ptr core_;
			std::string alias_;
		public:
			settings_registry(settings_impl_interface_ptr core) : core_(core) {}
			virtual ~settings_registry() {}
			void add(boost::shared_ptr<key_info> info) {
				keys_.push_back(info);
			}
			void add(boost::shared_ptr<path_info> info) {
				paths_.push_back(info);
			}

			settings_keys_easy_init add_key() {
				return settings_keys_easy_init(this);
			}
			settings_keys_easy_init add_key_to_path(std::string path) {
				return settings_keys_easy_init(path, this);
			}
			settings_keys_easy_init add_key_to_settings(std::string path) {
				return settings_keys_easy_init("/settings/" + path, this);
			}
			settings_paths_easy_init add_path() {
				return settings_paths_easy_init(this);
			}
			settings_paths_easy_init add_path_to_settings() {
				return settings_paths_easy_init("/settings", this);
			}

			void set_alias(std::string cur, std::string def) {
				alias_ = alias_extension::get_alias(cur, def);
			}
			void set_alias(std::string prefix, std::string cur, std::string def) {
				alias_ = alias_extension::get_alias(prefix, cur, def);
			}
			void set_alias(std::string alias) {
				alias_ = alias;
			}
			alias_extension alias() {
				return alias_extension(this, alias_);
			}
			alias_extension alias(std::string alias) {
				return alias_extension(this, alias);
			}
			alias_extension alias(std::string cur, std::string def) {
				return alias_extension(this, alias_extension::get_alias(cur, def));
			}
			alias_extension alias(std::string prefix, std::string cur, std::string def) {
				return alias_extension(this, alias_extension::get_alias(prefix, cur, def));
			}

			path_extension path(std::string path) {
				return path_extension(this, path);
			}

			void set_static_key(std::string path, std::string key, std::string value) {
				core_->set_string(path, key, value);
			}
			std::string get_static_string(std::string path, std::string key, std::string def_value) {
				return core_->get_string(path, key, def_value);
			}

			void register_key(std::string path, std::string key, int type, std::string title, std::string description, std::string defaultValue, bool advanced = false) {
				core_->register_key(path, key, type, title, description, defaultValue, advanced, false);
			}
			void register_all() {
				BOOST_FOREACH(key_list::value_type v, keys_) {
					if (v->key) {
						if (v->has_parent()) {
							core_->register_key(v->parent, v->key_name, v->key->get_type(), v->description.title, v->description.description, v->key->get_default_as_string(), v->description.advanced, v->is_sample);
							std::string desc = v->description.description + " parent for this key is found under: " + v->parent + " this is marked as advanced in favor of the parent.";
							core_->register_key(v->path, v->key_name, v->key->get_type(), v->description.title, desc, v->key->get_default_as_string(), true, false);
						} else {
							core_->register_key(v->path, v->key_name, v->key->get_type(), v->description.title, v->description.description, v->key->get_default_as_string(), v->description.advanced, v->is_sample);
						}
					}
				}
				BOOST_FOREACH(path_list::value_type v, paths_) {
					core_->register_path(v->path_name, v->description.title, v->description.description, v->description.advanced, v->is_sample);
					if (!v->subkey_description.title.empty()) {
						BOOST_FOREACH(const std::string &s, core_->get_keys(v->path_name))
							core_->register_key(v->path_name, s, NSCAPI::key_string, v->subkey_description.title, v->subkey_description.description, "", v->description.advanced, v->is_sample);
					}
				}
			}
			void clear() {
				keys_.clear();
				paths_.clear();
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
					} catch (const std::exception &e) {
						core_->err(__FILE__, __LINE__, "Failed to notify " + v->key_name + ": " + utf8::utf8_from_native(e.what()));
					} catch (...) {
						core_->err(__FILE__, __LINE__, "Failed to notify " + v->key_name);
					}
				}
				BOOST_FOREACH(path_list::value_type v, paths_) {
					try {
						if (v->path)
							v->path->notify(core_, v->path_name);
					} catch (const std::exception &e) {
						core_->err(__FILE__, __LINE__, "Failed to notify " + v->path_name + ": " + utf8::utf8_from_native(e.what()));
					} catch (...) {
						core_->err(__FILE__, __LINE__, "Failed to notify " + v->path_name);
					}
				}
			}
		};
	}
}