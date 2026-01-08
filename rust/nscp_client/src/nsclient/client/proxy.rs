use crate::nsclient::api::ApiClientApi;
use crate::nsclient::client::command_input::{CommandType, QueryCommand};
use crate::nsclient::client::events::{UICommand, UIEvent, send_or_error};
use crate::nsclient::client::log_widget::{LogLevel, LogRecord};
use tokio::select;
use tokio::sync::mpsc;
use tokio_util::sync::CancellationToken;

pub struct Status {
    pub(crate) error: Option<String>,
}

pub struct BackendProxy {
    api: Box<dyn ApiClientApi>,
    ui_sender: mpsc::Sender<UIEvent>,
    api_receiver: mpsc::Receiver<UICommand>,
    last_log_index: usize,
}

impl BackendProxy {
    pub fn new(
        api: Box<dyn ApiClientApi>,
        ui_sender: mpsc::Sender<UIEvent>,
        api_receiver: mpsc::Receiver<UICommand>,
    ) -> Self {
        Self {
            api,
            ui_sender,
            api_receiver,
            last_log_index: 0,
        }
    }

    pub async fn status(&self) -> anyhow::Result<Status> {
        self.api.get_log_status().await.map(|s| Status {
            error: s.last_error,
        })
    }

    pub async fn send_or_error(&self, event: UIEvent) {
        send_or_error(&self.ui_sender, event).await
    }

    pub async fn update_status(&self) {
        self.send_or_error(match self.status().await {
            Ok(status) => match status.error {
                Some(err) => UIEvent::Error(format!("Last error: {}", err)),
                None => UIEvent::Status("Connected to backend successfully.".to_string()),
            },
            Err(err) => UIEvent::Error(format!("Error connecting to backend: {}", err)),
        })
        .await;
    }
    pub async fn get_commands(&self) {
        self.send_or_error(match self.api.list_queries(&false).await {
            Ok(queries) => {
                let queries = queries.iter().map(|q| q.name.clone()).collect();
                UIEvent::Commands(queries)
            }
            Err(err) => UIEvent::Error(format!("Error connecting to backend: {}", err)),
        })
        .await;
    }

    pub async fn update_log(&mut self) {
        match self.api.get_logs_since(1, 100, self.last_log_index).await {
            Ok((response, next_last_index)) => {
                for entry in response.content.iter() {
                    let message = format!("[{}] {}", entry.level, entry.message);
                    let level = match entry.level.as_str() {
                        "debug" => LogLevel::Debug,
                        "info" => LogLevel::Info,
                        "error" => LogLevel::Error,
                        "warning" => LogLevel::Warning,
                        _ => LogLevel::Error,
                    };
                    self.send_or_error(UIEvent::Log(LogRecord { level, message }))
                        .await;
                }
                self.last_log_index = next_last_index;
            }
            Err(err) => {
                self.send_or_error(UIEvent::Error(format!(
                    "Error fetching log entries: {}",
                    err
                )))
                .await;
            }
        }
    }

    pub async fn event_loop(&mut self, token: CancellationToken) {
        self.get_commands().await;
        self.update_status().await;
        loop {
            self.update_log().await;
            select! {
                _ = token.cancelled() => {
                    return;
                },
                event = self.api_receiver.recv() => {
                    if let Some(event) = event {
                        if let Err(err) = self.on_api_event(event).await {
                            self.send_or_error(UIEvent::Error(format!(
                                "Error handling API event: {}",
                                err
                            )))
                            .await;
                        }
                    }
                },
                _ = tokio::time::sleep(tokio::time::Duration::from_secs(5)) => {}
            }
        }
    }
    pub async fn on_api_event(&self, event: UICommand) -> anyhow::Result<()> {
        match event {
            UICommand::Command(command) => match command.command {
                CommandType::Ping => {
                    self.handle_ping_command().await;
                    Ok(())
                }
                CommandType::Version => {
                    self.handle_version_command().await;
                    Ok(())
                }
                CommandType::Query(query) => {
                    self.handle_query_command(query).await;
                    Ok(())
                }
                CommandType::Refresh => {
                    self.update_status().await;
                    self.get_commands().await;
                    Ok(())
                }
                CommandType::History(_) => {
                    anyhow::bail!("History should not be sent to API");
                }
                CommandType::Help => {
                    anyhow::bail!("Help should not be sent to API");
                }
                CommandType::Exit => {
                    anyhow::bail!("Exit should not be sent to API");
                }
            },
        }
    }

    pub async fn handle_ping_command(&self) {
        match self.api.ping().await {
            Ok(result) => {
                self.send_or_error(UIEvent::Output(format!(
                    "Ping successful: {}.",
                    result.name
                )))
                .await;
            }
            Err(err) => {
                self.send_or_error(UIEvent::Error(format!("Ping failed: {}", err)))
                    .await;
            }
        }
    }

    pub async fn handle_version_command(&self) {
        match self.api.ping().await {
            Ok(version) => {
                self.send_or_error(UIEvent::Output(format!(
                    "Backend version: {}.",
                    version.version
                )))
                .await;
            }
            Err(err) => {
                self.send_or_error(UIEvent::Error(format!("Failed to get version: {}", err)))
                    .await;
            }
        }
    }

    pub async fn handle_query_command(&self, query: QueryCommand) {
        self.send_or_error(UIEvent::Output(format!("Executing: {}", query.command)))
            .await;
        match self
            .api
            .execute_query_nagios(&query.command, &query.args)
            .await
        {
            Ok(result) => {
                self.send_or_error(UIEvent::Output(format!("Response: {}", result.result)))
                    .await;
                for line in result.lines {
                    self.send_or_error(UIEvent::Output(line.render())).await;
                }
            }
            Err(err) => {
                self.send_or_error(UIEvent::Error(format!("Failed to execute query: {}", err)))
                    .await;
            }
        }
    }
}
