use crate::cli::ModulesCommand;
use crate::nsclient::api::ApiClientApi;
use crate::rendering::Rendering;

pub async fn route_module_commands(
    output: Rendering,
    api: Box<dyn ApiClientApi>,
    command: &ModulesCommand,
) -> anyhow::Result<()> {
    match &command {
        ModulesCommand::List { all, long } => match api.list_modules(all).await {
            Ok(modules) => {
                if output.is_flat() {
                    let flat_modules = modules.iter().map(|m| m.to_flat()).collect::<Vec<_>>();
                    output.render_flat_list(
                        &flat_modules,
                        long,
                        &["description", "name", "plugin_id"],
                    )
                } else {
                    output.render_nested_list(&modules)
                }
            }
            Err(e) => anyhow::bail!("Failed to fetch modules from: {:#}", e),
        },
        &ModulesCommand::Show { id } => match api.get_module(id).await {
            Ok(module) => {
                if output.is_flat() {
                    output.render_flat_single(&module.to_dict())
                } else {
                    output.render_nested_single(&module)
                }
            }
            Err(e) => anyhow::bail!("Failed to fetch module {id} from: {:#}", e),
        },
        &ModulesCommand::Load { id } => match api.module_command(id, "load").await {
            Ok(()) => {
                println!(
                    "Successfully unloaded module {}, you can now interact with it but next time the service is restarted it will be unloaded",
                    id
                );
                Ok(())
            }
            Err(e) => anyhow::bail!("Failed to load module {id} from: {:#}", e),
        },
        &ModulesCommand::Unload { id } => match api.module_command(id, "unload").await {
            Ok(()) => {
                println!("Successfully unloaded module {}", id);
                Ok(())
            }
            Err(e) => anyhow::bail!("Failed to unload module {id} from: {:#}", e),
        },
        &ModulesCommand::Enable { id } => match api.module_command(id, "enable").await {
            Ok(()) => {
                println!(
                    "Successfully enabled module {}, this module will be available if you restart the service or if you load it",
                    id
                );
                Ok(())
            }
            Err(e) => anyhow::bail!("Failed to enable module {id} from: {:#}", e),
        },
        &ModulesCommand::Disable { id } => match api.module_command(id, "disable").await {
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
            if let Err(e) = api.module_command(id, "load").await {
                anyhow::bail!("Failed to disabled load {id} from: {:#}", e);
            }
            if let Err(e) = api.module_command(id, "enable").await {
                anyhow::bail!("Failed to disabled enable {id} from: {:#}", e);
            }
            println!("Successfully loaded and enable module {}", id);
            Ok(())
        }
    }
}
