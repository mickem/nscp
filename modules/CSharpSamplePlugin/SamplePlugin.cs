using System;
using System.Collections.Generic;
using System.Text;
using System.Runtime.InteropServices;
using NSCP.Core;
using NSCP.Helpers;

namespace NSCP.Plugin
{
    public class PluginFactory : IPluginFactory
    {
        public IPlugin create(ICore core, PluginInstance instance)
        {
            return new test.MyPlugin(core, instance.PluginID);
        }
    }
}
namespace test
{
    public class MyQueryHandler : IQueryHandler
    {
        ICore core;
        LogHelper log;
        public MyQueryHandler(ICore core)
        {
            this.core = core;
            this.log = new LogHelper(core);
        }
        public bool isActive()
        {
            return true;
        }

        public Result onQuery(string command, byte[] request)
        {
            Result result = new Result();
            Plugin.QueryRequestMessage request_message = Plugin.QueryRequestMessage.CreateBuilder().MergeFrom(request).Build();
            log.debug("Got command: " + command);

            Plugin.QueryResponseMessage.Builder response_message = Plugin.QueryResponseMessage.CreateBuilder();
            response_message.SetHeader(Plugin.Common.Types.Header.CreateBuilder().Build());
            Plugin.QueryResponseMessage.Types.Response.Builder response = Plugin.QueryResponseMessage.Types.Response.CreateBuilder();
            response.SetCommand(command);
            response.SetResult(Plugin.Common.Types.ResultCode.OK);
            response.AddLines(Plugin.QueryResponseMessage.Types.Response.Types.Line.CreateBuilder().SetMessage("Hello from C#").Build());
            response_message.AddPayload(response.Build());

            return new Result(response_message.Build().ToByteArray());
        }

    }
    public class MyPlugin : IPlugin
    {
        private ICore core;
        private LogHelper log;
        private int plugin_id;
        public MyPlugin(ICore core, int plugin_id)
        {
            this.core = core;
            this.log = new LogHelper(core);
            this.plugin_id = plugin_id;
        }

        public bool load(int mode)
        {
            long port = new SettingsHelper(core, plugin_id).getInt("/settings/WEB/server", "port", 1234);
            log.info("Webserver port is: " + port);
            new RegistryHelper(core, plugin_id).registerCommand("check_dotnet", "This is a sample command written in C#");
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
        public PluginVersion getVersion()
        {
            return new PluginVersion(0, 0, 1);
        }

        public IQueryHandler getQueryHandler()
        {
            return new MyQueryHandler(core);
        }
        public ISubmissionHandler getSubmissionHandler()
        {
            return null;
        }
        public IMessageHandler getMessageHandler()
        {
            return null;
        }
        public IExecutionHandler getExecutionHandler()
        {
            return null;
        }

    }
}
