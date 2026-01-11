use crate::nsclient::api::Auth;
use crate::nsclient::build_client;

pub async fn login_and_fetch_key(
    url: &String,
    username: &String,
    password: &String,
    insecure: &bool,
    ca: Option<String>,
) -> anyhow::Result<String> {
    let api = build_client(
        url,
        30,
        "TODO",
        Auth::Password(username.to_owned(), password.to_owned()),
        *insecure,
        None,
        ca,
    )?;
    match api.login().await {
        Ok(details) => Ok(details.key),
        Err(e) => anyhow::bail!("Failed to login: {:#}", e),
    }
}
