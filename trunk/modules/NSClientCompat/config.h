#pragma once


// PDH Check interval (Check every x second)
#define CHECK_INTERVAL		1

// PDH CPU check backlog (x minutes)
#define BACK_INTERVAL		2000	

// Buffer size of incoming data (noteice this is the maximum request length!)
#define RECV_BUFFER_LEN		1024

// Commands								
// x = Implemeneted, - unimplemented, ! unsopported
#define REQ_CLIENTVERSION	1	// x
#define REQ_CPULOAD			2	// x
#define REQ_UPTIME			3	// x
#define REQ_USEDDISKSPACE	4	// x
#define REQ_SERVICESTATE	5	// x
#define REQ_PROCSTATE		6	// -
#define REQ_MEMUSE			7	// x
#define REQ_COUNTER			8	// -
//#define REQ_FILEAGE		9	// !
//#define REQ_INSTANCES		10	// !

