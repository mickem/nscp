#pragma once

namespace NSCAPI {

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



	// Various Nagios codes
	/*
	const int returnCRIT = 2;
	const int returnOK = 0;
	const int returnWARN = 1;
	const int returnUNKNOWN = 4;
	*/
	// Various NSCP extensions

//	const int returnInvalidBufferLen = -2;	// The return buffer was to small (might wanna call again with a larger one)


//	typedef int returnCodes;

	// Various function Return codes
//	const int isSuccess = 1;		// Everything went fine
//	const int hasFailed = 0;		// EVerything went to hell :)
//	const int isTrue = 1;			// Should be interpreted as "true"
//	const int isFalse = 0;			// Should be interpreted as "false"
//	const int isError = -1;			// Should be interpreted as "ERROR"
//	const int handled = 2;			// The command was handled by this module

	// Various message Types
	const int log = 0;				// Log message
	const int error = -1;			// Error (non critical)
	const int critical = -10;		// Critical error
	const int warning = 1;			// Warning
	const int debug = 666;			// Debug message

	typedef int messageTypes;		// Message type
};
