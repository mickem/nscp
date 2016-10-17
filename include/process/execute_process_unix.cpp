/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

