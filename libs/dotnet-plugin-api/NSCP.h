#pragma once

using namespace System;
using namespace System::Collections::Generic;

namespace NSCP {

	ref class SettingsKeyType {
		const static int STRING = 100;
		const static int BOOLEAN = 300;
		const static int NUMBER = 200;
	};

	public interface class ISettings {
		array<String^>^ getSection(String^ path);
		String^ getString(String^ path, String ^key, String ^defaultValue);
		void setString(String^ path, String ^key, String ^theValue);
		bool getBool(String^ path, String ^key, bool defaultValue);
		void setBool(String^ path, String ^key, bool theValue);
		int getInt(String^ path, String ^key, int defaultValue);
		void setInt(String^ path, String ^key, int theValue);
		bool save();
		bool registerPath(String^ path, String^ title, String^ description, bool advanced);
		bool register_key(String^ path, String^ key, int type, String^ title, String^ description, String^ defaultValue, bool advanced);

	};

	public ref class Codes {
		static int OK = 0;
		static int WARNING = 1;
		static int CRITICAL = 2;
		static int UNKNOWN = 3;
	};

	public interface class IRegistry {
		bool registerCommand(String^ command, String^ description);
		bool subscribeChannel(String^ channel);
	};

	public interface class ILogger {
		void debug(String^ message);
		void info(String^ message);
		void warn(String^ message);
		void error(String^ message);
		void fatal(String^ message);
	};

	typedef cli::array<Byte> protobuf_data;

	public ref class Result {
	public:
		protobuf_data^ data;
		int result;
	};
	public interface class ICore {
	public:
		Result^ query(protobuf_data^ request);
		Result^ exec(String^ target, protobuf_data^ request);
		Result^ submit(String^ channel, protobuf_data^ request);
		bool reload(String^ module);

		Result^ settings(protobuf_data^ request);
		Result^ registry(protobuf_data^ request);
		void log(protobuf_data^ request);

		ILogger^ getLogger();
		IRegistry^ getRegistry();
		ISettings^ getSettings();
	};


	public interface class IQueryHandler {
	public:
		bool isActive();
		Result^ onQuery(String^ command, protobuf_data^ request);
	};

	public interface class ISubmissionHandler {
	public:
		bool isActive();
		Result^ onSubmission(String^ channel, protobuf_data^ request);
	};

	public interface class IMessageHandler {
	public:
		bool isActive();
		bool onMessage(cli::array<char>^ request);
	};

	public interface class IExecutionHandler {
	public:
		bool isActive();
		Result^ onCommand(String^target, String^ command, protobuf_data^ request);
	};

	public ref class PluginVersion {
	public:
		PluginVersion(int major, int minor, int revision) : major(major), minor(minor), revision(revision) {}
		PluginVersion() : major(0), minor(0), revision(0) {}
		int major;
		int minor;
		int revision;
	};

	public interface class IPlugin {
		bool load(int mode);
		bool unload();

		String^ getName();
		String^ getDescription();
		PluginVersion^ getVersion();

		IQueryHandler^ getQueryHandler();
		ISubmissionHandler^ getSubmissionHandler();
		IMessageHandler^ getMessageHandler();
		IExecutionHandler^ getExecutionHandler();
	};

	public interface class IPluginFactory {
		IPlugin^ create(ICore ^core, int plugin_id, String^ alias);
	};
}
