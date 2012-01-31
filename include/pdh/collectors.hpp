/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#pragma once

#include <boost/lexical_cast.hpp>

#include <pdh/core.hpp>
#include <pdh/counters.hpp>
//#include <pdh.hpp>
#include <Mutex.h>
namespace PDHCollectors {
	typedef PDH::PDHException PDHException;
	const int format_large = 0x00000400;
	const int format_long = 0x00000100;
	const int format_double = 0x00000200;
/*
	class PDHException {
		std::wstring error_;
	public:
		PDHException(std::wstring error) : error_(error) {}
		std::wstring getError() const { return error_; }

	};
	*/
	class TPDHCounterMutex {
	public:
		virtual void lock() = 0;
		virtual bool hasLock(bool silent = true) = 0;
		virtual void release() = 0;
	};

	class PDHCounterNoMutex : public TPDHCounterMutex {
	public:
		void lock() {}
		bool hasLock(bool silent = true) {
			return true;
		}
		void release() {}
	};
	class PDHCounterNormalMutex : public TPDHCounterMutex {
		MutexHandler mutex_;
		ManualMutexLock lock_;
	public:
		PDHCounterNormalMutex() : lock_(mutex_) {}
		void lock() {
			lock_.lock();
		}
		bool hasLock(bool silent = true) {
			if (!silent && !lock_.hasMutex()) {
				std::wcout << _T("We never got the mutex... sorry...") << std::endl;
			}
			return lock_.hasMutex();
		}
		void release() {
			lock_.release();
		}
	};
	class PDHCounterMutexHandler {
	private:
		TPDHCounterMutex *mutex_;	// Handle to the mutex object.
	public:
		/**
		* Default c-tor.
		* Waits for the mutex object.
		* @param mutex The mutex to use
		* @timeout The timeout before abandoning wait
		*/
		PDHCounterMutexHandler(TPDHCounterMutex *mutex) : mutex_(mutex) {
			if (mutex == NULL) {
				std::wcout << _T("Error in mutex lock: ") << std::endl;
				mutex = NULL;
				return;
			}
			mutex->lock();
		}
		/**
		* An attempt to simplify the has mutex thingy (don't know if it works, haven't tried it since I wrote this class a few years ago :)
		* @return true if we have a mutex lock.
		*/
		operator bool () const {
			return mutex_!=NULL&&mutex_->hasLock();
		}
		/**
		* Check if we actually got the mutex (might have timed out)
		* @return 
		*/
		bool hasLock(bool silent = true) const {
			return mutex_!=NULL&&mutex_->hasLock(silent);
		}
		/**
		* Default d-tor.
		* Release the mutex
		*/
		virtual ~PDHCounterMutexHandler() {
			mutex_->release();
		}
	};

	class PDHCollector : public PDH::PDHCounterListener{
	public:
		virtual std::wstring get_string() = 0;
		virtual double get_double() = 0;
		virtual __int64 get_int64() = 0;
		virtual double get_average(int backlog) = 0;
		virtual void set_extra_format(DWORD format) = 0;
	};


	template <class TType, int TCollectionFormat, class TMutextHandler = PDHCounterNoMutex>
	class StaticPDHCounterListener {
	};

	template <class TType, class TMutextHandler>
	class StaticPDHCounterListener<TType, format_double, TMutextHandler> : public PDHCollector {
		TType value_;
		TMutextHandler mutex_;
		bool hasValue_;
		std::wstring lastError_;
		const PDH::PDHCounter *parent_;
		DWORD extra_format;
	public:
		StaticPDHCounterListener() : value_(0), hasValue_(false), parent_(NULL), extra_format(0) {}
		virtual void collect(const PDH::PDHCounter &counter) {
			PDHCounterMutexHandler mutex(&mutex_);
			if (!mutex.hasLock())
				return;
			value_ = counter.getDoubleValue();
			hasValue_ = true;
		}
		void attach(const PDH::PDHCounter *counter){ parent_ = counter;}
		void detach(const PDH::PDHCounter *counter){ parent_ = NULL; }
		TType getValue() {
			PDHCounterMutexHandler mutex(&mutex_);
			if (!mutex.hasLock())
				throw PDHException(get_name(), _T("Could not get mutex"));
			if (!hasValue_)
				throw PDHException(get_name(), _T("No value has been collected yet"));
			return value_;
		}
		void set_extra_format(DWORD format) {
			extra_format = format;
		}
		DWORD getFormat() const {
			return format_double|extra_format;
		}
	public:
		inline std::wstring get_string() {
			return boost::lexical_cast<std::wstring>(getValue());
		}
		inline double get_double() {
			return static_cast<double>(getValue());
		}
		__int64 get_int64() {
			return getValue();
		}
		inline double get_average(int backlog) {
			return static_cast<double>(getValue());
		}
	private:
		std::wstring get_name() const {
			if (parent_ != NULL)
				return parent_->getName();
			return _T("<UN ATTACHED>");
		}
	};

