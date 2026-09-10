// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Sample NSClient++ plugin written in C#. It exercises every handler the
// contract offers:
//   * a query (check command)        check_dotnet   -> "Hello from C#"
//   * a command-line command         dotnet_hello   -> submits a passive
//                                                     result to its own channel
//   * a channel (passive results)    dotnet_sample  -> logged when received
//   * the log message stream         counted, reported by dotnet_hello
using System;
using System.Threading;
using Google.Protobuf;
using NSCP.Core;
using NSCP.Helpers;

namespace NSCP.Plugin
{
    public class PluginFactory : IPluginFactory
    {
        public IPlugin create(ICore core, PluginInstance instance)
        {
            return new Sample.SamplePlugin(core, instance);
        }
    }
}

namespace NSCP.Plugin.Sample
{
    public class SamplePlugin : IPlugin
    {
        public const string Channel = "dotnet_sample";

        private readonly ICore core;
        private readonly PluginInstance instance;
        private readonly LogHelper log;
        private readonly QueryHandler queries;
        private readonly SubmissionHandler submissions;
        private readonly ExecutionHandler commands;
        private readonly MessageHandler messages;

        public SamplePlugin(ICore core, PluginInstance instance)
        {
            this.core = core;
            this.instance = instance;
            this.log = new LogHelper(core);
            this.messages = new MessageHandler();
            this.queries = new QueryHandler(log);
            this.submissions = new SubmissionHandler(log);
            this.commands = new ExecutionHandler(core, log, messages);
        }

        public bool load(int mode)
        {
            // Settings are read through the same protobuf channel native modules use.
            long port = Int64.Parse(new SettingsHelper(core, instance.PluginID).getString("/settings/WEB/server", "port", "1234"));
            log.info("Webserver port is: " + port);
            var registry = new RegistryHelper(core, instance.PluginID);
            registry.registerCommand("check_dotnet", "This is a sample command written in C#");
            registry.registerExecCommand("dotnet_hello", "Says hello and submits a passive result to the " + Channel + " channel");
            registry.registerChannel(Channel);
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
            return new PluginVersion(0, 0, 2);
        }

        public IQueryHandler getQueryHandler()
        {
            return queries;
        }

        public ISubmissionHandler getSubmissionHandler()
        {
            return submissions;
        }

        public IMessageHandler getMessageHandler()
        {
            return messages;
        }

        public IExecutionHandler getExecutionHandler()
        {
            return commands;
        }
    }

    public class QueryHandler : IQueryHandler
    {
        private readonly LogHelper log;

        public QueryHandler(LogHelper log)
        {
            this.log = log;
        }

        public bool isActive()
        {
            return true;
        }

        public Result onQuery(string command, byte[] request)
        {
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
            return new Result(response_message.ToByteArray());
        }
    }

    public class SubmissionHandler : ISubmissionHandler
    {
        private readonly LogHelper log;

        public SubmissionHandler(LogHelper log)
        {
            this.log = log;
        }

        public bool isActive()
        {
            return true;
        }

        public Result onSubmission(string channel, byte[] request)
        {
            var message = PB.Commands.SubmitRequestMessage.Parser.ParseFrom(request);
            var reply = new PB.Commands.SubmitResponseMessage();
            foreach (var payload in message.Payload)
            {
                var text = payload.Lines.Count > 0 ? payload.Lines[0].Message : "";
                log.info("Received submission on " + channel + ": " + payload.Command + " " + payload.Result + " " + text);
                var response = new PB.Commands.SubmitResponseMessage.Types.Response();
                response.Command = payload.Command;
                response.Result = new PB.Common.Result { Code = PB.Common.Result.Types.StatusCodeType.StatusOk, Message = "Received by C#" };
                reply.Payload.Add(response);
            }
            return new Result(reply.ToByteArray());
        }
    }

    public class MessageHandler : IMessageHandler
    {
        private int seen;

        public int Seen
        {
            get { return Volatile.Read(ref seen); }
        }

        public bool isActive()
        {
            return true;
        }

        public bool onMessage(byte[] request)
        {
            // Never log from a message handler: the entry would come straight back.
            var entry = PB.Log.LogEntry.Parser.ParseFrom(request);
            Interlocked.Add(ref seen, entry.Entry.Count);
            return true;
        }
    }

    public class ExecutionHandler : IExecutionHandler
    {
        private readonly ICore core;
        private readonly LogHelper log;
        private readonly MessageHandler messages;

        public ExecutionHandler(ICore core, LogHelper log, MessageHandler messages)
        {
            this.core = core;
            this.log = log;
            this.messages = messages;
        }

        public bool isActive()
        {
            return true;
        }

        public Result onCommand(string target, string command, byte[] request)
        {
            var message = PB.Commands.ExecuteRequestMessage.Parser.ParseFrom(request);
            log.debug("Got exec command: " + command);

            // Submit a passive result to our own channel: shows a plugin talking
            // to the core, and the core routing a submission back into .NET.
            var submission = new PB.Commands.SubmitRequestMessage { Channel = SamplePlugin.Channel };
            var result = new PB.Commands.QueryResponseMessage.Types.Response { Command = "check_dotnet", Result = PB.Common.ResultCode.Ok };
            result.Lines.Add(new PB.Commands.QueryResponseMessage.Types.Response.Types.Line { Message = "Hello from C#" });
            submission.Payload.Add(result);
            var submitted = core.submit(SamplePlugin.Channel, submission.ToByteArray());
            var status = "failed";
            if (submitted.result && submitted.data.Length > 0)
            {
                var reply = PB.Commands.SubmitResponseMessage.Parser.ParseFrom(submitted.data);
                status = reply.Payload.Count > 0 && reply.Payload[0].Result != null ? reply.Payload[0].Result.Message : "no answer";
            }

            var response_message = new PB.Commands.ExecuteResponseMessage();
            var response = new PB.Commands.ExecuteResponseMessage.Types.Response();
            response.Command = command;
            response.Result = PB.Common.ResultCode.Ok;
            response.Message = "Hello exec from C# (log entries seen: " + messages.Seen + ", submit: " + status + ")";
            response_message.Payload.Add(response);
            return new Result(response_message.ToByteArray());
        }
    }
}
