#include "stdafx.h"
#include ".\nsparser.h"
#include "strEx.h"
//#include <PDHCollector.h>
//#include <PDHCollectors.h>
//#include <EnumNtSrv.h>
#include <utilities.h>
//#include <pdh.h>
//#include <pdhmsg.h>

/*
#include "commands/CommandID.h"
#include "commands/NSCommandCLIENTVERSION.h"
#include "commands/NSCommandCPULOAD.h"
#include "commands/NSCommandMEMUSE.h"
#include "commands/NSCommandSERVICESTATE.h"
#include "commands/NSCommandUPTIME.h"
#include "commands/NSCommandUSEDDISKSPACE.h"

using namespace PDHCollector;

#define ADD_NS_COMMAND(cls) \
if (cls::getCommandID() == cmdID) \
	return cls::execute(args);
*/
std::string NSParser::execute(int cmdID, std::list<std::string> args) 
{
	/*
	ADD_NS_COMMAND(NSCommandCLIENTVERSION);	// 1
	ADD_NS_COMMAND(NSCommandCPULOAD);		// 2
	ADD_NS_COMMAND(NSCommandUPTIME);		// 3
	ADD_NS_COMMAND(NSCommandUSEDDISKSPACE);	// 4
	ADD_NS_COMMAND(NSCommandSERVICESTATE);	// 5
											// 6
	ADD_NS_COMMAND(NSCommandMEMUSE);		// 7
											// 8
											// 9
											// 10
											*/
	return "ERROR: Invalid command.";
}
