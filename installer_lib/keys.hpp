// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

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
// The local baseline this install starts from: GENERIC (the default) writes
// the configuration the installer has always written, anything else - "none",
// by convention - writes none of it. Ignored entirely when a management server
// is selected: there the configuration is not ours to write.
#define MONITORING_TOOL L"MONITORING_TOOL"
#define CONFIGURATION_TYPE L"CONFIGURATION_TYPE"
#define CONF_INCLUDES L"CONF_INCLUDES"
#define IMPORT_CONFIG L"IMPORT_CONFIG"

// Where this agent's configuration comes from (ManagementServerDlg):
// MANAGEMENT_SERVER is NONE (configured on this machine), FLEET (an NSClient
// fleet server manages it) or WEB (an nsclient.ini served over HTTP(S)). On a
// silent install it is derived from FLEET_SERVER / CONFIGURATION_TYPE, so a
// command line that predates the property still lands in the right mode.
#define MANAGEMENT_SERVER L"MANAGEMENT_SERVER"
#define MANAGEMENT_SERVER_NONE L"NONE"
#define MANAGEMENT_SERVER_FLEET L"FLEET"
#define MANAGEMENT_SERVER_WEB L"WEB"
// The WEB url as typed on the page, before it becomes CONFIGURATION_TYPE.
#define MANAGEMENT_URL L"MANAGEMENT_URL"
// The one opt-out of verifying who is on the other end, for both managed
// modes: a plain http url, or a server certificate that cannot be verified.
#define MANAGEMENT_INSECURE L"MANAGEMENT_INSECURE"
// Set when the modern layout was this page's idea rather than the operator's,
// so that going Back and answering None can undo it. The migration is one-way
// once it has run, so the undo has to happen before the install does.
#define MGMT_SET_LAYOUT L"MGMT_SET_LAYOUT"
// Set by DetectManagement when the mode was worked out from FLEET_SERVER or
// CONFIGURATION_TYPE rather than asked for by name. Such a command line
// predates this page: it asked for a fleet server or a configuration url, not
// for a different on-disk layout, so it does not get one.
#define MGMT_DERIVED L"MGMT_DERIVED"
// What ApplyManagement refused, shown by ManagementErrorDlg. Empty means the
// values were accepted, so the page may move on.
#define MGMT_ERROR L"MGMT_ERROR"
// Set by DetectManagement when this host already carries an enrollment
// manifest: the page then offers to keep that enrollment instead of asking for
// a server and a one-time token it has no use for.
#define MGMT_ENROLLED L"MGMT_ENROLLED"
#define MGMT_ENROLLED_SERVER L"MGMT_ENROLLED_SERVER"

// Fleet onboarding: enroll this host against an NSClient fleet server during
// install. FLEET_SERVER + FLEET_TOKEN come from the install command generated
// by the fleet server; the rest are escape hatches for unusual deployments.
// These are plain properties (no KEY_/DEFAULT_ prefixes): they are one-shot
// install-time instructions, not configuration we read back and diff.
#define FLEET_SERVER L"FLEET_SERVER"
#define FLEET_TOKEN L"FLEET_TOKEN"
#define FLEET_HOSTNAME L"FLEET_HOSTNAME"
#define FLEET_CA L"FLEET_CA"
#define FLEET_VERIFY_MODE L"FLEET_VERIFY_MODE"
#define FLEET_INSECURE L"FLEET_INSECURE"
// Bundle encryption key(s), base64, comma separated when rotating. Reaches the
// host only this way or via `nscp enroll --bundle-key`: never from the server.
#define FLEET_BUNDLE_KEY L"FLEET_BUNDLE_KEY"
// 1 to refuse every bundle that is not sealed. Stored in the manifest with the
// keys, out of the server's reach.
#define FLEET_REQUIRE_ENCRYPTED_BUNDLES L"FLEET_REQUIRE_ENCRYPTED_BUNDLES"

// Operator-supplied TLS material (GitHub #568): install your own certificate,
// private key and CA into ${certificate-path} instead of letting the service
// generate a self-signed certificate on first use. Like the fleet properties
// these are one-shot install-time instructions (paths to files on the
// installing machine), not configuration we read back and diff.
#define CERTIFICATE L"CERTIFICATE"
#define CERTIFICATE_KEY L"CERTIFICATE_KEY"
#define CERTIFICATE_CA L"CERTIFICATE_CA"

// On-disk layout (experimental): LAYOUT=modern moves the writable state to
// %ProgramData%\NSClient++ and restricts it to SYSTEM and administrators, while
// the program stays in Program Files. A plain property for now, with no UI.
//
// Absent means "keep whatever this host already uses", read from boot.ini - so
// an upgrade never silently moves an installation, and never silently moves a
// modern one back. See docs/design/shared-folder-migration.md.
#define LAYOUT_MODE L"LAYOUT"

#define OP5_SERVER L"OP5_SERVER"
#define OP5_USER L"OP5_USER"
#define OP5_PASSWORD L"OP5_PASSWORD"
#define OP5_HOSTGROUPS L"OP5_HOSTGROUPS"
#define OP5_CONTACTGROUP L"OP5_CONTACTGROUP"

#define BACKUP_FILE L"BACKUP_FILE"
