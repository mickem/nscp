use clap::{Parser, Subcommand, ValueEnum};

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
}

#[derive(Subcommand)]
pub enum Commands {
    /// Communicate with NSClient
    #[command(name = "nsclient")]
    NSClient {
        /// The URL of the NSClient server
        #[arg(short = 'U', long, default_value = "https://localhost:8443")]
        url: String,
        /// The base path of the API
        #[arg(short, long, default_value = "/")]
        base_path: String,
        /// The timeout in seconds
        #[arg(short, long, default_value_t = 30)]
        timeout_s: u64,
        /// The user agent to use
        #[arg(short = 'A', long, default_value = "nscp-client")]
        user_agent: String,
        /// Allow untrusted connections
        #[arg(short, long)]
        insecure: bool,
        /// Username for authentication
        #[arg(short = 'u', long, default_value = "admin")]
        username: String,
        /// Password for authentication
        #[arg(short = 'p', long)]
        password: String,
        /// The subcommand to run
        #[command(subcommand)]
        command: NSClientCommands,
    },
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
    },
    /// Show details about a module
    Show {
        id: String,
    },
    /// Load a module so it can be used.
    Load {
        id: String,
    },
    /// Unload a module so it cannot be used.
    Unload {
        id: String,
    },
    /// Enable a module so it will be loaded on startup.
    Enable {
        id: String,
    },
    /// Disable a module so it will no longer be loaded on startup.
    Disable {
        id: String,
    },
    /// Load and enable a module so it can be used.
    Use {
        id: String,
    },
}
