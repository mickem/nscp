use clap::{Args, Parser, Subcommand, ValueEnum};

/// Define the available output formats
#[derive(ValueEnum, Clone, Debug)]
pub enum OutputFormat {
    Text,
    Json,
    Yaml,
    Csv,
}

#[derive(ValueEnum, Clone, Debug)]
pub enum OutputStyle {
    Rounded,
    Blank,
    Markdown,
}

fn parse_kv_option(raw: &str) -> Result<(String, String), String> {
    let trimmed = raw.trim();
    if trimmed.is_empty() {
        return Err("expected KEY[=VALUE]".to_string());
    }
    let (raw_key, value_part) = match trimmed.split_once('=') {
        Some((key, value)) => (key, Some(value)),
        None => (trimmed, None),
    };
    let key = raw_key.trim_start_matches('-').trim();
    if key.is_empty() {
        return Err("option name cannot be empty".to_string());
    }
    let value = value_part
        .map(|v| v.trim().to_string())
        .unwrap_or_else(|| "".to_string());
    Ok((key.to_string(), value))
}

#[derive(Parser)]
#[command(name = "check_nsclient")]
#[command(author)]
#[command(version)]
#[command(about = "NSClient command line client", long_about = None)]
pub struct Cli {
    /// Turn debugging information on
    #[arg(short, long, action = clap::ArgAction::Count)]
    pub(crate) debug: u8,

    /// Show more information
    #[arg(long, value_enum)]
    pub output_long: bool,

    /// Set the output style (if format is text)
    #[arg(long, value_enum, default_value_t = OutputStyle::Rounded)]
    pub output_style: OutputStyle,

    /// Set the output format (text, json, yaml or csv)
    #[arg(long, value_enum, default_value_t = OutputFormat::Text)]
    pub output: OutputFormat,

    /// Use WSL workarounds for keyring (token storage)
    #[arg(long)]
    pub(crate) wsl: bool,

    #[command(subcommand)]
    pub(crate) command: Commands,
}

#[derive(Subcommand)]
pub enum NSClientCommands {
    /// Send a ping to ensure NSClient++ can be reached
    Ping {},
    /// Show version
    Version {},
    /// Manage modules
    Modules {
        #[command(subcommand)]
        command: ModulesCommand,
    },
    /// Execute and show queries
    Queries {
        #[command(subcommand)]
        command: QueriesCommand,
    },
    /// Inspect/acknowledge logs
    Logs {
        #[command(subcommand)]
        command: LogsCommand,
    },
    /// Manage scripts
    Scripts {
        #[command(subcommand)]
        command: ScriptsCommand,
    },
    /// Inspect settings
    Settings {
        #[command(subcommand)]
        command: SettingsCommand,
    },
    /// Metrics & health surfaces
    Metrics {
        #[command(subcommand)]
        command: MetricsCommand,
    },
    /// Auth / session helpers
    Auth {
        #[command(subcommand)]
        command: AuthCommand,
    },
    /// Legacy command (same as client)
    Test {},
    /// Connect to and interact with NSClient
    Client {},
}
#[derive(Args)]
pub struct NSClientCommandOptions {
    /// The timeout in seconds
    #[arg(short, long, default_value_t = 30)]
    pub(crate) timeout_s: u64,
    /// The user agent to use
    #[arg(short = 'A', long, default_value = "nscp-client")]
    pub(crate) user_agent: String,
    /// The profile to connect to
    #[arg(short = 'p', long)]
    pub(crate) profile: Option<String>,
    /// The subcommand to run
    #[command(subcommand)]
    pub(crate) command: NSClientCommands,
}

#[derive(Subcommand)]
pub enum ProfileCommands {
    /// List all profiles
    List {},
    /// Show details about a profile
    Show { id: String },
    /// Set the default profile
    SetDefault { id: String },
    /// Remove a profile
    Remove { id: String },
}

#[derive(Subcommand)]
pub enum Commands {
    /// Communicate with NSClient
    #[command(name = "nsclient")]
    NSClient(NSClientCommandOptions),
    /// Show version
    Version {},
    /// Manage profiles
    Profile {
        #[command(subcommand)]
        command: ProfileCommands,
    },
}

#[derive(Subcommand)]
pub enum ModulesCommand {
    /// List modules
    List {
        /// List all modules (not just loaded modules)
        #[arg(short, long)]
        all: bool,
        /// Show all information (same as --output-long)
        #[arg(short, long)]
        long: bool,
    },
    /// Show details about a module
    Show { id: String },
    /// Load a module so it can be used.
    Load { id: String },
    /// Unload a module so it cannot be used.
    Unload { id: String },
    /// Enable a module so it will be loaded on startup.
    Enable { id: String },
    /// Disable a module so it will no longer be loaded on startup.
    Disable { id: String },
    /// Load and enable a module so it can be used.
    Use { id: String },
}

