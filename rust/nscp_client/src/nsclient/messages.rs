use serde::{Deserialize, Serialize};
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
    pub(crate) fn to_dict(&self) -> HashMap<String, String> {
        let mut map = HashMap::new();
        map.insert("name".to_string(), self.name.clone());
        map.insert("version".to_string(), self.version.clone());
        map
    }
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
    #[tabled(skip)]
    pub load_url: String,
    #[tabled(skip)]
    pub module_url: String,
    #[tabled(skip)]
    pub unload_url: String,
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
    pub load_url: String,
    pub unload_url: String,
}

impl ModulesResult {
    pub(crate) fn to_dict(&self) -> HashMap<String, String> {
        let mut map = HashMap::new();
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
