using System;
using System.Collections.Generic;
using System.Text;
using System.Runtime.InteropServices;
//using NSCP;

namespace NSCP.Plugin {
    public class PluginFactory : NSCP.IPluginFactory
    {
        public NSCP.IPlugin create(NSCP.ICore core, String alias)
        {
            return new test.MyPlugin(core);
        }
    }
}
namespace test
{
    public class MyQueryHandler : NSCP.IQueryHandler
    {
        NSCP.ICore core;
        public MyQueryHandler(NSCP.ICore core)
        {
            this.core = core;
        }
        public bool isActive()
        {
            return true;
        }

        public NSCP.Result onQuery(string command, byte[] request)
        {
            NSCP.Result result = new NSCP.Result();
            Plugin.QueryRequestMessage request_message = Plugin.QueryRequestMessage.CreateBuilder().MergeFrom(request).Build();
            string intcommand = request_message.GetPayload(0).Command;
            core.getLogger().error("Got command: " + intcommand + "/" + command);

            Plugin.Common.Types.Header.Builder header = Plugin.Common.Types.Header.CreateBuilder();
            header.SetVersion(Plugin.Common.Types.Version.VERSION_1);

            Plugin.QueryResponseMessage.Builder response_message = Plugin.QueryResponseMessage.CreateBuilder();
            Plugin.QueryResponseMessage.Types.Response.Builder response = Plugin.QueryResponseMessage.Types.Response.CreateBuilder();
            response.SetCommand(command);
            response.SetResult(Plugin.Common.Types.ResultCode.OK);
            response.SetMessage("Hello from C#");
            response_message.AddPayload(response.Build());
            response_message.SetHeader(header.Build());

            result.data = response_message.Build().ToByteArray();
            result.result = 1;
            return result;
        }

    }
    public class MyPlugin : NSCP.IPlugin
    {
        NSCP.ICore core;
        public MyPlugin(NSCP.ICore core)
        {
            this.core = core;
        }

        public bool load(int mode)
        {
            core.getLogger().error("Hello World from C#");
            core.getRegistry().registerCommand("check_dotnet", "This is a sample command written in C#");
            return true;
        }
        public bool unload()
        {
            return true;
        }
        public string getName()
        {
            return "Sample C# Module";
        }
        public string getDescription()
        {
            return "Sample C# Module";
        }
        public NSCP.PluginVersion getVersion()
        {
            return new NSCP.PluginVersion(0, 0, 1);
        }

        public NSCP.IQueryHandler getQueryHandler()
        {
            return new MyQueryHandler(core);
        }
        public NSCP.ISubmissionHandler getSubmissionHandler()
        {
            return null;
        }
        public NSCP.IMessageHandler getMessageHandler()
        {
            return null;
        }
        public NSCP.IExecutionHandler getExecutionHandler()
        {
            return null;
        }
    }
}
