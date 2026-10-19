//! /sidedt composes local fingerprints with verify_user_info
//! using the same immutable account/device snapshot.
use crate::{
    error::ApiError,
    module::verify_user_info,
    util::{
        generate_simulate,
        request::{self, ApiClient, Identity},
    },
};
pub async fn execute(
    client: &ApiClient,
    identity: &Identity,
    query: verify_user_info::Query,
) -> Result<Vec<u8>, ApiError> {
    let fingerprint = generate_simulate::generate(identity)?;
    request::encode(&verify_user_info::execute(client, identity, query, fingerprint).await?)
}
