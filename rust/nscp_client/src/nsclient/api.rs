use crate::nsclient::messages::{
    ExecuteNagiosResult, ExecuteResult, ListModulesResult, ListQueriesResult, LogRecord, LogStatus,
    LoginResponse, Metrics, ModulesResult, PaginatedResponse, PingResult, QueryResult,
    ScriptRuntimes, SettingsCommandAction, SettingsCommandRequest, SettingsDescription,
    SettingsEntry, SettingsStatus,
};
use crate::tokens::load_token;
use reqwest::header::{AUTHORIZATION, HeaderMap};
use reqwest::{ClientBuilder, Method};
use serde::de::DeserializeOwned;

pub struct ApiClient {
    client: reqwest::Client,
    base_url: String,
    username: String,
    password: Option<String>,
}

impl ApiClient {
    pub(crate) fn new(
        builder: ClientBuilder,
        base_url: &str,
        username: String,
        password: Option<String>,
    ) -> anyhow::Result<Self> {
        Ok(Self {
            client: builder.build()?,
            base_url: base_url.to_owned(),
            username,
            password,
        })
    }

    pub(crate) async fn ping(&self) -> anyhow::Result<PingResult> {
        self.get_json("api/v2/info").await
    }

    pub(crate) async fn get_logs(
        &self,
        page: u64,
        size: u64,
        level: Option<&str>,
    ) -> anyhow::Result<PaginatedResponse<Vec<LogRecord>>> {
        let mut params: Vec<(String, String)> = Vec::new();
        params.push(("page".to_string(), page.to_string()));
        params.push(("size".to_string(), size.to_string()));
        if let Some(level) = level {
            params.push(("level".to_string(), level.to_string()));
        }
        let query: Vec<(&str, &str)> = params
            .iter()
            .map(|(k, v)| (k.as_str(), v.as_str()))
            .collect();
        let path = "api/v2/logs";
        let response = self
            .send(self.authed_request(Method::GET, path)?.query(&query), path)
            .await?;
        let headers = response.headers().clone();
        let content = response.json::<Vec<LogRecord>>().await?;
        Ok(Self::build_page(content, &headers))
    }

    pub(crate) async fn get_log_status(&self) -> anyhow::Result<LogStatus> {
        self.get_json("api/v2/logs/status").await
    }

    pub(crate) async fn reset_log_status(&self) -> anyhow::Result<()> {
        self.delete("api/v2/logs/status").await
    }

    pub(crate) async fn list_modules(&self, all: &bool) -> anyhow::Result<Vec<ListModulesResult>> {
        let params = [("all", all.to_string())];
        self.get_with_query("api/v2/modules", &params).await
    }

    pub(crate) async fn get_module(&self, id: &str) -> anyhow::Result<ModulesResult> {
        self.get_json(&format!("api/v2/modules/{id}")).await
    }

    pub(crate) async fn module_command(&self, id: &str, command: &str) -> anyhow::Result<()> {
        let path = format!("api/v2/modules/{id}/commands/{command}");
        self.get_empty(&path).await
    }

    pub(crate) async fn list_queries(&self, all: &bool) -> anyhow::Result<Vec<ListQueriesResult>> {
        let params = [("all", all.to_string())];
        self.get_with_query("api/v2/queries", &params).await
    }

    pub(crate) async fn get_query(&self, id: &str) -> anyhow::Result<QueryResult> {
        self.get_json(&format!("api/v2/queries/{id}")).await
    }

    pub(crate) async fn execute_query(
        &self,
        id: &str,
        args: &[(String, String)],
    ) -> anyhow::Result<ExecuteResult> {
        let path = format!("api/v2/queries/{id}/commands/execute");
        self.get_with_owned_query(&path, args).await
    }

    pub(crate) async fn execute_query_nagios(
        &self,
        id: &str,
        args: &[(String, String)],
    ) -> anyhow::Result<ExecuteNagiosResult> {
        let path = format!("api/v2/queries/{id}/commands/execute_nagios");
        self.get_with_owned_query(&path, args).await
    }

    pub(crate) async fn list_script_runtimes(&self) -> anyhow::Result<Vec<ScriptRuntimes>> {
        self.get_json("api/v2/scripts").await
    }
    pub(crate) async fn list_scripts(&self, runtime: &str) -> anyhow::Result<Vec<String>> {
        self.get_json(&format!("api/v2/scripts/{}", runtime)).await
    }

    pub(crate) async fn get_settings_status(&self) -> anyhow::Result<SettingsStatus> {
        self.get_json("api/v2/settings/status").await
    }

    pub(crate) async fn get_settings(&self) -> anyhow::Result<Vec<SettingsEntry>> {
        self.get_json("api/v2/settings").await
    }

