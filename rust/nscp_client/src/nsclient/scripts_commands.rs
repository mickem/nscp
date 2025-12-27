use crate::cli::ScriptsCommand;
use crate::nsclient::api::ApiClient;
use crate::rendering::Rendering;

pub async fn route_script_commands(
    output: Rendering,
    api: &ApiClient,
    command: &ScriptsCommand,
) -> anyhow::Result<()> {
    match command {
        ScriptsCommand::ListRuntimes {} => match api.list_script_runtimes().await {
            Ok(scripts) => {
                if output.is_flat() {
                    output.render_flat_list(&scripts, &false, &[])
                } else {
                    output.render_nested_single(&scripts)
                }
            }
            Err(e) => anyhow::bail!("Failed to fetch scripts: {:#}", e),
        },
        ScriptsCommand::List { runtime } => match api.list_scripts(runtime).await {
            Ok(scripts) => {
                if output.is_flat() {
                    output.render_flat_list(&scripts, &false, &[])
                } else {
                    output.render_nested_single(&scripts)
                }
            }
            Err(e) => anyhow::bail!("Failed to fetch scripts: {:#}", e),
        },
    }
}
