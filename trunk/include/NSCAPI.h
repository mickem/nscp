#pragma once

namespace NSCAPI {

	// Various Nagios codes
	const int returnCRIT = 2;
	const int returnOK = 0;
	const int returnWARN = 1;
	const int returnUNKNOWN = 4;
	typedef int returnCodes;

	// Various Return codes
	const int success = 1;			// Everything went fine
	const int failed = 0;			// EVerything went to hell :)
	const int istrue = 1;			// Should be interpreted as "true"
	const int isfalse = 0;			// Should be interpreted as "false"
	const int handled = 2;			// The command was handled by this module
	const int invalidBufferLen = -1;// The return buffer was to small (might wanna call again with a larger one)

	// Various message Types
	const int log = 0;				// Log message
	const int error = -1;			// Error (non critical)
	const int critical = -10;		// Critical error
	const int warning = 1;			// Warning
	const int debug = 666;			// Debug message

	typedef int messageTypes;		// Message type
};
