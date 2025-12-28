use crate::cli::AuthCommand;
use crate::nsclient::api::ApiClientApi;
use crate::rendering::Rendering;
use crate::tokens::{clear_token, store_token};

pub async fn route_auth_commands(
    output: Rendering,
    api: Box<dyn ApiClientApi>,
    command: &AuthCommand,
) -> anyhow::Result<()> {
    match command {
        AuthCommand::Login { username, password } => match api.login(username, password).await {
            Ok(token) => {
                store_token(username, &token).await?;
                output.print("Successfully logged in");
                Ok(())
            }
            Err(e) => anyhow::bail!("Failed to login: {:#}", e),
        },
        AuthCommand::Logout { username } => {
            if let Err(e) = clear_token(username) {
                anyhow::bail!("Failed to logout: {:#}", e);
            } else {
                output.print("Successfully logged out");
                Ok(())
            }
        }
    }
}
