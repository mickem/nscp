#pragma once

namespace NSParser {
	typedef std::list<std::string> ArgumentList;

/*
	class NSCommand {
		static int getCommandID() const;
		static void execute(ArgumentList args) const;
	};
*/
	std::string execute(int cmdID, std::list<std::string> args);
}

