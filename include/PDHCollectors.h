#pragma once

#include <PDHCounter.h>

namespace PDHCollectors {
	class StaticPDHCounterListener : public PDH::PDHCounterListener {
		__int64 value_;
	public:
		virtual void collect(const PDH::PDHCounter &counter) {
			setValue(counter.getInt64Value());
		}
		void attach(const PDH::PDHCounter &counter){}
		void detach(const PDH::PDHCounter &counter){}
		void setValue(__int64 value) {
			value_ = value;
		}
		__int64 getValue() const {
			return value_;
		}
	};


	class RoundINTPDHBufferListener : public PDH::PDHCounterListener {
		unsigned int length;
		int *buffer;
		unsigned int current;
	public:
		RoundINTPDHBufferListener(int length_) : length(length_), current(0) {
			buffer = new int[length];
			for (unsigned int i=0; i<length;i++)
				buffer[i] = 0;
		}
		virtual ~RoundINTPDHBufferListener() {
			delete [] buffer;
		}
		virtual void collect(const PDH::PDHCounter &counter) {
			pushValue(static_cast<int>(counter.getInt64Value()));
		}
		void attach(const PDH::PDHCounter &counter){}
		void detach(const PDH::PDHCounter &counter){}
		void pushValue(int value) {
			buffer[current++] = value;
			if (current >= length)
				current = 0;
		}
		int getAvrage(unsigned int backItems) const {
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
			return static_cast<int>(ret/backItems);
		}
	};

}