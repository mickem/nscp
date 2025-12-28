use indexmap::IndexMap;
use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::collections::HashMap;
use tabled::Tabled;

#[derive(Debug, Serialize, Deserialize, Tabled)]
pub struct PingResult {
    #[tabled()]
    pub name: String,
    #[tabled()]
    pub version: String,
}

impl PingResult {
    pub(crate) fn to_dict(&self) -> IndexMap<String, String> {
        let mut map = IndexMap::new();
        map.insert("name".to_string(), self.name.clone());
        map.insert("version".to_string(), self.version.clone());
        map
    }
}

#[derive(Debug, Serialize, Deserialize, Tabled)]
pub struct LogRecord {
    #[tabled()]
    pub level: String,
    #[tabled()]
    pub date: String,
    #[tabled()]
    pub file: String,
    #[tabled()]
    pub line: u64,
    #[tabled()]
    pub message: String,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct LogStatus {
    pub errors: u64,
    pub last_error: Option<String>,
}

#[derive(Debug, Serialize, Deserialize, Tabled)]
pub struct ScriptRuntimes {
    #[tabled()]
    pub module: String,
    #[tabled()]
    pub name: String,
    #[tabled()]
    pub title: String,
}

pub type Metrics = HashMap<String, Value>;

#[derive(Debug, Serialize)]
#[serde(bound(serialize = "T: Serialize"))]
pub struct PaginatedResponse<T> {
    pub content: T,
    pub page: u64,
    pub pages: u64,
    pub limit: u64,
    pub count: u64,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct SettingsStatus {
    pub context: String,
    #[serde(rename = "type")]
    pub status_type: String,
    #[serde(rename = "has_changed")]
    pub has_changed: bool,
}

impl SettingsStatus {
    pub(crate) fn to_dict(&self) -> IndexMap<String, String> {
        let mut map = IndexMap::new();
        map.insert("context".to_string(), self.context.clone());
        map.insert("type".to_string(), self.status_type.clone());
        map.insert("has_changed".to_string(), self.has_changed.to_string());
        map
    }
}

#[derive(Debug, Serialize, Deserialize, Tabled)]
pub struct SettingsEntry {
    pub key: String,
    pub path: String,
    pub value: String,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct SettingsDescription {
    pub default_value: String,
    pub description: String,
    pub icon: String,
    pub is_advanced_key: bool,
    pub is_object: bool,
    pub is_sample_key: bool,
    pub is_template_key: bool,
    pub key: String,
    pub path: String,
    #[serde(rename = "type")]
    pub value_type: String,
    pub plugins: Vec<String>,
    pub sample_usage: String,
    pub title: String,
    pub value: String,
}
impl SettingsDescription {
    pub fn to_flat(&self) -> FlatSettingsDescription {
        FlatSettingsDescription {
            default_value: self.default_value.clone(),
            description: self.description.clone(),
            icon: self.icon.clone(),
            is_advanced_key: self.is_advanced_key,
            is_object: self.is_object,
            is_sample_key: self.is_sample_key,
            is_template_key: self.is_template_key,
            key: self.key.clone(),
            path: self.path.clone(),
            value_type: self.value_type.clone(),
            plugins: self.plugins.join(", "),
            sample_usage: self.sample_usage.clone(),
            title: self.title.clone(),
            value: self.value.clone(),
        }
    }
}

#[derive(Debug, Serialize, Deserialize, Tabled)]
pub struct FlatSettingsDescription {
    #[tabled()]
    pub key: String,
    #[tabled()]
    pub path: String,
    #[tabled()]
    pub value: String,
    #[tabled()]
    pub default_value: String,
    #[tabled()]
    pub description: String,
    #[tabled()]
    pub icon: String,
    #[tabled()]
    pub is_advanced_key: bool,
    #[tabled()]
    pub is_object: bool,
    #[tabled()]
    pub is_sample_key: bool,
    #[tabled()]
    pub is_template_key: bool,
    #[tabled()]
    #[serde(rename = "type")]
    pub value_type: String,
    #[tabled()]
    pub plugins: String,
    #[tabled()]
    pub sample_usage: String,
    #[tabled()]
    pub title: String,
}

#[derive(Debug, Serialize, Deserialize, Clone, Copy)]
#[serde(rename_all = "lowercase")]
pub enum SettingsCommandAction {
    Load,
    Save,
    Reload,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct SettingsCommandRequest {
    pub command: SettingsCommandAction,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct LoginResponse {
    pub key: String,
}

#[derive(Debug, Serialize, Deserialize, Tabled)]
pub struct ListModulesMetadata {
    pub alias: String,
    pub plugin_id: String,
}

#[derive(Debug, Serialize, Deserialize, Tabled)]
pub struct ListModulesResult {
    #[tabled()]
    pub id: String,
    #[tabled()]
    pub name: String,
    #[tabled()]
    pub title: String,
    #[tabled()]
    pub description: String,
    #[tabled()]
    pub enabled: bool,
    #[tabled()]
    pub loaded: bool,
    #[tabled(inline)]
    pub metadata: ListModulesMetadata,
}

#[derive(Debug, Serialize, Deserialize, Tabled)]
pub struct FlatListModulesResult {
    #[tabled()]
    pub id: String,
    #[tabled()]
    pub name: String,
    #[tabled()]
    pub title: String,
    #[tabled()]
    pub description: String,
    #[tabled()]
    pub enabled: bool,
    #[tabled()]
    pub loaded: bool,
    #[tabled()]
    pub alias: String,
    #[tabled()]
    pub plugin_id: String,
}
impl ListModulesResult {
    pub fn to_flat(&self) -> FlatListModulesResult {
        FlatListModulesResult {
            id: self.id.clone(),
            name: self.name.clone(),
            title: self.title.clone(),
            description: self.description.clone(),
            enabled: self.enabled,
            loaded: self.loaded,
            alias: self.metadata.alias.clone(),
            plugin_id: self.metadata.plugin_id.clone(),
        }
    }
}

#[derive(Debug, Serialize, Deserialize)]
pub struct ModulesResult {
    pub id: String,
    pub name: String,
    pub title: String,
    pub description: String,
    pub enabled: bool,
    pub loaded: bool,
    pub metadata: ListModulesMetadata,
}

impl ModulesResult {
    pub(crate) fn to_dict(&self) -> IndexMap<String, String> {
        let mut map = IndexMap::new();
        map.insert("id".to_string(), self.id.clone());
        map.insert("name".to_string(), self.name.clone());
        map.insert("title".to_string(), self.title.clone());
        map.insert("description".to_string(), self.description.clone());
        map.insert("enabled".to_string(), self.enabled.to_string());
        map.insert("loaded".to_string(), self.loaded.to_string());
        map.insert("alias".to_string(), self.metadata.alias.clone());
        map.insert("plugin_id".to_string(), self.metadata.plugin_id.clone());
        map
    }
}

#[derive(Debug, Serialize, Deserialize, Tabled)]
pub struct ListQueriesResult {
    #[tabled()]
    pub name: String,
    #[tabled()]
    pub title: String,
    #[tabled()]
    pub description: String,
    #[tabled()]
    pub plugin: String,
}

#[derive(Debug, Serialize, Deserialize, Tabled)]
pub struct QueryResult {
    #[tabled()]
    pub name: String,
    #[tabled()]
    pub title: String,
    #[tabled()]
    pub description: String,
    #[tabled()]
    pub plugin: String,
    #[tabled(skip)]
    pub metadata: HashMap<String, String>,
}

impl QueryResult {
    pub(crate) fn to_dict(&self) -> IndexMap<String, String> {
        let mut map = IndexMap::new();
        map.insert("name".to_string(), self.name.clone());
        map.insert("title".to_string(), self.title.clone());
        map.insert("description".to_string(), self.description.clone());
        map.insert("plugin".to_string(), self.plugin.clone());
        map
    }
}

#[derive(Debug, Serialize, Deserialize)]
pub struct PerfData {
    pub value: Option<f64>,
    pub unit: Option<String>,
    pub warning: Option<f64>,
    pub critical: Option<f64>,
    pub minimum: Option<f64>,
    pub maximum: Option<f64>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct ExecuteLine {
    pub message: String,
    pub perf: HashMap<String, PerfData>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct ExecuteResult {
    pub command: String,
    pub lines: Vec<ExecuteLine>,
    pub result: i32,
}
impl ExecuteResult {
    pub(crate) fn to_dict(&self) -> IndexMap<String, String> {
        let mut map = IndexMap::new();
        map.insert("command".to_string(), self.command.clone());
        for line in &self.lines {
            map.insert("output".to_owned(), clean_up_line(&line.message));
            for (key, perf) in &line.perf {
                map.insert(key.clone(), perf_to_simple_string(perf));
            }
        }
        map.insert("result".to_string(), result_to_string(&self.result));
        map
    }
}

const OFFSET: usize = 0;
const TAB_LENGTH: usize = 8;
fn clean_up_line(line: &str) -> String {
    let mut result = String::with_capacity(line.len());
    let mut len = OFFSET;
    for c in line.chars() {
        if c == '\t' {
            let count = TAB_LENGTH - (len % TAB_LENGTH);
            for _ in 0..count {
                result.push(' ');
            }
            len += count;
        } else if c == '\n' || c == '\r' {
            result.push(c);
            len = OFFSET;
        } else {
            result.push(c);
            len += 1;
        }
    }
    result
}

fn result_to_string(result: &i32) -> String {
    match result {
        0 => "OK".to_string(),
        1 => "WARNING".to_string(),
        2 => "CRITICAL".to_string(),
        3 => "UNKNOWN".to_string(),
        _ => "UNKNOWN".to_string(),
    }
}

fn perf_to_simple_string(perf: &PerfData) -> String {
    let mut parts = Vec::new();

    if let Some(value) = perf.value {
        let mut value_part = value.to_string();
        if let Some(unit) = &perf.unit {
            value_part.push_str(unit);
        }
        parts.push(value_part);
    }

    if let Some(warning) = perf.warning {
        parts.push(format!("warning: {}", warning));
    }

    if let Some(critical) = perf.critical {
        parts.push(format!("critical: {}", critical));
    }

    if let Some(minimum) = perf.minimum {
        parts.push(format!("minimum: {}", minimum));
    }

    if let Some(maximum) = perf.maximum {
        parts.push(format!("maximum: {}", maximum));
    }

    parts.join(", ")
}

#[derive(Debug, Serialize, Deserialize)]
pub struct ExecuteNagiosLine {
    pub message: String,
    pub perf: String,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct ExecuteNagiosResult {
    pub command: String,
    pub lines: Vec<ExecuteNagiosLine>,
    pub result: String,
}

impl ExecuteNagiosResult {
    pub(crate) fn to_dict(&self) -> IndexMap<String, String> {
        let mut map = IndexMap::new();
        map.insert("command".to_string(), self.command.clone());
        map.insert(
            "lines".to_string(),
            serde_json::to_string(&self.lines).unwrap(),
        );
        map.insert("result".to_string(), self.result.clone());
        map
    }

    pub(crate) fn get_exit_code(&self) -> i32 {
        match self.result.to_uppercase().as_str() {
            "OK" | "0" => 0,
            "WARNING" | "1" => 1,
            "CRITICAL" | "2" => 2,
            "UNKNOWN" | "3" => 3,
            _ => 3,
        }
    }
}