	template <class TType, class TMutextHandler>
	class StaticPDHCounterListener<TType, format_long, TMutextHandler> : public PDHCollector {
		TType value_;
		TMutextHandler mutex_;
		bool hasValue_;
		const PDH::PDHCounter *parent_;
		DWORD extra_format;
	public:
		StaticPDHCounterListener() : value_(0), hasValue_(false), parent_(NULL), extra_format(0) {}
		virtual void collect(const PDH::PDHCounter &counter) {
			PDHCounterMutexHandler mutex(&mutex_);
			if (!mutex.hasLock())
				return;
			value_ = counter.getIntValue();
			hasValue_ = true;
		}
		void attach(const PDH::PDHCounter *counter){ parent_ = counter;}
		void detach(const PDH::PDHCounter *counter){ parent_ = NULL; }
		TType getValue() {
			PDHCounterMutexHandler mutex(&mutex_);
			if (!mutex.hasLock())
				throw PDHException(get_name(), _T("Could not get mutex"));
			if (!hasValue_)
				throw PDHException(get_name(), _T("No value has been collected yet"));
			return value_;
		}
		void set_extra_format(DWORD format) {
			extra_format = format;
		}
		DWORD getFormat() const {
			return format_long|extra_format;
		}
		inline std::wstring get_string() {
			return to_wstring(getValue());
		}
		inline double get_double() {
			return static_cast<double>(getValue());
		}
		__int64 get_int64() {
			return getValue();
		}
		inline double get_average(int backlog) {
			return static_cast<double>(getValue());
		}
	private:
		std::wstring get_name() const {
			if (parent_ != NULL)
				return parent_->getName();
			return _T("<UN ATTACHED>");
		}
	};

	template <class TType, class TMutextHandler>
	class StaticPDHCounterListener<TType, format_large, TMutextHandler> : public PDHCollector {
		TMutextHandler mutex_;
		TType value_;
		bool hasValue_;
		const PDH::PDHCounter *parent_;
		DWORD extra_format;
	public:
		StaticPDHCounterListener() : value_(0), hasValue_(false), parent_(NULL), extra_format(0) {}
		virtual void collect(const PDH::PDHCounter &counter) {
			PDHCounterMutexHandler mutex(&mutex_);
			if (!mutex.hasLock())
				return;
			value_ = counter.getInt64Value();
			hasValue_ = true;
		}
		void attach(const PDH::PDHCounter *counter){ parent_ = counter;}
		void detach(const PDH::PDHCounter *counter){ parent_ = NULL; }
		TType getValue() {
			PDHCounterMutexHandler mutex(&mutex_);
			if (!mutex.hasLock())
				throw PDHException(get_name(), std::wstring(_T("Could not get mutex")));
			if (!hasValue_)
				throw PDHException(get_name(), std::wstring(_T("No value has been collected yet")));
			return value_;
		}
		void set_extra_format(DWORD format) {
			extra_format = format;
		}
		DWORD getFormat() const {
			return format_large|extra_format;
		}
		inline std::wstring get_string() {
			return boost::lexical_cast<std::wstring>(getValue());
		}
		inline double get_double() {
			return static_cast<double>(getValue());
		}
		__int64 get_int64() {
			return getValue();
		}
		inline double get_average(int backlog) {
			return static_cast<double>(getValue());
		}
	private:
		std::wstring get_name() const {
			if (parent_ != NULL)
				return parent_->getName();
			return _T("<UN ATTACHED>");
		}
	};


	template <class TType, class TMutextHandler>
	class RoundINTPDHBufferListenerImpl : public PDHCollector {
		TMutextHandler mutex_;
		unsigned int length;
		TType *buffer;
		unsigned int current;
		bool hasValue_;
		const PDH::PDHCounter *parent_;
	public:
		DWORD extra_format;
		RoundINTPDHBufferListenerImpl() : buffer(NULL), length(0), current(0), hasValue_(false), parent_(NULL), extra_format(0) {}
		RoundINTPDHBufferListenerImpl(int length_) : buffer(NULL), length(0), current(0), hasValue_(false), parent_(NULL) {
			resize(length_);
		}
		virtual ~RoundINTPDHBufferListenerImpl() {
			PDHCounterMutexHandler mutex(&mutex_);
			if (!mutex.hasLock())
				return;
			delete [] buffer;
		}

