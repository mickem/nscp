use crate::cli::LogsCommand;
use crate::nsclient::api::ApiClientApi;
use crate::rendering::Rendering;

pub async fn route_log_commands(
    output: Rendering,
    api: Box<dyn ApiClientApi>,
    command: &LogsCommand,
) -> anyhow::Result<()> {
    match command {
        LogsCommand::List {
            page,
            size,
            level,
            long,
        } => match api.get_logs(*page, *size, level.clone()).await {
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

#[cfg(test)]
mod tests {
    use super::*;
    use crate::cli::{OutputFormat, OutputStyle};
    use crate::nsclient::api::mocks::MockApiClientApiImpl;
    use crate::nsclient::messages::{LogRecord, PaginatedResponse};
    use crate::rendering::StringRender;

    #[tokio::test]
    async fn test_ping_text() {
        let mut api = MockApiClientApiImpl::new();
        api.expect_get_logs().returning(|_, _, _| {
            Ok(PaginatedResponse {
                content: vec![LogRecord {
                    level: "level".to_string(),
                    date: "date".to_string(),
                    file: "file".to_string(),
                    line: 123,
                    message: "message".to_string(),
                }],
                page: 0,
                pages: 5,
                limit: 10,
                count: 3,
            })
        });

        let output_sink = Box::new(StringRender::new());
        let output_ref = output_sink.string.clone();

        let output = Rendering::new(OutputFormat::Text, OutputStyle::Rounded, false, output_sink);

        let result = route_log_commands(
            output,
            Box::new(api),
            &LogsCommand::List {
                page: 5,
                size: 100,
                level: None,
                long: false,
            },
        )
        .await;

        assert_eq!(
            output_ref.borrow().as_str(),
            r#"╭───────┬──────┬─────────╮
│ level │ date │ message │
├───────┼──────┼─────────┤
│ level │ date │ message │
╰───────┴──────┴─────────╯
"#
        );
        assert!(result.is_ok());
    }
}
