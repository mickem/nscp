use crate::cli::{SettingsCommand, SettingsCommandActionCli};
use crate::nsclient::api::ApiClient;
use crate::nsclient::messages::{SettingsCommandAction, SettingsEntry};
use crate::rendering::Rendering;

fn map_action(action: &SettingsCommandActionCli) -> SettingsCommandAction {
    match action {
        SettingsCommandActionCli::Load => SettingsCommandAction::Load,
        SettingsCommandActionCli::Save => SettingsCommandAction::Save,
        SettingsCommandActionCli::Reload => SettingsCommandAction::Reload,
    }
}

pub async fn route_settings_commands(
    output: Rendering,
    api: &ApiClient,
    command: &SettingsCommand,
) -> anyhow::Result<()> {
    match command {
        SettingsCommand::Status {} => match api.get_settings_status().await {
            Ok(status) => output.render_nested_single(&status),
            Err(e) => anyhow::bail!("Failed to fetch settings status: {:#}", e),
        },
        SettingsCommand::List {} => match api.get_settings().await {
            Ok(settings) => {
                if output.is_flat() {
                    output.render_flat_list(&settings, &false, &[])
                } else {
                    output.render_nested_single(&settings)
                }
            }
            Err(e) => anyhow::bail!("Failed to fetch settings entries: {:#}", e),
        },
        SettingsCommand::Descriptions {} => match api.get_settings_descriptions().await {
            Ok(descriptions) => {
                if output.is_flat() {
                    anyhow::bail!("Settings descriptions only support nested outputs");
                }
                output.render_nested_single(&descriptions)
            }
            Err(e) => anyhow::bail!("Failed to fetch settings descriptions: {:#}", e),
        },
        SettingsCommand::Set { path, key, value } => {
            let entry = SettingsEntry {
                path: path.clone(),
                key: key.clone(),
                value: value.clone(),
            };
            match api.update_settings(&entry).await {
                Ok(()) => {
                    output.print(&format!("Updated {path}/{key}"));
                    Ok(())
                }
                Err(e) => anyhow::bail!("Failed to update setting {path}/{key}: {:#}", e),
            }
        }
        SettingsCommand::Command { action } => match api.settings_command(map_action(action)).await
        {
            Ok(()) => {
                output.print(&format!("Executed {action:?} command"));
                Ok(())
            }
            Err(e) => anyhow::bail!("Failed to execute settings command {action:?}: {:#}", e),
        },
    }
}
