use crate::config;
use crate::nsclient::login_helper::login_and_fetch_key;
use crate::nsclient::messages::{
    ExecuteNagiosResult, ExecuteResult, ListModulesResult, ListQueriesResult, LogRecord, LogStatus,
    LoginResponse, Metrics, ModulesResult, PaginatedResponse, PingResult, QueryResult,
    ScriptRuntimes, SettingsCommandAction, SettingsCommandRequest, SettingsDescription,
    SettingsEntry, SettingsStatus,
};
use async_trait::async_trait;
#[cfg(test)]
use mockall::automock;
use reqwest::header::{AUTHORIZATION, HeaderMap};
use reqwest::{ClientBuilder, Method, Response};
use serde::de::DeserializeOwned;

fn header_or_zero(headers: &HeaderMap, key: &str) -> u64 {
    headers
        .get(key)
        .and_then(|value| value.to_str().ok())
        .and_then(|value| value.parse::<u64>().ok())
        .unwrap_or(0)
}
#[derive(Clone)]
pub enum Auth {
    Password(String, String),
    Token(String),
}

pub struct ApiClient {
    client: reqwest::Client,
    base_url: String,
    auth: Auth,
    id: Option<String>,
}

impl ApiClient {
    pub(crate) fn new(
        builder: ClientBuilder,
        base_url: &str,
        auth: Auth,
        id: Option<String>,
    ) -> anyhow::Result<Self> {
        Ok(Self {
            client: builder.build()?,
            base_url: base_url.to_owned(),
            auth,
            id,
        })
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

    async fn refresh_token(&self) -> anyhow::Result<()> {
        if let Some(id) = &self.id {
            let profile = match config::get_nsclient_profile(&id)? {
                Some(profile) => profile,
                None => {
                    anyhow::bail!("Failed to refresh token because profile {id} does not exist")
                }
            };
            let password = config::get_password(&id)?;

            let token = login_and_fetch_key(
                &profile.url,
                &profile.username,
                &password,
                &profile.insecure,
                profile.ca,
            )
            .await?;
            config::update_token(&id, &token)?;
        }
        Ok(())
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
        match &self.auth {
            Auth::Password(username, password) => Ok(self
                .client
                .request(method, url)
                .basic_auth(username, Some(password.clone()))),
            Auth::Token(token) => Ok(self
                .client
                .request(method, url)
                .header(AUTHORIZATION, format!("Bearer {token}"))),
        }
    }

    async fn send(&self, builder: reqwest::RequestBuilder, path: &str) -> anyhow::Result<Response> {
        let builder_clone = builder.try_clone();
        let response = builder.send().await?;
        if response.status() == reqwest::StatusCode::UNAUTHORIZED
            || response.status() == reqwest::StatusCode::FORBIDDEN
        {
            self.refresh_token().await?;
            if let Some(builder) = builder_clone {
                let response = builder.send().await?;
                if response.status() == reqwest::StatusCode::UNAUTHORIZED
                    || response.status() == reqwest::StatusCode::FORBIDDEN
                {
                    anyhow::bail!("Failed to refresh token");
                }
                if !response.status().is_success() {
                    anyhow::bail!("Invalid response status from {path}: {}", response.status());
                }
                return Ok(response);
            }
            anyhow::bail!(
                "Authentication failure from {path}, and it was not possible to refresh the token: {}",
                response.status()
            );
        }
        if !response.status().is_success() {
            anyhow::bail!("Invalid response status from {path}: {}", response.status());
        }
        Ok(response)
    }

    fn url_for(&self, path: &str) -> String {
        format!("{}{}", self.base_url, path)
    }

    fn build_page<T>(content: T, headers: &HeaderMap) -> PaginatedResponse<T> {
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

#[cfg_attr(test, automock)]
#[async_trait]
pub trait ApiClientApi: Send + Sync {
    async fn ping(&self) -> anyhow::Result<PingResult>;
    async fn get_logs(
        &self,
        page: u64,
        size: u64,
        level: Option<String>,
    ) -> anyhow::Result<PaginatedResponse<Vec<LogRecord>>>;
    async fn get_logs_since(
        &self,
        page: u64,
        size: u64,
        since: usize,
    ) -> anyhow::Result<(PaginatedResponse<Vec<LogRecord>>, usize)>;
    async fn get_log_status(&self) -> anyhow::Result<LogStatus>;
    async fn reset_log_status(&self) -> anyhow::Result<()>;
    async fn list_modules(&self, all: &bool) -> anyhow::Result<Vec<ListModulesResult>>;
    async fn get_module(&self, id: &str) -> anyhow::Result<ModulesResult>;
    async fn module_command(&self, id: &str, command: &str) -> anyhow::Result<()>;
    async fn list_queries(&self, all: &bool) -> anyhow::Result<Vec<ListQueriesResult>>;
    async fn get_query(&self, id: &str) -> anyhow::Result<QueryResult>;
    async fn execute_query(
        &self,
        id: &str,
        args: &[(String, String)],
    ) -> anyhow::Result<ExecuteResult>;
    async fn execute_query_nagios(
        &self,
        id: &str,
        args: &[(String, String)],
    ) -> anyhow::Result<ExecuteNagiosResult>;
    async fn list_script_runtimes(&self) -> anyhow::Result<Vec<ScriptRuntimes>>;
    async fn list_scripts(&self, runtime: &str) -> anyhow::Result<Vec<String>>;
    async fn get_settings_status(&self) -> anyhow::Result<SettingsStatus>;
    async fn get_settings(&self) -> anyhow::Result<Vec<SettingsEntry>>;
    async fn get_settings_descriptions(&self) -> anyhow::Result<Vec<SettingsDescription>>;
    async fn update_settings(&self, settings: &SettingsEntry) -> anyhow::Result<()>;
    async fn settings_command(&self, command: SettingsCommandAction) -> anyhow::Result<()>;
    async fn login(&self) -> anyhow::Result<LoginResponse>;
    async fn get_metrics(&self) -> anyhow::Result<Metrics>;
}

#[async_trait::async_trait]
impl ApiClientApi for ApiClient {
    async fn ping(&self) -> anyhow::Result<PingResult> {
        self.get_json("api/v2/info").await
    }

    async fn get_logs(
        &self,
        page: u64,
        size: u64,
        level: Option<String>,
    ) -> anyhow::Result<PaginatedResponse<Vec<LogRecord>>> {
        let mut params: Vec<(String, String)> = Vec::new();
        params.push(("page".to_string(), page.to_string()));
        params.push(("per_page".to_string(), size.to_string()));
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

    async fn get_logs_since(
        &self,
        page: u64,
        size: u64,
        since: usize,
    ) -> anyhow::Result<(PaginatedResponse<Vec<LogRecord>>, usize)> {
        let params: Vec<(String, String)> = vec![
            ("page".to_string(), page.to_string()),
            ("per_page".to_string(), size.to_string()),
            ("since".to_string(), since.to_string()),
        ];
        let query: Vec<(&str, &str)> = params
            .iter()
            .map(|(k, v)| (k.as_str(), v.as_str()))
            .collect();
        let path = "api/v2/logs/since";
        let response = self
            .send(self.authed_request(Method::GET, path)?.query(&query), path)
            .await?;
        let headers = response.headers().clone();
        let content = response.json::<Vec<LogRecord>>().await?;
        let last_index = header_or_zero(&headers, "X-Log-Index") as usize;
        Ok((Self::build_page(content, &headers), last_index))
    }

    async fn get_log_status(&self) -> anyhow::Result<LogStatus> {
        self.get_json("api/v2/logs/status").await
    }

    async fn reset_log_status(&self) -> anyhow::Result<()> {
        self.delete("api/v2/logs/status").await
    }

    async fn list_modules(&self, all: &bool) -> anyhow::Result<Vec<ListModulesResult>> {
        let params = [("all", all.to_string())];
        self.get_with_query("api/v2/modules", &params).await
    }

    async fn get_module(&self, id: &str) -> anyhow::Result<ModulesResult> {
        self.get_json(&format!("api/v2/modules/{id}")).await
    }

    async fn module_command(&self, id: &str, command: &str) -> anyhow::Result<()> {
        let path = format!("api/v2/modules/{id}/commands/{command}");
        self.get_empty(&path).await
    }

    async fn list_queries(&self, all: &bool) -> anyhow::Result<Vec<ListQueriesResult>> {
        let params = [("all", all.to_string())];
        self.get_with_query("api/v2/queries", &params).await
    }

    async fn get_query(&self, id: &str) -> anyhow::Result<QueryResult> {
        self.get_json(&format!("api/v2/queries/{id}")).await
    }

    async fn execute_query(
        &self,
        id: &str,
        args: &[(String, String)],
    ) -> anyhow::Result<ExecuteResult> {
        let path = format!("api/v2/queries/{id}/commands/execute");
        self.get_with_owned_query(&path, args).await
    }

    async fn execute_query_nagios(
        &self,
        id: &str,
        args: &[(String, String)],
    ) -> anyhow::Result<ExecuteNagiosResult> {
        let path = format!("api/v2/queries/{id}/commands/execute_nagios");
        self.get_with_owned_query(&path, args).await
    }

    async fn list_script_runtimes(&self) -> anyhow::Result<Vec<ScriptRuntimes>> {
        self.get_json("api/v2/scripts").await
    }
    async fn list_scripts(&self, runtime: &str) -> anyhow::Result<Vec<String>> {
        self.get_json(&format!("api/v2/scripts/{}", runtime)).await
    }

    async fn get_settings_status(&self) -> anyhow::Result<SettingsStatus> {
        self.get_json("api/v2/settings/status").await
    }

    async fn get_settings(&self) -> anyhow::Result<Vec<SettingsEntry>> {
        self.get_json("api/v2/settings").await
    }

    async fn get_settings_descriptions(&self) -> anyhow::Result<Vec<SettingsDescription>> {
        self.get_json("api/v2/settings/descriptions").await
    }

    async fn update_settings(&self, settings: &SettingsEntry) -> anyhow::Result<()> {
        self.send(
            self.authed_request(Method::PUT, "api/v2/settings")?
                .json(settings),
            "api/v2/settings",
        )
        .await
        .map(|_| ())
    }

    async fn settings_command(&self, command: SettingsCommandAction) -> anyhow::Result<()> {
        let payload = SettingsCommandRequest { command };
        self.send(
            self.authed_request(Method::POST, "api/v2/settings/command")?
                .json(&payload),
            "api/v2/settings/command",
        )
        .await
        .map(|_| ())
    }

    async fn login(&self) -> anyhow::Result<LoginResponse> {
        self.get_json("api/v2/login").await
    }

    async fn get_metrics(&self) -> anyhow::Result<Metrics> {
        self.get_json("api/v2/metrics").await
    }
}

#[cfg(test)]
pub mod mocks {
    use super::*;
    use mockall::mock;

    mock! {
        pub ApiClientApiImpl {}

        #[async_trait::async_trait]
        impl ApiClientApi for ApiClientApiImpl {
            async fn ping(&self) -> anyhow::Result<PingResult>;
            async fn get_logs(
                &self,
                page: u64,
                size: u64,
                level: Option<String>,
            ) -> anyhow::Result<PaginatedResponse<Vec<LogRecord>>>;
            async fn get_logs_since(&self, page: u64, size: u64, since: usize) -> anyhow::Result<(PaginatedResponse<Vec<LogRecord>>, usize)>;
            async fn get_log_status(&self) -> anyhow::Result<LogStatus>;
            async fn reset_log_status(&self) -> anyhow::Result<()>;
            async fn list_modules(&self, all: &bool) -> anyhow::Result<Vec<ListModulesResult>>;
            async fn get_module(&self, id: &str) -> anyhow::Result<ModulesResult>;
            async fn module_command(&self, id: &str, command: &str) -> anyhow::Result<()>;
            async fn list_queries(&self, all: &bool) -> anyhow::Result<Vec<ListQueriesResult>>;
            async fn get_query(&self, id: &str) -> anyhow::Result<QueryResult>;
            async fn execute_query(
                &self,
                id: &str,
                args: &[(String, String)],
            ) -> anyhow::Result<ExecuteResult>;
            async fn execute_query_nagios(
                &self,
                id: &str,
                args: &[(String, String)],
            ) -> anyhow::Result<ExecuteNagiosResult>;
            async fn list_script_runtimes(&self) -> anyhow::Result<Vec<ScriptRuntimes>>;
            async fn list_scripts(&self, runtime: &str) -> anyhow::Result<Vec<String>>;
            async fn get_settings_status(&self) -> anyhow::Result<SettingsStatus>;
            async fn get_settings(&self) -> anyhow::Result<Vec<SettingsEntry>>;
            async fn get_settings_descriptions(&self) -> anyhow::Result<Vec<SettingsDescription>>;
            async fn update_settings(&self, settings: &SettingsEntry) -> anyhow::Result<()>;
            async fn settings_command(
                &self,
                command: SettingsCommandAction,
            ) -> anyhow::Result<()>;
            async fn login(&self) -> anyhow::Result<LoginResponse>;
            async fn get_metrics(&self) -> anyhow::Result<Metrics>;
        }
    }
}
