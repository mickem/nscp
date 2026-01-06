use crossterm::event::{KeyCode, KeyEvent, KeyEventKind, KeyModifiers};
use tui_prompts::FocusState::Focused;
use tui_prompts::{State, TextState};

const VALID_COMMANDS: &[&str] = &[
    "ping", "version", "query", "modules", "settings", "metrics", "refresh",
];

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

pub enum CommandType {
    Ping,
    Version,
    Refresh,
    Query(QueryCommand),
}
pub struct Command {
    pub command: CommandType,
}

#[derive(Debug)]
pub struct CommandInput<'a> {
    command_state: TextState<'a>,
    available_commands: Vec<String>,
}

impl<'a> CommandInput<'a> {
    pub(crate) fn new() -> Self {
        CommandInput {
            command_state: TextState::new().with_focus(Focused),
            available_commands: Vec::new(),
        }
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

    pub fn get_command(&mut self) -> anyhow::Result<Command> {
        let tokens = tokenize_command(self.command_state.value())?;
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

    pub(crate) fn handle_key_event(&mut self, event: KeyEvent) {
        if event.kind == KeyEventKind::Release {
            return;
        }

        match (event.code, event.modifiers) {
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
            }
            (KeyCode::Delete, _) | (KeyCode::Char('d'), KeyModifiers::CONTROL) => {
                self.command_state.delete()
            }
            (KeyCode::Char(c), _) => self.command_state.push(c),
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
        "query" => Ok(Command {
            command: CommandType::Query(QueryCommand::from_tokens(&tokens[1..])),
        }),
        _ => Ok(Command {
            command: CommandType::Query(QueryCommand::from_tokens(tokens)),
        }),
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
