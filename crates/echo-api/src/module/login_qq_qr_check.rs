//! /login/qq/qr/check: raw callback is parsed as data, never executed as JS.
use crate::{
    error::ApiError,
    module::login_qq_qr_create::Session,
    util::{
        login_config, provider,
        request::{self, ApiClient},
    },
};
use serde::Deserialize;
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Query {
    pub session: Session,
}
fn callback(bytes: &[u8]) -> Result<Vec<String>, ApiError> {
    let text = std::str::from_utf8(bytes)
        .map_err(|_| ApiError::decode())?
        .trim();
    let args = text
        .strip_prefix("ptuiCB(")
        .and_then(|s| s.trim_end_matches(';').strip_suffix(')'))
        .ok_or_else(ApiError::decode)?;
    // Names can contain escaped quotes or UTF-16 escapes. Parse the callback's
    // string grammar only; never eval a provider response as JavaScript.
    let mut fields = Vec::new();
    let mut chars = args.chars().peekable();
    while chars.peek().is_some() {
        while chars.peek().is_some_and(|c| c.is_whitespace()) {
            chars.next();
        }
        if chars.next() != Some('\'') {
            return Err(ApiError::decode());
        }
        let mut units = Vec::new();
        loop {
            let character = chars.next().ok_or_else(ApiError::decode)?;
            if character == '\'' {
                break;
            }
            if character == '\\' {
                let escaped = chars.next().ok_or_else(ApiError::decode)?;
                match escaped {
                    'x' | 'u' => {
                        let count = if escaped == 'x' { 2 } else { 4 };
                        let mut unit = 0u16;
                        for _ in 0..count {
                            let digit = chars
                                .next()
                                .and_then(|c| c.to_digit(16))
                                .ok_or_else(ApiError::decode)?;
                            unit = (unit << 4) | digit as u16;
                        }
                        units.push(unit);
                    }
                    '\\' | '\'' | '"' => units.push(escaped as u16),
                    'n' => units.push(10),
                    'r' => units.push(13),
                    't' => units.push(9),
                    'b' => units.push(8),
                    'f' => units.push(12),
                    _ => return Err(ApiError::decode()),
                }
            } else {
                if character == '\n' || character == '\r' {
                    return Err(ApiError::decode());
                }
                let mut encoded = [0; 2];
                units.extend_from_slice(character.encode_utf16(&mut encoded));
            }
        }
        fields.push(String::from_utf16(&units).map_err(|_| ApiError::decode())?);
        while chars.peek().is_some_and(|c| c.is_whitespace()) {
            chars.next();
        }
        if chars.peek().is_none() {
            break;
        }
        if chars.next() != Some(',') {
            return Err(ApiError::decode());
        }
        if chars.peek().is_none() {
            return Err(ApiError::decode());
        }
    }
    if fields.is_empty() {
        return Err(ApiError::decode());
    }
    Ok(fields)
}
pub fn convert(bytes: &[u8]) -> Result<&'static str, ApiError> {
    let fields = callback(bytes)?;
    match fields[0].as_str() {
        "66" => Ok("waiting"),
        "65" => Ok("expired"),
        "67" => Ok("scanned"),
        "0" => Ok("provider_authorized"),
        _ => Err(ApiError::new("provider", "QQ扫码失败，请重新获取二维码")),
    }
}
/// Extract only data. Provider custom callback URLs are not navigated or executed.
pub fn credentials_url(
    url: &str,
) -> Result<Option<crate::util::login_protocol::Authorization>, ApiError> {
    let parsed = reqwest::Url::parse(url).map_err(|_| ApiError::decode())?;
    let pairs = parsed
        .query_pairs()
        .chain(
            reqwest::Url::parse(&format!(
                "https://callback.invalid/?{}",
                parsed.fragment().unwrap_or_default()
            ))
            .map_err(|_| ApiError::decode())?
            .query_pairs(),
        )
        .map(|(k, v)| (k.into_owned(), v.into_owned()))
        .collect::<std::collections::BTreeMap<_, _>>();
    match (pairs.get("openid"), pairs.get("access_token")) {
        (Some(id), Some(token)) if !id.is_empty() && !token.is_empty() => {
            Ok(Some(crate::util::login_protocol::Authorization {
                openid: id.clone(),
                access_token: token.clone(),
            }))
        }
        _ => Ok(None),
    }
}
fn html_value(text: &str, key: &str) -> Option<String> {
    let (_, tail) = text.split_once(key)?;
    let tail = tail.trim_start_matches(['\"', '\'', ' ', ':', '=']);
    let value = tail
        .split(['&', '\"', '\'', ' ', '\n', '\r', '\t'])
        .next()?;
    (!value.is_empty() && value.len() <= 16384).then(|| value.to_owned())
}
async fn authorize(
    client: &ApiClient,
    session: &Session,
    initial: &str,
    poll_headers: &reqwest::header::HeaderMap,
) -> Result<crate::util::login_protocol::Authorization, ApiError> {
    use crate::module::login_qq_qr_create::{cookie_string, cookies};
    let mut jar = std::collections::BTreeMap::new();
    for pair in session.cookie.split(';') {
        if let Some((k, v)) = pair.trim().split_once('=') {
            jar.insert(k.to_owned(), v.to_owned());
        }
    }
    cookies(poll_headers, &mut jar)?;
    let initial = if initial.starts_with("http") || initial.starts_with("auth:") {
        initial.to_owned()
    } else {
        format!("https://{initial}")
    };
    let mut url = reqwest::Url::parse(&initial).map_err(|_| ApiError::decode())?;
    for _ in 0..10 {
        if let Some(credentials) = credentials_url(url.as_str())? {
            return Ok(credentials);
        }
        if !matches!(
            url.host_str(),
            Some(
                "xui.ptlogin2.qq.com"
                    | "ssl.ptlogin2.qq.com"
                    | "ptlogin2.qq.com"
                    | "ui.ptlogin2.qq.com"
                    | "graph.qq.com"
                    | "connect.qq.com"
                    | "openmobile.qq.com"
            )
        ) {
            return Err(ApiError::new("provider", "QQ授权地址无效"));
        }
        let cookie = cookie_string(&jar);
        let response = client
            .provider_request(
                reqwest::Method::GET,
                url.clone(),
                &[
                    ("User-Agent", provider::QQ_USER_AGENT),
                    ("Referer", &session.xlogin_url),
                    ("Cookie", &cookie),
                    (
                        "Accept",
                        "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
                    ),
                    ("Accept-Language", "zh-CN,zh;q=0.9"),
                ],
            )
            .await?;
        cookies(&response.headers, &mut jar)?;
        if let Some(location) = response.headers.get("location") {
            url = url
                .join(location.to_str().map_err(|_| ApiError::decode())?)
                .map_err(|_| ApiError::decode())?;
        } else {
            let text = std::str::from_utf8(&response.bytes).map_err(|_| ApiError::decode())?;
            if let (Some(openid), Some(access_token)) =
                (html_value(text, "openid"), html_value(text, "access_token"))
            {
                return Ok(crate::util::login_protocol::Authorization {
                    openid,
                    access_token,
                });
            }
            break;
        }
    }
    Err(ApiError::new("provider", "QQ授权凭证获取失败，请重新扫码"))
}

