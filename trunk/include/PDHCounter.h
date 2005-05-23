#pragma once

#include <list>
#include <pdh.h>
#include <pdhmsg.h>
#include <assert.h>


namespace PDH {
	class PDHException {
	public:
		std::string str_;
		int errCode_;
	public:
		PDHException(std::string str, int errCode = 0) : str_(str), errCode_(errCode) {
		}
	};

	class PDHCounter;
	class PDHCounterListener {
	public:
		virtual void collect(const PDHCounter &counter) = 0;
		virtual void attach(const PDHCounter &counter) = 0;
		virtual void detach(const PDHCounter &counter) = 0;
	};

	class PDHCounter
	{
	private:
		HCOUNTER hCounter_;
		std::string name_;
		PDH_FMT_COUNTERVALUE data_;
		PDHCounterListener *listener_;

	public:

		PDHCounter(std::string name, PDHCounterListener *listener) : name_(name), listener_(listener), hCounter_(NULL){
		}
		virtual ~PDHCounter(void) {
			if (hCounter_ != NULL)
				remove();
		}
		const HCOUNTER getCounter() const {
			return hCounter_;
		}
		const std::string getName() const {
			return name_;
		}
		void addToQuery(HQUERY hQuery) {
			PDH_STATUS status;
			assert(hQuery != NULL);
			if (hCounter_ != NULL)
				throw PDHException("addToQuery failed (already opened): " + name_);
			if (listener_)
				listener_->attach(*this);
			if ((status = PdhAddCounter(hQuery, name_.c_str(), 0, &hCounter_)) != ERROR_SUCCESS) 
				throw PDHException("PdhOpenQuery failed: " + name_, status);
			assert(hCounter_ != NULL);
		}
		void remove() {
			if (hCounter_ == NULL)
				return;
			PDH_STATUS status;
			if (listener_)
				listener_->detach(*this);
			if ((status = PdhRemoveCounter(hCounter_)) != ERROR_SUCCESS)
				throw PDHException("PdhRemoveCounter failed", status);
			hCounter_ = NULL;
		}
		void collect() {
			if (hCounter_ == NULL)
				return;
			PDH_STATUS status;
			if ((status = PdhGetFormattedCounterValue(hCounter_, PDH_FMT_LARGE , NULL, &data_)) != ERROR_SUCCESS)
				throw PDHException("PdhGetFormattedCounterValue failed", status);
			if (listener_)
				listener_->collect(*this);
		}
		double getDoubleValue() const {
			return data_.doubleValue;
		}
		__int64 getInt64Value() const {
			return data_.largeValue;
		}
		std::string getStringValue() const {
			return data_.AnsiStringValue;
		}
	};

	class PDHQuery 
	{
	private:
		typedef std::list<PDHCounter*> CounterList;
		CounterList counters_;
		HQUERY hQuery_;
	public:
		PDHQuery() : hQuery_(NULL) {
		}
		virtual ~PDHQuery(void) {
			if (hQuery_)
				close();
			for (CounterList::iterator it = counters_.begin(); it != counters_.end(); it++) {
				delete (*it);
			}
			counters_.clear();
		}

		void addCounter(std::string name, PDHCounterListener *listener) {
			PDHCounter *counter = new PDHCounter(name, listener);
			counters_.push_back(counter);
		}

		void open() {
			assert(hQuery_ == NULL);
			PDH_STATUS status;
			if( (status = PdhOpenQuery( NULL, 0, &hQuery_ )) != ERROR_SUCCESS)
				throw PDHException("PdhOpenQuery failed", status);
			for (CounterList::iterator it = counters_.begin(); it != counters_.end(); it++) {
				(*it)->addToQuery(getQueryHandle());
			}
		}

		void close() {
			assert(hQuery_ != NULL);
			DWORD x = PDH_INVALID_HANDLE;
			PDH_STATUS status;
			for (CounterList::iterator it = counters_.begin(); it != counters_.end(); it++) {
				(*it)->remove();
			}
			if( (status = PdhCloseQuery(hQuery_)) != ERROR_SUCCESS)
				throw PDHException("PdhCloseQuery failed", status);
			hQuery_ = NULL;
			for (CounterList::iterator it = counters_.begin(); it != counters_.end(); it++) {
				delete (*it);
			}
			counters_.clear();
		}

		void collect() {
			PDH_STATUS status;
			if ((status = PdhCollectQueryData(hQuery_)) != ERROR_SUCCESS)
				throw PDHException("PdhCollectQueryData failed: ", status);
			for (CounterList::iterator it = counters_.begin(); it != counters_.end(); it++) {
				(*it)->collect();
			}
		}

		HQUERY getQueryHandle() const {
			return hQuery_;
		}
	};


}