#[derive(Subcommand)]
pub enum QueriesCommand {
    /// List queries
    List {
        /// List all queries (not just loaded queries)
        #[arg(short, long)]
        all: bool,
        /// Show all information (same as --output-long)
        #[arg(short, long)]
        long: bool,
    },
    /// Show details about a query
    Show { id: String },
    /// Execute a query (show output)
    #[command(trailing_var_arg = true)]
    Execute {
        id: String,
        /// Arguments to pass to the query (use KEY=VALUE or bare KEY for boolean flags)
        #[arg(value_name = "KEY=VALUE", value_parser = parse_kv_option)]
        args: Vec<(String, String)>,
    },
    /// Execute a query (Nagios compatible output)
    #[command(trailing_var_arg = true)]
    ExecuteNagios {
        id: String,
        /// Additional query options (use key=value, values keep order specified)
        #[arg(value_name = "KEY=VALUE", value_parser = parse_kv_option)]
        args: Vec<(String, String)>,
    },
}

#[derive(Subcommand)]
pub enum LogsCommand {
    /// List log records (paginated)
    List {
        /// Page number (starts at 0)
        #[arg(long, default_value_t = 1u64)]
        page: u64,
        /// Page size
        #[arg(long, default_value_t = 50u64)]
        size: u64,
        /// Filter by level (INFO/WARNING/ERROR/...)
        #[arg(long)]
        level: Option<String>,
        /// Show file/line columns
        #[arg(short, long)]
        long: bool,
    },
    /// Show current log counter status
    Status {},
    /// Reset aggregated log status counters
    Reset {},
}

#[derive(Subcommand)]
pub enum ScriptsCommand {
    /// List scripts
    ListRuntimes {},
    List {
        #[arg(long)]
        runtime: String,
    },
}

#[derive(Subcommand)]
pub enum SettingsCommand {
    /// Summary if settings are dirty
    Status {},
    /// List settings entries
    List {},
    /// Show setting descriptions
    Descriptions {
        /// Show all information (same as --output-long)
        #[arg(short, long)]
        long: bool,
    },
    /// Update a setting value
    Set {
        /// Path of the setting (section)
        #[arg(long)]
        path: String,
        /// Key of the setting
        #[arg(long)]
        key: String,
        /// New value
        #[arg(long)]
        value: String,
    },
    /// Issue settings command (load/save/reload)
    Command {
        #[arg(value_enum)]
        action: SettingsCommandActionCli,
    },
}

#[derive(Clone, Copy, Debug, ValueEnum)]
pub enum SettingsCommandActionCli {
    Load,
    Save,
    Reload,
}

#[derive(Subcommand)]
pub enum MetricsCommand {
    /// Dump Prometheus style metrics
    Show {},
}

#[derive(Subcommand)]
pub enum AuthCommand {
    /// Login and store token
    Login {
        /// Profile ID to store the token under
        #[arg(default_value = "default")]
        id: String,
        /// NSClient++ URL
        #[arg(long, default_value = "https://localhost:8443")]
        url: String,
        /// Username to login with
        #[arg(long, default_value = "admin")]
        username: String,
        /// Password to login with
        #[arg(long)]
        password: String,
        /// Allow insecure TLS connections (i.e. dont validate certificate)
        #[arg(long)]
        insecure: bool,
        /// CA File to use for TLS connections
        #[arg(long)]
        ca: Option<String>,
    },
    /// Logout and forget stored token
    Logout { id: String },
    /// Refresh the api key (using the stored password)
    Refresh {
        /// Profile ID of profile to refresh token
        #[arg(default_value = "default")]
        id: String,
    },
}

#[cfg(test)]
mod tests {
    use super::parse_kv_option;

    #[test]
    fn parse_kv_option_supports_key_value_pairs() {
        let parsed = parse_kv_option("foo=bar").unwrap();
        assert_eq!(parsed, ("foo".to_string(), "bar".to_string()));
    }

    #[test]
    fn parse_kv_option_supports_bare_flags() {
        let parsed = parse_kv_option("--help").unwrap();
        assert_eq!(parsed, ("help".to_string(), "".to_string()));
    }

    #[test]
    fn parse_kv_option_rejects_empty_input() {
        assert!(parse_kv_option("   ").is_err());
    }

    #[test]
    fn parse_kv_option_rejects_missing_key_before_equals() {
        assert!(parse_kv_option("=value").is_err());
    }
}
