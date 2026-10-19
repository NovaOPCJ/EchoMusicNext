//! Playlist deletion uses type=1 even for a newly created type=0 list.
use crate::{
    domain::LibraryPlaylistChange,
    error::ApiError,
    util::{
        cloudlist,
        crypto::PlaylistEnvelope,
        request::{self, ApiClient, Identity},
    },
};
use serde::Deserialize;
use std::collections::BTreeMap;
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Query {
    pub cloud_list_id: u64,
}
#[derive(Deserialize)]
struct Data {
    #[serde(deserialize_with = "crate::util::response::scalar")]
    userid: u64,
    #[serde(deserialize_with = "crate::util::response::scalar")]
    pre_total_ver: u64,
    #[serde(deserialize_with = "crate::util::response::scalar")]
    total_ver: u64,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    list_count: Option<u64>,
    #[serde(deserialize_with = "crate::util::response::list")]
    info: Vec<Item>,
}
#[derive(Deserialize)]
struct Item {
    #[serde(deserialize_with = "crate::util::response::scalar")]
    listid: u64,
    #[serde(deserialize_with = "crate::util::response::scalar")]
    status: u32,
    #[serde(deserialize_with = "crate::util::response::scalar")]
    code: i64,
}
pub fn convert(bytes: &[u8], list_id: u64) -> Result<LibraryPlaylistChange, ApiError> {
    let v: Data = cloudlist::decode(bytes)?;
    for item in &v.info {
        if item.status != 1 || item.code != 1 {
            return Err(ApiError::business(item.code));
        }
    }
    Ok(LibraryPlaylistChange {
        user_id: v.userid,
        cloud_list_id: v.info.first().map(|item| item.listid).unwrap_or(list_id),
        previous_total_revision: v.pre_total_ver,
        total_revision: v.total_ver,
        playlist_count: v.list_count,
        cover_url: None,
    })
}
pub async fn execute(
    client: &ApiClient,
    identity: &Identity,
    query: Query,
) -> Result<Vec<u8>, ApiError> {
    cloudlist::account(identity)?;
    cloudlist::id(query.cloud_list_id)?;
    let envelope = PlaylistEnvelope::new(
        &serde_json::json!({"listid":query.cloud_list_id,"total_ver":0,"type":1}),
        identity.userid,
        &identity.token,
    )?;
    let time = request::unix_seconds()?;
    let response = client
        .request(
            identity,
            "/v2/delete_list",
            Some("cloudlist.service.kugou.com"),
            reqwest::Method::POST,
            BTreeMap::from([
                ("clienttime".into(), time.to_string()),
                ("key".into(), cloudlist::keyed_time(time)),
                ("last_area".into(), "gztx".into()),
                ("last_time".into(), time.to_string()),
                ("p".into(), envelope.portrait.to_uppercase()),
            ]),
            envelope.base64_body(),
        )
        .await?;
    let bytes = envelope.decrypt(&response)?;
    request::encode(&convert(&bytes, query.cloud_list_id)?)
}
