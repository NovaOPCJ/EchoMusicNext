//! Shared transport only. Endpoint DTOs and conversions stay in their modules.
use crate::{
    error::ApiError,
    util::{
        crypto::PlaylistEnvelope,
        request::{self, ApiClient, Endpoint, Identity, Origin},
    },
};
use md5::{Digest, Md5};
use serde::de::DeserializeOwned;
use std::collections::BTreeMap;

pub fn account(identity: &Identity) -> Result<(), ApiError> {
    if identity.userid == 0 || identity.token.is_empty() {
        Err(ApiError::new("authentication", "请先登录"))
    } else {
        Ok(())
    }
}
pub fn id(value: u64) -> Result<(), ApiError> {
    if value == 0 || value > i64::MAX as u64 {
        Err(ApiError::new("input", "云歌单或文件 ID 无效"))
    } else {
        Ok(())
    }
}
pub fn text(value: &str, limit: usize, allow_empty: bool) -> Result<(), ApiError> {
    if (!allow_empty && value.trim().is_empty()) || value.len() > limit || value.contains('\0') {
        Err(ApiError::new("input", "歌单文本参数无效"))
    } else {
        Ok(())
    }
}
pub fn decode<T: DeserializeOwned>(bytes: &[u8]) -> Result<T, ApiError> {
    #[derive(serde::Deserialize)]
    struct Status {
        #[serde(deserialize_with = "crate::util::response::scalar")]
        status: u32,
        #[serde(deserialize_with = "crate::util::response::scalar")]
        error_code: i64,
        #[serde(default)]
        data: serde_json::Value,
    }
    let status: Status = request::decode(bytes)?;
    if status.status != 1 && status.error_code == 20017 {
        return Err(ApiError::authentication(20017));
    }
    request::success(status.status, status.error_code)?;
    crate::util::response::from_value(status.data).map_err(|_| ApiError::decode())
}
pub fn keyed_time(time: u64) -> String {
    format!(
        "{:x}",
        Md5::digest(format!("3116LnT6xpN3khm36zse0QzvmgTZ3waWdRSA11440{time}"))
    )
}
pub fn write_params(identity: &Identity) -> Result<BTreeMap<String, String>, ApiError> {
    Ok(BTreeMap::from([
        ("last_time".into(), request::unix_seconds()?.to_string()),
        ("last_area".into(), "gztx".into()),
        ("userid".into(), identity.userid.to_string()),
        ("token".into(), identity.token.clone()),
    ]))
}
pub async fn encrypted(
    client: &ApiClient,
    identity: &Identity,
    path: &str,
    body: &serde_json::Value,
) -> Result<Vec<u8>, ApiError> {
    account(identity)?;
    let envelope = PlaylistEnvelope::new(body, identity.userid, &identity.token)?;
    let time = request::unix_seconds()?;
    let response = client
        .request_cloud_with_metadata(
            identity,
            Endpoint {
                origin: Origin::Gateway,
                path,
                router: Some("cloudlist.service.kugou.com"),
                method: reqwest::Method::POST,
                params: BTreeMap::from([
                    ("clienttime".into(), time.to_string()),
                    ("key".into(), keyed_time(time)),
                    ("p".into(), envelope.portrait.to_uppercase()),
                ]),
                body: envelope.body.clone(),
                headers: &[
                    ("Content-Type", "application/json;charset=utf-8"),
                    ("kg-rc", "1"),
                    ("kg-thash", "5d816a0"),
                    ("kg-rec", "1"),
                    ("kg-rf", "B9EDA08A64250DEFFBCADDEE00F8F25F"),
                ],
            },
        )
        .await?;
    // Success and invalid-token responses both use encrypted bodies.
    envelope.decrypt(&response.bytes)
}
