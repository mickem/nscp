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

    #[arg(long, value_enum)]
    pub output_long: bool,

    #[arg(long, value_enum, default_value_t = OutputStyle::Rounded)]
    pub output_style: OutputStyle,

    #[arg(long, value_enum, default_value_t = OutputFormat::Text)]
    pub output: OutputFormat,

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
}
#[derive(Args)]
pub struct NSClientCommandOptions {
    /// The URL of the NSClient server
    #[arg(short = 'U', long, default_value = "https://localhost:8443")]
    pub(crate) url: String,
    /// The base path of the API
    #[arg(short, long, default_value = "/")]
    pub(crate) base_path: String,
    /// The timeout in seconds
    #[arg(short, long, default_value_t = 30)]
    pub(crate) timeout_s: u64,
    /// The user agent to use
    #[arg(short = 'A', long, default_value = "nscp-client")]
    pub(crate) user_agent: String,
    /// Allow untrusted connections
    #[arg(short, long)]
    pub(crate) insecure: bool,
    /// Username for authentication
    #[arg(short = 'u', long, default_value = "admin")]
    pub(crate) username: String,
    /// Password for authentication
    #[arg(short = 'p', long)]
    pub(crate) password: String,
    /// The subcommand to run
    #[command(subcommand)]
    pub(crate) command: NSClientCommands,
}

#[derive(Subcommand)]
pub enum Commands {
    /// Communicate with NSClient
    #[command(name = "nsclient")]
    NSClient(NSClientCommandOptions),
    /// Show version
    Version {},
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
