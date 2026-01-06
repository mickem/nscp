use crate::nsclient::client::command_input::{CommandInput, CommandType};
use crate::nsclient::client::events::{APIEvent, UIEvent};
use crate::nsclient::client::status_widget::StatusWidget;
use crossterm::event::{KeyCode, KeyEvent};
use ratatui::layout::{Constraint, Direction, Layout, Rect};
use ratatui::prelude::Style;
use ratatui::style::palette::tailwind::SLATE;
use ratatui::text::Line;
use ratatui::widgets::{Block, Borders, List, ListItem};
use ratatui::{DefaultTerminal, Frame};
use tokio::sync::mpsc;
use tokio::sync::mpsc::Sender;
use tui_prompts::{Prompt, TextPrompt};
use crate::config::{load_history, store_history};

pub struct UI<'a> {
    exit: bool,
    command: CommandInput<'a>,
    status: StatusWidget,
    logs: Vec<String>,
    api_sender: Sender<APIEvent>,
}

impl UI<'_> {
    pub fn new(api_sender: Sender<APIEvent>) -> Self {
        let history = load_history().unwrap_or_else(|e| {
            eprintln!("Failed to load history: {}", e);
            vec![]
        });
        Self {
            exit: false,
            command: CommandInput::new(history),
            status: StatusWidget::default(),
            logs: Vec::new(),
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
                    match event {
                        UIEvent::Key(key_event) => {
                            if key_event.code == KeyCode::Esc {
                                self.exit();
                                return Ok(());
                            }
                            self.handle_key_event(key_event).await
                        },
                        UIEvent::Status(event) => self.on_info(&event),
                        UIEvent::Error(error) => self.on_error(&error),
                        UIEvent::Log(error) => self.on_log(&error),
                        UIEvent::Commands(commands) => {
                            self.command.update_commands(commands);
                        }
                    }
                    if self.exit {
                        return Ok(());
                    }
                }
            }
        }
    }

    pub(crate) fn on_log(&mut self, log: &str) {
        self.logs.push(log.to_owned());
    }

    pub(crate) fn on_info(&mut self, status: &str) {
        self.status.on_info(status);
    }
    pub(crate) fn on_error(&mut self, error: &str) {
        self.status.on_error(error);
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

        let log_block = Block::default()
            .borders(Borders::ALL)
            .style(Style::default());

        let height = (chunks[1].height - 2) as usize;
        if self.logs.len() > height {
            let excess = self.logs.len() - height;
            self.logs.drain(0..excess);
        }

        let items: Vec<ListItem> = self
            .logs
            .iter()
            .map(|l| ListItem::new(Line::styled(l, SLATE.c200)))
            .collect();

        let log = List::new(items).block(log_block);

        frame.render_widget(log, chunks[1]);

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
                        self.on_log(&line);
                    }
                }
                CommandType::Exit => self.exit(),
                CommandType::Help => {
                    let result = self.command.handle_help();
                    for line in result {
                        self.on_log(&line);
                    }
                }

                _ => {
                    if let Err(e) = self.api_sender.send(APIEvent::Command(cmd)).await {
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
