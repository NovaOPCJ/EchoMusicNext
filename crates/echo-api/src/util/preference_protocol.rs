//! Older preference envelope has a seconds timestamp encoded as a STRING in
//! PKCS#1-v1.5 RSA, unlike login's raw RSA/clienttime_ms. Token is AES-encrypted,
//! absent from the signed URL.
use crate::{
    error::ApiError,
    util::{
        crypto,
        request::{self, ApiClient, Endpoint, Identity, Origin},
        signature, user_protocol,
    },
};
use std::collections::BTreeMap;
pub async fn send(
    client: &ApiClient,
    identity: &Identity,
    fields: Option<BTreeMap<String, String>>,
) -> Result<Vec<u8>, ApiError> {
    user_protocol::require_account(identity)?;
    let time = request::unix_seconds()?;
    let (portrait, encrypted) = crypto::preference_envelope(&identity.token, time)?;
    let update = fields.is_some();
    let mut body =
        serde_json::json!({"plat":1,"userid":identity.userid,"p":portrait,"params":encrypted});
    if let Some(fields) = fields {
        body["data"] = serde_json::to_value(fields).map_err(|_| ApiError::encoding())?;
    }
    let body = request::encode(&body)?;
    let mut params = BTreeMap::from([
        ("appid".into(), "3116".into()),
        ("clientver".into(), "11440".into()),
        ("clienttime".into(), time.to_string()),
        ("mid".into(), identity.mid.clone()),
        ("dfid".into(), identity.dfid.clone()),
        ("uuid".into(), identity.uuid.clone()),
    ]);
    params.insert("signature".into(), signature::android(&params, &body));
    client
        .request_grade(
            identity,
            Endpoint {
                origin: Origin::Gateway,
                path: if update {
                    "/userpreferservice/v1/update_user_conf"
                } else {
                    "/userpreferservice/v1/get_user_conf"
                },
                router: None,
                method: reqwest::Method::POST,
                params,
                body,
                headers: &[
                    ("Content-Type", "application/json"),
                    ("KG-Rec", "1"),
                    ("KG-RC", "1"),
                    (
                        "User-Agent",
                        if update {
                            "Android15-1070-11440-201-0-update_user_conf-wifi"
                        } else {
                            "Android15-1070-11440-201-0-get_user_conf-wifi"
                        },
                    ),
                ],
            },
        )
        .await
}
