using System;
using System.Collections.Generic;
using System.Text;
using System.Runtime.InteropServices;

namespace CsharpSamplePlugin
{
    public class SamplePlugin
    {
        public class version {
            public version(int major, int minor, int revision) {
                this.major = major;
                this.minor = minor;
                this.revision = revision;
            }
            public int major;
            public int minor;
            public int revision;
        }
        public bool loadModule() {
            return true;
        }
        public bool unloadModule() {
            return true;
        }
        public string getModuleName() {
            return "Sample C# Module";
        }
        public string getModuleDescription() {
            return "Sample C# Module";
        }
    	public version getModuleVersion() {
	    	return new version(0, 0, 1 );
    	}

        public bool hasCommandHandler() {
            return true;
        }
        public bool hasMessageHandler() {
            return false;
        }
    	public int handleCommand(String command, List<String> args, ref String message, ref String perf) {
            if (command == "check_net") {
                message = "Everything is not going to be ok!";
                perf = "performance data is cool";
                return 1;
            }
            return -1;
    	}
    }
}
