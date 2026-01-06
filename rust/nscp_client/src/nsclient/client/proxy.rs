use crate::nsclient::api::ApiClientApi;
use crate::nsclient::client::command_input::{CommandType, QueryCommand};
use crate::nsclient::client::events::{APIEvent, UIEvent, send_or_error};
use tokio::select;
use tokio::sync::mpsc;
use tokio_util::sync::CancellationToken;

pub struct Status {
    pub(crate) error: Option<String>,
}

pub struct BackendProxy {
    api: Box<dyn ApiClientApi>,
    ui_sender: mpsc::Sender<UIEvent>,
    api_receiver: mpsc::Receiver<APIEvent>,
    last_log_record_hash: Option<u64>,
}

impl BackendProxy {
    pub fn new(
        api: Box<dyn ApiClientApi>,
        ui_sender: mpsc::Sender<UIEvent>,
        api_receiver: mpsc::Receiver<APIEvent>,
    ) -> Self {
        Self {
            api,
            ui_sender,
            api_receiver,
            last_log_record_hash: None,
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
        match self.api.get_logs(1, 10, None).await {
            Ok(response) => {
                // TODO: This is pretty ugly, a better API is needed...
                let mut new_records = Vec::new();
                for entry in response.content.iter().rev() {
                    let hash = entry.calculate_hash();
                    if let Some(last_hash) = self.last_log_record_hash
                        && hash == last_hash
                    {
                        break;
                    }
                    new_records.push(entry);
                }
                for entry in new_records.iter().rev() {
                    self.send_or_error(UIEvent::Log(format!("[{}] {}", entry.date, entry.message)))
                        .await;
                    self.last_log_record_hash = Some(entry.calculate_hash());
                }
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
                        self.on_api_event(event).await;
                    }
                },
                _ = tokio::time::sleep(tokio::time::Duration::from_secs(5)) => {}
            }
        }
    }
    pub async fn on_api_event(&self, event: APIEvent) {
        match event {
            APIEvent::Command(command) => match command.command {
                CommandType::Ping => {
                    self.handle_ping_command().await;
                }
                CommandType::Version => {
                    self.handle_version_command().await;
                }
                CommandType::Query(query) => {
                    self.handle_query_command(query).await;
                }
                CommandType::Refresh => {
                    self.update_status().await;
                    self.get_commands().await;
                }
            },
        }
    }

    pub async fn handle_ping_command(&self) {
        match self.api.ping().await {
            Ok(result) => {
                self.send_or_error(UIEvent::Status(format!(
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
                self.send_or_error(UIEvent::Status(format!(
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
        match self
            .api
            .execute_query_nagios(&query.command, &query.args)
            .await
        {
            Ok(result) => {
                self.send_or_error(UIEvent::Log(format!(
                    "Query result: {}: {}",
                    result.command, result.result
                )))
                .await;
                for line in result.lines {
                    self.send_or_error(UIEvent::Log(format!(
                        " - {} | {}",
                        line.message, line.perf
                    )))
                    .await;
                }
            }
            Err(err) => {
                self.send_or_error(UIEvent::Error(format!("Query failed: {}", err)))
                    .await;
            }
        }
    }
}