pub async fn execute(client: &ApiClient, query: Query) -> Result<Vec<u8>, ApiError> {
    let session = query.session;
    if session.qrsig.is_empty()
        || session.pt_login_sig.is_empty()
        || session.cookie.is_empty()
        || session.openlogin_data.is_empty()
        || session.cookie.len() > 32768
    {
        return Err(ApiError::new("input", "QQ二维码会话无效"));
    }
    let token = session.ptqrtoken.to_string();
    let nonce = format!("0.{}", rsa::rand_core::OsRng.next_u64());
    use rsa::rand_core::RngCore;
    let url = provider::url(
        "https://xui.ptlogin2.qq.com/ssl/ptqrlogin",
        &[
            ("u1", "http://connect.qq.com"),
            ("from_ui", "1"),
            ("type", "1"),
            ("ptlang", "2052"),
            ("ptqrtoken", &token),
            ("daid", "381"),
            ("aid", "716027609"),
            ("pt_3rd_aid", login_config::QQ_LITE_APPID),
            ("pt_openlogin_data", &session.openlogin_data),
            ("device", "2"),
            ("ptopt", "1"),
            ("pt_uistyle", "35"),
            ("jsver", "v1.36.0"),
            ("login_sig", &session.pt_login_sig),
            ("r", &nonce),
        ],
    )?;
    let response = client
        .provider_request(
            reqwest::Method::GET,
            url,
            &[
                ("User-Agent", provider::QQ_USER_AGENT),
                ("Referer", &session.xlogin_url),
                ("Cookie", &session.cookie),
            ],
        )
        .await?;
    let state = convert(&response.bytes)?;
    if state == "provider_authorized" {
        let fields = callback(&response.bytes)?;
        let redirect = fields
            .get(2)
            .filter(|value| !value.is_empty())
            .ok_or_else(|| ApiError::new("provider", "QQ授权未返回跳转地址"))?;
        let authorization = authorize(client, &session, redirect, &response.headers).await?;
        return request::encode(&serde_json::json!({"state":state,"authorization":authorization}));
    }
    request::encode(&serde_json::json!({"state":state}))
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn callback_parses_names_as_data_and_rejects_extra_code() {
        // Generated grammar checks, not upstream response fixtures.
        let fields=callback(br"ptuiCB('0','0','auth://tauth.qq.com/#openid=a&access_token=b','0','ok','O\'Connor \uD83D\uDE00');").unwrap();
        assert_eq!(fields[5], "O'Connor 😀");
        assert!(callback(b"ptuiCB('66','0','','0','unit','',)").is_err());
        assert!(callback(b"ptuiCB('66';'0','','0','unit','')").is_err());
        assert!(callback(b"ptuiCB('66','0','','0','unit','');other()").is_err());
    }
}
