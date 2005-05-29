#pragma once

#include <PDHCounter.h>

namespace PDHCollectors {
	template <class TType = __int64, DWORD TCollectionFormat = PDH_FMT_LARGE>
	class StaticPDHCounterListener : public PDH::PDHCounterListener {
		TType value_;
	public:
		virtual void collect(const PDH::PDHCounter &counter) {
			switch (TCollectionFormat) {
				case PDH_FMT_LARGE:
					setValue(counter.getInt64Value());
					break;
				case PDH_FMT_DOUBLE:
					setValue(counter.getDoubleValue());
					break;
				default:
					return;
			}
		}
		void attach(const PDH::PDHCounter &counter){}
		void detach(const PDH::PDHCounter &counter){}
		void setValue(TType value) {
			value_ = value;
		}
		TType getValue() const {
			return value_;
		}
		DWORD getFormat() const {
			return TCollectionFormat;
		}
	};

	template <class TType = __int64, DWORD TCollectionFormat = PDH_FMT_LARGE>
	class RoundINTPDHBufferListener : public PDH::PDHCounterListener {
		unsigned int length;
		TType *buffer;
		unsigned int current;
	public:
		RoundINTPDHBufferListener() : buffer(NULL), length(0), current(0) {}
		RoundINTPDHBufferListener(int length_) : length(length_), current(0) {
			buffer = new int[length];
			for (unsigned int i=0; i<length;i++)
				buffer[i] = 0;
		}
		virtual ~RoundINTPDHBufferListener() {
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
		virtual void collect(const PDH::PDHCounter &counter) {
			switch (TCollectionFormat) {
				case PDH_FMT_LONG:
					pushValue(counter.getInt64Value());
					break;
				case PDH_FMT_DOUBLE:
					pushValue(counter.getInt64Value());
					break;
				default:
					return;
			}
		}
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
		TType getAvrage(unsigned int backItems) const {
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
		DWORD getFormat() const {
			return TCollectionFormat;
		}
	};

}