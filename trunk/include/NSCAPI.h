#pragma once

namespace NSCAPI {

#ifdef DEBUG
	typedef enum {
		returnCRIT = 2,
		returnOK = 0,
		returnWARN = 1,
		returnUNKNOWN = 4,
		returnInvalidBufferLen = -2,
		returnIgnored = -1
	} nagiosReturn;
	typedef enum {
		istrue = 1, 
		isfalse = 0
	} boolReturn;
	typedef enum {
		isSuccess = 1, 
		hasFailed = 0,
		isInvalidBufferLen = -2
	} errorReturn;
#else
	const int returnCRIT = 2;
	const int returnOK = 0;
	const int returnWARN = 1;
	const int returnUNKNOWN = 4;
	const int returnInvalidBufferLen = -2;
	const int returnIgnored = -1;
	const int istrue = 1; 
	const int isfalse = 0;
	const int isSuccess = 1; 
	const int hasFailed = 0;
	const int isInvalidBufferLen = -2;
	typedef int nagiosReturn;
	typedef int boolReturn;
	typedef int errorReturn;
#endif


	// Various message Types
	const int log = 0;				// Log message
	const int error = -1;			// Error (non critical)
	const int critical = -10;		// Critical error
	const int warning = 1;			// Warning
	const int debug = 666;			// Debug message

	typedef int messageTypes;		// Message type
};
