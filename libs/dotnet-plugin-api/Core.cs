// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

using System;

namespace NSCP.Core
{
    /// <summary>
    /// Result of a call into (or out of) NSClient++: a success flag plus a
    /// serialized protocol buffer message.
    /// </summary>
    public class Result
    {
        public Result(bool result, byte[] data)
        {
            this.result = result;
            this.data = data ?? Array.Empty<byte>();
        }

        public Result(byte[] data) : this(true, data)
        {
        }

        public Result() : this(false, Array.Empty<byte>())
        {
        }

        public byte[] data;
        public bool result;
    }

    /// <summary>
    /// Identity handed to a plugin when it is created: the plugin id NSClient++
    /// knows the hosting module by (use it in registry/settings requests) and
    /// the alias the plugin was configured under.
    /// </summary>
    public class PluginInstance
    {
        public PluginInstance(int pluginId, string alias)
        {
            PluginID = pluginId;
            Alias = alias;
        }

        public int PluginID { get; }
        public string Alias { get; }
    }

    /// <summary>
    /// The NSClient++ core as seen from a plugin. Every call carries a serialized
    /// protocol buffer request (see libs/protobuf/*.proto) and returns the
    /// serialized response.
    /// </summary>
    public interface ICore
    {
        Result query(byte[] request);
        Result exec(string target, byte[] request);
        Result submit(string channel, byte[] request);
        bool reload(string module);

        Result settings(byte[] request);
        Result registry(byte[] request);
        void log(byte[] request);
    }

    /// <summary>Nagios status codes as used in plugin results.</summary>
    public static class Codes
    {
        public const int OK = 0;
        public const int WARNING = 1;
        public const int CRITICAL = 2;
        public const int UNKNOWN = 3;
    }

    public interface IPluginCore : ICore
    {
        PluginInstance getInstance();
    }

    public interface IQueryHandler
    {
        bool isActive();
        /// <param name="command">The command being queried (lower case).</param>
        /// <param name="request">A serialized PB.Commands.QueryRequestMessage.</param>
        /// <returns>A serialized PB.Commands.QueryResponseMessage.</returns>
        Result onQuery(string command, byte[] request);
    }

    public interface ISubmissionHandler
    {
        bool isActive();
        Result onSubmission(string channel, byte[] request);
    }

    public interface IMessageHandler
    {
        bool isActive();
        bool onMessage(byte[] request);
    }

    public interface IExecutionHandler
    {
        bool isActive();
        Result onCommand(string target, string command, byte[] request);
    }

    public class PluginVersion
    {
        public PluginVersion(int major, int minor, int revision)
        {
            this.major = major;
            this.minor = minor;
            this.revision = revision;
        }

        public PluginVersion() : this(0, 0, 0)
        {
        }

        public int major;
        public int minor;
        public int revision;

        public override string ToString()
        {
            return major + "." + minor + "." + revision;
        }
    }

    /// <summary>
    /// A plugin. Created by an <see cref="IPluginFactory"/>, then
    /// <see cref="load"/>ed once NSClient++ has finished booting the module.
    /// Return null from the handler getters for the features the plugin does
    /// not implement.
    /// </summary>
    public interface IPlugin
    {
        bool load(int mode);
        bool unload();

        string getName();
        string getDescription();
        PluginVersion getVersion();

        IQueryHandler getQueryHandler();
        ISubmissionHandler getSubmissionHandler();
        IMessageHandler getMessageHandler();
        IExecutionHandler getExecutionHandler();
    }

    /// <summary>
    /// Entry point of a plugin assembly. NSClient++ instantiates the configured
    /// factory class (default <c>NSCP.Plugin.PluginFactory</c>) with its
    /// parameterless constructor and asks it for the plugin.
    /// </summary>
    public interface IPluginFactory
    {
        IPlugin create(ICore core, PluginInstance instance);
    }
}
