use crate::cli::AuthCommand;
use crate::config;
use crate::nsclient::api::Auth;
use crate::nsclient::build_client;
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
            let api = build_client(
                url,
                30,
                "TODO",
                Auth::Password(username.to_owned(), password.to_owned()),
                *insecure,
            )?;
            match api.login().await {
                Ok(details) => {
                    config::add_nsclient_profile(id, url, *insecure, &details.key)?;
                    output.print("Successfully logged in");
                    Ok(())
                }
                Err(e) => anyhow::bail!("Failed to login: {:#}", e),
            }
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
