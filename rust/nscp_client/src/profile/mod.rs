use crate::cli::ProfileCommands;
use crate::config::{self, NSClientProfile};
use crate::rendering::Rendering;
use crate::tokens;
use crate::tokens::KeyType;
use indexmap::IndexMap;
use serde::Serialize;
use tabled::Tabled;

#[derive(Tabled, Serialize)]
struct ProfileRow {
    #[tabled()]
    id: String,
    #[tabled()]
    url: String,
    #[tabled(display("crate::profile::display_bool"))]
    insecure: bool,
    #[tabled(display("crate::profile::display_bool"))]
    default: bool,
    #[tabled(display("crate::profile::display_bool"))]
    has_token: bool,
    #[tabled(display("crate::profile::display_bool"))]
    has_password: bool,
}

fn display_bool(value: &bool) -> String {
    if *value {
        "yes".to_string()
    } else {
        "no".to_string()
    }
}

fn map_profile_to_row(profile: &NSClientProfile, default_id: Option<&str>) -> ProfileRow {
    let has_token = tokens::token_exists(KeyType::Token, &profile.id);
    let has_password = tokens::token_exists(KeyType::Password, &profile.id);
    ProfileRow {
        id: profile.id.clone(),
        url: profile.url.clone(),
        insecure: profile.insecure,
        default: default_id == Some(profile.id.as_str()),
        has_token,
        has_password,
    }
}

fn profile_to_indexmap(profile: &NSClientProfile) -> IndexMap<String, String> {
    let has_token = tokens::token_exists(KeyType::Token, &profile.id);
    let has_password = tokens::token_exists(KeyType::Password, &profile.id);
    let mut map = IndexMap::new();
    map.insert("id".to_string(), profile.id.clone());
    map.insert("url".to_string(), profile.url.clone());
    map.insert("insecure".to_string(), profile.insecure.to_string());
    map.insert("has_token".to_string(), has_token.to_string());
    map.insert("has_password".to_string(), has_password.to_string());
    map
}

pub async fn route_profile(output: Rendering, command: &ProfileCommands) -> anyhow::Result<()> {
    match &command {
        ProfileCommands::List {} => {
            let (profiles, default_id) = config::list_nsclient_profiles()?;
            if profiles.is_empty() {
                output.print("No profiles configured");
                return Ok(());
            }
            if output.is_flat() {
                let rows: Vec<ProfileRow> = profiles
                    .iter()
                    .map(|profile| map_profile_to_row(profile, default_id.as_deref()))
                    .collect();
                output.render_flat_list(&rows, &false, &[])?;
            } else {
                output.render_nested_list(&profiles)?;
            }
        }
        ProfileCommands::Show { id } => {
            let profile = config::get_nsclient_profile(id)?
                .ok_or_else(|| anyhow::anyhow!("Profile with id '{id}' does not exist"))?;
            if output.is_flat() {
                let map = profile_to_indexmap(&profile);
                output.render_flat_single(&map)?;
            } else {
                output.render_nested_single(&profile)?;
            }
        }
        ProfileCommands::SetDefault { id } => {
            config::set_default_nsclient_profile(id)?;
            output.print("Default profile updated");
        }
        ProfileCommands::Remove { id } => {
            config::remove_nsclient_profile(id)?;
            output.print("Profile removed");
        }
    }
    Ok(())
}
