#pragma once

namespace parsers {
	namespace where {		
	
		template<typename THandler>
		struct variable {
			typedef typename THandler::object_type object_type;
			variable(std::wstring name) : name(name) {}

			bool bind(value_type type, THandler & handler);
			long long get_int(object_type & instance) const;
			std::wstring get_string(object_type & instance) const;

			variable( const variable& other ) : i_fn(other.i_fn), s_fn(other.s_fn), name(other.name) {}
			const variable& operator=( const variable& other ) {
				i_fn = other.i_fn;
				s_fn = other.s_fn;
				name = other.name;
				return *this;
			}


			boost::function<long long(object_type*)> i_fn;
			boost::function<std::wstring(object_type*)> s_fn;
			std::wstring name;
		};



		template<typename THandler>
		bool variable<THandler>::bind(value_type type, THandler & handler) {
			if (type_is_int(type)) {
				i_fn = handler.bind_int(name);
				if (i_fn.empty())
					handler.error(_T("Failed to bind (int) variable: ") + name);
				return !i_fn.empty();
			} else {
				s_fn = handler.bind_string(name);
				if (s_fn.empty())
					handler.error(_T("Failed to bind (string) variable: ") + name);
				return !s_fn.empty();;
			}
			handler.error(_T("Failed to bind (unknown) variable: ") + name);
			return false;
		}

		template<typename THandler>
		long long variable<THandler>::get_int(object_type & instance) const {
			if (!i_fn.empty())
				return i_fn(&instance);
			if (!s_fn.empty())
				instance.error(_T("Int variable bound to string: ") + name);
			else
				instance.error(_T("Int variable not bound: ") + name);
			return -1;
		}
		template<typename THandler>
		std::wstring variable<THandler>::get_string(object_type & instance) const {
			if (!s_fn.empty())
				return s_fn(&instance);
			if (!i_fn.empty())
				instance.error(_T("String variable bound to int: ") + name);
			else
				instance.error(_T("String variable not bound: ") + name);
			return _T("");
		}
	}
}