    pub(crate) async fn get_settings_descriptions(
        &self,
    ) -> anyhow::Result<Vec<SettingsDescription>> {
        self.get_json("api/v2/settings/descriptions").await
    }

    pub(crate) async fn update_settings(&self, settings: &SettingsEntry) -> anyhow::Result<()> {
        self.send(
            self.authed_request(Method::PUT, "api/v2/settings")?
                .json(settings),
            "api/v2/settings",
        )
        .await
        .map(|_| ())
    }

    pub(crate) async fn settings_command(
        &self,
        command: SettingsCommandAction,
    ) -> anyhow::Result<()> {
        let payload = SettingsCommandRequest { command };
        self.send(
            self.authed_request(Method::POST, "api/v2/settings/command")?
                .json(&payload),
            "api/v2/settings/command",
        )
        .await
        .map(|_| ())
    }

    pub(crate) async fn login(&self, username: &str, password: &str) -> anyhow::Result<String> {
        let path = "api/v2/login";
        let url = self.url_for(path);
        let response = self
            .client
            .get(&url)
            .basic_auth(username, Some(password))
            .send()
            .await?;
        if !response.status().is_success() {
            anyhow::bail!("Invalid response status from {path}: {}", response.status());
        }
        Ok(response.json::<LoginResponse>().await?.key)
    }

    pub(crate) async fn get_metrics(&self) -> anyhow::Result<Metrics> {
        self.get_json("api/v2/metrics").await
    }

    async fn get_json<T: DeserializeOwned>(&self, path: &str) -> anyhow::Result<T> {
        let response = self
            .send(self.authed_request(Method::GET, path)?, path)
            .await?;
        Ok(response.json::<T>().await?)
    }

    async fn get_empty(&self, path: &str) -> anyhow::Result<()> {
        self.send(self.authed_request(Method::GET, path)?, path)
            .await
            .map(|_| ())
    }

    async fn get_with_query<T: DeserializeOwned>(
        &self,
        path: &str,
        query: &[(&str, String)],
    ) -> anyhow::Result<T> {
        let refs: Vec<(&str, &str)> = query.iter().map(|(k, v)| (*k, v.as_str())).collect();
        let response = self
            .send(self.authed_request(Method::GET, path)?.query(&refs), path)
            .await?;
        Ok(response.json::<T>().await?)
    }

    async fn get_with_owned_query<T: DeserializeOwned>(
        &self,
        path: &str,
        query: &[(String, String)],
    ) -> anyhow::Result<T> {
        let refs: Vec<(&str, &str)> = query
            .iter()
            .map(|(k, v)| (k.as_str(), v.as_str()))
            .collect();
        let response = self
            .send(self.authed_request(Method::GET, path)?.query(&refs), path)
            .await?;
        Ok(response.json::<T>().await?)
    }

    async fn delete(&self, path: &str) -> anyhow::Result<()> {
        self.send(self.authed_request(Method::DELETE, path)?, path)
            .await
            .map(|_| ())
    }

    fn authed_request(
        &self,
        method: Method,
        path: &str,
    ) -> anyhow::Result<reqwest::RequestBuilder> {
        let url = self.url_for(path);
        if self.password.is_some() {
            Ok(self
                .client
                .request(method, url)
                .basic_auth(&self.username, self.password.clone()))
        } else {
            let token = load_token(&self.username)?;
            Ok(self
                .client
                .request(method, url)
                .header(AUTHORIZATION, format!("Bearer {token}")))
        }
    }

    async fn send(
        &self,
        builder: reqwest::RequestBuilder,
        path: &str,
    ) -> anyhow::Result<reqwest::Response> {
        let response = builder.send().await?;
        if !response.status().is_success() {
            anyhow::bail!("Invalid response status from {path}: {}", response.status());
        }
        Ok(response)
    }

    fn url_for(&self, path: &str) -> String {
        format!("{}{}", self.base_url, path)
    }

    fn build_page<T>(content: T, headers: &HeaderMap) -> PaginatedResponse<T> {
        fn header_or_zero(headers: &HeaderMap, key: &str) -> u64 {
            headers
                .get(key)
                .and_then(|value| value.to_str().ok())
                .and_then(|value| value.parse::<u64>().ok())
                .unwrap_or(0)
        }
        let count = header_or_zero(headers, "X-Pagination-Count");
        let page = header_or_zero(headers, "X-Pagination-Page");
        let limit = header_or_zero(headers, "X-Pagination-Limit");
        let pages = if limit == 0 { 0 } else { count.div_ceil(limit) };
        PaginatedResponse {
            content,
            page,
            pages,
            limit,
            count,
        }
    }
}
