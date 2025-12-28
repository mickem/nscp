use crate::nsclient::api::ApiClientApi;
use crate::rendering::Rendering;

pub async fn handle_ping_command(
    output: Rendering,
    api: Box<dyn ApiClientApi>,
) -> anyhow::Result<()> {
    match api.ping().await {
        Ok(item) => {
            output.print(&format!(
                "Successfully pinged {} version {}",
                item.name, item.version
            ));
            Ok(())
        }
        Err(e) => anyhow::bail!("Failed to ping server: {:#}", e),
    }
}
pub async fn handle_version_command(
    output: Rendering,
    api: Box<dyn ApiClientApi>,
) -> anyhow::Result<()> {
    match api.ping().await {
        Ok(item) => {
            if output.is_flat() {
                output.render_flat_single(&item.to_dict())
            } else {
                output.render_nested_single(&item)
            }
        }
        Err(e) => anyhow::bail!("Failed to fetch version from: {:#}", e),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::cli::{OutputFormat, OutputStyle};
    use crate::nsclient::api::mocks::MockApiClientApiImpl;
    use crate::nsclient::messages::PingResult;
    use crate::rendering::StringRender;
    use anyhow::anyhow;

    #[tokio::test]
    async fn test_ping_text() {
        let mut api = MockApiClientApiImpl::new();
        api.expect_ping().returning(|| {
            Ok(PingResult {
                name: "NSClient++".to_string(),
                version: "0.5.2.35".to_string(),
            })
        });

        let output_sink = Box::new(StringRender::new());
        let output_ref = output_sink.string.clone();

        let output = Rendering::new(OutputFormat::Text, OutputStyle::Rounded, false, output_sink);

        let result = handle_ping_command(output, Box::new(api)).await;

        assert_eq!(
            output_ref.borrow().as_str(),
            "Successfully pinged NSClient++ version 0.5.2.35\n"
        );
        assert!(result.is_ok());
    }

    #[tokio::test]
    async fn test_ping_error() {
        let mut api = MockApiClientApiImpl::new();
        api.expect_ping().returning(|| Err(anyhow!("boom")));

        let output_sink = Box::new(StringRender::new());
        let output = Rendering::new(OutputFormat::Text, OutputStyle::Rounded, false, output_sink);

        let result = handle_ping_command(output, Box::new(api)).await;

        assert!(result.is_err());
        assert!(
            result
                .unwrap_err()
                .to_string()
                .contains("Failed to ping server")
        );
    }

    #[tokio::test]
    async fn test_version_json() {
        let mut api = MockApiClientApiImpl::new();
        api.expect_ping().returning(|| {
            Ok(PingResult {
                name: "NSClient++".to_string(),
                version: "0.5.2.35".to_string(),
            })
        });

        let output_sink = Box::new(StringRender::new());
        let output_ref = output_sink.string.clone();

        let output = Rendering::new(OutputFormat::Json, OutputStyle::Rounded, false, output_sink);

        let result = handle_version_command(output, Box::new(api)).await;

        assert_eq!(
            output_ref.borrow().as_str(),
            r#"{
  "name": "NSClient++",
  "version": "0.5.2.35"
}
"#
        );
        assert!(result.is_ok());
    }

    #[tokio::test]
    async fn test_version_text() {
        let mut api = MockApiClientApiImpl::new();
        api.expect_ping().returning(|| {
            Ok(PingResult {
                name: "NSClient++".to_string(),
                version: "0.5.2.35".to_string(),
            })
        });

        let output_sink = Box::new(StringRender::new());
        let output_ref = output_sink.string.clone();

        let output = Rendering::new(OutputFormat::Text, OutputStyle::Rounded, false, output_sink);

        let result = handle_version_command(output, Box::new(api)).await;

        assert_eq!(
            output_ref.borrow().as_str(),
            r#"╭─────────┬────────────╮
│ name    │ NSClient++ │
│ version │ 0.5.2.35   │
╰─────────┴────────────╯
"#
        );
        assert!(result.is_ok());
    }

    #[tokio::test]
    async fn test_version_yaml() {
        let mut api = MockApiClientApiImpl::new();
        api.expect_ping().returning(|| {
            Ok(PingResult {
                name: "NSClient++".to_string(),
                version: "0.5.2.35".to_string(),
            })
        });

        let output_sink = Box::new(StringRender::new());
        let output_ref = output_sink.string.clone();

        let output = Rendering::new(OutputFormat::Yaml, OutputStyle::Rounded, false, output_sink);

        let result = handle_version_command(output, Box::new(api)).await;

        assert_eq!(
            output_ref.borrow().as_str(),
            r#"name: NSClient++
version: 0.5.2.35

"#
        );
        assert!(result.is_ok());
    }

    #[tokio::test]
    async fn test_version_csv() {
        let mut api = MockApiClientApiImpl::new();
        api.expect_ping().returning(|| {
            Ok(PingResult {
                name: "NSClient++".to_string(),
                version: "0.5.2.35".to_string(),
            })
        });

        let output_sink = Box::new(StringRender::new());
        let output_ref = output_sink.string.clone();

        let output = Rendering::new(OutputFormat::Csv, OutputStyle::Rounded, false, output_sink);

        let result = handle_version_command(output, Box::new(api)).await;

        assert_eq!(
            output_ref.borrow().as_str(),
            r#"name,NSClient++
version,0.5.2.35

"#
        );
        assert!(result.is_ok());
    }

    #[tokio::test]
    async fn test_version_error() {
        let mut api = MockApiClientApiImpl::new();
        api.expect_ping().returning(|| Err(anyhow!("boom")));

        let output_sink = Box::new(StringRender::new());
        let output = Rendering::new(OutputFormat::Text, OutputStyle::Rounded, false, output_sink);

        let result = handle_version_command(output, Box::new(api)).await;

        assert!(result.is_err());
        assert!(
            result
                .unwrap_err()
                .to_string()
                .contains("Failed to fetch version from")
        );
    }
}
