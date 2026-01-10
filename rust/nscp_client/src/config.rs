use crate::constants::SERVICE_NAME;
use crate::tokens;
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Debug)]
pub struct NSClientProfile {
    pub(crate) id: String,
    #[serde(default = "String::default")]
    pub(crate) url: String,
    #[serde(default = "String::default")]
    pub(crate) username: String,
    #[serde(skip_serializing_if = "Option::is_none", default = "Option::default")]
    pub(crate) ca: Option<String>,
    pub(crate) insecure: bool,
}

#[derive(Serialize, Deserialize, Debug, Default)]
struct NSClientConfig {
    #[serde(skip_serializing_if = "Vec::is_empty", default = "Vec::default")]
    nsclient_profiles: Vec<NSClientProfile>,
    default_nsclient_profile: Option<String>,
    #[serde(skip_serializing_if = "Vec::is_empty", default = "Vec::default")]
    history: Vec<String>,
}

fn load_config() -> anyhow::Result<NSClientConfig> {
    Ok(confy::load(SERVICE_NAME, None)?)
}

pub fn add_nsclient_profile(
    id: &str,
    url: &str,
    insecure: bool,
    username: &str,
    password: &str,
    api_key: &str,
    ca: Option<String>,
) -> anyhow::Result<()> {
    let mut cfg: NSClientConfig = load_config()?;
    cfg.nsclient_profiles.retain(|p| p.id != id);
    let profile = NSClientProfile {
        id: id.to_string(),
        url: url.to_string(),
        username: username.to_string(),
        insecure,
        ca,
    };
    cfg.nsclient_profiles.push(profile);
    if cfg.default_nsclient_profile.is_none() {
        cfg.default_nsclient_profile = Some(id.to_string());
    }
    confy::store(SERVICE_NAME, None, cfg)?;
    tokens::store_token(KeyType::Password, id, password)?;
    tokens::store_token(KeyType::Token, id, api_key)
}

pub fn set_default_nsclient_profile(id: &str) -> anyhow::Result<()> {
    let mut cfg: NSClientConfig = confy::load(SERVICE_NAME, None)?;
    if cfg.nsclient_profiles.iter().any(|p| p.id == id) {
        cfg.default_nsclient_profile = Some(id.to_string());
        confy::store(SERVICE_NAME, None, cfg)?;
        Ok(())
    } else {
        anyhow::bail!("Profile with id '{}' does not exist", id);
    }
}

pub fn get_default_nsclient_profile() -> anyhow::Result<Option<NSClientProfile>> {
    let cfg: NSClientConfig = confy::load(SERVICE_NAME, None)?;
    if let Some(default_id) = cfg.default_nsclient_profile
        && let Some(profile) = cfg
            .nsclient_profiles
            .into_iter()
            .find(|p| p.id == default_id)
    {
        return Ok(Some(profile));
    }
    Ok(None)
}

pub fn get_nsclient_profile(id: &str) -> anyhow::Result<Option<NSClientProfile>> {
    let cfg: NSClientConfig = confy::load(SERVICE_NAME, None)?;
    if let Some(profile) = cfg.nsclient_profiles.into_iter().find(|p| p.id == id) {
        return Ok(Some(profile));
    }
    Ok(None)
}

pub fn get_api_key(id: &str) -> anyhow::Result<String> {
    match tokens::load_token(KeyType::Token, id) {
        Ok(token) => Ok(token),
        Err(e) => anyhow::bail!("Failed to load API key for profile '{}': {:#}", id, e),
    }
}

pub fn get_password(id: &str) -> anyhow::Result<String> {
    match tokens::load_token(KeyType::Password, id) {
        Ok(token) => Ok(token),
        Err(e) => anyhow::bail!("Failed to load password for profile '{}': {:#}", id, e),
    }
}

pub fn update_token(id: &str, token: &str) -> anyhow::Result<()> {
    tokens::store_token(KeyType::Token, id, token)
}

pub fn remove_nsclient_profile(id: &str) -> anyhow::Result<()> {
    let mut cfg: NSClientConfig = confy::load(SERVICE_NAME, None)?;
    if let Some(pos) = cfg.nsclient_profiles.iter().position(|p| p.id == id) {
        cfg.nsclient_profiles.remove(pos);
        if cfg.default_nsclient_profile.as_deref() == Some(id) {
            cfg.default_nsclient_profile = None;
        }
        confy::store(SERVICE_NAME, None, cfg)?;
        tokens::clear_token(id)
    } else {
        anyhow::bail!("Profile with id '{}' does not exist", id);
    }
}

pub fn list_nsclient_profiles() -> anyhow::Result<(Vec<NSClientProfile>, Option<String>)> {
    let cfg: NSClientConfig = confy::load(SERVICE_NAME, None)?;
    Ok((cfg.nsclient_profiles, cfg.default_nsclient_profile))
}

