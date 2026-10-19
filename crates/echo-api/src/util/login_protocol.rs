//! Fixed lite login proof construction; account refresh t1 is unrelated.
use crate::{
    error::ApiError,
    util::{crypto, request::Identity},
};
pub fn proofs(identity: &Identity, time: u64) -> Result<(String, String), ApiError> {
    if identity.guid.is_empty() || identity.mac.is_empty() || identity.device_name.is_empty() {
        return Err(ApiError::new("input", "登录设备上下文不完整"));
    }
    Ok((
        crypto::aes256_hex(
            format!("|{time}").as_bytes(),
            b"5e4ef500e9597fe004bd09a46d8add98",
            b"04bd09a46d8add98",
        )?,
        crypto::aes256_hex(
            format!(
                "{}|0f607264fc6318a92b9e13c65db7cd3c|{}|{}|{time}",
                identity.guid, identity.mac, identity.device_name
            )
            .as_bytes(),
            b"fd14b35e3f81af3817a20ae7adae7020",
            b"17a20ae7adae7020",
        )?,
    ))
}

/// Returns raw bytes plus request seed; each endpoint owns its response DTO.
/// Separate OAuth exchange from this request so an SSA retry never reuses a one-shot code.
pub async fn openplat(
    client: &super::request::ApiClient,
    identity: &Identity,
    partner: u32,
    authorization: &Authorization,
) -> Result<(Vec<u8>, String), ApiError> {
    if authorization.openid.is_empty()
        || authorization.access_token.is_empty()
        || authorization.openid.len() > 4096
        || authorization.access_token.len() > 16384
    {
        return Err(ApiError::new("input", "第三方登录凭证无效"));
    }
    let mut device = identity.clone();
    device.userid = 0;
    device.token.clear();
    device.t1.clear();
    let time = super::request::unix_milliseconds()?;
    let envelope = crypto::login_parameter_envelope(
        &serde_json::json!({"access_token":authorization.access_token}),
        time,
    )?;
    let (t1, t2) = proofs(&device, time)?;
    let mut body = serde_json::json!({"dev":device.device_name,"force_login":1,"partnerid":partner,
        "clienttime_ms":time,"t1":t1,"t2":t2,"t3":"MCwwLDAsMCwwLDAsMCwwLDA=",
        "openid":authorization.openid,"params":envelope.params,"pk":envelope.pk});
    if partner == 1 {
        body["third_appid"] = serde_json::json!(super::login_config::QQ_LITE_APPID);
    }
    let bytes = client
        .request(
            &device,
            "/v6/login_by_openplat",
            Some("login.user.kugou.com"),
            reqwest::Method::POST,
            Default::default(),
            super::request::encode(&body)?,
        )
        .await?;
    Ok((bytes, envelope.seed))
}
/// In-memory only; never Debug or exposed as a QML property.
#[derive(serde::Deserialize, serde::Serialize)]
#[serde(deny_unknown_fields)]
pub struct Authorization {
    pub openid: String,
    pub access_token: String,
}
