use crate::cli::QueriesCommand;
use crate::nsclient::api::ApiClientApi;
use crate::rendering::Rendering;

pub async fn route_query_commands(
    output: Rendering,
    api: Box<dyn ApiClientApi>,
    command: &QueriesCommand,
) -> anyhow::Result<()> {
    match &command {
        QueriesCommand::List { all, long } => match api.list_queries(all).await {
            Ok(queries) => {
                if output.is_flat() {
                    output.render_flat_list(&queries, long, &["description", "name", "plugin_id"])
                } else {
                    output.render_nested_list(&queries)
                }
            }
            Err(e) => anyhow::bail!("Failed to fetch queries from: {:#}", e),
        },
        &QueriesCommand::Show { id } => match api.get_query(id).await {
            Ok(query) => {
                if output.is_flat() {
                    output.render_flat_single(&query.to_dict())
                } else {
                    output.render_nested_single(&query)
                }
            }
            Err(e) => anyhow::bail!("Failed to fetch query {id} from: {:#}", e),
        },
        &QueriesCommand::Execute { id, args } => match api.execute_query(id, args).await {
            Ok(result) => {
                if output.is_flat() {
                    output.render_flat_single(&result.to_dict())
                } else {
                    output.render_nested_single(&result)
                }
            }
            Err(e) => anyhow::bail!("Failed to execute query {id} from: {:#}", e),
        },
        &QueriesCommand::ExecuteNagios { id, args } => {
            match api.execute_query_nagios(id, args).await {
                Ok(result) => {
                    let exit = result.get_exit_code();
                    if output.is_text() {
                        for line in result.lines {
                            if line.perf.is_empty() {
                                println!("{}", line.message);
                            } else {
                                println!("{}|{}", line.message, line.perf);
                            }
                        }
                        std::process::exit(exit);
                    }
                    if let Err(e) = if output.is_flat() {
                        output.render_flat_single(&result.to_dict())
                    } else {
                        output.render_nested_single(&result)
                    } {
                        eprintln!("Failed to render output: {:#}", e);
                        std::process::exit(4);
                    }
                    std::process::exit(exit);
                }
                Err(e) => anyhow::bail!("Failed to execute query {id} from: {:#}", e),
            }
        }
    }
}
