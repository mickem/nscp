use crate::cli::LogsCommand;
use crate::nsclient::api::ApiClient;
use crate::rendering::Rendering;

pub async fn route_log_commands(
    output: Rendering,
    api: &ApiClient,
    command: &LogsCommand,
) -> anyhow::Result<()> {
    match command {
        LogsCommand::List {
            page,
            size,
            level,
            long,
        } => match api.get_logs(*page, *size, level.as_deref()).await {
            Ok(page_response) => {
                if output.is_flat() {
                    output.render_flat_list(&page_response.content, long, &["file", "line"])
                } else {
                    output.render_nested_single(&page_response)
                }
            }
            Err(e) => anyhow::bail!("Failed to fetch logs: {:#}", e),
        },
        LogsCommand::Status {} => match api.get_log_status().await {
            Ok(status) => output.render_nested_single(&status),
            Err(e) => anyhow::bail!("Failed to obtain log status: {:#}", e),
        },
        LogsCommand::Reset {} => match api.reset_log_status().await {
            Ok(()) => {
                output.print("Successfully reset log status");
                Ok(())
            }
            Err(e) => anyhow::bail!("Failed to reset log status: {:#}", e),
        },
    }
}
