use crate::nsclient::messages::{ListModulesResult, ModulesResult, PingResult};
use reqwest::ClientBuilder;

pub struct ApiClient {
    client: reqwest::Client,
    base_url: String,
    username: String,
    password: String,
}

impl ApiClient {
    pub(crate) fn new(
        builder: ClientBuilder,
        base_url: String,
        username: String,
        password: String,
    ) -> anyhow::Result<Self> {
        Ok(Self {
            client: builder.build()?,
            base_url,
            username,
            password,
        })
    }

    pub(crate) async fn ping(&self) -> anyhow::Result<PingResult> {
        let url = format!("{}api/v2/info", self.base_url);
        let response = self
            .client
            .get(&url)
            .basic_auth(&self.username, Some(&self.password))
            .send()
            .await?;

        if !response.status().is_success() {
            anyhow::bail!("Invalid response status from {url}: {}", response.status());
        }

        Ok(response.json::<PingResult>().await?)
    }

    pub(crate) async fn list_modules(&self, all: &bool) -> anyhow::Result<Vec<ListModulesResult>> {
        let url = format!("{}api/v2/modules", self.base_url);
        let response = self
            .client
            .get(&url)
            .query(&[("all", all.to_string())])
            .basic_auth(&self.username, Some(&self.password))
            .send()
            .await?;
        if !response.status().is_success() {
            anyhow::bail!("Invalid response status from {url}: {}", response.status());
        }
        Ok(response.json::<Vec<ListModulesResult>>().await?)
    }
    pub(crate) async fn get_module(&self, id: &str) -> anyhow::Result<ModulesResult> {
        let url = format!("{}api/v2/modules/{}", self.base_url, id);
        let response = self
            .client
            .get(&url)
            .basic_auth(&self.username, Some(&self.password))
            .send()
            .await?;
        if !response.status().is_success() {
            anyhow::bail!("Invalid response status from {url}: {}", response.status());
        }
        Ok(response.json::<ModulesResult>().await?)
    }

    pub(crate) async fn module_command(&self, id: &str, command: &str) -> anyhow::Result<()> {
        let url = format!(
            "{}api/v2/modules/{}/commands/{}",
            self.base_url, id, command
        );
        let response = self
            .client
            .get(&url)
            .basic_auth(&self.username, Some(&self.password))
            .send()
            .await?;
        if !response.status().is_success() {
            anyhow::bail!("Invalid response status from {url}: {}", response.status());
        }
        Ok(())
    }
}
