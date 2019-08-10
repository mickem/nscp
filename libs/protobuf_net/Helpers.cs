using NSCP.Core;
using PB.Log;
using PB.Settings;
using PB.Common;
using PB.Registry;
using System;
using System.IO;
using System.Diagnostics;
using System.Collections.Generic;
using Google.Protobuf;

namespace NSCP.Helpers
{

    public class LogHelper
    {
        private ICore core;
        public LogHelper(ICore core)
        {
            this.core = core;
        }
        public void error(string message)
        {
            StackFrame CallStack = new StackFrame(1, true);
            log(CallStack.GetFileName(), CallStack.GetFileLineNumber(), LogEntry.Types.Entry.Types.Level.LogError, message);
        }
        public void log(string file, int line, LogEntry.Types.Entry.Types.Level level, string message)
        {
            LogEntry newLogEntry = new LogEntry();
            LogEntry.Types.Entry newEntry = new LogEntry.Types.Entry();
            newEntry.Level = level;
            newEntry.Message = message;
            newLogEntry.Entry.Add(newEntry);
            core.log(newLogEntry.ToByteArray());
        }
        public void critical(string message)
        {
            StackFrame CallStack = new StackFrame(1, true);
            log(CallStack.GetFileName(), CallStack.GetFileLineNumber(), LogEntry.Types.Entry.Types.Level.LogCritical, message);
        }
        public void info(string message)
        {
            StackFrame CallStack = new StackFrame(1, true);
            log(CallStack.GetFileName(), CallStack.GetFileLineNumber(), LogEntry.Types.Entry.Types.Level.LogInfo, message);
        }
        public void debug(string message)
        {
            StackFrame CallStack = new StackFrame(1, true);
            log(CallStack.GetFileName(), CallStack.GetFileLineNumber(), LogEntry.Types.Entry.Types.Level.LogDebug, message);
        }
        public void warning(string message)
        {
            StackFrame CallStack = new StackFrame(1, true);
            log(CallStack.GetFileName(), CallStack.GetFileLineNumber(), LogEntry.Types.Entry.Types.Level.LogWarning, message);
        }
    }

    public class SettingsHelper
    {
        private ICore core;
        private LogHelper log;
        private int plugin_id;
        public SettingsHelper(ICore core, int plugin_id)
        {
            this.core = core;
            this.log = new LogHelper(core);
            this.plugin_id = plugin_id;
        }

        public List<string> getKeys(string path)
        {
            List<string> ret = new List<string>();
            SettingsRequestMessage newMessage = new SettingsRequestMessage();
            SettingsRequestMessage.Types.Request.Types.Query newQuery = new SettingsRequestMessage.Types.Request.Types.Query();
            newQuery.Node = new Node();
            newQuery.Node.Path = path;
            newQuery.Recursive = false;
            SettingsRequestMessage.Types.Request request = new SettingsRequestMessage.Types.Request();
            request.PluginId = plugin_id;
            request.Query = newQuery;
            newMessage.Payload.Add(request);

            NSCP.Core.Result res = core.settings(newMessage.ToByteArray());

            if (!res.result)
            {
                log.error("Failed to get value: " + path);
                return ret;
            }
            SettingsResponseMessage response_message = SettingsResponseMessage.Parser.ParseFrom(res.data);
            if (response_message.Payload == null || response_message.Payload.Count == 0 || response_message.Payload[0].Result == null)
            {
                log.error("Failed to get value: " + path);
                return ret;
            }

            if (response_message.Payload[0].Result.Code != PB.Common.Result.Types.StatusCodeType.StatusOk)
            {
                log.error("Failed to get value: " + path);
                return ret;
            }
            foreach (Node node in response_message.Payload[0].Query.Nodes)
            {
                ret.Add(node.Value);
            }
            return ret;
        }


