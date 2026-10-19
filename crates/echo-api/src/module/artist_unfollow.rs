//! Artist relationship mutation; ACK status/error_code sampled before mapping.
use crate::{
    error::ApiError,
    util::{
        crypto, relation,
        request::{self, ApiClient, Identity},
    },
};
use serde::Deserialize;
use std::collections::BTreeMap;
#[derive(Deserialize)]
struct Wire {
    #[serde(default, deserialize_with = "crate::util::response::strict_optional")]
    status: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::strict_optional")]
    error_code: Option<i64>,
    #[serde(default, deserialize_with = "crate::util::response::strict_optional")]
    code: Option<i64>,
}
pub fn convert(bytes: &[u8]) -> Result<serde_json::Value, ApiError> {
    let wire: Wire = request::decode(bytes)?;
    // Match the source action's success alternatives without inventing absent codes.
    if wire.status != Some(1) && wire.code != Some(200) && wire.error_code != Some(0) {
        return Err(ApiError::business(
            wire.error_code.or(wire.code).unwrap_or(0),
        ));
    }
    Ok(serde_json::json!({"following":false}))
}
pub async fn execute(
    client: &ApiClient,
    identity: &Identity,
    id: u64,
) -> Result<Vec<u8>, ApiError> {
    relation::require_account(identity)?;
    crate::util::cloudlist::id(id)?;
    let time = request::unix_seconds()?;
    let singer = serde_json::json!(id);
    let (portrait, params) = crypto::artist_follow_envelope(
        &serde_json::json!({"singerid":singer,"token":identity.token}),
        time,
    )?;
    let body = serde_json::json!({"plat":0,"userid":serde_json::json!(identity.userid),"singerid":singer,"source":7,"p":portrait,"params":params});
    let bytes = client
        .request(
            identity,
            "/followservice/v3/unfollow_singer",
            None,
            reqwest::Method::POST,
            BTreeMap::new(),
            request::encode(&body)?,
        )
        .await?;
    request::encode(&convert(&bytes)?)
}
