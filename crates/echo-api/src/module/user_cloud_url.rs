use crate::{
    error::ApiError,
    util::{
        request::{self, ApiClient, Identity},
        signature,
    },
};
use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Query {
    pub hash: String,
    #[serde(default)]
    pub album_audio_id: u64,
    #[serde(default)]
    pub audio_id: u64,
    #[serde(default)]
    pub name: String,
}
#[derive(Deserialize)]
struct Wire {
    #[serde(deserialize_with = "crate::util::response::scalar")]
    status: u32,
    #[serde(default, deserialize_with = "crate::util::response::scalar")]
    error_code: i64,
    #[serde(default)]
    data: serde_json::Value,
}
#[derive(Deserialize, Default)]
struct Data {
    #[serde(default, deserialize_with = "crate::util::response::text")]
    url: String,
    #[serde(default)]
    backup_url: serde_json::Value,
    #[serde(
        rename = "fileSize",
        default,
        deserialize_with = "crate::util::response::optional"
    )]
    size: Option<u64>,
    #[serde(
        rename = "extName",
        default,
        deserialize_with = "crate::util::response::text"
    )]
    extension: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    hash: String,
    #[serde(flatten)]
    extra: BTreeMap<String, serde_json::Value>,
}
#[derive(Serialize)]
pub struct CloudSource {
    pub urls: Vec<String>,
    pub backup_urls: Vec<String>,
    pub file_size_bytes: Option<u64>,
    pub extension: String,
    pub hash: String,
    pub provider_extension: BTreeMap<String, serde_json::Value>,
}
pub fn convert(bytes: &[u8]) -> Result<CloudSource, ApiError> {
    let wire: Wire = request::decode(bytes)?;
    request::success(wire.status, wire.error_code)?;
    let data: Data =
        crate::util::response::from_value(wire.data).map_err(|_| ApiError::decode())?;
    let backup_urls = match data.backup_url {
        serde_json::Value::String(s) => vec![s],
        serde_json::Value::Array(a) => a
            .into_iter()
            .filter_map(|v| v.as_str().map(str::to_owned))
            .collect(),
        _ => vec![],
    };
    Ok(CloudSource {
        urls: if data.url.is_empty() {
            vec![]
        } else {
            vec![data.url]
        },
        backup_urls,
        file_size_bytes: data.size,
        extension: data.extension,
        hash: data.hash,
        provider_extension: data.extra,
    })
}
pub async fn execute(
    client: &ApiClient,
    identity: &Identity,
    q: Query,
) -> Result<Vec<u8>, ApiError> {
    crate::util::cloudlist::account(identity)?;
    if !crate::module::song_climax::valid_hash(&q.hash) {
        return Err(ApiError::new("input", "云盘歌曲hash无效"));
    }
    let hash = q.hash.to_lowercase();
    let params = BTreeMap::from([
        ("hash".into(), hash.clone()),
        ("ssa_flag".into(), "is_fromtrack".into()),
        ("version".into(), "20102".into()),
        ("ssl".into(), "0".into()),
        ("album_audio_id".into(), q.album_audio_id.to_string()),
        ("pid".into(), "20026".into()),
        ("audio_id".into(), q.audio_id.to_string()),
        ("kv_id".into(), "2".into()),
        ("key".into(), signature::cloud_key(&hash)),
        ("bucket".into(), "musicclound".into()),
        ("name".into(), q.name),
        ("with_res_tag".into(), "0".into()),
    ]);
    let bytes = client
        .request(
            identity,
            "/bsstrackercdngz/v2/query_musicclound_url",
            None,
            reqwest::Method::GET,
            params,
            vec![],
        )
        .await?;
    request::encode(&convert(&bytes)?)
}
#[cfg(all(test, feature = "local-api-fixtures"))]
mod tests {
    use super::*;
    #[test]
    fn captured_cloud_sources_keep_size_and_backup() {
        for text in [
            include_str!(
                "../../../../tests/fixtures/api/user_cloud_url/source-0-fallback-20261019.json"
            ),
            include_str!(
                "../../../../tests/fixtures/api/user_cloud_url/source-1-fallback-20261019.json"
            ),
        ] {
            let fixture: serde_json::Value = serde_json::from_str(text).unwrap();
            let raw = &fixture["responses"][0]["payload"];
            let result = convert(&serde_json::to_vec(raw).unwrap()).unwrap();
            assert_eq!(result.urls[0], raw["data"]["url"].as_str().unwrap());
            assert_eq!(
                result.backup_urls[0],
                raw["data"]["backup_url"].as_str().unwrap()
            );
            assert_eq!(
                result.file_size_bytes,
                raw["data"]["fileSize"].as_str().unwrap().parse().ok()
            );
        }
    }
}
