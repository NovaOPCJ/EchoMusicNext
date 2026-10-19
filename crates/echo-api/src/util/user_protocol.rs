//! Endpoint-owned older user protocol: body key/RSA, no injected query token.
use crate::{
    error::ApiError,
    util::{
        crypto,
        request::{self, Identity},
    },
};
use md5::{Digest, Md5};
pub fn require_account(identity: &Identity) -> Result<(), ApiError> {
    if identity.userid == 0 || identity.token.is_empty() {
        return Err(ApiError::new("authentication", "请先登录"));
    }
    Ok(())
}
pub fn timestamp_key(time: u64) -> String {
    format!(
        "{:x}",
        Md5::digest(format!("3116LnT6xpN3khm36zse0QzvmgTZ3waWdRSA11440{time}").as_bytes())
    )
}
pub fn body(identity: &Identity, time: u64) -> Result<serde_json::Value, ApiError> {
    require_account(identity)?;
    let p = crypto::user_portrait(&request::encode(
        &serde_json::json!({"clienttime":time,"token":identity.token}),
    )?)?;
    Ok(
        serde_json::json!({"userid":identity.userid,"dfid":identity.dfid,"plat":1,"nickimg":1,"appid":3116,"clientver":11440,"mid":identity.mid,"clienttime":time,"key":timestamp_key(time),"p":p}),
    )
}
