using NSCP.Core;
using Plugin;
using System;
using System.IO;
using System.Diagnostics;
using System.Collections.Generic;

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
            log(CallStack.GetFileName(), CallStack.GetFileLineNumber(), LogEntry.Types.Entry.Types.Level.LOG_ERROR, message);
        }
        public void log(string file, int line, LogEntry.Types.Entry.Types.Level level, string message)
        {
            LogEntry.Builder newEntry = LogEntry.CreateBuilder();
            newEntry.AddEntry(LogEntry.Types.Entry.CreateBuilder().SetLevel(level).SetMessage(message).Build());
            LogEntry entry = newEntry.Build();
            using (MemoryStream stream = new MemoryStream())
            {
                entry.WriteTo(stream);
                core.log(stream.ToArray());
            }
        }
        public void critical(string message)
        {
            StackFrame CallStack = new StackFrame(1, true);
            log(CallStack.GetFileName(), CallStack.GetFileLineNumber(), LogEntry.Types.Entry.Types.Level.LOG_CRITICAL, message);
        }
        public void info(string message)
        {
            StackFrame CallStack = new StackFrame(1, true);
            log(CallStack.GetFileName(), CallStack.GetFileLineNumber(), LogEntry.Types.Entry.Types.Level.LOG_INFO, message);
        }
        public void debug(string message)
        {
            StackFrame CallStack = new StackFrame(1, true);
            log(CallStack.GetFileName(), CallStack.GetFileLineNumber(), LogEntry.Types.Entry.Types.Level.LOG_DEBUG, message);
        }
        public void warning(string message)
        {
            StackFrame CallStack = new StackFrame(1, true);
            log(CallStack.GetFileName(), CallStack.GetFileLineNumber(), LogEntry.Types.Entry.Types.Level.LOG_WARNING, message);
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
            SettingsRequestMessage.Builder newMessage = SettingsRequestMessage.CreateBuilder();
            SettingsRequestMessage.Types.Request.Types.Query.Builder newQuery = SettingsRequestMessage.Types.Request.Types.Query.CreateBuilder();
            newQuery.SetNode(Settings.Types.Node.CreateBuilder().SetPath(path).Build());
            newQuery.SetRecursive(false);
            newQuery.SetType(Common.Types.DataType.STRING);
            newMessage.AddPayload(SettingsRequestMessage.Types.Request.CreateBuilder().SetPluginId(plugin_id).SetQuery(newQuery.Build()).Build());
            System.IO.MemoryStream stream = new System.IO.MemoryStream();
            newMessage.Build().WriteTo(stream);
            NSCP.Core.Result res = core.settings(stream.ToArray());
            if (!res.result)
            {
                log.error("Failed to get value: " + path);
                return ret;
            }
            SettingsResponseMessage response_message = SettingsResponseMessage.ParseFrom(res.data);
            if (response_message.GetPayload(0).Result.Code != Common.Types.Result.Types.StatusCodeType.STATUS_OK)
            {
                log.error("Failed to get value: " + path);
                return ret;
            }
            foreach (string value in response_message.GetPayload(0).Query.Value.ListDataList)
            {
                ret.Add(value);
            }
            return ret;
        }


        public string getString(string path, string key, string defaultValue)
        {
            SettingsRequestMessage.Builder newMessage = SettingsRequestMessage.CreateBuilder();
            SettingsRequestMessage.Types.Request.Types.Query.Builder newQuery = SettingsRequestMessage.Types.Request.Types.Query.CreateBuilder();
            newQuery.SetNode(Settings.Types.Node.CreateBuilder().SetPath(path).SetKey(key).Build());
            newQuery.SetDefaultValue(Common.Types.AnyDataType.CreateBuilder().SetStringData(defaultValue).Build());
            newQuery.SetType(Common.Types.DataType.STRING);
            newMessage.AddPayload(SettingsRequestMessage.Types.Request.CreateBuilder().SetPluginId(plugin_id).SetQuery(newQuery.Build()).Build());
            System.IO.MemoryStream stream = new System.IO.MemoryStream();
            newMessage.Build().WriteTo(stream);
            NSCP.Core.Result res = core.settings(stream.ToArray());
            if (!res.result)
            {
                log.error("Failed to get value: " + path);
                return defaultValue;
            }
            SettingsResponseMessage response_message = SettingsResponseMessage.ParseFrom(res.data);
            if (response_message.GetPayload(0).Result.Code != Common.Types.Result.Types.StatusCodeType.STATUS_OK)
            {
                log.error("Failed to get value: " + path);
                return defaultValue;
            }
            return response_message.GetPayload(0).Query.Value.StringData;
        }


        public void setString(string path, string key, string value)
        {
            SettingsRequestMessage.Builder newMessage = SettingsRequestMessage.CreateBuilder();
            SettingsRequestMessage.Types.Request.Types.Update.Builder newQuery = SettingsRequestMessage.Types.Request.Types.Update.CreateBuilder();
            newQuery.SetNode(Settings.Types.Node.CreateBuilder().SetPath(path).SetKey(key).Build());
            newQuery.SetValue(Common.Types.AnyDataType.CreateBuilder().SetStringData(value).Build());
            newMessage.AddPayload(SettingsRequestMessage.Types.Request.CreateBuilder().SetPluginId(plugin_id).SetUpdate(newQuery.Build()).Build());
            System.IO.MemoryStream stream = new System.IO.MemoryStream();
            newMessage.Build().WriteTo(stream);
            NSCP.Core.Result res = core.settings(stream.ToArray());
            if (!res.result)
            {
                log.error("Failed to set value: " + path);
                return;
            }
            SettingsResponseMessage response_message = SettingsResponseMessage.ParseFrom(res.data);
            if (response_message.GetPayload(0).Result.Code != Common.Types.Result.Types.StatusCodeType.STATUS_OK)
            {
                log.error("Failed to set value: " + path);
                return;
            }
        }

        public bool getBool(string path, string key, bool defaultValue)
        {
            SettingsRequestMessage.Builder newMessage = SettingsRequestMessage.CreateBuilder();
            SettingsRequestMessage.Types.Request.Types.Query.Builder newQuery = SettingsRequestMessage.Types.Request.Types.Query.CreateBuilder();
            newQuery.SetNode(Settings.Types.Node.CreateBuilder().SetPath(path).SetKey(key).Build());
            newQuery.SetDefaultValue(Common.Types.AnyDataType.CreateBuilder().SetBoolData(defaultValue).Build());
            newQuery.SetType(Common.Types.DataType.BOOL);
            newMessage.AddPayload(SettingsRequestMessage.Types.Request.CreateBuilder().SetPluginId(plugin_id).SetQuery(newQuery.Build()).Build());
            System.IO.MemoryStream stream = new System.IO.MemoryStream();
            newMessage.Build().WriteTo(stream);
            NSCP.Core.Result res = core.settings(stream.ToArray());
            if (!res.result)
            {
                log.error("Failed to get value: " + path);
                return defaultValue;
            }
            SettingsResponseMessage response_message = SettingsResponseMessage.ParseFrom(res.data);
            if (response_message.GetPayload(0).Result.Code != Common.Types.Result.Types.StatusCodeType.STATUS_OK)
            {
                log.error("Failed to get value: " + path);
                return defaultValue;
            }
            return response_message.GetPayload(0).Query.Value.BoolData;
        }


        public void setBool(string path, string key, bool value)
        {
            SettingsRequestMessage.Builder newMessage = SettingsRequestMessage.CreateBuilder();
            SettingsRequestMessage.Types.Request.Types.Update.Builder newQuery = SettingsRequestMessage.Types.Request.Types.Update.CreateBuilder();
            newQuery.SetNode(Settings.Types.Node.CreateBuilder().SetPath(path).SetKey(key).Build());
            newQuery.SetValue(Common.Types.AnyDataType.CreateBuilder().SetBoolData(value).Build());
            newMessage.AddPayload(SettingsRequestMessage.Types.Request.CreateBuilder().SetPluginId(plugin_id).SetUpdate(newQuery.Build()).Build());
            System.IO.MemoryStream stream = new System.IO.MemoryStream();
            newMessage.Build().WriteTo(stream);
            NSCP.Core.Result res = core.settings(stream.ToArray());
            if (!res.result)
            {
                log.error("Failed to set value: " + path);
                return;
            }
            SettingsResponseMessage response_message = SettingsResponseMessage.ParseFrom(res.data);
            if (response_message.GetPayload(0).Result.Code != Common.Types.Result.Types.StatusCodeType.STATUS_OK)
            {
                log.error("Failed to set value: " + path);
                return;
            }
        }

        public long getInt(string path, string key, int defaultValue)
        {
            SettingsRequestMessage.Builder newMessage = SettingsRequestMessage.CreateBuilder();
            SettingsRequestMessage.Types.Request.Types.Query.Builder newQuery = SettingsRequestMessage.Types.Request.Types.Query.CreateBuilder();
            newQuery.SetNode(Settings.Types.Node.CreateBuilder().SetPath(path).SetKey(key).Build());
            newQuery.SetDefaultValue(Common.Types.AnyDataType.CreateBuilder().SetIntData(defaultValue).Build());
            newQuery.SetType(Common.Types.DataType.INT);
            newMessage.AddPayload(SettingsRequestMessage.Types.Request.CreateBuilder().SetPluginId(plugin_id).SetQuery(newQuery.Build()).Build());
            System.IO.MemoryStream stream = new System.IO.MemoryStream();
            newMessage.Build().WriteTo(stream);
            NSCP.Core.Result res = core.settings(stream.ToArray());
            if (!res.result)
            {
                log.error("Failed to get value: " + path);
                return defaultValue;
            }
            SettingsResponseMessage response_message = SettingsResponseMessage.ParseFrom(res.data);
            if (response_message.GetPayload(0).Result.Code != Common.Types.Result.Types.StatusCodeType.STATUS_OK)
            {
                log.error("Failed to get value: " + path);
                return defaultValue;
            }
            return response_message.GetPayload(0).Query.Value.IntData;
        }

        public void setInt(string path, string key, int value)
        {
            SettingsRequestMessage.Builder newMessage = SettingsRequestMessage.CreateBuilder();
            SettingsRequestMessage.Types.Request.Types.Update.Builder newQuery = SettingsRequestMessage.Types.Request.Types.Update.CreateBuilder();
            newQuery.SetNode(Settings.Types.Node.CreateBuilder().SetPath(path).SetKey(key).Build());
            newQuery.SetValue(Common.Types.AnyDataType.CreateBuilder().SetIntData(value).Build());
            newMessage.AddPayload(SettingsRequestMessage.Types.Request.CreateBuilder().SetPluginId(plugin_id).SetUpdate(newQuery.Build()).Build());
            System.IO.MemoryStream stream = new System.IO.MemoryStream();
            newMessage.Build().WriteTo(stream);
            NSCP.Core.Result res = core.settings(stream.ToArray());
            if (!res.result)
            {
                log.error("Failed to set value: " + path);
                return;
            }
            SettingsResponseMessage response_message = SettingsResponseMessage.ParseFrom(res.data);
            if (response_message.GetPayload(0).Result.Code != Common.Types.Result.Types.StatusCodeType.STATUS_OK)
            {
                log.error("Failed to set value: " + path);
                return;
            }
        }

        public bool load(string context)
        {
            SettingsRequestMessage.Builder newMessage = SettingsRequestMessage.CreateBuilder();
            SettingsRequestMessage.Types.Request.Types.Control.Builder registration_builder = SettingsRequestMessage.Types.Request.Types.Control.CreateBuilder();
            registration_builder.SetContext(context);
            registration_builder.SetCommand(Settings.Types.Command.LOAD);
            newMessage.AddPayload(SettingsRequestMessage.Types.Request.CreateBuilder().SetPluginId(plugin_id).SetControl(registration_builder.Build()).Build());
            SettingsRequestMessage message = newMessage.Build();
            System.IO.MemoryStream stream = new System.IO.MemoryStream();
            message.WriteTo(stream);
            NSCP.Core.Result res = core.settings(stream.ToArray());
            if (!res.result)
            {
                log.error("Failed to save: " + context);
                return false;
            }
            SettingsResponseMessage response_message = SettingsResponseMessage.ParseFrom(res.data);
            if (response_message.GetPayload(0).Result.Code != Common.Types.Result.Types.StatusCodeType.STATUS_OK)
            {
                log.error("Failed to save: " + context);
                return false;
            }
            return true;
        }
        public bool save(string context)
        {
            SettingsRequestMessage.Builder newMessage = SettingsRequestMessage.CreateBuilder();
            SettingsRequestMessage.Types.Request.Types.Control.Builder registration_builder = SettingsRequestMessage.Types.Request.Types.Control.CreateBuilder();
            registration_builder.SetContext(context);
            registration_builder.SetCommand(Settings.Types.Command.SAVE);
            newMessage.AddPayload(SettingsRequestMessage.Types.Request.CreateBuilder().SetPluginId(plugin_id).SetControl(registration_builder.Build()).Build());
            SettingsRequestMessage message = newMessage.Build();
            System.IO.MemoryStream stream = new System.IO.MemoryStream();
            message.WriteTo(stream);
            NSCP.Core.Result res = core.settings(stream.ToArray());
            if (!res.result)
            {
                log.error("Failed to save: " + context);
                return false;
            }
            SettingsResponseMessage response_message = SettingsResponseMessage.ParseFrom(res.data);
            if (response_message.GetPayload(0).Result.Code != Common.Types.Result.Types.StatusCodeType.STATUS_OK)
            {
                log.error("Failed to save: " + context);
                return false;
            }
            return true;
        }
        public bool reload(string context)
        {
            SettingsRequestMessage.Builder newMessage = SettingsRequestMessage.CreateBuilder();
            SettingsRequestMessage.Types.Request.Types.Control.Builder registration_builder = SettingsRequestMessage.Types.Request.Types.Control.CreateBuilder();
            registration_builder.SetContext(context);
            registration_builder.SetCommand(Settings.Types.Command.RELOAD);
            newMessage.AddPayload(SettingsRequestMessage.Types.Request.CreateBuilder().SetPluginId(plugin_id).SetControl(registration_builder.Build()).Build());
            SettingsRequestMessage message = newMessage.Build();
            System.IO.MemoryStream stream = new System.IO.MemoryStream();
            message.WriteTo(stream);
            NSCP.Core.Result res = core.settings(stream.ToArray());
            if (!res.result)
            {
                log.error("Failed to save: " + context);
                return false;
            }
            SettingsResponseMessage response_message = SettingsResponseMessage.ParseFrom(res.data);
            if (response_message.GetPayload(0).Result.Code != Common.Types.Result.Types.StatusCodeType.STATUS_OK)
            {
                log.error("Failed to save: " + context);
                return false;
            }
            return true;
        }
        public bool registerPath(string path, string title, string description, bool advanced)
        {
            SettingsRequestMessage.Builder newMessage = SettingsRequestMessage.CreateBuilder();
            SettingsRequestMessage.Types.Request.Types.Registration.Builder registration_builder = SettingsRequestMessage.Types.Request.Types.Registration.CreateBuilder();
            registration_builder.SetNode(Settings.Types.Node.CreateBuilder().SetPath(path).Build());
            registration_builder.SetInfo(Settings.Types.Information.CreateBuilder().SetTitle(title).SetDescription(description).Build());
            newMessage.AddPayload(SettingsRequestMessage.Types.Request.CreateBuilder().SetPluginId(plugin_id).SetRegistration(registration_builder.Build()).Build());
            SettingsRequestMessage message = newMessage.Build();
            System.IO.MemoryStream stream = new System.IO.MemoryStream();
            message.WriteTo(stream);
            NSCP.Core.Result res = core.settings(stream.ToArray());
            if (!res.result)
            {
                log.error("Failed to describe path: " + path);
                return false;
            }
            SettingsResponseMessage response_message = SettingsResponseMessage.ParseFrom(res.data);
            if (response_message.GetPayload(0).Result.Code != Common.Types.Result.Types.StatusCodeType.STATUS_OK)
            {
                log.error("Failed to describe path: " + path);
                return false;
            }
            return true;

        }

        public bool registerKey(string path, string key, int type, string title, string description, string defaultValue, bool advanced)
        {
            SettingsRequestMessage.Builder newMessage = SettingsRequestMessage.CreateBuilder();
            SettingsRequestMessage.Types.Request.Types.Registration.Builder registration_builder = SettingsRequestMessage.Types.Request.Types.Registration.CreateBuilder();
            registration_builder.SetNode(Settings.Types.Node.CreateBuilder().SetPath(path).SetKey(key).Build());
            registration_builder.SetInfo(Settings.Types.Information.CreateBuilder().SetTitle(title).SetDescription(description).Build());
            newMessage.AddPayload(SettingsRequestMessage.Types.Request.CreateBuilder().SetPluginId(plugin_id).SetRegistration(registration_builder.Build()).Build());
            SettingsRequestMessage message = newMessage.Build();
            System.IO.MemoryStream stream = new System.IO.MemoryStream();
            message.WriteTo(stream);
            NSCP.Core.Result res = core.settings(stream.ToArray());
            if (!res.result)
            {
                log.error("Failed to describe key: " + path);
                return false;
            }
            SettingsResponseMessage response_message = SettingsResponseMessage.ParseFrom(res.data);
            if (response_message.GetPayload(0).Result.Code != Common.Types.Result.Types.StatusCodeType.STATUS_OK)
            {
                log.error("Failed to describe key: " + path);
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
            RegistryRequestMessage.Builder newMessage = RegistryRequestMessage.CreateBuilder();
            RegistryRequestMessage.Types.Request.Types.Registration.Builder newRegistration = RegistryRequestMessage.Types.Request.Types.Registration.CreateBuilder();
            newRegistration.SetName(command);
            newRegistration.SetPluginId(plugin_id);
            newRegistration.SetType(Registry.Types.ItemType.QUERY);
            newRegistration.SetInfo(Registry.Types.Information.CreateBuilder().SetDescription(description).Build());
            newMessage.AddPayload(RegistryRequestMessage.Types.Request.CreateBuilder().SetRegistration(newRegistration.Build()).Build());

            NSCP.Core.Result response;
            using (MemoryStream stream = new MemoryStream())
            {
                newMessage.Build().WriteTo(stream);
                response = core.registry(stream.ToArray());
            }
            if (!response.result)
            {
                log.error("Failed to register: " + command);
                return false;
            }
            RegistryResponseMessage response_message = RegistryResponseMessage.ParseFrom(response.data);
            if (response_message.GetPayload(0).Result.Code != Common.Types.Result.Types.StatusCodeType.STATUS_OK)
            {
                log.error("Failed to register: " + command);
                return false;
            }
            return true;
        }

    }
}
