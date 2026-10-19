//! Shared room transport/envelope, not a generic public routing API.
use crate::{
    error::ApiError,
    util::request::{self, ApiClient, Identity},
};
use serde::{Deserialize, de::DeserializeOwned};
use serde_json::{Value, json};
use std::collections::BTreeMap;
#[derive(Deserialize)]
struct Envelope {
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    status: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    error_code: Option<i64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    errcode: Option<i64>,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    error_msg: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    error: String,
    #[serde(default)]
    data: Value,
}
pub fn data(bytes: &[u8]) -> Result<Value, ApiError> {
    let wire: Envelope = request::decode(bytes)?;
    let code = wire.error_code.or(wire.errcode).unwrap_or(0);
    if wire.status == Some(0) || code != 0 {
        let mut error = if code == 51002 {
            ApiError::authentication(code)
        } else {
            ApiError::business(code)
        };
        let message = if !wire.error_msg.is_empty() {
            wire.error_msg
        } else {
            wire.error
        };
        if !message.is_empty() {
            error.message = message;
        }
        return Err(error);
    }
    Ok(wire.data)
}
pub fn decode<T: DeserializeOwned>(value: Value) -> Result<T, ApiError> {
    request::decode_value(value, &[])
}
pub fn auth(identity: &Identity) -> Result<Value, ApiError> {
    if identity.userid == 0 || identity.token.is_empty() {
        return Err(ApiError::authentication(51002));
    }
    Ok(json!({"userid":identity.userid,"token":identity.token}))
}
pub fn audio(audio: &crate::domain::listen_together::AudioRef) -> Value {
    json!({"hash":audio.hash,"mixsongid":audio.song_id,"fid":0})
}
pub fn progress(progress: &crate::domain::listen_together::InitialPlayback) -> Value {
    json!({"hash":progress.audio.hash,"album_audio_id":progress.audio.song_id,
        "progress":progress.position_ms/1000,"pause":if progress.playing {1} else {2},
        "play_mode":progress.play_mode.to_string()})
}
pub async fn request(
    client: &ApiClient,
    identity: &Identity,
    path: &str,
    method: reqwest::Method,
    params: BTreeMap<String, String>,
    body: Option<Value>,
) -> Result<Vec<u8>, ApiError> {
    client
        .request(
            identity,
            path,
            None,
            method,
            params,
            match body {
                Some(value) => request::encode(&value)?,
                None => Vec::new(),
            },
        )
        .await
}
pub fn params(room_id: &str) -> BTreeMap<String, String> {
    BTreeMap::from([("roomid".into(), room_id.into())])
}
