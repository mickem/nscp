mod api;
mod messages;

use crate::cli::{ModulesCommand, NSClientCommands};
use crate::nsclient::api::ApiClient;
use crate::rendering::Rendering;
use std::time::Duration;

pub async fn route_ns_client(
    output: Rendering,
    command: &NSClientCommands,
    url: &str,
    base_path: &str,
    timeout_s: &u64,
    user_agent: &str,
    insecure: &bool,
    username: &String,
    password: &String,
) -> anyhow::Result<()> {
    let url = url.trim_end_matches('/');
    let base_path = base_path.trim_matches('/');

    let base_url = format!("{}/{}", url, base_path);
    let base_url = if base_url.ends_with("/") {
        base_url
    } else {
        format!("{base_url}/")
    };

    let client = reqwest::Client::builder()
        .timeout(Duration::from_secs(timeout_s.to_owned()))
        .user_agent(user_agent)
        .danger_accept_invalid_certs(insecure.to_owned());

    let api = ApiClient::new(client, base_url, username.clone(), password.clone())?;

    match command {
        NSClientCommands::Ping {} => match api.ping().await {
            Ok(item) => {
                println!("Successfully pinged {} version {}", item.name, item.version);
                Ok(())
            }
            Err(e) => anyhow::bail!("Failed to ping server: {:#}", e),
        },
        NSClientCommands::Version {} => match api.ping().await {
            Ok(item) => {
                if output.is_flat() {
                    output.render_flat_single(&item.to_dict())
                } else {
                    output.render_nested_single(&item)
                }
            }
            Err(e) => anyhow::bail!("Failed to fetch version from: {:#}", e),
        },
        NSClientCommands::Modules { command } => match &command {
            ModulesCommand::List { all } => match api.list_modules(all).await {
                Ok(modules) => {
                    if output.is_flat() {
                        let flat_modules = modules.iter().map(|m| m.to_flat()).collect::<Vec<_>>();
                        output
                            .render_flat_list(&flat_modules, &["description", "name", "plugin_id"])
                    } else {
                        output.render_nested_list(&modules)
                    }
                }
                Err(e) => anyhow::bail!("Failed to fetch modules from: {:#}", e),
            },
            &ModulesCommand::Show { id } => match api.get_module(&id).await {
                Ok(module) => {
                    if output.is_flat() {
                        output.render_flat_single(&module.to_dict())
                    } else {
                        output.render_nested_single(&module)
                    }
                }
                Err(e) => anyhow::bail!("Failed to fetch module {id} from: {:#}", e),
            },
            &ModulesCommand::Load { id } => match api.module_command(&id, "load").await {
                Ok(()) => {
                    println!(
                        "Successfully unloaded module {}, you can now interact with it but next time the service is restarted it will be unloaded",
                        id
                    );
                    Ok(())
                }
                Err(e) => anyhow::bail!("Failed to load module {id} from: {:#}", e),
            },
            &ModulesCommand::Unload { id } => match api.module_command(&id, "unload").await {
                Ok(()) => {
                    println!("Successfully unloaded module {}", id);
                    Ok(())
                }
                Err(e) => anyhow::bail!("Failed to unload module {id} from: {:#}", e),
            },
            &ModulesCommand::Enable { id } => match api.module_command(&id, "enable").await {
                Ok(()) => {
                    println!(
                        "Successfully enabled module {}, this module will be available if you restart the service or if you load it",
                        id
                    );
                    Ok(())
                }
                Err(e) => anyhow::bail!("Failed to enable module {id} from: {:#}", e),
            },
            &ModulesCommand::Disable { id } => match api.module_command(&id, "disable").await {
                Ok(()) => {
                    println!(
                        "Successfully disabled module {}, this module will not be available if you restart the service",
                        id
                    );
                    Ok(())
                }
                Err(e) => anyhow::bail!("Failed to disabled module {id} from: {:#}", e),
            },
            &ModulesCommand::Use { id } => {
                if let Err(e) = api.module_command(&id, "load").await {
                    anyhow::bail!("Failed to disabled load {id} from: {:#}", e);
                }
                if let Err(e) = api.module_command(&id, "enable").await {
                    anyhow::bail!("Failed to disabled enable {id} from: {:#}", e);
                }
                println!("Successfully loaded and enable module {}", id);
                Ok(())
            }
        },
    }
}
