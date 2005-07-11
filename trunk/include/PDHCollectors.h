#pragma once

#include <PDHCounter.h>

namespace PDHCollectors {
	const int format_large = 0x00000400;
	const int format_long = 0x00000100;
	const int format_double = 0x00000200;

	template <class TType = __int64, int TCollectionFormat = format_large>
	class StaticPDHCounterListener {};

	template <class TType>
	class StaticPDHCounterListener<TType, format_double> : public PDH::PDHCounterListener {
		TType value_;
	public:
		StaticPDHCounterListener() : value_(0) {}
		virtual void collect(const PDH::PDHCounter &counter) {
			value_ = counter.getDoubleValue();
		}
		void attach(const PDH::PDHCounter &counter){}
		void detach(const PDH::PDHCounter &counter){}
		TType getValue() const {
			return value_;
		}
		DWORD getFormat() const {
			return format_double;
		}
	};

	template <class TType>
	class StaticPDHCounterListener<TType, format_long> : public PDH::PDHCounterListener {
		TType value_;
	public:
		StaticPDHCounterListener() : value_(0) {}
		virtual void collect(const PDH::PDHCounter &counter) {
			value_ = counter.getIntValue();
		}
		void attach(const PDH::PDHCounter &counter){}
		void detach(const PDH::PDHCounter &counter){}
		TType getValue() const {
			return value_;
		}
		DWORD getFormat() const {
			return format_long;
		}
	};

	template <class TType>
	class StaticPDHCounterListener<TType, format_large> : public PDH::PDHCounterListener {
		TType value_;
	public:
		StaticPDHCounterListener() : value_(0) {}
		virtual void collect(const PDH::PDHCounter &counter) {
			value_ = counter.getInt64Value();
		}
		void attach(const PDH::PDHCounter &counter){}
		void detach(const PDH::PDHCounter &counter){}
		TType getValue() const {
			return value_;
		}
		DWORD getFormat() const {
			return format_large;
		}
	};


	template <class TType = __int64>
	class RoundINTPDHBufferListenerImpl : public PDH::PDHCounterListener {
		unsigned int length;
		TType *buffer;
		unsigned int current;
	public:
		RoundINTPDHBufferListenerImpl() : buffer(NULL), length(0), current(0) {}
		RoundINTPDHBufferListenerImpl(int length_) : length(length_), current(0) {
			buffer = new int[length];
			for (unsigned int i=0; i<length;i++)
				buffer[i] = 0;
		}
		virtual ~RoundINTPDHBufferListenerImpl() {
			delete [] buffer;
		}

		/**
		* Resize the buffer to a new length
		*
		* @todo Make this copy the old buffer if there is one.
		*
		* @param newLength The new length
		*/
		void resize(int newLength) {
			delete [] buffer;

			current = 0;
			length = newLength;

			buffer = new TType[length];
			for (unsigned int i=0; i<length;i++)
				buffer[i] = 0;

		}

		virtual void collect(const PDH::PDHCounter &counter) = 0;

		void attach(const PDH::PDHCounter &counter){}
		void detach(const PDH::PDHCounter &counter){}
		void pushValue(TType value) {
			if (buffer == NULL)
				return;
			if (current >= length)
				return;
			buffer[current++] = value;
			if (current >= length)
				current = 0;
		}
		double getAvrage(unsigned int backItems) const {
			if ((backItems == 0) || (backItems >= length))
				return -1;
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
		inline unsigned int getLength() const {
			return length;
		}
	};


	template <class TType = __int64, DWORD TCollectionFormat = format_large>
	class RoundINTPDHBufferListener : public RoundINTPDHBufferListenerImpl<TType> {
	};

	template <class TType>
	class RoundINTPDHBufferListener<TType, format_double> : public RoundINTPDHBufferListenerImpl<TType> {
	public:
		RoundINTPDHBufferListener() {}
		RoundINTPDHBufferListener(int length) : RoundINTPDHBufferListenerImpl(length) {}

		virtual void collect(const PDH::PDHCounter &counter) {
			pushValue(counter.getDoubleValue());
		}
		virtual DWORD getFormat() const {
			return format_double;
		}
	};

	template <class TType>
	class RoundINTPDHBufferListener<TType, format_long> : public RoundINTPDHBufferListenerImpl<TType> {
	public:
		RoundINTPDHBufferListener() {}
		RoundINTPDHBufferListener(int length) : RoundINTPDHBufferListenerImpl(length) {}

		virtual void collect(const PDH::PDHCounter &counter) {
			pushValue(counter.getIntValue());
		}
		virtual DWORD getFormat() const {
			return format_long;
		}
	};

	template <class TType>
	class RoundINTPDHBufferListener<TType, format_large> : public RoundINTPDHBufferListenerImpl<TType> {
	public:
		RoundINTPDHBufferListener() {}
		RoundINTPDHBufferListener(int length) : RoundINTPDHBufferListenerImpl(length) {}

		virtual void collect(const PDH::PDHCounter &counter) {
			pushValue(counter.getInt64Value());
		}
		virtual DWORD getFormat() const {
			return format_large;
		}
	};
}