pub fn store_history(history: Vec<String>) -> anyhow::Result<()> {
    let mut cfg: NSClientConfig = confy::load(SERVICE_NAME, None)?;
    cfg.history = history;
    confy::store(SERVICE_NAME, None, cfg)?;
    Ok(())
}

pub fn load_history() -> anyhow::Result<Vec<String>> {
    let cfg: NSClientConfig = confy::load(SERVICE_NAME, None)?;
    Ok(cfg.history)
}

#[cfg(test)]
#[path = "custom_mock_keyring.rs"]
mod custom_mock_keyring;

use crate::tokens::KeyType;
#[cfg(test)]
use keyring::set_default_credential_builder;
#[cfg(test)]
use std::env;
#[cfg(test)]
use tempfile::{TempDir, tempdir};

#[cfg(test)]
pub fn mock_test_config() -> TempDir {
    println!("Setting up mock test config");
    set_default_credential_builder(custom_mock_keyring::default_credential_builder());
    let tmp = tempdir().unwrap();
    let fake_home = tmp.path();
    unsafe {
        if cfg!(windows) {
            env::set_var("APPDATA", fake_home);
        } else {
            env::set_var("HOME", fake_home);
            env::set_var("XDG_CONFIG_HOME", fake_home);
        }
    }
    tmp
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    #[serial_test::serial(config)]
    fn test_add_and_get_and_delete_profile() {
        let tmp = mock_test_config();
        let _ = add_nsclient_profile(
            "test1",
            "http://localhost:8080",
            false,
            "username",
            "password",
            "apikey1",
        );
        let profile = get_nsclient_profile("test1").unwrap().unwrap();
        assert_eq!(profile.id, "test1");
        assert_eq!(profile.url, "http://localhost:8080");
        assert_eq!(profile.insecure, false);
        let api_key = get_api_key("test1").unwrap();
        assert_eq!(api_key, "apikey1");
        let _ = remove_nsclient_profile("test1");
        let profile = get_nsclient_profile("test1").unwrap();
        assert!(profile.is_none());
        drop(tmp);
    }

    #[test]
    #[serial_test::serial(config)]
    fn test_add_should_replace_old_profile() {
        let tmp = mock_test_config();
        let _ = add_nsclient_profile("test2", "old-url", false, "username", "password", "");
        let profile = get_nsclient_profile("test2").unwrap().unwrap();
        assert_eq!(profile.url, "old-url");

        let _ = add_nsclient_profile("test2", "new-url", false, "username", "password", "");
        let profile = get_nsclient_profile("test2").unwrap().unwrap();
        assert_eq!(profile.url, "new-url");
        drop(tmp);
    }

    #[test]
    #[serial_test::serial(config)]
    fn test_set_and_get_default_profile() {
        let tmp = mock_test_config();
        let _ = add_nsclient_profile(
            "test3",
            "http://localhost:8081",
            true,
            "username",
            "password",
            "apikey2",
        );
        let _ = set_default_nsclient_profile("test3");
        let default_profile = get_default_nsclient_profile().unwrap().unwrap();
        assert_eq!(default_profile.id, "test3");
        drop(tmp);
    }

    #[test]
    #[serial_test::serial(config)]
    fn test_set_nonexistent_default_profile() {
        let tmp = mock_test_config();
        let result = set_default_nsclient_profile("nonexistent");
        assert!(result.is_err());
        drop(tmp);
    }

    #[test]
    #[serial_test::serial(config)]
    fn test_get_no_default_profile() {
        let tmp = mock_test_config();
        let default_profile = get_default_nsclient_profile().unwrap();
        assert!(default_profile.is_none());
        drop(tmp);
    }

    #[test]
    #[serial_test::serial(config)]
    fn test_getting_nonexistent_profile() {
        let tmp = mock_test_config();
        let profile = get_nsclient_profile("nonexistent").unwrap();
        assert!(profile.is_none());
        drop(tmp);
    }
    #[test]
    #[serial_test::serial(config)]
    fn test_remove_nonexistent_profile() {
        let tmp = mock_test_config();
        let result = remove_nsclient_profile("nonexistent");
        assert!(result.is_err());
        drop(tmp);
    }

    #[test]
    #[serial_test::serial(config)]
    fn test_list_profiles() {
        let tmp = mock_test_config();
        let _ = add_nsclient_profile(
            "test1",
            "http://localhost:8080",
            false,
            "username",
            "password",
            "apikey1",
        );
        let _ = add_nsclient_profile(
            "test2",
            "http://localhost:9090",
            true,
            "username",
            "password",
            "apikey2",
        );

        let (profiles, default_id) = list_nsclient_profiles().unwrap();
        assert_eq!(profiles.len(), 2);
        let ids: Vec<String> = profiles.into_iter().map(|p| p.id).collect();
        assert!(ids.contains(&"test1".to_string()));
        assert!(ids.contains(&"test2".to_string()));
        assert_eq!(default_id.unwrap(), "test1".to_string());

        drop(tmp);
    }
}