		virtual void set_extra_format(DWORD format) {
			extra_format = format;
		}

		/**
		* Resize the buffer to a new length
		*
		* @todo Make this copy the old buffer if there is one.
		*
		* @param newLength The new length
		*/
		void resize(int newLength) {
			PDHCounterMutexHandler mutex(&mutex_);
			if (!mutex.hasLock())
				return;
			delete [] buffer;

			current = 0;
			length = newLength;

			buffer = new TType[length];
			for (unsigned int i=0; i<length;i++)
				buffer[i] = 0;

		}

		virtual void collect(const PDH::PDHCounter &counter) = 0;

		void attach(const PDH::PDHCounter *counter){ parent_ = counter;}
		void detach(const PDH::PDHCounter *counter){ parent_ = NULL; }
		void pushValue(TType value) {
			PDHCounterMutexHandler mutex(&mutex_);
			if (!mutex.hasLock())
				return;
			if (buffer == NULL)
				return;
			if (current >= length)
				return;
			hasValue_ = true;
			buffer[current++] = value;
			if (current >= length)
				current = 0;
		}
		double getAvrage(unsigned int backItems) {
			PDHCounterMutexHandler mutex(&mutex_);
			if (!mutex.hasLock(true))
				throw PDHException(get_name(), _T("Failed to get mutex :("));
			if (!hasValue_)
				throw PDHException(get_name(), _T("No value has been collected yet"));
			if (backItems == 0)
				throw PDHException(get_name(), _T("No timeframe given on command line (ie. time=0)"));
			if (backItems >= length)
				throw PDHException(get_name(), _T("Length given larger then interval: ") + strEx::itos(backItems) + _T(" >= ") + strEx::itos(length));
			double ret = 0;
			if (current >= backItems) {
				// Handle "whole" list.
				for (unsigned int i=current-backItems; i<current;i++)
					ret += buffer[i];
			} else {
				// Handle split list.
				for (unsigned int i=0; i<current;i++)
					ret += buffer[i];
				for (unsigned int i=length-backItems+current; i<length;i++)
					ret += buffer[i];
			}
			return (ret/backItems);
		}
		inline std::wstring get_string() {
			return boost::lexical_cast<std::wstring>(getAvrage(length-1));
		}
		inline double get_double() {
			return getAvrage(length-1);
		}
		__int64 get_int64() {
			return static_cast<__int64>(getAvrage(length-1));
		}
		inline double get_average(int backlog) {
			return getAvrage(backlog);
		}
		inline unsigned int getLength() const {
			return length;
		}
	private:
		std::wstring get_name() const {
			if (parent_ != NULL)
				return parent_->getName();
			return _T("<UN ATTACHED>");
		}
	};


	template <class TType, DWORD TCollectionFormat, class TMutextHandler = PDHCounterNoMutex>
	class RoundINTPDHBufferListener : public RoundINTPDHBufferListenerImpl<TType, TMutextHandler> {
	};

	template <class TType, class TMutextHandler>
	class RoundINTPDHBufferListener<TType, format_double, TMutextHandler> : public RoundINTPDHBufferListenerImpl<TType, TMutextHandler> {
	public:
		RoundINTPDHBufferListener() {}
		RoundINTPDHBufferListener(int length) : RoundINTPDHBufferListenerImpl(length) {}

		virtual void collect(const PDH::PDHCounter &counter) {
			pushValue(counter.getDoubleValue());
		}
		virtual DWORD getFormat() const {
			return format_double|extra_format;
		}
	};

	template <class TType, class TMutextHandler>
	class RoundINTPDHBufferListener<TType, format_long, TMutextHandler> : public RoundINTPDHBufferListenerImpl<TType, TMutextHandler> {
	public:
		RoundINTPDHBufferListener() {}
		RoundINTPDHBufferListener(int length) : RoundINTPDHBufferListenerImpl(length) {}

		virtual void collect(const PDH::PDHCounter &counter) {
			pushValue(counter.getIntValue());
		}
		virtual DWORD getFormat() const {
			return format_long|extra_format;
		}
	};

	template <class TType, class TMutextHandler>
	class RoundINTPDHBufferListener<TType, format_large, TMutextHandler> : public RoundINTPDHBufferListenerImpl<TType, TMutextHandler> {
	public:
		RoundINTPDHBufferListener() {}
		RoundINTPDHBufferListener(int length) : RoundINTPDHBufferListenerImpl(length) {}

		virtual void collect(const PDH::PDHCounter &counter) {
			pushValue(counter.getInt64Value());
		}
		virtual DWORD getFormat() const {
			return format_large|extra_format;
		}
	};
}