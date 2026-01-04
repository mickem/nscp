use crate::nsclient::client::command_input::CommandInput;
use crate::nsclient::client::events::UIEvent;
use crate::nsclient::client::status_widget::StatusWidget;
use crossterm::event::{KeyCode, KeyEvent};
use ratatui::layout::{Constraint, Direction, Layout, Rect};
use ratatui::prelude::Style;
use ratatui::style::palette::tailwind::SLATE;
use ratatui::text::Line;
use ratatui::widgets::{Block, Borders, List, ListItem};
use ratatui::{DefaultTerminal, Frame};
use tokio::sync::mpsc;
use tui_prompts::{Prompt, TextPrompt};

pub struct UI<'a> {
    exit: bool,
    command: CommandInput<'a>,
    status: StatusWidget,
    logs: Vec<String>,
}

impl UI<'_> {
    pub fn new() -> Self {
        Self {
            exit: false,
            command: CommandInput::default(),
            status: StatusWidget::default(),
            logs: Vec::new(),
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
                                return Ok(());
                            }
                            self.handle_key_event(key_event)
                        },
                        UIEvent::Status(event) => self.on_info(&event),
                        UIEvent::Error(error) => self.on_error(&error),
                        UIEvent::Log(error) => self.on_log(&error),
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
            .enumerate()
            .map(|(_, l)| ListItem::new(Line::styled(l, SLATE.c200)))
            .collect();

        let log = List::new(items).block(log_block);

        frame.render_widget(log, chunks[1]);

        self.status.render(frame, chunks[0]);

        self.draw_text_prompt(frame, chunks[2]);
    }

    fn draw_text_prompt(&mut self, frame: &mut Frame, area: Rect) {
        TextPrompt::from("Command").draw(frame, area, self.command.get_state());
    }
    fn handle_key_event(&mut self, key_event: KeyEvent) {
        match key_event.code {
            KeyCode::Esc => self.exit(),
            KeyCode::Enter => {
                self.execute();
            }
            _ => self.command.handle_key_event(key_event),
        }
    }

    fn exit(&mut self) {
        self.exit = true;
    }

    fn execute(&mut self) {
        if !self.command.has_value() {
            return;
        }
        let command = match self.command.get_command() {
            Ok(cmd) => cmd,
            Err(_) => {
                println!("Invalid command");
                return;
            }
        };
    }
}
