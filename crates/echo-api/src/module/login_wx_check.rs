//! /login/wx/check: completion-delayed long polling; response timeout is 45s.
use crate::{
    error::ApiError,
    util::{
        provider,
        request::{self, ApiClient},
    },
};
use serde::{Deserialize, Serialize};
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Query {
    pub key: String,
}
#[derive(Deserialize)]
struct Wire {
    #[serde(deserialize_with = "crate::util::response::scalar")]
    wx_errcode: i64,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    wx_code: String,
}
#[derive(Serialize)]
pub struct Status {
    pub state: &'static str,
    pub code: Option<String>,
}
pub fn convert(bytes: &[u8]) -> Result<Status, ApiError> {
    let wire: Wire = request::decode(bytes)?;
    let state = match wire.wx_errcode {
        408 => "waiting",
        404 => "scanned",
        402 | 403 => "expired",
        405 if !wire.wx_code.is_empty() && wire.wx_code.len() <= 4096 => "provider_code",
        _ => {
            return Err(ApiError::new(
                "provider",
                "微信扫码状态异常，请重新获取二维码",
            ));
        }
    };
    Ok(Status {
        state,
        code: if state != "provider_code" || wire.wx_code.is_empty() {
            None
        } else {
            Some(wire.wx_code)
        },
    })
}
pub async fn execute(client: &ApiClient, query: Query) -> Result<Vec<u8>, ApiError> {
    if query.key.is_empty() || query.key.len() > 256 {
        return Err(ApiError::new("input", "微信二维码会话无效"));
    }
    let response = client
        .provider_request(
            reqwest::Method::GET,
            provider::url(
                "https://long.open.weixin.qq.com/connect/l/qrconnect",
                &[("f", "json"), ("uuid", &query.key)],
            )?,
            &[],
        )
        .await?;
    request::encode(&convert(&response.bytes)?)
}