        public string getString(string path, string key, string defaultValue)
        {
            SettingsRequestMessage newMessage = new SettingsRequestMessage();
            SettingsRequestMessage.Types.Request.Types.Query newQuery = new SettingsRequestMessage.Types.Request.Types.Query();
            newQuery.Node = new Node();
            newQuery.Node.Path = path;
            newQuery.Node.Key = key;
            newQuery.DefaultValue = defaultValue;
            SettingsRequestMessage.Types.Request request = new SettingsRequestMessage.Types.Request();
            request.PluginId = plugin_id;
            request.Query = newQuery;
            newMessage.Payload.Add(request);
            NSCP.Core.Result res = core.settings(newMessage.ToByteArray());
            if (!res.result)
            {
                log.error("Failed to get value: " + path);
                return defaultValue;
            }
            SettingsResponseMessage response_message = SettingsResponseMessage.Parser.ParseFrom(res.data);
            if (response_message.Payload[0].Result.Code != PB.Common.Result.Types.StatusCodeType.StatusOk)
            {
                log.error("Failed to get value: " + path);
                return defaultValue;
            }
            return response_message.Payload[0].Query.Node.Value;
        }


        public void setString(string path, string key, string value)
        {
            SettingsRequestMessage newMessage = new SettingsRequestMessage();
            SettingsRequestMessage.Types.Request.Types.Update newQuery = new SettingsRequestMessage.Types.Request.Types.Update();
            newQuery.Node = new Node();
            newQuery.Node.Path = path;
            newQuery.Node.Key = key;
            newQuery.Node.Value = value;
            SettingsRequestMessage.Types.Request request = new SettingsRequestMessage.Types.Request();
            request.PluginId = plugin_id;
            request.Update = newQuery;
            newMessage.Payload.Add(request);
            NSCP.Core.Result res = core.settings(newMessage.ToByteArray());
            if (!res.result)
            {
                log.error("Failed to set value: " + path);
                return;
            }
            SettingsResponseMessage response_message = SettingsResponseMessage.Parser.ParseFrom(res.data);
            if (response_message.Payload[0].Result.Code != PB.Common.Result.Types.StatusCodeType.StatusOk)
            {
                log.error("Failed to set value: " + path);
                return;
            }
        }
        
        public bool load(string context)
        {
            SettingsRequestMessage newMessage = new SettingsRequestMessage();
            SettingsRequestMessage.Types.Request.Types.Control registration_builder = new SettingsRequestMessage.Types.Request.Types.Control();
            registration_builder.Context = context;
            registration_builder.Command = PB.Settings.Command.Load;
            SettingsRequestMessage.Types.Request request = new SettingsRequestMessage.Types.Request();
            request.PluginId = plugin_id;
            request.Control = registration_builder;
            newMessage.Payload.Add(request);
            NSCP.Core.Result res = core.settings(newMessage.ToByteArray());
            if (!res.result)
            {
                log.error("Failed to load: " + context);
                return false;
            }
            SettingsResponseMessage response_message = SettingsResponseMessage.Parser.ParseFrom(res.data);
            if (response_message.Payload[0].Result.Code != PB.Common.Result.Types.StatusCodeType.StatusOk)
            {
                log.error("Failed to load: " + context);
                return false;
            }
            return true;
        }
        public bool save(string context)
        {
            SettingsRequestMessage newMessage = new SettingsRequestMessage();
            SettingsRequestMessage.Types.Request.Types.Control registration_builder = new SettingsRequestMessage.Types.Request.Types.Control();
            registration_builder.Context = context;
            registration_builder.Command = PB.Settings.Command.Save;
            SettingsRequestMessage.Types.Request request = new SettingsRequestMessage.Types.Request();
            request.PluginId = plugin_id;
            request.Control = registration_builder;
            newMessage.Payload.Add(request);
            NSCP.Core.Result res = core.settings(newMessage.ToByteArray());
            if (!res.result)
            {
                log.error("Failed to save: " + context);
                return false;
            }
            SettingsResponseMessage response_message = SettingsResponseMessage.Parser.ParseFrom(res.data);
            if (response_message.Payload[0].Result.Code != PB.Common.Result.Types.StatusCodeType.StatusOk)
            {
                log.error("Failed to save: " + context);
                return false;
            }
            return true;
        }
        public bool reload(string context)
        {
            SettingsRequestMessage newMessage = new SettingsRequestMessage();
            SettingsRequestMessage.Types.Request.Types.Control registration_builder = new SettingsRequestMessage.Types.Request.Types.Control();
            registration_builder.Context = context;
            registration_builder.Command = PB.Settings.Command.Reload;
            SettingsRequestMessage.Types.Request request = new SettingsRequestMessage.Types.Request();
            request.PluginId = plugin_id;
            request.Control = registration_builder;
            newMessage.Payload.Add(request);
            NSCP.Core.Result res = core.settings(newMessage.ToByteArray());
            if (!res.result)
            {
                log.error("Failed to reload: " + context);
                return false;
            }
            SettingsResponseMessage response_message = SettingsResponseMessage.Parser.ParseFrom(res.data);
            if (response_message.Payload[0].Result.Code != PB.Common.Result.Types.StatusCodeType.StatusOk)
            {
                log.error("Failed to reload: " + context);
                return false;
            }
            return true;
        }
        public bool registerPath(string path, string title, string description, bool advanced)
        {
            SettingsRequestMessage newMessage = new SettingsRequestMessage();
            SettingsRequestMessage.Types.Request.Types.Registration registration_builder = new SettingsRequestMessage.Types.Request.Types.Registration();
            registration_builder.Node = new Node();
            registration_builder.Node.Path = path;
            registration_builder.Info = new PB.Settings.Information();
            registration_builder.Info.Title = title;
            registration_builder.Info.Description = description;

            SettingsRequestMessage.Types.Request request = new SettingsRequestMessage.Types.Request();
            request.PluginId = plugin_id;
            request.Registration = registration_builder;
            newMessage.Payload.Add(request);

            NSCP.Core.Result res = core.settings(newMessage.ToByteArray());
            if (!res.result)
            {
                log.error("Failed to describe path: " + path);
                return false;
            }
            SettingsResponseMessage response_message = SettingsResponseMessage.Parser.ParseFrom(res.data);
            if (response_message.Payload == null || response_message.Payload.Count == 0 || response_message.Payload[0].Result == null)
            {
                log.error("Failed to describe path: " + path);
                return false;
            }

            if (response_message.Payload[0].Result.Code != PB.Common.Result.Types.StatusCodeType.StatusOk)
            {
                return false;
            }
            return true;
        }

