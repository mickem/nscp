mod api;
mod auth_commands;
mod generic_commands;
mod logs_commands;
mod messages;
mod metrics_commands;
mod module_commands;
mod query_commands;
mod scripts_commands;
mod settings_commands;

use crate::cli::{NSClientCommandOptions, NSClientCommands};
use crate::nsclient::api::{ApiClient, ApiClientApi};
use crate::nsclient::auth_commands::route_auth_commands;
use crate::nsclient::generic_commands::{handle_ping_command, handle_version_command};
use crate::nsclient::logs_commands::route_log_commands;
use crate::nsclient::metrics_commands::route_metrics_commands;
use crate::nsclient::module_commands::route_module_commands;
use crate::nsclient::query_commands::route_query_commands;
use crate::nsclient::scripts_commands::route_script_commands;
use crate::nsclient::settings_commands::route_settings_commands;
use crate::rendering::Rendering;
use std::time::Duration;

fn preprocess_url(url: &str, base_path: &str) -> String {
    let url = url.trim_end_matches('/');
    let base_path = base_path.trim_matches('/');

    let base_url = format!("{}/{}", url, base_path);
    if base_url.ends_with("/") {
        base_url
    } else {
        format!("{base_url}/")
    }
}

pub fn build_client(args: &NSClientCommandOptions) -> anyhow::Result<Box<dyn ApiClientApi>> {
    let url = preprocess_url(&args.url, &args.base_path);
    let client = reqwest::Client::builder()
        .timeout(Duration::from_secs(args.timeout_s.to_owned()))
        .user_agent(&args.user_agent)
        .danger_accept_invalid_certs(args.insecure.to_owned());
    let client = ApiClient::new(client, &url, args.username.clone(), args.password.clone())?;
    Ok(Box::new(client))
}

pub async fn route_ns_client(
    output: Rendering,
    args: &NSClientCommandOptions,
) -> anyhow::Result<()> {
    let api = build_client(args)?;

    match &args.command {
        NSClientCommands::Ping {} => handle_ping_command(output, api).await,
        NSClientCommands::Version {} => handle_version_command(output, api).await,
        NSClientCommands::Modules { command } => route_module_commands(output, api, command).await,
        NSClientCommands::Queries { command } => route_query_commands(output, api, command).await,
        NSClientCommands::Logs { command } => route_log_commands(output, api, command).await,
        NSClientCommands::Scripts { command } => route_script_commands(output, api, command).await,
        NSClientCommands::Settings { command } => {
            route_settings_commands(output, api, command).await
        }
        NSClientCommands::Metrics { command } => route_metrics_commands(output, api, command).await,
        NSClientCommands::Auth { command } => route_auth_commands(output, api, command).await,
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    use crate::cli::OutputFormat;
    use crate::cli::OutputStyle;
    use crate::rendering::StringRender;
    use wiremock::matchers::{method, path};
    use wiremock::{Mock, MockServer, ResponseTemplate};

    #[tokio::test]
    async fn test_ping() {
        let mock_server = MockServer::start().await;

        Mock::given(method("GET"))
            .and(path("/api/v2/info"))
            .respond_with(ResponseTemplate::new(200).set_body_json(serde_json::json!({
                "name": "NSClient++",
                "version": "0.5.2.35"
            })))
            .mount(&mock_server)
            .await;

        let output_sink = Box::new(StringRender::new());
        let output_ref = output_sink.string.clone();

        let output = Rendering::new(OutputFormat::Text, OutputStyle::Rounded, false, output_sink);

        let result = route_ns_client(
            output,
            &NSClientCommandOptions {
                command: NSClientCommands::Ping {},
                url: mock_server.uri(),
                base_path: "/".to_owned(),
                timeout_s: 30,
                user_agent: "nscp-client".to_owned(),
                insecure: false,
                username: "admin".to_owned(),
                password: Some("password".to_string()),
            },
        )
        .await;

        assert_eq!(
            output_ref.borrow().as_str(),
            "Successfully pinged NSClient++ version 0.5.2.35\n"
        );
        assert!(result.is_ok());
    }

    #[tokio::test]
    async fn test_version() {
        let mock_server = MockServer::start().await;

        Mock::given(method("GET"))
            .and(path("/api/v2/info"))
            .respond_with(ResponseTemplate::new(200).set_body_json(serde_json::json!({
                "name": "NSClient++",
                "version": "0.5.2.35"
            })))
            .mount(&mock_server)
            .await;

        let mock_output = StringRender::new();
        let output_sink = Box::new(mock_output);
        let output_ref = output_sink.string.clone();

        let output = Rendering::new(OutputFormat::Text, OutputStyle::Rounded, false, output_sink);

        let result = route_ns_client(
            output,
            &NSClientCommandOptions {
                command: NSClientCommands::Version {},
                url: mock_server.uri(),
                base_path: "/".to_owned(),
                timeout_s: 30,
                user_agent: "nscp-client".to_owned(),
                insecure: false,
                username: "admin".to_owned(),
                password: Some("password".to_string()),
            },
        )
        .await;

        assert_eq!(
            output_ref.borrow().as_str(),
            r#"╭─────────┬────────────╮
│ name    │ NSClient++ │
│ version │ 0.5.2.35   │
╰─────────┴────────────╯
"#
        );
        assert!(result.is_ok());
    }
}
