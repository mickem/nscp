use crate::nsclient::client::command_input::Command;
use crate::nsclient::client::log_widget::LogRecord;
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
    Output(String),
    Error(String),
    Log(LogRecord),
    Commands(Vec<String>),
}

pub enum UICommand {
    Command(Command),
}
