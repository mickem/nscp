mod cli;
mod nsclient;
mod rendering;
mod tokens;

use crate::cli::{Cli, Commands};
use crate::nsclient::route_ns_client;
use crate::rendering::{PrintRender, Rendering};
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
        Commands::NSClient(args) => {
            let output_sink = Box::new(PrintRender::new());

            route_ns_client(
                Rendering::new(cli.output, cli.output_style, cli.output_long, output_sink),
                args,
            )
            .await?
        }
        Commands::Version {} => {
            println!("Version: {}", env!("CARGO_PKG_VERSION"));
        }
    }

    Ok(())
}
