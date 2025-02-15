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
            PB.Commands.QueryRequestMessage request_message = PB.Commands.QueryRequestMessage.Parser.ParseFrom(request);
            log.debug("Got command: " + command);

            PB.Commands.QueryResponseMessage response_message = new PB.Commands.QueryResponseMessage();
            PB.Commands.QueryResponseMessage.Types.Response response = new PB.Commands.QueryResponseMessage.Types.Response();
            response.Command = command;
            response.Result = PB.Common.ResultCode.Ok;
            PB.Commands.QueryResponseMessage.Types.Response.Types.Line line = new PB.Commands.QueryResponseMessage.Types.Response.Types.Line();
            line.Message = "Hello from C#";
            response.Lines.Add(line);
            response_message.Payload.Add(response);

            System.IO.MemoryStream stream = new System.IO.MemoryStream();
            response_message.WriteTo(new Google.Protobuf.CodedOutputStream(stream));
            return new Result(stream.ToArray());
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
            long port = Int64.Parse(new SettingsHelper(core, plugin_id).getString("/settings/WEB/server", "port", "1234"));
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
