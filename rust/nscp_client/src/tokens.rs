use crate::constants::SERVICE_NAME;
use keyring::Entry;
use std::fmt::{Display, Formatter};
use std::sync::LazyLock;
use std::sync::atomic::{AtomicBool, Ordering};

static WSL_WORKAROUND: LazyLock<AtomicBool> = LazyLock::new(|| AtomicBool::new(false));

pub fn enable_wsl_workaround() {
    WSL_WORKAROUND.store(true, Ordering::Relaxed);
}

pub fn make_entry(key_type: &KeyType, id: &str) -> anyhow::Result<Entry> {
    if WSL_WORKAROUND.load(Ordering::Relaxed) {
        Ok(Entry::new_with_target(
            SERVICE_NAME,
            SERVICE_NAME,
            &key_type.get_key(id),
        )?)
    } else {
        Ok(Entry::new(SERVICE_NAME, &key_type.get_key(id))?)
    }
}

fn show_wsl_info() {
    if !WSL_WORKAROUND.load(Ordering::Relaxed) {
        eprintln!(
            "Failed to store token in keystore, if you're running under wsl try specifying: check_nsclient --wsl"
        );
    }
}

pub enum KeyType {
    Token,
    Password,
}

impl Display for KeyType {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        match self {
            KeyType::Token => write!(f, "token"),
            KeyType::Password => write!(f, "password"),
        }
    }
}
impl KeyType {
    pub fn get_key(&self, profile_id: &str) -> String {
        match self {
            KeyType::Token => format!("{profile_id}_token"),
            KeyType::Password => format!("{profile_id}_password"),
        }
    }
}
pub fn store_token(key_type: KeyType, id: &str, token: &str) -> anyhow::Result<()> {
    let entry = make_entry(&key_type, id)?;
    if let Err(e) = entry.set_password(token) {
        show_wsl_info();
        anyhow::bail!("Failed to store token in keystore: {e:?}")
    }
    Ok(())
}

pub fn load_token(key_type: KeyType, id: &str) -> anyhow::Result<String> {
    let entry = make_entry(&key_type, id)?;
    match entry.get_password() {
        Ok(token) => Ok(token),
        Err(err) => {
            show_wsl_info();
            anyhow::bail!("No {key_type} found for user {id}: {err:?}")
        }
    }
}

pub fn clear_token(id: &str) -> anyhow::Result<()> {
    for key_type in &[KeyType::Token, KeyType::Password] {
        let entry = make_entry(key_type, id)?;
        let _ = entry.delete_credential();
    }
    Ok(())
}

pub fn token_exists(key_type: KeyType, id: &str) -> bool {
    match make_entry(&key_type, id) {
        Ok(entry) => entry.get_password().is_ok(),
        Err(_) => false,
    }
}
