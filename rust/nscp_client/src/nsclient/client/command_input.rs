use crossterm::event::{KeyCode, KeyEvent, KeyEventKind, KeyModifiers};
use tui_prompts::FocusState::Focused;
use tui_prompts::{State, TextState};

const VALID_COMMANDS: &[&str] = &[
    "ping", "version", "query", "modules", "settings", "metrics", "refresh", "history", "exit",
    "help",
];

const MAX_HISTORY_LENGTH: usize = 30;

pub struct QueryCommand {
    pub command: String,
    pub args: Vec<(String, String)>,
}

impl QueryCommand {
    fn from_tokens(tokens: &[String]) -> Self {
        Self {
            command: tokens[0].clone(),
            args: parse_arguments(&tokens[1..]),
        }
    }
}

fn parse_arguments(args: &[String]) -> Vec<(String, String)> {
    let mut map = Vec::new();
    for arg in args {
        if let Some((key, value)) = arg.split_once('=') {
            map.push((key.to_string(), value.to_string()));
        } else {
            map.push((arg.to_string(), String::new()));
        }
    }
    map
}

pub enum HistoryCommand {
    List,
    Clear,
    Delete(usize),
}

pub enum CommandType {
    Ping,
    Version,
    Refresh,
    Exit,
    Help,
    Query(QueryCommand),
    History(HistoryCommand),
}
pub struct Command {
    pub command: CommandType,
}

#[derive(Debug)]
pub struct CommandInput<'a> {
    command_state: TextState<'a>,
    available_commands: Vec<String>,
    history: Vec<String>,
    history_index: Option<usize>,
    has_changed: bool,
}


impl<'a> CommandInput<'a> {
    pub(crate) fn new(history: Vec<String>) -> Self {
        CommandInput {
            command_state: TextState::new().with_focus(Focused),
            available_commands: Vec::new(),
            history,
            history_index: None,
            has_changed: false,
        }
    }

    pub(crate) fn get_history(&self) -> Vec<String> {
        self.history.clone()
    }

    pub fn update_commands(&mut self, commands: Vec<String>) {
        self.available_commands = commands;
    }

