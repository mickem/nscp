use crate::nsclient::api::ApiClientApi;
use crate::nsclient::client::events::{UIEvent, send_or_error};
use tokio::sync::mpsc;
use tokio_util::sync::CancellationToken;

pub struct Status {
    pub(crate) error: Option<String>,
}

pub struct BackendProxy {
    api: Box<dyn ApiClientApi>,
    ui_sender: mpsc::Sender<UIEvent>,
}

impl BackendProxy {
    pub fn new(api: Box<dyn ApiClientApi>, ui_sender: mpsc::Sender<UIEvent>) -> Self {
        Self { api, ui_sender }
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

    pub async fn update_log(&self) {
        match self.api.get_logs(1, 10, None).await {
            Ok(response) => {
                for entry in response.content {
                    self.send_or_error(UIEvent::Log(format!("[{}] {}", entry.date, entry.message)))
                        .await;
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

    pub async fn event_loop(&self, token: CancellationToken) {
        loop {
            self.update_status().await;
            self.update_log().await;
            if token.is_cancelled() {
                return;
            }
            tokio::time::sleep(tokio::time::Duration::from_secs(5)).await;
        }
    }
}
