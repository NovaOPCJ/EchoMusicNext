//! Internal /sidedt acknowledgement. It does not grant account authorization;
//! data.msg is descriptive and is not an acceptance condition.
use crate::{
    error::ApiError,
    util::{
        crypto::verification_envelope,
        generate_simulate::Fingerprint,
        request::{self, ApiClient, Endpoint, Identity, Origin},
    },
};
use serde::{Deserialize, Serialize};
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Query {
    pub event_id: String,
    pub verify_type: u32,
    pub proof: String,
}
#[derive(Deserialize)]
struct Wire {
    status: WireNumber,
    #[serde(default, deserialize_with = "crate::util::response::strict_optional")]
    error_code: Option<WireNumber>,
    #[serde(default, deserialize_with = "crate::util::response::strict_optional")]
    err_code: Option<WireNumber>,
    #[serde(default, deserialize_with = "crate::util::response::strict_optional")]
    errcode: Option<WireNumber>,
}
#[derive(Deserialize)]
#[serde(untagged)]
enum WireNumber {
    Integer(i64),
    Text(String),
}
impl WireNumber {
    fn integer(self) -> Result<i64, ApiError> {
        match self {
            Self::Integer(value) => Ok(value),
            Self::Text(value) => value.trim().parse().map_err(|_| ApiError::decode()),
        }
    }
}
#[derive(Serialize)]
pub struct Acknowledgement {
    pub acknowledged: bool,
}
pub fn convert(bytes: &[u8]) -> Result<Acknowledgement, ApiError> {
    let wire: Wire = request::decode(bytes)?;
    let status = wire.status.integer()?;
    let code = wire
        .error_code
        .or(wire.err_code)
        .or(wire.errcode)
        .map(WireNumber::integer)
        .transpose()?
        .unwrap_or(0);
    // The reference resumes the original request on status=1 with no nonzero
    // error code. Requiring the invalid-event sample's data.msg rejects valid
    // CAPTCHA acknowledgements; it also incorrectly implies a body proves login.
    if status != 1 || code != 0 {
        return Err(ApiError::business(code));
    }
    Ok(Acknowledgement { acknowledged: true })
}

fn response_shape(bytes: &[u8]) -> serde_json::Value {
    fn kind(value: Option<&serde_json::Value>) -> &'static str {
        match value {
            None => "absent",
            Some(serde_json::Value::Null) => "null",
            Some(serde_json::Value::Bool(_)) => "boolean",
            Some(serde_json::Value::Number(_)) => "number",
            Some(serde_json::Value::String(_)) => "string",
            Some(serde_json::Value::Array(_)) => "array",
            Some(serde_json::Value::Object(_)) => "object",
        }
    }
    // Only fixed status/code paths may contribute numerical values. No payload
    // strings, arbitrary keys, request, proof, event, account or device are logged.
    let body = serde_json::from_slice::<serde_json::Value>(bytes).ok();
    let mut shape = serde_json::Map::new();
    shape.insert("body_kind".into(), kind(body.as_ref()).into());
    for field in ["status", "error_code", "err_code", "errcode"] {
        let value = body.as_ref().and_then(|body| body.get(field));
        let number = value.and_then(|value| {
            value
                .as_i64()
                .or_else(|| value.as_str()?.trim().parse::<i64>().ok())
        });
        shape.insert(
            field.into(),
            serde_json::json!({"kind":kind(value), "number":number}),
        );
    }
    let data = body.as_ref().and_then(|body| body.get("data"));
    shape.insert("data_kind".into(), kind(data).into());
    shape.insert(
        "data_msg_kind".into(),
        kind(data.and_then(|data| data.get("msg"))).into(),
    );
    shape.into()
}
pub async fn execute(
    client: &ApiClient,
    identity: &Identity,
    query: Query,
    fingerprint: Fingerprint,
) -> Result<Acknowledgement, ApiError> {
    if query.event_id.trim().is_empty()
        || query.event_id.len() > 4096
        || query.proof.trim().is_empty()
        || query.proof.len() > 16384
    {
        return Err(ApiError::new("input", "验证事件或验证码无效"));
    }
    if !matches!(query.verify_type, 23 | 32) {
        return Err(ApiError::new("input", "此验证类型不能提交验证码"));
    }
    let envelope = if query.verify_type == 32 {
        serde_json::json!({"code":query.proof})
    } else {
        serde_json::json!({})
    };
    let (pk, params) = verification_envelope(&envelope)?;
    let mut body = serde_json::json!({"eventid":query.event_id,"userid":identity.userid,"platid":2,"v_type":query.verify_type,"wasm":1,"i":"","sid":fingerprint.sid,"edt":fingerprint.edt,"pk":pk,"params":params});
    body[if query.verify_type == 32 {
        "code"
    } else {
        "verifycode"
    }] = serde_json::Value::String(query.proof);
    let bytes = client
        .request_endpoint(
            identity,
            Endpoint {
                origin: Origin::Verification,
                path: "/v4/verify_user_info",
                router: None,
                method: reqwest::Method::POST,
                params: std::collections::BTreeMap::from([("clientver".into(), "11510".into())]),
                body: request::encode(&body)?,
                headers: &[],
            },
        )
        .await?;
    // Explicit diagnostic run only; fixed-field projection enters the Qt logger.
    // This projection is not a raw-response fixture or a successful-proof sample.
    if std::env::var("ECHO_VERIFY_DIAGNOSTICS").as_deref() == Ok("1") {
        echo_log::write(
            2,
            "native.verification",
            format!(
                "verify_user_info response shape: {}",
                response_shape(&bytes)
            ),
        );
    }
    convert(&bytes)
}

#[cfg(test)]
mod tests {
    #[test]
    fn diagnostic_never_contains_payload_text_or_unknown_keys() {
        let shape = super::response_shape(br#"{"status":"1","error_code":0,"token":"private-token","data":{"msg":"private-message","ticket":"private-proof","user_id":42}}"#);
        let encoded = shape.to_string();
        assert!(
            !encoded.contains("private")
                && !encoded.contains("token")
                && !encoded.contains("ticket")
                && !encoded.contains("user_id")
        );
        assert_eq!(shape["status"]["number"], 1);
        assert_eq!(shape["data_msg_kind"], "string");
    }
}
