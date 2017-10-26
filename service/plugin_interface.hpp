#pragma once

#include <NSCAPI.h>
#include <nsclient/logger/logger.hpp>

#include <string>

#include <boost/filesystem/path.hpp>
namespace nsclient {
    namespace core {
    class plugin_exception : public std::exception {
      std::string file_;
      std::string error_;
    public:
      //////////////////////////////////////////////////////////////////////////
      /// Constructor takes an error message.
      /// @param error the error message
      ///
      /// @author mickem
      plugin_exception(const std::string &module, const std::string &error) : file_(module), error_(error) {}
      virtual ~plugin_exception() throw() {}

      //////////////////////////////////////////////////////////////////////////
      /// Retrieve the error message from the exception.
      /// @return the error message
      ///
      /// @author mickem
      const char* what() const throw() { return error_.c_str(); }
      const std::string file() const throw() { return file_; }
      std::string reason() const throw() { return error_; }
    };

    class plugin_interface : public nsclient::logging::logging_subscriber {
    private:
      unsigned int plugin_id_;
      std::string alias_;
    public:
      plugin_interface(const unsigned int id, std::string alias)
        : plugin_id_(id)
        , alias_(alias)
      {}
      
	  virtual ~plugin_interface() {}

      virtual bool load_plugin(NSCAPI::moduleLoadMode mode) = 0;
      virtual void unload_plugin() = 0;

      virtual std::string getName() = 0;
      virtual std::string getDescription() = 0;
      virtual std::string get_version() = 0;

      virtual bool hasCommandHandler() = 0;
      virtual NSCAPI::nagiosReturn handleCommand(const std::string request, std::string &reply) = 0;
      virtual bool hasNotificationHandler() = 0;
      virtual NSCAPI::nagiosReturn handleNotification(const char *channel, std::string &request, std::string &reply) = 0;
      virtual NSCAPI::nagiosReturn handle_schedule(const std::string &request) = 0;
      virtual bool hasMessageHandler() = 0;
      virtual void handleMessage(const char* data, unsigned int len) = 0;
      virtual bool has_on_event() = 0;
      virtual NSCAPI::nagiosReturn on_event(const std::string &request) = 0;
      virtual bool hasMetricsFetcher() = 0;
      virtual NSCAPI::nagiosReturn fetchMetrics(std::string &request) = 0;
      virtual bool hasMetricsSubmitter() = 0;
      virtual NSCAPI::nagiosReturn submitMetrics(const std::string &request) = 0;
      virtual bool has_command_line_exec() = 0;
      virtual int commandLineExec(bool targeted, std::string &request, std::string &reply) = 0;
      virtual bool has_routing_handler() = 0;
      virtual bool route_message(const char *channel, const char* buffer, unsigned int buffer_len, char **new_channel_buffer, char **new_buffer, unsigned int *new_buffer_len) = 0;

      virtual bool is_duplicate(boost::filesystem::path file, std::string alias) = 0;
      virtual std::string getModule() = 0;

      std::string get_alias() {
        return alias_;
      }
      std::string get_alias_or_name() {
        if (!alias_.empty())
          return alias_;
        return getModule();
      }
      unsigned int get_id() const {
        return plugin_id_; 
      }

    };
  }
}