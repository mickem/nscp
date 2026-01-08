mod command_input;
mod events;
mod input_event_loop;
mod log_widget;
mod proxy;
mod status_widget;
mod ui;

use tokio::sync::mpsc;

use crate::nsclient::api::ApiClientApi;
use crate::nsclient::client::proxy::BackendProxy;
use crate::nsclient::client::ui::UI;
use tokio_util::sync::CancellationToken;

pub async fn run_client(api: Box<dyn ApiClientApi>) -> anyhow::Result<()> {
    let (tx_ui, rx_ui) = mpsc::channel(32);
    let (tx_api, rx_api) = mpsc::channel(32);
    let terminal = ratatui::init();
    let token = CancellationToken::new();

    tokio::spawn(input_event_loop::event_loop(token.clone(), tx_ui.clone()));

    let tx_clone = tx_ui.clone();
    let mut backend = BackendProxy::new(api, tx_clone, rx_api);
    let token_clone = token.clone();
    tokio::spawn(async move { backend.event_loop(token_clone).await });

    let mut app = UI::new(tx_api);
    if let Err(e) = app.event_loop(terminal, rx_ui).await {
        eprintln!("Application error: {}", e);
    }
    token.cancel();

    ratatui::restore();
    Ok(())
}
