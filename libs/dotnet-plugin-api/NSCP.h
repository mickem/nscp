#pragma once

using namespace System;
using namespace System::Collections::Generic;

namespace NSCP {
	namespace Core {
		typedef cli::array<Byte> protobuf_data;

		public ref class Result {
		public:
			Result(protobuf_data^ data) {
				this->data = data;
				this->result = true;
			}
			Result() {
				this->result = false;
			}
			protobuf_data^ data;
			bool result;
		};

		public ref class PluginInstance {
		public:
			PluginInstance(long PluginID_, String^ Alias_) {
				PluginID = PluginID_;
				Alias = Alias_;
			}
			property long PluginID;
			property String^ Alias;
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

			PluginInstance^ getInstance();
		};

		public ref class Codes {
			static int OK = 0;
			static int WARNING = 1;
			static int CRITICAL = 2;
			static int UNKNOWN = 3;
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
			IPlugin^ create(ICore ^core, PluginInstance^ instance);
		};
	}
}
