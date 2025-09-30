#define INT_NSCP_ERROR_CONTEXT L"NSCP_ERROR_CONTEXT"
#define INT_NSCP_ERROR L"NSCP_ERROR"
#define INT_LAST_LOG L"NSCP_LAST_LOG"
#define INT_CONF_CAN_CHANGE L"CONF_CAN_CHANGE"
#define INT_CONF_CAN_CHANGE_REASON L"CONF_CAN_CHANGE_REASON"
#define INT_CONF_HAS_ERRORS L"CONF_HAS_ERRORS"

#define ALLOW_CONFIGURATION L"ALLOW_CONFIGURATION"

// Prefixes for properties:
// No prefix: Actual value set on command line
#define PREFIX_NA L""
// KEY_: Value which is currently in use
#define PREFIX_KEY L"KEY_"
// DEFAULT_: Value used by either existing config or default value in the system.
#define PREFIX_DEF L"DEFAULT_"
// When detetmaning which config keys to update only values different from default will be set:
// I.e.:
// XXX=1, config xxx=2, tool says DEFAULT_XXX=3 => xxx=1 as 1 is the wanted value and different from 3
// XXX= , config xxx=2, tool says DEFAULT_XXX=3 => no action as value is same as default.
// XXX=1, config xxx=1, tool says DEFAULT_XXX=2 => no action as value is same as default.
// XXX=1, config xxx=2, tool says DEFAULT_XXX=2 => xxx=1 as 1 is the wanted value and different from 2

// Prefixed properties
#define ALLOWED_HOSTS L"ALLOWED_HOSTS"
#define NSCLIENT_PWD L"NSCLIENT_PWD"
#define CONF_SCHEDULER L"CONF_SCHEDULER"
#define CONF_CHECKS L"CONF_CHECKS"
#define CONF_NRPE L"CONF_NRPE"
#define CONF_NSCA L"CONF_NSCA"
#define CONF_WEB L"CONF_WEB"
#define CONF_NSCLIENT L"CONF_NSCLIENT"
#define CONF_WMI L"CONF_WMI"
#define NRPEMODE L"NRPEMODE"
#define MONITORING_TOOL L"MONITORING_TOOL"
#define CONFIGURATION_TYPE L"CONFIGURATION_TYPE"
#define CONF_INCLUDES L"CONF_INCLUDES"
#define IMPORT_CONFIG L"IMPORT_CONFIG"

#define MONITORING_TOOL_OP5 L"OP5"

#define OP5_SERVER L"OP5_SERVER"
#define OP5_USER L"OP5_USER"
#define OP5_PASSWORD L"OP5_PASSWORD"
#define OP5_HOSTGROUPS L"OP5_HOSTGROUPS"
#define OP5_CONTACTGROUP L"OP5_CONTACTGROUP"

#define BACKUP_FILE L"BACKUP_FILE"
