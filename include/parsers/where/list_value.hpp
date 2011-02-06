#pragma once

namespace parsers {
	namespace where {
		template<typename THandler>
		struct list_value {
			list_value() {
			}
			list_value(expression_ast<THandler> const& subject) {
				list.push_back(subject);
			}
			void append(expression_ast<THandler> const& subject) {
				list.push_back(subject);
			}
			list_value<THandler>& operator+=(expression_ast<THandler> const& subject) {
				list.push_back(subject);
				return *this;
			}

			typedef std::list<expression_ast<THandler> > list_type;
			list_type list;
		};

	}
}