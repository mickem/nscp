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

#include "nrdp.hpp"

#include <error.hpp>
#include <tinyxml2.h>
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
		child->InsertEndChild(doc.NewText(strEx::s::xtos(item.result).c_str()));
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
}