use std::time::Duration;
use reqwest::ClientBuilder;
use serde::{Deserialize, Serialize};
use crate::cli::NSClientCommands;

pub async fn route_ns_client(command: &NSClientCommands, url: &str, base_path: &str, timeout_s: &u64, user_agent: &str, insecure: &bool, username: &String, password: &String) -> anyhow::Result<()>{
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
        NSClientCommands::Ping {} => {
            match api.ping().await {
                Ok(item) => {
                    let version = match item.version {
                        Some(version) => version,
                        None => {
                            anyhow::bail!("No version returned from API")
                        }
                    };
                    let name = match item.name {
                        Some(name) => name,
                        None => {
                            anyhow::bail!("No name returned from API")
                        }
                    };
                    println!("Successfully pinged {name} version {version}");
                    Ok(())
                }
                Err(e) => anyhow::bail!("Failed to ping server: {:#}", e)
            }

        }
    }

}

// 2. Define Data Structures for the API (using Serde)
// ------------------------------------------------------------------
#[derive(Debug, Serialize, Deserialize)]
pub struct TodoItem {
    #[serde(default)] // Handle cases where API might not return ID on creation
    id: Option<u32>,
    #[serde(rename = "userId")]
    user_id: u32,
    title: String,
    completed: bool,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct PingVersion {
    name: Option<String>,
    version: Option<String>
}

// 3. API Client Logic
// ------------------------------------------------------------------
pub struct ApiClient {
    client: reqwest::Client,
    base_url: String,
    username: String,
    password: String,
}


impl ApiClient {
    fn new(builder: ClientBuilder, base_url: String, username: String, password: String) -> anyhow::Result<Self> {

        Ok(Self {
            client : builder.build()?,
            base_url,
            username,
            password,
        })
    }

    pub(crate) async fn ping(&self) -> anyhow::Result<PingVersion>{
        let url = format!("{}api/v2/info", self.base_url);
        let response = self.client.get(&url)
            .basic_auth(&self.username, Some(&self.password))
            .send().await?;

        // Error handling for non-200 status codes
        if !response.status().is_success() {
            anyhow::bail!("API request failed with status: {}", response.status());
        }

        let item = response.json::<PingVersion>().await?;
        Ok(item)
    }

    pub(crate) async fn get_todo(&self, id: u32) -> anyhow::Result<TodoItem> {
        let url = format!("{}/todos/{}", self.base_url, id);
        let response = self.client.get(&url)
            .basic_auth(&self.username, Some(&self.password))
            .send().await?;

        // Error handling for non-200 status codes
        if !response.status().is_success() {
            anyhow::bail!("API request failed with status: {}", response.status());
        }

        let item = response.json::<TodoItem>().await?;
        Ok(item)
    }

    pub(crate) async fn create_todo(&self, title: String) -> anyhow::Result<TodoItem> {
        let url = format!("{}/todos", self.base_url);

        let new_item = TodoItem {
            id: None,
            user_id: 1, // hardcoded for demo
            title,
            completed: false,
        };

        let response = self.client.post(&url)
            .basic_auth(&self.username, Some(&self.password))
            .json(&new_item)
            .send()
            .await?;

        if !response.status().is_success() {
            anyhow::bail!("Failed to create item: {}", response.status());
        }

        // The mock API returns the object with a new ID
        let created_item = response.json::<TodoItem>().await?;
        Ok(created_item)
    }
}
