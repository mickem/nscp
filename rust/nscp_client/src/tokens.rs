use crate::constants::SERVICE_NAME;
use keyring::Entry;

pub fn store_token(id: &str, token: &str) -> anyhow::Result<()> {
    let entry = Entry::new(SERVICE_NAME, id)?;
    entry.set_password(token)?;
    Ok(())
}

pub fn load_token(id: &str) -> anyhow::Result<String> {
    let entry = Entry::new(SERVICE_NAME, id)?;
    match entry.get_password() {
        Ok(token) => Ok(token),
        Err(err) => {
            anyhow::bail!("No token found for user {id}: {err:?}")
        }
    }
}

pub fn clear_token(id: &str) -> anyhow::Result<()> {
    let entry = Entry::new(SERVICE_NAME, id)?;
    let _ = entry.delete_credential();
    Ok(())
}

pub fn token_exists(id: &str) -> bool {
    match Entry::new(SERVICE_NAME, id) {
        Ok(entry) => entry.get_password().is_ok(),
        Err(_) => false,
    }
}
