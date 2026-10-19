//! Generated login control and validation cases.
//! These inputs are unit-test values, not upstream response fixtures.
use echo_api::{module, util::crypto};
#[test]
fn verification_ack_does_not_require_descriptive_body_and_never_accepts_failure() {
    // Generated control-flow cases, not successful upstream CAPTCHA responses.
    for bytes in [
        br#"{"status":1,"error_code":0,"data":null}"#.as_slice(),
        br#"{"status":1,"error_code":0,"data":{}}"#,
        br#"{"status":1,"error_code":0,"data":"ok"}"#,
        br#"{"status":"1","err_code":"0"}"#,
        br#"{"status":1}"#,
    ] {
        assert!(
            module::verify_user_info::convert(bytes)
                .unwrap()
                .acknowledged
        );
    }
    for bytes in [
        br#"{"status":0,"error_code":30791,"data":null}"#.as_slice(),
        br#"{"status":1,"errcode":30791,"data":{}}"#,
    ] {
        let error = module::verify_user_info::convert(bytes).err().unwrap();
        assert_eq!(error.kind, "business");
        assert_eq!(error.code, Some(30791));
    }
    for bytes in [
        br#"{"status":null}"#.as_slice(),
        br#"{"status":1,"error_code":"invalid"}"#,
    ] {
        assert_eq!(
            module::verify_user_info::convert(bytes).err().unwrap().kind,
            "decode"
        );
    }
}
fn encrypted(secret: &serde_json::Value, seed: &str) -> String {
    use md5::{Digest, Md5};
    let key = format!("{:x}", Md5::digest(seed));
    crypto::aes256_hex(
        &serde_json::to_vec(secret).unwrap(),
        key.as_bytes(),
        &key.as_bytes()[16..],
    )
    .unwrap()
}
#[test]
fn initial_login_decrypts_object_and_keeps_unknown_expiry_absent() {
    let seed = "unit-only";
    let bytes=serde_json::to_vec(&serde_json::json!({"status":1,"error_code":0,"data":{"userid":17,"nickname":"test","pic":"","secu_params":encrypted(&serde_json::json!({"token":"unit-token","t1":"unit-refresh"}),seed)}})).unwrap();
    for convert in [
        module::login::convert,
        module::login_cellphone::convert,
        module::login_qq::convert,
        module::login_openplat::convert,
    ] {
        let value: serde_json::Value =
            serde_json::from_slice(&convert(&bytes, seed).unwrap()).unwrap();
        assert_eq!(value["user_id"], 17);
        assert_eq!(value["token"], "unit-token");
        assert_eq!(value["refresh_credential"], "unit-refresh");
        assert!(value["expires_at_seconds"].is_null());
        assert!(convert(&bytes, "wrong-seed").is_err());
    }
}
#[test]
fn password_and_sms_plain_token_branches_still_require_user_identity() {
    let bytes = br#"{"status":1,"error_code":0,"data":{"userid":42,"token":"unit-token"}}"#;
    for convert in [module::login::convert, module::login_cellphone::convert] {
        let value: serde_json::Value =
            serde_json::from_slice(&convert(bytes, "unused").unwrap()).unwrap();
        assert_eq!(value["user_id"], 42);
        assert!(value["refresh_credential"].is_null());
        assert!(
            convert(
                br#"{"status":1,"error_code":0,"data":{"token":"unit-token"}}"#,
                "unused"
            )
            .is_err()
        );
    }
}
#[test]
fn sms_candidates_preserve_selection_without_sample_identity_checks() {
    let bytes=serde_json::to_vec(&serde_json::json!({"status":0,"error_code":34175,"data":{"info_list":[{"userid":42,"username":"account","pic":""}]}})).unwrap();
    let value: serde_json::Value =
        serde_json::from_slice(&module::login_cellphone::convert(&bytes, "unused").unwrap())
            .unwrap();
    assert_eq!(value["state"], "select_account");
    assert_eq!(value["candidates"][0]["nickname"], "account");
    assert!(
        module::login_cellphone::convert(
            br#"{"status":0,"error_code":34175,"data":{"info_list":[{"userid":0}]}}"#,
            "unused"
        )
        .is_ok()
    );
}
#[test]
fn provider_success_never_bypasses_exchange_or_accepts_empty_proof() {
    let wx =
        module::login_wx_check::convert(br#"{"wx_errcode":405,"wx_code":"unit-code"}"#).unwrap();
    assert_eq!(wx.state, "provider_code");
    assert_eq!(wx.code.as_deref(), Some("unit-code"));
    assert!(module::login_wx_check::convert(br#"{"wx_errcode":405,"wx_code":""}"#).is_err());
    assert_eq!(
        module::login_qq_qr_check::convert(b"ptuiCB('67','0','','0','unit','')").unwrap(),
        "scanned"
    );
    assert_eq!(
        module::login_qq_qr_check::convert(b"ptuiCB('65','0','','0','unit','')").unwrap(),
        "expired"
    );
    let auth = module::login_qq_qr_check::credentials_url(
        "auth://tauth.qq.com/#openid=unit-id&access_token=unit-token",
    )
    .unwrap()
    .unwrap();
    assert_eq!(auth.openid, "unit-id");
    assert!(
        module::login_openplat::convert_oauth(br#"{"access_token":"","openid":"unit-id"}"#)
            .is_err()
    );
}
#[test]
fn verification_configuration_distinguishes_sms_login_and_official_client_actions() {
    for (kind, provider) in [
        (32, "sms"),
        (38, "login"),
        (34, "real_name"),
        (36, "bind_phone"),
        (51, "account_risk"),
    ] {
        let bytes=serde_json::to_vec(&serde_json::json!({"status":1,"error_code":0,"data":{"show":{"msg":"unit-message"},"v_type_list":[kind],"partnerid_map":{},"sessionid":"unit-session","txappid":"","url":"","business":1,"v_type":kind,"vde":0}})).unwrap();
        let info = module::get_verify_info::convert(&bytes).unwrap();
        assert_eq!(info.provider, provider);
        assert_eq!(info.display_message, "unit-message");
    }
}

#[test]
fn source_only_verification_kinds_preserve_missing_metadata_and_vendor_names() {
    let sms =
        module::get_verify_info::convert(br#"{"status":1,"error_code":0,"data":{"v_type":32}}"#)
            .unwrap();
    assert_eq!(sms.provider, "sms");
    assert!(sms.business_id.is_none() && sms.session_id.is_none() && sms.protocol_flag.is_none());
    let other = module::get_verify_info::convert(
        br#"{"status":1,"error_code":0,"data":{"v_type":24,"url":"KGCodeGT|unit"}}"#,
    )
    .unwrap();
    assert_eq!(other.provider, "unsupported");
    assert_eq!(other.vendor, "GT");
    assert!(module::get_verify_info::convert(br#"{"status":1,"error_code":0,"data":{"v_type":23,"url":"KGCodeTX|197787253","txappid":"197787253"}}"#).is_ok());
}

#[test]
fn password_failure_preserves_provider_text_and_code_without_credential_objects() {
    // Generated branch inputs; these are not successful upstream CAPTCHA responses.
    let bytes = serde_json::to_vec(&serde_json::json!({"status":0,"error_code":12345,"data":"provider explanation","message":"secondary"})).unwrap();
    let error = module::login::convert(&bytes, "unused").err().unwrap();
    assert_eq!(error.code, Some(12345));
    assert_eq!(error.message, "provider explanation");
    let bytes = serde_json::to_vec(&serde_json::json!({"status":0,"error_code":12345,"data":{"token":"never display"},"msg":"message explanation"})).unwrap();
    let error = module::login::convert(&bytes, "unused").err().unwrap();
    assert_eq!(error.message, "message explanation");
}
