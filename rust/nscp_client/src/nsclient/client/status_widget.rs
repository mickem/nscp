use ratatui::Frame;
use ratatui::layout::Rect;
use ratatui::prelude::{Color, Style, Text};
use ratatui::widgets::{Block, Borders, Paragraph};

enum StatusLevel {
    Info,
    Error,
}
pub struct StatusWidget {
    status: String,
    level: StatusLevel,
}

impl Default for StatusWidget {
    fn default() -> Self {
        Self {
            status: "...".to_string(),
            level: StatusLevel::Info,
        }
    }
}

impl StatusWidget {
    pub(crate) fn on_info(&mut self, msg: &str) {
        self.status = msg.to_owned();
        self.level = StatusLevel::Info;
    }

    pub(crate) fn on_error(&mut self, error: &str) {
        self.status = error.to_owned();
        self.level = StatusLevel::Error;
    }

    pub fn render(&self, frame: &mut Frame, rect: Rect) {
        let block = Block::default()
            .borders(Borders::ALL)
            .style(Style::default());
        let color = match self.level {
            StatusLevel::Info => Color::Green,
            StatusLevel::Error => Color::Red,
        };

        let paragraph = Paragraph::new(Text::styled(
            self.status.as_str(),
            Style::default().fg(color),
        ))
        .block(block);

        frame.render_widget(paragraph, rect);
    }
}
