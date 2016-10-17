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

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

#include <pdh/pdh_query.hpp>

namespace PDH {
	PDHQuery::~PDHQuery(void) {
		removeAllCounters();
	}

	void PDHQuery::addCounter(pdh_instance instance) {
		if (instance->has_instances()) {
			BOOST_FOREACH(pdh_instance child, instance->get_instances()) {
				counters_.push_back(boost::make_shared<PDHCounter>(child));
			}
		} else
			counters_.push_back(boost::make_shared<PDHCounter>(instance));
	}

	bool PDHQuery::has_counters() {
		return !counters_.empty();
	}

	void PDHQuery::removeAllCounters() {
		if (hQuery_)
			close();
		counters_.clear();
	}

	void PDHQuery::on_unload() {
		if (hQuery_ == NULL)
			return;
		BOOST_FOREACH(counter_type c, counters_) {
			c->remove();
		}
		pdh_error status = factory::get_impl()->PdhCloseQuery(hQuery_);
		if (status.is_error())
			throw pdh_exception("PdhCloseQuery failed", status);
		hQuery_ = NULL;
	}
	void PDHQuery::on_reload() {
		if (hQuery_ != NULL)
			return;
		pdh_error status = factory::get_impl()->PdhOpenQuery(NULL, 0, &hQuery_);
		if (status.is_error())
			throw pdh_exception("PdhOpenQuery failed", status);
		BOOST_FOREACH(counter_type c, counters_) {
			c->addToQuery(getQueryHandle());
		}
	}

	void PDHQuery::open() {
		if (hQuery_ != NULL)
			throw pdh_exception("query is not null!");
		factory::get_impl()->add_listener(this);
		on_reload();
	}

	void PDHQuery::close() {
		if (hQuery_ == NULL)
			throw pdh_exception("query is null!");
		factory::get_impl()->remove_listener(this);
		on_unload();
		counters_.clear();
	}

	void PDHQuery::gatherData(bool ignore_errors) {
		collect();
		BOOST_FOREACH(counter_type c, counters_) {
			pdh_error status = c->collect();
			if (status.is_invalid_data()) {
				Sleep(1000);
				collect();
				status = c->collect();
			}
			if (status.is_negative_denominator()) {
				Sleep(500);
				collect();
				status = c->collect();
			}
			if (status.is_negative_denominator()) {
				if (!hasDisplayedInvalidCOunter_) {
					hasDisplayedInvalidCOunter_ = true;
					throw pdh_exception(c->getName() + " Negative denominator issue (check FAQ for ways to solve this): ", status);
				}
			} else if (!ignore_errors && status.is_error()) {
				throw pdh_exception(c->getName() + " Failed to poll counter " + c->get_path(), status);
			}
		}
	}
	void PDHQuery::collect() {
		pdh_error status = factory::get_impl()->PdhCollectQueryData(hQuery_);
		if (status.is_error())
			throw pdh_exception("PdhCollectQueryData failed: ", status);
	}

	PDH::PDH_HQUERY PDHQuery::getQueryHandle() const {
		return hQuery_;
	}
}