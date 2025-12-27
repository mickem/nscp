use crate::cli::MetricsCommand;
use crate::nsclient::api::ApiClient;
use crate::rendering::Rendering;
use indexmap::IndexMap;

pub async fn route_metrics_commands(
    output: Rendering,
    api: &ApiClient,
    command: &MetricsCommand,
) -> anyhow::Result<()> {
    match command {
        MetricsCommand::Show {} => match api.get_metrics().await {
            Ok(metrics) => {
                if output.is_flat() {
                    let mut flat = IndexMap::new();
                    for (key, value) in metrics.iter() {
                        flat.insert(key.clone(), value.to_string());
                    }
                    output.render_flat_single(&flat)
                } else {
                    output.render_nested_single(&metrics)
                }
            }
            Err(e) => anyhow::bail!("Failed to fetch metrics: {:#}", e),
        },
    }
}
