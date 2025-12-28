use keyring::Entry;

const SERVICE_NAME: &str = "check_nsclient";

pub async fn store_token(username: &str, token: &str) -> anyhow::Result<()> {
    let entry = Entry::new(SERVICE_NAME, username)?;
    entry.set_password(token)?;
    Ok(())
}

pub fn load_token(username: &str) -> anyhow::Result<String> {
    let entry = Entry::new(SERVICE_NAME, username)?;
    match entry.get_password() {
        Ok(token) => Ok(token),
        Err(err) => {
            anyhow::bail!("No token found for user {username}: {err:?}")
        }
    }
}

pub fn clear_token(username: &str) -> anyhow::Result<()> {
    let entry = Entry::new(SERVICE_NAME, username)?;
    let _ = entry.delete_password();
    Ok(())
}
