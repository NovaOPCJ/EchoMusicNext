//! Song climax times are decimal-string milliseconds.
//! Unknown hashes can return data: [{}].
use crate::{
    error::ApiError,
    util::request::{self, ApiClient, Endpoint, Identity, Origin},
};
use reqwest::Method;
use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Query {
    pub hash: String,
}
#[derive(Deserialize)]
struct Wire {
    #[serde(deserialize_with = "crate::util::response::scalar")]
    status: u32,
    #[serde(deserialize_with = "crate::util::response::scalar")]
    error_code: i64,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    data: Option<Vec<Item>>,
}
#[derive(Deserialize)]
struct Item {
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    start_time: Option<Milliseconds>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    end_time: Option<Milliseconds>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    hash: Option<String>,
}
#[derive(Deserialize)]
#[serde(try_from = "serde_json::Value")]
struct Milliseconds(u64);
impl TryFrom<serde_json::Value> for Milliseconds {
    type Error = &'static str;
    fn try_from(value: serde_json::Value) -> Result<Self, Self::Error> {
        let number = match value {
            serde_json::Value::String(text) => text.parse::<u64>().ok(),
            serde_json::Value::Number(number) => crate::util::numeric::exact_unsigned(&number),
            _ => None,
        };
        number.map(Self).ok_or("invalid unsigned milliseconds")
    }
}
#[derive(Debug, PartialEq, Serialize)]
pub struct ClimaxSegment {
    pub start_ms: Option<u64>,
    pub end_ms: Option<u64>,
    pub hash: Option<String>,
}
pub fn valid_hash(hash: &str) -> bool {
    hash.len() == 32 && hash.bytes().all(|v| v.is_ascii_hexdigit())
}
pub fn convert(bytes: &[u8]) -> Result<Vec<ClimaxSegment>, ApiError> {
    let wire: Wire = request::decode(bytes)?;
    if wire.status != 1 {
        return Err(ApiError::business(wire.error_code));
    }
    let items = wire.data.unwrap_or_default();
    let mut segments = Vec::new();
    for item in items {
        let start_ms = item.start_time.map(|value| value.0);
        let end_ms = item.end_time.map(|value| value.0);
        if start_ms.is_some() || end_ms.is_some() || item.hash.is_some() {
            segments.push(ClimaxSegment {
                start_ms,
                end_ms,
                hash: item.hash,
            });
        }
    }
    Ok(segments)
}
pub async fn execute(
    client: &ApiClient,
    query: Query,
    identity: &Identity,
) -> Result<Vec<u8>, ApiError> {
    if !valid_hash(&query.hash) {
        return Err(ApiError::new("input", "歌曲hash无效"));
    }
    let data = serde_json::to_string(&serde_json::json!([{"hash": query.hash}]))
        .map_err(|_| ApiError::encoding())?;
    let bytes = client
        .request_endpoint(
            identity,
            Endpoint {
                origin: Origin::Climax,
                path: "/v1/audio_climax/audio",
                router: None,
                method: Method::GET,
                params: BTreeMap::from([("data".into(), data)]),
                body: Vec::new(),
                headers: &[
                    ("kg-rc", "1"),
                    ("kg-thash", "5d816a0"),
                    ("kg-rec", "1"),
                    ("kg-rf", "B9EDA08A64250DEFFBCADDEE00F8F25F"),
                ],
            },
        )
        .await?;
    request::encode(&convert(&bytes)?)
}
