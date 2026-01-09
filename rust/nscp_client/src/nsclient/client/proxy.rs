use crate::nsclient::api::ApiClientApi;
use crate::nsclient::client::command_input::{CommandType, ModuleCommand, QueryCommand};
use crate::nsclient::client::events::{UICommand, UIEvent, send_or_error};
use crate::nsclient::client::log_widget::{LogLevel, LogRecord};
use crate::nsclient::messages::Metrics;
use std::time::Duration;
use tokio::select;
use tokio::sync::mpsc;
use tokio_util::sync::CancellationToken;

fn bool_to_check(value: bool) -> String {
    if value { "✓".into() } else { "☐".into() }
}

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
    pub async fn update_commands(&self) {
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
                self.on_error(format!("Error fetching log entries: {}", err))
                    .await;
            }
        }
    }

    pub async fn event_loop(&mut self, token: CancellationToken) {
        self.update_commands().await;
        self.update_status().await;
        loop {
            self.house_keeping().await;
            select! {
                _ = token.cancelled() => {
                    return;
                },
                event = self.api_receiver.recv() => self.handle_ui_event(event).await,
                _ = tokio::time::sleep(Duration::from_secs(5)) => {}
            }
        }
    }

    pub async fn house_keeping(&mut self) {
        self.update_log().await;
        self.update_metrics().await;
    }

    pub async fn handle_ui_event(&mut self, event: Option<UICommand>) {
        if let Some(event) = event
            && let Err(err) = self.on_api_request(event).await
        {
            self.on_error(format!("Error handling API event: {}", err))
                .await;
        }
    }
    pub async fn on_api_request(&mut self, event: UICommand) -> anyhow::Result<()> {
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
                CommandType::Module(command) => {
                    if let Err(e) = self.handle_module_command(command).await {
                        self.on_error(format!("Error handling module command: {}", e))
                            .await
                    }
                    Ok(())
                }
                CommandType::Queries => {
                    let commands = self.api.list_queries(&false).await?;
                    self.update_log().await;
                    for command in commands {
                        self.output(command.name).await;
                    }
                    Ok(())
                }
                CommandType::Refresh => {
                    self.update_status().await;
                    self.update_commands().await;
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
                self.output(format!("Ping successful: {}.", result.name))
                    .await;
            }
            Err(err) => {
                self.on_error(format!("Ping failed: {}", err)).await;
            }
        }
    }

    pub async fn handle_version_command(&self) {
        match self.api.ping().await {
            Ok(version) => {
                self.output(format!("Backend version: {}.", version.version))
                    .await;
            }
            Err(err) => {
                self.on_error(format!("Failed to get version: {}", err))
                    .await;
            }
        }
    }

    pub async fn handle_query_command(&self, query: QueryCommand) {
        self.output(format!("Executing: {}", query.command)).await;
        match self
            .api
            .execute_query_nagios(&query.command, &query.args)
            .await
        {
            Ok(result) => {
                self.output(format!("Response: {}", result.result)).await;
                for line in result.lines {
                    self.output(line.render()).await;
                }
            }
            Err(err) => {
                self.on_error(format!("Failed to execute query: {}", err))
                    .await;
            }
        }
    }

    pub async fn handle_module_command(&mut self, command: ModuleCommand) -> anyhow::Result<()> {
        match command {
            ModuleCommand::List => {
                let modules = self.api.list_modules(&true).await?;
                self.update_log().await;
                for module in modules {
                    self.output(format!("{} {}", bool_to_check(module.loaded), module.name))
                        .await;
                }
                Ok(())
            }
            ModuleCommand::Load(module) => {
                self.api.module_command(&module, "load").await?;
                self.update_log().await;
                self.update_commands().await;
                self.output(format!("Module {} loaded.", module)).await;
                Ok(())
            }
            ModuleCommand::Unload(module) => {
                self.api.module_command(&module, "unload").await?;
                self.update_log().await;
                self.update_commands().await;
                self.output(format!("Module {} unloaded.", module)).await;
                Ok(())
            }
        }
    }

    async fn update_metrics(&self) {
        let metrics = match self.api.get_metrics().await {
            Ok(metrics) => metrics,
            Err(err) => {
                self.on_error(format!("Error fetching metrics: {}", err))
                    .await;
                return;
            }
        };
        let cpu_user = get_float(&metrics, "system.cpu.total.user");
        let cpu_kernel = get_float(&metrics, "system.cpu.total.kernel");
        let memory_used = get_float(&metrics, "system.mem.physical.used");
        let memory_total = get_float(&metrics, "system.mem.physical.total");
        let memory = if memory_total != 0.0f64 {
            memory_used / memory_total
        } else {
            0.0
        };
        self.send_or_error(UIEvent::Performance(cpu_user, cpu_kernel, memory))
            .await;
    }

    pub async fn on_error(&self, message: String) {
        self.send_or_error(UIEvent::Error(message)).await;
    }
    pub async fn output(&self, message: String) {
        self.send_or_error(UIEvent::Output(message)).await;
    }
}

fn get_float(metrics: &Metrics, key: &str) -> f64 {
    metrics
        .get(key)
        .and_then(|value| value.as_f64())
        .unwrap_or(0.0)
}
