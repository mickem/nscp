use crate::cli::AuthCommand;
use crate::config;
use crate::nsclient::login_helper::login_and_fetch_key;
use crate::rendering::Rendering;

pub async fn route_auth_commands(output: Rendering, command: &AuthCommand) -> anyhow::Result<()> {
    match command {
        AuthCommand::Login {
            id,
            username,
            password,
            url,
            insecure,
        } => {
            let key = match login_and_fetch_key(url, username, password, insecure).await {
                Ok(key) => key,
                Err(e) => anyhow::bail!("Failed to login: {:#}", e),
            };
            config::add_nsclient_profile(id, url, *insecure, &username, &password, &key)?;
            output.print("Successfully logged in");
            Ok(())
        }
        AuthCommand::Refresh { id } => {
            let profile = match config::get_nsclient_profile(id)? {
                Some(profile) => profile,
                None => anyhow::bail!("Profile with id '{id}' does not exist"),
            };
            let password = config::get_password(id)?;
            let key = match login_and_fetch_key(
                &profile.url,
                &profile.username,
                &password,
                &profile.insecure,
            )
            .await
            {
                Ok(key) => key,
                Err(e) => anyhow::bail!("Failed to login: {:#}", e),
            };
            config::update_token(id, &key)?;
            output.print("Token successfully refreshed");
            Ok(())
        }
        AuthCommand::Logout { id } => {
            if let Err(e) = config::remove_nsclient_profile(id) {
                anyhow::bail!("Failed to logout: {:#}", e);
            } else {
                output.print("Successfully logged out");
                Ok(())
            }
        }
    }
}