    pub(crate) fn get_state(&mut self) -> &mut TextState<'a> {
        &mut self.command_state
    }

    pub fn has_value(&self) -> bool {
        let value = self.command_state.value();
        !value.trim().is_empty()
    }

    fn add_history(&mut self, value: &str) {
        let value = value.trim();
        if value.is_empty() {
            return;
        }
        self.history.retain(|x| x != value);
        self.history.push(value.to_owned());
        if self.history.len() > MAX_HISTORY_LENGTH {
            self.history.remove(0);
        }
    }

    pub fn get_command(&mut self) -> anyhow::Result<Command> {
        let value = self.command_state.value().to_owned();
        self.add_history(&value);

        self.history_index = None;
        self.has_changed = false;

        let tokens = tokenize_command(&value)?;
        if tokens.is_empty() || !is_valid_command(&tokens[0], &self.available_commands) {
            anyhow::bail!("Invalid command")
        }

        self.command_state.truncate();
        *self.command_state.status_mut() = tui_prompts::Status::Pending;
        parse_command(&tokens)
    }

    fn set_status_ok(&mut self) {
        *self.command_state.status_mut() = tui_prompts::Status::Done;
    }
    fn set_status_error(&mut self) {
        *self.command_state.status_mut() = tui_prompts::Status::Aborted;
    }
    fn set_status_pending(&mut self) {
        *self.command_state.status_mut() = tui_prompts::Status::Pending;
    }

    fn push_history_if_changed(&mut self) {
        if self.has_changed {
            let value = self.command_state.value().to_owned();
            self.add_history(&value);
            self.has_changed = false;
        }
    }
    fn history_up(&mut self) {
        if self.history.is_empty() {
            return;
        }
        self.push_history_if_changed();
        let new_index = match self.history_index {
            None => self.history.len() - 1,
            Some(i) => {
                if i > 0 {
                    i - 1
                } else {
                    0
                }
            }
        };
        self.history_index = Some(new_index);
        self.update_input_from_history();
    }

    fn history_down(&mut self) {
        if self.history.is_empty() {
            return;
        }
        self.push_history_if_changed();
        if let Some(i) = self.history_index {
            if i < self.history.len() - 1 {
                self.history_index = Some(i + 1);
                self.update_input_from_history();
            } else {
                self.history_index = None;
                self.command_state.truncate();
            }
        }
    }

    pub(crate) fn handle_history(&mut self, command: HistoryCommand) -> Vec<String> {
        match command {
            HistoryCommand::List => self
                .history
                .iter()
                .enumerate()
                .map(|(i, cmd)| format!("{i}: {cmd}"))
                .collect(),
            HistoryCommand::Clear => {
                self.history.clear();
                vec!["History cleared".into()]
            }
            HistoryCommand::Delete(index) => {
                self.history.remove(index);
                vec!["History deleted".into()]
            }
        }
    }

    fn update_input_from_history(&mut self) {
        if let Some(idx) = self.history_index {
            if let Some(cmd) = self.history.get(idx) {
                self.command_state.truncate();
                self.command_state.value_mut().push_str(cmd);
                self.command_state.move_end();
            }
        }
    }

    pub(crate) fn handle_key_event(&mut self, event: KeyEvent) {
        if event.kind == KeyEventKind::Release {
            return;
        }

        match (event.code, event.modifiers) {
            (KeyCode::Up, _) => {
                self.history_up();
                return;
            }
            (KeyCode::Down, _) => {
                self.history_down();
                return;
            }
            (KeyCode::Left, _) | (KeyCode::Char('b'), KeyModifiers::CONTROL) => {
                self.command_state.move_left();
                return;
            }
            (KeyCode::Right, _) | (KeyCode::Char('f'), KeyModifiers::CONTROL) => {
                self.command_state.move_right();
                return;
            }
            (KeyCode::Home, _) | (KeyCode::Char('a'), KeyModifiers::CONTROL) => {
                self.command_state.move_start();
                return;
            }
            (KeyCode::End, _) | (KeyCode::Char('e'), KeyModifiers::CONTROL) => {
                self.command_state.move_end();
                return;
            }
            (KeyCode::Backspace, _) | (KeyCode::Char('h'), KeyModifiers::CONTROL) => {
                self.command_state.backspace();
                self.has_changed = true;
            }
            (KeyCode::Delete, _) | (KeyCode::Char('d'), KeyModifiers::CONTROL) => {
                self.command_state.delete();
                self.has_changed = true;
            }
            (KeyCode::Char(c), _) => {
                self.command_state.push(c);
                self.has_changed = true;
            }
            _ => {
                return;
            }
        }
        let value = self.command_state.value();
        if value.trim().is_empty() {
            self.set_status_pending();
            return;
        }

        match tokenize_command(value) {
            Ok(tokens) if !tokens.is_empty() => {
                if is_valid_command(&tokens[0], &self.available_commands) {
                    self.set_status_ok();
                } else {
                    self.set_status_error();
                }
            }
            _ => self.set_status_error(),
        }
    }

    pub fn handle_help(&mut self) -> Vec<String> {
        vec![
            "Available commands:".into(),
            "  help:    Show help".into(),
            "  exit:    Exit program (Esc)".into(),
            "  history: Show and manipulate history".into(),
            "  ping:    Check if backend is reachable".into(),
            "  version: Show backend version".into(),
            "  query:   Execute a query (check command)".into(),
            "  ...      Any query (check_command) can be executed as-is".into(),

        ]
    }
}

