//! Cloud creation and source-1/source-2 collection. Collection ACKs were captured
//! separately and share the canonical library mutation envelope.
use crate::{
    domain::LibraryPlaylistChange,
    error::ApiError,
    util::{
        cloudlist,
        request::{self, ApiClient, Identity},
    },
};
use serde::Deserialize;
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Query {
    pub title: String,
    pub private: bool,
    #[serde(default)]
    pub collection: Option<Collection>,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Collection {
    pub source: u32,
    pub creator_id: Option<u64>,
    pub creator_list_id: u64,
    pub creator_global_id: String,
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
    info: Info,
}
#[derive(Deserialize)]
struct Info {
    #[serde(deserialize_with = "crate::util::response::scalar")]
    listid: u64,
    #[serde(deserialize_with = "crate::util::response::scalar")]
    status: u32,
    #[serde(deserialize_with = "crate::util::response::scalar")]
    code: i64,
}
pub fn convert(bytes: &[u8]) -> Result<LibraryPlaylistChange, ApiError> {
    let v: Data = cloudlist::decode(bytes)?;
    if v.info.status != 1 || v.info.code != 1 {
        return Err(ApiError::business(v.info.code));
    }
    Ok(LibraryPlaylistChange {
        user_id: v.userid,
        cloud_list_id: v.info.listid,
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
    cloudlist::text(&query.title, 1024, false)?;
    let mut body = serde_json::json!({"userid":identity.userid,"token":identity.token,"total_ver":0,"name":query.title,"type":0,"source":1,
        "is_pri":u32::from(query.private),"list_create_gid":"","from_shupinmv":0});
    let params = if let Some(collection) = query.collection {
        cloudlist::id(collection.creator_list_id)?;
        body["type"] = 1.into();
        body["is_pri"] = 0.into();
        body["source"] = collection.source.into();
        body["list_create_listid"] = collection.creator_list_id.into();
        body["list_create_gid"] = collection.creator_global_id.into();
        if let Some(id) = collection.creator_id {
            body["list_create_userid"] = id.into();
        }
        std::collections::BTreeMap::new()
    } else {
        cloudlist::write_params(identity)?
    };
    let bytes = client
        .request(
            identity,
            "/cloudlist.service/v5/add_list",
            None,
            reqwest::Method::POST,
            params,
            request::encode(&body)?,
        )
        .await?;
    request::encode(&convert(&bytes)?)
}
