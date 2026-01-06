use crate::nsclient::client::events::{UIEvent, send_or_error};
use crossterm::event;
use crossterm::event::Event;
use std::time::Duration;
use tokio::sync::mpsc;
use tokio_util::sync::CancellationToken;

pub async fn event_loop(cancellation_token: CancellationToken, ui_sender: mpsc::Sender<UIEvent>) {
    let tick_rate = Duration::from_millis(250);
    loop {
        if cancellation_token.is_cancelled() {
            return;
        }
        match event::poll(tick_rate) {
            Ok(true) => match event::read() {
                Ok(Event::Key(key)) => send_or_error(&ui_sender, UIEvent::Key(key)).await,
                Err(e) => send_or_error(&ui_sender, UIEvent::Error(format!("Error: {e}"))).await,
                _ => {}
            },
            Ok(false) => {}
            Err(e) => send_or_error(&ui_sender, UIEvent::Error(format!("Error: {e}"))).await,
        };
        tokio::task::yield_now().await;
    }
}