fn parse_command(tokens: &[String]) -> anyhow::Result<Command> {
    if tokens.is_empty() {
        anyhow::bail!("No command provided");
    }
    match tokens[0].to_lowercase().as_str() {
        "ping" => Ok(Command {
            command: CommandType::Ping,
        }),
        "version" => Ok(Command {
            command: CommandType::Version,
        }),
        "refresh" => Ok(Command {
            command: CommandType::Refresh,
        }),
        "history" => Ok(Command {
            command: parse_history_command(&tokens[1..])?,
        }),
        "exit" => Ok(Command {
            command: CommandType::Exit,
        }),
        "help" => Ok(Command {
            command: CommandType::Help,
        }),
        "query" => Ok(Command {
            command: CommandType::Query(QueryCommand::from_tokens(&tokens[1..])),
        }),
        _ => Ok(Command {
            command: CommandType::Query(QueryCommand::from_tokens(tokens)),
        }),
    }
}

fn parse_history_command(args: &[String]) -> anyhow::Result<CommandType> {
    if args.is_empty() {
        return Ok(CommandType::History(HistoryCommand::List));
    }
    match args[0].to_lowercase().as_str() {
        "list" => Ok(CommandType::History(HistoryCommand::List)),
        "clear" => Ok(CommandType::History(HistoryCommand::Clear)),
        "delete" => {
            if args.len() != 2 {
                anyhow::bail!("Invalid syntax: history delete <index>");
            }
            let index = match args[1].parse::<usize>() {
                Ok(value) => value,
                Err(e) => anyhow::bail!("Error: {e}"),
            };
            Ok(CommandType::History(HistoryCommand::Delete(index)))
        }
        &_ => anyhow::bail!("Invalid history command"),
    }
}

fn is_valid_command(candidate: &str, queries: &[String]) -> bool {
    VALID_COMMANDS
        .iter()
        .any(|cmd| candidate.eq_ignore_ascii_case(cmd))
        | queries
            .iter()
            .any(|cmd| candidate.eq_ignore_ascii_case(cmd))
}

fn tokenize_command(input: &str) -> anyhow::Result<Vec<String>> {
    let mut tokens = Vec::new();
    let mut current = String::new();
    let mut in_quotes = false;
    let mut escape_next = false;

    for ch in input.chars() {
        if escape_next {
            current.push(ch);
            escape_next = false;
            continue;
        }
        match ch {
            '\\' => escape_next = true,
            '"' => in_quotes = !in_quotes,
            c if c.is_whitespace() && !in_quotes => {
                if !current.is_empty() {
                    tokens.push(std::mem::take(&mut current));
                }
            }
            c => current.push(c),
        }
    }

    if escape_next {
        anyhow::bail!("Dangling escape character");
    }
    if in_quotes {
        anyhow::bail!("Unterminated quote");
    }
    if !current.is_empty() {
        tokens.push(current);
    }
    Ok(tokens)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn command_input_tokenizer_handles_quotes() {
        let tokens = tokenize_command(r#"ping "example.com""#).unwrap();
        assert_eq!(tokens, vec!["ping".to_string(), "example.com".to_string()]);
    }

    #[test]
    fn command_input_tokenizer_handles_quotes_and_escapes() {
        let tokens = tokenize_command(r#"queries "\"with spaces\"" arg"#).unwrap();
        assert_eq!(
            tokens,
            vec![
                "queries".to_string(),
                "\"with spaces\"".to_string(),
                "arg".to_string()
            ]
        );
    }

    #[test]
    fn command_input_tokenizer_rejects_unterminated_quote() {
        assert_eq!(
            tokenize_command("\"incomplete").unwrap_err().to_string(),
            "Unterminated quote"
        );
    }

    #[test]
    fn command_input_accepts_valid_command_case_insensitive() {
        assert!(is_valid_command("PiNg", &[]));
        assert!(!is_valid_command("unknown", &[]));
        assert!(is_valid_command("ExtraCommand", &["ExtraCommand".into()]));
    }
}
