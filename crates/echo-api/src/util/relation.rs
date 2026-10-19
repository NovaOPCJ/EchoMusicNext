//! Shared follow/unfollow envelope: body key/RSA, fixed device headers and no URL signature.
use crate::{
    error::ApiError,
    util::{crypto, request},
};
use md5::{Digest, Md5};

pub fn require_account(identity: &request::Identity) -> Result<(), ApiError> {
    if identity.userid == 0 || identity.token.is_empty() {
        return Err(ApiError::new("authentication", "请先登录"));
    }
    Ok(())
}

pub fn key(time: u64) -> String {
    // lite appid/clientver and salt observed in the follow/unfollow sample.
    format!(
        "{:x}",
        Md5::digest(format!("3116LnT6xpN3khm36zse0QzvmgTZ3waWdRSA11440{time}").as_bytes())
    )
}

pub fn body(identity: &request::Identity, target: u64) -> Result<serde_json::Value, ApiError> {
    require_account(identity)?;
    let time = request::unix_seconds()?;
    let portrait = crypto::user_portrait(&request::encode(
        &serde_json::json!({"clienttime":time,"token":identity.token,"t_userid":target}),
    )?)?;
    Ok(serde_json::json!({
        "userid": identity.userid,
        "source": 0,
        "clienttime": time,
        "mid": identity.mid,
        "key": key(time),
        "dfid": identity.dfid,
        "clientver": 11440,
        "appid": 3116,
        "p": portrait
    }))
}

pub fn headers(identity: &request::Identity, router: &'static str) -> [(&'static str, String); 7] {
    [
        ("x-router", router.into()),
        (
            "User-Agent",
            "Android15-AndroidPhone-11440-201-0-User-wifi".into(),
        ),
        ("Content-Type", "text/plain; charset=ISO-8859-1".into()),
        ("KG-FAKE", identity.userid.to_string()),
        ("KG-FAKE-TYPE", "0,0".into()),
        ("KG-FAKE-SUBTYPE", "0,0".into()),
        ("KG-RFB", "0".into()),
    ]
}
