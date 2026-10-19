//! Shared relation-user protocol with signed Android URL identity,
//! RSA clienttime/token portrait and KG-TID=473.
use crate::{
    error::ApiError,
    module::blacklist_list::Label,
    util::{
        crypto,
        request::{self, ApiClient, Endpoint, Identity, Origin},
        user_protocol,
    },
};
use std::collections::BTreeMap;
pub async fn send(
    client: &ApiClient,
    identity: &Identity,
    label: Label,
    mut body: serde_json::Value,
    edit: bool,
) -> Result<Vec<u8>, ApiError> {
    user_protocol::require_account(identity)?;
    let portrait = crypto::user_portrait(&request::encode(
        &serde_json::json!({"clienttime":request::unix_seconds()?,"token":identity.token}),
    )?)?;
    body["userid"] = identity.userid.into();
    body["label"] = label.name().into();
    body["source"] = label.source().into();
    body["p"] = portrait.into();
    client
        .request_endpoint(
            identity,
            Endpoint {
                origin: Origin::RelationUser,
                path: if edit {
                    "/v1/edit_list_items"
                } else {
                    "/v2/get_list_items"
                },
                router: None,
                method: reqwest::Method::POST,
                params: BTreeMap::new(),
                body: request::encode(&body)?,
                headers: &[("KG-TID", "473"), ("Content-Type", "application/json")],
            },
        )
        .await
}
