//! QQ QR creation parses authorization HTML, xlogin cookies and PNG bytes.
//! Session material is returned to the C++ login service, never to QML.
use crate::{
    error::ApiError,
    util::{
        login_config, provider,
        request::{self, ApiClient},
    },
};
use md5::{Digest, Md5};
use reqwest::{Method, Url};
use serde::{Deserialize, Serialize};
use std::{
    collections::BTreeMap,
    time::{SystemTime, UNIX_EPOCH},
};
#[derive(Deserialize, Serialize)]
#[serde(deny_unknown_fields)]
pub struct Session {
    pub xlogin_url: String,
    pub pt_login_sig: String,
    pub qrsig: String,
    pub cookie: String,
    pub ptqrtoken: u32,
    pub openlogin_data: String,
}
#[derive(Serialize)]
struct Created {
    session: Session,
    image_data_url: String,
}
pub(crate) fn cookies(
    headers: &reqwest::header::HeaderMap,
    jar: &mut BTreeMap<String, String>,
) -> Result<(), ApiError> {
    for cookie in headers.get_all("set-cookie") {
        let pair = cookie
            .to_str()
            .map_err(|_| ApiError::decode())?
            .split(';')
            .next()
            .ok_or_else(ApiError::decode)?;
        let (name, value) = pair.split_once('=').ok_or_else(ApiError::decode)?;
        if !name.is_empty() {
            jar.insert(name.into(), value.into());
        }
    }
    Ok(())
}
pub(crate) fn cookie_string(jar: &BTreeMap<String, String>) -> String {
    jar.iter()
        .map(|(k, v)| format!("{k}={v}"))
        .collect::<Vec<_>>()
        .join("; ")
}
fn xlogin(bytes: &[u8]) -> Result<Url, ApiError> {
    let text = std::str::from_utf8(bytes).map_err(|_| ApiError::decode())?;
    let src = text
        .split_once("src = \"")
        .and_then(|(_, tail)| tail.split_once('"').map(|p| p.0))
        .ok_or_else(ApiError::decode)?;
    let mut decoded = String::new();
    let mut remaining = src;
    while let Some((head, tail)) = remaining.split_once("\\x") {
        decoded.push_str(head);
        if tail.len() < 2 {
            return Err(ApiError::decode());
        }
        let byte = u8::from_str_radix(&tail[..2], 16).map_err(|_| ApiError::decode())?;
        if !byte.is_ascii() {
            return Err(ApiError::decode());
        }
        decoded.push(char::from(byte));
        remaining = &tail[2..];
    }
    decoded.push_str(remaining);
    let url = Url::parse(&decoded).map_err(|_| ApiError::decode())?;
    if url.scheme() != "https"
        || url.host_str() != Some("xui.ptlogin2.qq.com")
        || url.path() != "/cgi-bin/xlogin"
    {
        return Err(ApiError::decode());
    }
    Ok(url)
}
pub(crate) fn hash33(value: &str) -> u32 {
    value.encode_utf16().fold(0u32, |sum, c| {
        sum.wrapping_mul(33).wrapping_add(u32::from(c))
    }) & 0x7fffffff
}
pub async fn execute(client: &ApiClient) -> Result<Vec<u8>, ApiError> {
    let time = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map_err(|_| ApiError::new("clock", "系统时钟无效"))?
        .as_secs()
        .to_string();
    let sign = format!(
        "{:x}",
        Md5::digest(format!("fe4a24d80fcf253a00676a808f62c2c6_{time}"))
    );
    let appid = login_config::QQ_LITE_APPID;
    let url = provider::url(
        "https://openmobile.qq.com/oauth2.0/m_authorize",
        &[
            ("cancel_display", "1"),
            ("sdkp", "a"),
            ("display", "mobile"),
            ("format", "json"),
            ("sign", &sign),
            ("sdkv", "3.5.11.lite"),
            ("response_type", "token"),
            ("status_os", "11"),
            ("client_id", appid),
            ("switch", "1"),
            ("status_version", "30"),
            ("show_download_ui", "true"),
            ("pf", "openmobile_android"),
            ("scope", "all"),
            ("compat_v", "1"),
            ("status_machine", "MEIZU+18+Pro"),
            ("style", "qr"),
            ("time", &time),
            ("redirect_uri", "auth://tauth.qq.com/"),
        ],
    )?;
    let headers = [
        ("User-Agent", provider::QQ_USER_AGENT),
        ("Referer", "https://xui.ptlogin2.qq.com/"),
    ];
    let response = client.provider_request(Method::GET, url, &headers).await?;
    let url = xlogin(&response.bytes)?;
    let response = client
        .provider_request(Method::GET, url.clone(), &headers)
        .await?;
    let mut jar = BTreeMap::new();
    cookies(&response.headers, &mut jar)?;
    let sig = jar
        .get("pt_login_sig")
        .filter(|s| !s.is_empty())
        .ok_or_else(ApiError::decode)?
        .clone();
    let cookie = cookie_string(&jar);
    let nonce = format!("0.{}", rsa::rand_core::OsRng.next_u64());
    use rsa::rand_core::RngCore;
    let qr = provider::url(
        "https://xui.ptlogin2.qq.com/ssl/ptqrshow",
        &[
            ("s", "8"),
            ("e", "0"),
            ("appid", "716027609"),
            ("type", "0"),
            ("t", &nonce),
            ("daid", "381"),
            ("pt_3rd_aid", appid),
        ],
    )?;
    let response = client
        .provider_request(
            Method::GET,
            qr,
            &[
                ("User-Agent", provider::QQ_USER_AGENT),
                ("Referer", url.as_str()),
                ("Cookie", &cookie),
            ],
        )
        .await?;
    cookies(&response.headers, &mut jar)?;
    let qrsig = jar
        .get("qrsig")
        .filter(|s| !s.is_empty())
        .ok_or_else(ApiError::decode)?
        .clone();
    if !response.bytes.starts_with(b"\x89PNG\r\n\x1a\n") {
        return Err(ApiError::decode());
    }
    use base64::Engine;
    request::encode(&Created {
        session: Session {
            xlogin_url: url.to_string(),
            pt_login_sig: sig,
            ptqrtoken: hash33(&qrsig),
            qrsig,
            cookie: cookie_string(&jar),
            openlogin_data: url.query().unwrap_or_default().into(),
        },
        image_data_url: format!(
            "data:image/png;base64,{}",
            base64::engine::general_purpose::STANDARD.encode(response.bytes)
        ),
    })
}
