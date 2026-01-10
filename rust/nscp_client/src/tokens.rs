use crate::constants::SERVICE_NAME;
use keyring::Entry;
use std::fmt::{Display, Formatter};

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
    let entry = Entry::new(SERVICE_NAME, &key_type.get_key(id))?;
    entry.set_password(token)?;
    Ok(())
}

pub fn load_token(key_type: KeyType, id: &str) -> anyhow::Result<String> {
    let entry = Entry::new(SERVICE_NAME, &key_type.get_key(id))?;
    match entry.get_password() {
        Ok(token) => Ok(token),
        Err(err) => {
            anyhow::bail!("No {key_type} token found for user {id}: {err:?}")
        }
    }
}

pub fn clear_token(id: &str) -> anyhow::Result<()> {
    for key_type in &[KeyType::Token, KeyType::Password] {
        let entry = Entry::new(SERVICE_NAME, &key_type.get_key(id))?;
        let _ = entry.delete_credential();
    }
    Ok(())
}

pub fn token_exists(id: &str) -> bool {
    match Entry::new(SERVICE_NAME, id) {
        Ok(entry) => entry.get_password().is_ok(),
        Err(_) => false,
    }
}
