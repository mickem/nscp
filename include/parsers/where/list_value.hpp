#pragma once

namespace parsers {
	namespace where {
		struct list_value {
			list_value() {
			}
			list_value(expression_ast const& subject) {
				list.push_back(subject);
			}
			void append(expression_ast const& subject) {
				list.push_back(subject);
			}
			list_value& operator+=(expression_ast const& subject) {
				list.push_back(subject);
				return *this;
			}

			typedef std::list<expression_ast> list_type;
			list_type list;
		};

	}
}