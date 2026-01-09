use ratatui::Frame;
use ratatui::layout::{Direction, Rect};
use ratatui::prelude::{Color, Style};
use ratatui::text::Line;
use ratatui::widgets::{Bar, BarChart, BarGroup, Block};

#[derive(Default)]
pub struct StatusWidget {
    user: u64,
    kernel: u64,
    memory: u64,
}

impl StatusWidget {
    pub(crate) fn on_performance(&mut self, user: f64, kernel: f64, memory: f64) {
        self.user = user as u64;
        self.kernel = kernel as u64;
        self.memory = (100.0 * memory) as u64;
    }

    pub fn render(&self, frame: &mut Frame, rect: Rect) {
        let bars: Vec<Bar> = vec![
            Bar::default()
                .value(self.user)
                .label(Line::raw("User"))
                .style(get_style(self.user)),
            Bar::default()
                .value(self.kernel)
                .label(Line::raw("Kernel"))
                .style(get_style(self.kernel)),
            Bar::default()
                .value(self.memory)
                .label(Line::raw("Memory"))
                .style(get_style(self.memory)),
        ];

        let chart = BarChart::default()
            .block(Block::new())
            .data(BarGroup::default().bars(&bars))
            .bar_width(1)
            .bar_gap(0)
            .max(100)
            .direction(Direction::Horizontal);

        frame.render_widget(chart, rect);
    }
}

fn get_style(value: u64) -> Style {
    if value > 80 {
        return Style::new().fg(Color::Red);
    }
    if value > 50 {
        return Style::new().fg(Color::Yellow);
    }
    Style::new().fg(Color::Green)
}
