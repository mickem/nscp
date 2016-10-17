/**************************************************************************
*   Copyright (C) 2004-2008 by Michael Medin <michael@medin.name>         *
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

#include <string>
#include <NSCAPI.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h> 
#include <errno.h>

#include <process/execute_process.hpp>
#include <buffer.hpp>

#define BUFFER_SIZE 4096

bool early_timeout = false;
typedef hlp::buffer<char> buffer_type;

//extern int errno;

/* handle timeouts when executing commands via my_system() */
void my_system_sighandler(int sig) {
	//exit(NSCAPI::returnUNKNOWN);
	printf("\n\n\nTIMEOUT TIMEOUT TIMEOUT\n\n\n");
}

void process::kill_all() {
	// TODO: Fixme
}


int process::execute_process(process::exec_arguments args, std::string &output) {
	NSCAPI::nagiosReturn result;
	int fd[2];
	std::size_t bytes_read=0;
	early_timeout=false;

	pipe(fd);
	//fcntl(fd[0],F_SETFL,O_NONBLOCK);
	//fcntl(fd[1],F_SETFL,O_NONBLOCK);


	//time(&start_time);

	//signal(SIGALRM, my_system_sighandler);
	//alarm(args.timeout);

	FILE *fp = popen(args.command.c_str(), "r");
	close(fd[1]);
	if (fp == NULL) {
		close(fd[0]);
		output = "NRPE: Call to popen() failed";
		return NSCAPI::query_return_codes::returnUNKNOWN;
	} else {
		buffer_type buffer(BUFFER_SIZE);
		while ( (bytes_read=fread(buffer.get(),1,buffer.size()-1,fp)) >0 ) {
			if (bytes_read > 0 && bytes_read < BUFFER_SIZE) {
				buffer[bytes_read] = 0;
				output += std::string(buffer.get());
			}
		}

		int status = pclose(fp);
		if (status == -1 || !WIFEXITED(status))
			result = NSCAPI::query_return_codes::returnUNKNOWN;
		else
			result=WEXITSTATUS(status);
	}
	close(fd[0]);
	//alarm(0);
	return result;
}

