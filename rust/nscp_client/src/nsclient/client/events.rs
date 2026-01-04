use crossterm::event::KeyEvent;
use tokio::sync::mpsc;

pub async fn send_or_error<T>(sender: &mpsc::Sender<T>, event: T) {
    if let Err(e) = sender.send(event).await {
        eprintln!("Error sending event: {}", e);
    }
}
pub enum UIEvent {
    Key(KeyEvent),
    Status(String),
    Error(String),
    Log(String),
}
