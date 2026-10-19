//! Native password login request with transport-level SSA challenges.
//! Successful live authentication requires separate integration verification.
use crate::{
    error::ApiError,
    util::{
        crypto,
        request::{self, ApiClient, Identity},
    },
};
use serde::Deserialize;

/// Sensitive input deliberately has no Debug/Serialize implementation.
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Query {
    pub username: String,
    pub password: String,
    #[serde(default)]
    pub code: String,
}
#[derive(Deserialize)]
struct Wire {
    #[serde(deserialize_with = "crate::util::response::scalar")]
    status: u32,
    #[serde(deserialize_with = "crate::util::response::scalar")]
    error_code: i64,
    #[serde(default)]
    data: serde_json::Value,
    // Failure text follows the provider error chain, independently of successful credentials.
    #[serde(default)]
    error: serde_json::Value,
    #[serde(default)]
    message: serde_json::Value,
    #[serde(default)]
    msg: serde_json::Value,
}
#[derive(Deserialize)]
struct WireAccount {
    #[serde(default)]
    #[serde(deserialize_with = "crate::util::response::scalar")]
    userid: u64,
    #[serde(default)]
    #[serde(deserialize_with = "crate::util::response::optional")]
    t1: Option<String>,
    #[serde(default)]
    #[serde(deserialize_with = "crate::util::response::optional")]
    t_expire_time: Option<u64>,
    #[serde(default)]
    #[serde(deserialize_with = "crate::util::response::text")]
    nickname: String,
    #[serde(default)]
    #[serde(deserialize_with = "crate::util::response::text")]
    pic: String,
    #[serde(default)]
    #[serde(deserialize_with = "crate::util::response::scalar")]
    secu_params: String,
    #[serde(default)]
    #[serde(deserialize_with = "crate::util::response::scalar")]
    token: String,
}
#[derive(Deserialize, Default)]
struct WireCredentials {
    #[serde(deserialize_with = "crate::util::response::scalar")]
    token: String,
    #[serde(default)]
    #[serde(deserialize_with = "crate::util::response::optional")]
    userid: Option<u64>,
    #[serde(default)]
    #[serde(deserialize_with = "crate::util::response::optional")]
    t1: Option<String>,
    #[serde(default)]
    #[serde(deserialize_with = "crate::util::response::optional")]
    t_expire_time: Option<u64>,
    #[serde(default)]
    #[serde(deserialize_with = "crate::util::response::optional")]
    nickname: Option<String>,
    #[serde(default)]
    #[serde(deserialize_with = "crate::util::response::optional")]
    pic: Option<String>,
}
fn authorization(
    data: serde_json::Value,
    seed: &str,
) -> Result<crate::domain::LoginAuthorization, ApiError> {
    let account: WireAccount =
        crate::util::response::from_value(data).map_err(|_| ApiError::decode())?;
    let secret = if account.secu_params.is_empty() {
        // Keep a plaintext token unchanged when no secure envelope is returned.
        serde_json::Value::String(account.token)
    } else {
        crypto::decode_login_credentials(&account.secu_params, seed)?
    };
    let credentials: WireCredentials = if let Some(token) = secret.as_str() {
        WireCredentials {
            token: token.to_owned(),
            ..Default::default()
        }
    } else {
        crate::util::response::from_value(secret).map_err(|_| ApiError::decode())?
    };
    let user_id = credentials.userid.unwrap_or(account.userid);
    if user_id == 0 || user_id > i64::MAX as u64 || credentials.token.is_empty() {
        return Err(ApiError::new("authentication", "服务未返回有效登录凭证"));
    }
    Ok(crate::domain::LoginAuthorization {
        user_id,
        token: credentials.token,
        refresh_credential: credentials.t1.or(account.t1).filter(|v| !v.is_empty()),
        expires_at_seconds: credentials
            .t_expire_time
            .or(account.t_expire_time)
            .filter(|v| *v > 0),
        nickname: credentials.nickname.unwrap_or(account.nickname),
        avatar_url: credentials.pic.unwrap_or(account.pic),
    })
}
pub fn convert(bytes: &[u8], seed: &str) -> Result<Vec<u8>, ApiError> {
    let wire: Wire = request::decode(bytes)?;
    if let Err(mut error) = request::success(wire.status, wire.error_code) {
        if error.kind == "business" {
            // Read error text from data/error/message/msg. Display only bounded text,
            // never an object or encrypted credential envelope.
            if let Some(message) = [&wire.data, &wire.error, &wire.message, &wire.msg]
                .into_iter()
                .filter_map(|value| value.as_str())
                .map(str::trim)
                .find(|value| !value.is_empty() && value.len() <= 1024)
            {
                error.message = message.to_owned();
            }
        }
        return Err(error);
    }
    request::encode(&authorization(wire.data, seed)?)
}
pub async fn execute(
    client: &ApiClient,
    identity: &Identity,
    query: Query,
) -> Result<Vec<u8>, ApiError> {
    if query.username.trim().is_empty()
        || query.username.len() > 256
        || query.password.is_empty()
        || query.password.len() > 256
        || query.code.len() > 128
        || identity.device_name.is_empty()
    {
        return Err(ApiError::new("input", "账号、密码或设备参数无效"));
    }
    // Login must never inherit another account, including direct Rust/FFI callers.
    let mut anonymous = identity.clone();
    anonymous.userid = 0;
    anonymous.token.clear();
    anonymous.t1.clear();
    let timestamp_ms = u64::try_from(
        std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .map_err(|_| ApiError::new("clock", "系统时钟无效"))?
            .as_millis(),
    )
    .map_err(|_| ApiError::new("clock", "系统时钟无效"))?;
    let envelope = crypto::login_parameter_envelope(
        &serde_json::json!({
            "pwd":query.password, "code":query.code, "clienttime_ms":timestamp_ms
        }),
        timestamp_ms,
    )?;
    // These are fixed protocol proof constants, NOT the account refresh field t1.
    let body = serde_json::json!({
        "plat":1, "support_multi":1, "clienttime_ms":timestamp_ms,
        "t1":T1, "t2":T2, "t3":T3, "dev":anonymous.device_name,
        "username":query.username, "params":envelope.params, "pk":envelope.pk
    });
    convert(
        &client
            .request(
                &anonymous,
                "/v9/login_by_pwd",
                Some("login.user.kugou.com"),
                reqwest::Method::POST,
                Default::default(),
                request::encode(&body)?,
            )
            .await?,
        &envelope.seed,
    )
}

const T1: &str = "562a6f12a6e803453647d16a08f5f0c2ff7eee692cba2ab74cc4c8ab47fc467561a7c6b586ce7dc46a63613b246737c03a1dc8f8d162d8ce1d2c71893d19f1d4b797685a4c6d3d81341cbde65e488c4829a9b4d42ef2df470eb102979fa5adcdd9b4eecfea8b909ff7599abeb49867640f10c3c70fc444effca9d15db44a9a6c907731e2bb0f22cd9b3536380169995693e5f0e2424e3378097d3813186e3fe96bbe7023808a0981b4e2b6135a76faac";
const T2: &str = "31c4daf4cf480169ccea1cb7d4a209295865a9d2b788510301694db229b87807469ea0d41b4d4b9173c2151da7294aeebfc9738df154bbdf11a4e117bb5dff6a3af8ce5ce333e681c1f29a44038f27567d58992eb81283e080778ac77db1400fdf49b7cf7e26be2e5af4da7830cc3be4";
const T3: &str = "MCwwLDAsMCwwLDAsMCwwLDA=";
