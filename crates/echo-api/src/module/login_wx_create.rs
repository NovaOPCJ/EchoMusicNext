//! WeChat QR creation: token, SDK ticket, then signed QR connection.
use crate::{
    error::ApiError,
    util::{
        login_config, provider,
        request::{self, ApiClient},
    },
};
use md5::{Digest, Md5};
use serde::{Deserialize, Serialize};
use std::time::{SystemTime, UNIX_EPOCH};
#[derive(Deserialize)]
struct Token {
    #[serde(deserialize_with = "crate::util::response::scalar")]
    access_token: String,
}
#[derive(Deserialize)]
struct Ticket {
    #[serde(deserialize_with = "crate::util::response::scalar")]
    errcode: i64,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    ticket: Option<String>,
}
#[derive(Deserialize)]
struct Wire {
    #[serde(deserialize_with = "crate::util::response::scalar")]
    errcode: i64,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    uuid: Option<String>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    qrcode: Option<Image>,
}
#[derive(Deserialize)]
struct Image {
    #[serde(deserialize_with = "crate::util::response::scalar")]
    qrcodebase64: String,
}
#[derive(Serialize)]
pub struct Session {
    pub key: String,
    pub image_data_url: String,
}
pub async fn execute(client: &ApiClient) -> Result<Vec<u8>, ApiError> {
    let appid = login_config::WX_LITE_APPID;
    let token = client
        .provider_request(
            reqwest::Method::GET,
            provider::url(
                "https://api.weixin.qq.com/cgi-bin/token",
                &[
                    ("appid", appid),
                    ("secret", login_config::WX_LITE_SECRET),
                    ("grant_type", "client_credential"),
                ],
            )?,
            &[],
        )
        .await?;
    let token: Token = request::decode(&token.bytes)?;
    let ticket = client
        .provider_request(
            reqwest::Method::GET,
            provider::url(
                "https://api.weixin.qq.com/cgi-bin/ticket/getticket",
                &[("access_token", &token.access_token), ("type", "2")],
            )?,
            &[],
        )
        .await?;
    let ticket: Ticket = request::decode(&ticket.bytes)?;
    if ticket.errcode != 0 {
        return Err(ApiError::business(ticket.errcode));
    }
    let value = ticket
        .ticket
        .filter(|s| !s.is_empty())
        .ok_or_else(ApiError::decode)?;
    let timestamp = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map_err(|_| ApiError::new("clock", "系统时钟无效"))?
        .as_millis()
        .to_string();
    // Random nonce and timestamp are per-creation; no persisted access-token cache.
    let nonce = format!(
        "{:x}",
        Md5::digest(rsa::rand_core::OsRng.next_u64().to_be_bytes())
    );
    use rsa::rand_core::RngCore;
    use sha1::Sha1;
    let signature = format!(
        "{:x}",
        Sha1::digest(format!(
            "appid={appid}&noncestr={nonce}&sdk_ticket={value}&timestamp={timestamp}"
        ))
    );
    let response = client
        .provider_request(
            reqwest::Method::GET,
            provider::url(
                "https://open.weixin.qq.com/connect/sdk/qrconnect",
                &[
                    ("appid", appid),
                    ("noncestr", &nonce),
                    ("timestamp", &timestamp),
                    ("scope", "snsapi_userinfo"),
                    ("signature", &signature),
                ],
            )?,
            &[],
        )
        .await?;
    request::encode(&convert(&response.bytes)?)
}
pub fn convert(bytes: &[u8]) -> Result<Session, ApiError> {
    let wire: Wire = request::decode(bytes)?;
    if wire.errcode != 0 {
        return Err(ApiError::business(wire.errcode));
    }
    let key = wire
        .uuid
        .filter(|s| !s.is_empty())
        .ok_or_else(ApiError::decode)?;
    let image = wire.qrcode.ok_or_else(ApiError::decode)?;
    Ok(Session {
        key,
        image_data_url: format!("data:image/jpeg;base64,{}", image.qrcodebase64),
    })
}