        public bool registerKey(string path, string key, int type, string title, string description, string defaultValue, bool advanced)
        {

            SettingsRequestMessage newMessage = new SettingsRequestMessage();
            SettingsRequestMessage.Types.Request.Types.Registration registration_builder = new SettingsRequestMessage.Types.Request.Types.Registration();
            registration_builder.Node = new Node();
            registration_builder.Node.Path = path;
            registration_builder.Node.Key = key;
            registration_builder.Info = new PB.Settings.Information();
            registration_builder.Info.Title = title;
            registration_builder.Info.Description = description;
            NSCP.Core.Result res = core.settings(newMessage.ToByteArray());
            if (!res.result)
            {
                log.error("Failed to describe path: " + path);
                return false;
            }
            SettingsResponseMessage response_message = SettingsResponseMessage.Parser.ParseFrom(res.data);
            if (response_message.Payload[0].Result.Code != PB.Common.Result.Types.StatusCodeType.StatusOk)
            {
                log.error("Failed to describe path: " + path);
                return false;
            }
            return true;
        }
    }

    public class RegistryHelper
    {
        private ICore core;
        private LogHelper log;
        private int plugin_id;
        public RegistryHelper(ICore core, int plugin_id)
        {
            this.core = core;
            this.log = new LogHelper(core);
            this.plugin_id = plugin_id;
        }

        public bool registerCommand(string command, string description)
        {
            RegistryRequestMessage newMessage = new RegistryRequestMessage();
            RegistryRequestMessage.Types.Request.Types.Registration newRegistration = new RegistryRequestMessage.Types.Request.Types.Registration();
            newRegistration.Name = command;
            newRegistration.PluginId = plugin_id;
            newRegistration.Type = PB.Registry.ItemType.Query;
            newRegistration.Info = new PB.Registry.Information();
            newRegistration.Info.Description = description;
            RegistryRequestMessage.Types.Request request = new RegistryRequestMessage.Types.Request();
            request.Registration = newRegistration;
            newMessage.Payload.Add(request);

            NSCP.Core.Result response = core.registry(newMessage.ToByteArray());
            if (!response.result)
            {
                log.error("Failed to register: " + command);
                return false;
            }
            RegistryResponseMessage response_message = RegistryResponseMessage.Parser.ParseFrom(response.data);
            if (response_message.Payload[0].Result.Code != PB.Common.Result.Types.StatusCodeType.StatusOk)
            {
                log.error("Failed to register: " + command);
                return false;
            }
            return true;
        }

    }
}
