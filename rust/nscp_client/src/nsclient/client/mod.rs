mod command_input;
mod events;
mod input_event_loop;
mod proxy;
mod status_widget;
mod ui;

use tokio::sync::mpsc;

use crate::nsclient::api::ApiClientApi;
use crate::nsclient::client::proxy::BackendProxy;
use crate::nsclient::client::ui::UI;
use tokio_util::sync::CancellationToken;

pub async fn run_client(api: Box<dyn ApiClientApi>) -> anyhow::Result<()> {
    let (tx, rx) = mpsc::channel(32);
    let terminal = ratatui::init();
    let token = CancellationToken::new();

    tokio::spawn(input_event_loop::event_loop(token.clone(), tx.clone()));

    let tx_clone = tx.clone();
    let backend = BackendProxy::new(api, tx_clone);
    let token_clone = token.clone();
    tokio::spawn(async move { backend.event_loop(token_clone).await });

    let mut app = UI::new();
    if let Err(e) = app.event_loop(terminal, rx).await {
        eprintln!("Application error: {}", e);
    }
    token.cancel();

    ratatui::restore();
    Ok(())
}
