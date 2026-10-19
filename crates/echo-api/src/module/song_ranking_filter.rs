use crate::{
    error::ApiError,
    util::request::{self, ApiClient, Identity},
};
use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Query {
    pub album_audio_id: u64,
    #[serde(default = "one")]
    pub page: u32,
    #[serde(default = "thirty")]
    pub page_size: u32,
}
fn one() -> u32 {
    1
}
fn thirty() -> u32 {
    30
}
#[derive(Deserialize)]
struct Wire {
    #[serde(deserialize_with = "crate::util::response::scalar")]
    status: u32,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    errcode: Option<i64>,
    #[serde(default, deserialize_with = "crate::util::response::object")]
    data: Ranking,
}
#[derive(Default, Deserialize, Serialize)]
pub struct Ranking {
    #[serde(default, deserialize_with = "crate::util::response::list")]
    pub list: Vec<Achievement>,
    #[serde(flatten)]
    pub extension: BTreeMap<String, serde_json::Value>,
}
#[derive(Default, Deserialize, Serialize)]
pub struct Achievement {
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub area: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub count: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub platform: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub top_ranking: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub logo_pic: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub name: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub platform_name: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub share_pic: String,
    #[serde(flatten)]
    pub extension: BTreeMap<String, serde_json::Value>,
}
pub fn convert(bytes: &[u8]) -> Result<Ranking, ApiError> {
    let w: Wire = request::decode(bytes)?;
    request::success(w.status, w.errcode.unwrap_or_default())?;
    Ok(w.data)
}
pub async fn execute(
    client: &ApiClient,
    identity: &Identity,
    q: Query,
) -> Result<Vec<u8>, ApiError> {
    let mut p = BTreeMap::from([("album_audio_id".into(), q.album_audio_id.to_string())]);
    p.insert("page".into(), q.page.to_string());
    p.insert("pagesize".into(), q.page_size.to_string());
    let bytes = client
        .request(
            identity,
            "/grow/v1/song_ranking/unlock/v2/ranking_filter",
            None,
            reqwest::Method::GET,
            p,
            Vec::new(),
        )
        .await?;
    request::encode(&convert(&bytes)?)
}
