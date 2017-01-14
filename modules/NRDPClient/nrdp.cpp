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

#include "nrdp.hpp"

#include <str/xtos.hpp>
#include <utf8.hpp>

#include <tinyxml2.h>

#include <boost/foreach.hpp>

namespace nrdp {
	void data::add_host(std::string host, NSCAPI::nagiosReturn result, std::string message) {
		item_type item;
		item.type = type_host;
		item.host = host;
		item.result = result;
		item.message = message;
		items.push_back(item);
	}

	void data::add_service(std::string host, std::string service, NSCAPI::nagiosReturn result, std::string message) {
		item_type item;
		item.type = type_service;
		item.host = host;
		item.service = service;
		item.result = result;
		item.message = message;
		items.push_back(item);
	}

	void data::add_command(std::string command, std::list<std::string> args) {
		item_type item;
		item.type = type_command;
		item.message = command;
		items.push_back(item);
	}

	void render_item(tinyxml2::XMLDocument &doc, tinyxml2::XMLNode* parent, const nrdp::data::item_type &item) {
		tinyxml2::XMLNode* node = parent->InsertEndChild(doc.NewElement("checkresult"));
		tinyxml2::XMLNode* child;
		if (item.type == nrdp::data::type_service)
			node->ToElement()->SetAttribute("type", "service");
		else if (item.type == nrdp::data::type_host)
			node->ToElement()->SetAttribute("type", "host");
		child = node->InsertEndChild(doc.NewElement("hostname"));
		child->InsertEndChild(doc.NewText(item.host.c_str()));
		if (item.type == nrdp::data::type_service) {
			child = node->InsertEndChild(doc.NewElement("servicename"));
			child->InsertEndChild(doc.NewText(item.service.c_str()));
		}
		child = node->InsertEndChild(doc.NewElement("state"));
		child->InsertEndChild(doc.NewText(str::xtos(item.result).c_str()));
		child = node->InsertEndChild(doc.NewElement("output"));
		child->InsertEndChild(doc.NewText(item.message.c_str()));
	}

	std::string data::render_request() const {
		tinyxml2::XMLDocument doc;
		doc.InsertEndChild(doc.NewDeclaration());
		tinyxml2::XMLNode* element = doc.InsertEndChild(doc.NewElement("checkresults"));
		BOOST_FOREACH(const item_type &item, items) {
			render_item(doc, element, item);
		}
		tinyxml2::XMLPrinter printer;
		doc.Print(&printer);
		return printer.CStr();
	}

	boost::tuple<int, std::string> data::parse_response(const std::string &str) {
		tinyxml2::XMLDocument doc;
		doc.Parse(str.c_str(), str.length());
		tinyxml2::XMLNode* node = doc.FirstChildElement("result");
		if (node == NULL) {
			return boost::make_tuple(-1, "Invalid response from server");
		}
		tinyxml2::XMLNode* nStatus = node->FirstChildElement("status");
		tinyxml2::XMLNode* nError = node->FirstChildElement("message");
		if (nStatus == NULL || nError == NULL) {
			return boost::make_tuple(-1, "Invalid response from server");
		}
		tinyxml2::XMLNode* tnStatus = nStatus->FirstChild();
		tinyxml2::XMLNode* tnError = nError->FirstChild();

		std::string status = tnStatus->Value();
		std::string error = tnError->Value();
		return boost::make_tuple(str::stox<int>(status, -1), error);
	}
}