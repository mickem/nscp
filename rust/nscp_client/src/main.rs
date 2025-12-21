mod cli;
mod nsclient;
mod rendering;

use crate::cli::{Cli, Commands};
use crate::nsclient::route_ns_client;
use crate::rendering::Rendering;
use clap::Parser;

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let cli = Cli::parse();

    // Handle global flags (like debug)
    if cli.debug > 0 {
        println!("Debug mode enabled (level {})", cli.debug);
    }

    // Handle Subcommands
    match &cli.command {
        Commands::NSClient {
            command,
            url,
            base_path,
            timeout_s,
            user_agent,
            insecure,
            username,
            password,
        } => {
            route_ns_client(
                Rendering::new(cli.output, cli.output_style, cli.output_long),
                command,
                url,
                base_path,
                timeout_s,
                user_agent,
                insecure,
                username,
                password,
            )
            .await?
        }
        Commands::Version {} => {
            println!("Version: {}", env!("CARGO_PKG_VERSION"));
        }
    }

    Ok(())
}
