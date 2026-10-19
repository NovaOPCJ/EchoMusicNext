//! Observed upstream fixtures only; they do not establish interactive login success.
use echo_api::module::{login_qq_qr_check, login_wx_check};
fn payload(name: &str, condition: &str) -> serde_json::Value {
    let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
        .join(format!("../../tests/fixtures/api/{name}/{condition}.json"));
    let record: serde_json::Value = serde_json::from_slice(&std::fs::read(path).unwrap()).unwrap();
    record["responses"].as_array().unwrap().last().unwrap()["payload"].clone()
}
#[test]
fn qq_waiting_callback_is_data_and_malformed_javascript_is_rejected() {
    let body = payload("login_qq_qr_check", "anonymous-waiting");
    assert_eq!(
        login_qq_qr_check::convert(body["callback"].as_str().unwrap().as_bytes()).unwrap(),
        "waiting"
    );
    assert!(login_qq_qr_check::convert(b"alert('66')").is_err());
    assert_eq!(
        login_qq_qr_check::convert(b"ptuiCB('66')").unwrap(),
        "waiting"
    );
    assert_eq!(
        login_qq_qr_check::convert(b"ptuiCB('66','0','','0','unit','','extra')").unwrap(),
        "waiting"
    );
}
#[test]
fn wechat_waiting_and_actual_expiry_do_not_authorize_an_account() {
    let waiting = serde_json::to_vec(&payload("login_wx_check", "anonymous-waiting")).unwrap();
    let result = login_wx_check::convert(&waiting).unwrap();
    assert_eq!(result.state, "waiting");
    assert!(result.code.is_none());
    let expired = serde_json::to_vec(&payload("login_wx_check", "interactive-expired")).unwrap();
    let result = login_wx_check::convert(&expired).unwrap();
    assert_eq!(result.state, "expired");
    assert!(result.code.is_none());
    let mut unexpected: serde_json::Value = serde_json::from_slice(&waiting).unwrap();
    unexpected["wx_code"] = serde_json::json!("unexpected-authorization");
    let result = login_wx_check::convert(&serde_json::to_vec(&unexpected).unwrap()).unwrap();
    assert_eq!(result.state, "waiting");
    assert!(result.code.is_none());
}
