use crate::config::{load_history, store_history};
use crate::nsclient::client::command_input::{CommandInput, CommandType};
use crate::nsclient::client::events::{UICommand, UIEvent};
use crate::nsclient::client::log_widget::{LogRecord, LogWidget};
use crate::nsclient::client::status_widget::StatusWidget;
use crossterm::event::{KeyCode, KeyEvent};
use ratatui::layout::{Constraint, Direction, Layout, Rect};
use ratatui::{DefaultTerminal, Frame};
use tokio::sync::mpsc;
use tokio::sync::mpsc::Sender;
use tui_prompts::{Prompt, TextPrompt};

pub struct UI<'a> {
    exit: bool,
    command: CommandInput<'a>,
    status: StatusWidget,
    log: LogWidget,
    api_sender: Sender<UICommand>,
}

impl UI<'_> {
    pub fn new(api_sender: Sender<UICommand>) -> Self {
        let history = load_history().unwrap_or_else(|e| {
            eprintln!("Failed to load history: {}", e);
            vec![]
        });
        Self {
            exit: false,
            command: CommandInput::new(history),
            log: LogWidget::new(),
            status: StatusWidget::default(),
            api_sender,
        }
    }

    pub async fn event_loop(
        &mut self,
        mut terminal: DefaultTerminal,
        mut rx: mpsc::Receiver<UIEvent>,
    ) -> anyhow::Result<()> {
        loop {
            terminal.draw(|f| self.draw(f))?;
            tokio::select! {
                Some(event) = rx.recv() => {
                    self.handle_ui_event(event).await;
                    if self.exit {
                        return Ok(());
                    }
                }
            }
        }
    }

    async fn handle_ui_event(&mut self, event: UIEvent) {
        match event {
            UIEvent::Key(key_event) => {
                if key_event.code == KeyCode::Esc {
                    self.exit();
                    return;
                }
                self.handle_key_event(key_event).await
            }
            UIEvent::Status(event) => self.on_error(&event),
            UIEvent::Output(event) => self.output(&event),
            UIEvent::Error(error) => self.on_error(&error),
            UIEvent::Log(log) => self.on_log(log),
            UIEvent::Commands(commands) => {
                self.command.update_commands(commands);
            }
            UIEvent::Performance(user, kernel, memory) => {
                self.status.on_performance(user, kernel, memory)
            }
        }
    }

    pub(crate) fn on_log(&mut self, log: LogRecord) {
        self.log.add(log);
    }

    pub fn output(&self, message: &str) {
        self.log.add(LogRecord::from_output(message))
    }

    pub(crate) fn on_error(&mut self, error: &str) {
        self.log.add(LogRecord::from_error(error));
    }

    fn draw(&mut self, frame: &mut Frame) {
        let chunks = Layout::default()
            .direction(Direction::Vertical)
            .constraints([
                Constraint::Length(3),
                Constraint::Min(1),
                Constraint::Length(3),
            ])
            .split(frame.area());

        self.log.render(frame, chunks[1]);
        self.status.render(frame, chunks[0]);

        self.draw_text_prompt(frame, chunks[2]);
    }

    fn draw_text_prompt(&mut self, frame: &mut Frame, area: Rect) {
        TextPrompt::from("Command").draw(frame, area, self.command.get_state());
    }
    async fn handle_key_event(&mut self, key_event: KeyEvent) {
        match key_event.code {
            KeyCode::Esc => self.exit(),
            KeyCode::Enter => {
                self.execute().await;
            }
            _ => self.command.handle_key_event(key_event),
        }
    }

    fn exit(&mut self) {
        let history = self.command.get_history();
        if let Err(e) = store_history(history) {
            eprintln!("Failed to save history: {}", e);
        }
        self.exit = true;
    }

    async fn execute(&mut self) {
        if !self.command.has_value() {
            return;
        }
        match self.command.get_command() {
            Ok(cmd) => match cmd.command {
                CommandType::History(command) => {
                    let result = self.command.handle_history(command);
                    for line in result {
                        self.output(&line);
                    }
                }
                CommandType::Exit => self.exit(),
                CommandType::Help => {
                    let result = self.command.handle_help();
                    for line in result {
                        self.output(&line);
                    }
                }

                _ => {
                    if let Err(e) = self.api_sender.send(UICommand::Command(cmd)).await {
                        eprintln!("Error sending API event: {}", e);
                    }
                }
            },
            Err(_) => {
                self.on_error("Invalid command");
            }
        };
    }
}
