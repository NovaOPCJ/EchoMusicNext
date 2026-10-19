use crate::{
    error::ApiError,
    util::request::{self, ApiClient, Identity},
};
use serde::Deserialize;
use std::collections::BTreeMap;
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Query {
    pub hash: String,
}
pub fn convert(b: &[u8], hash: &str) -> Result<Option<u64>, ApiError> {
    let w: BTreeMap<String, serde_json::Value> = request::decode(b)?;
    Ok(w.get(hash)
        .and_then(|v| crate::util::response::from_value::<u64>(v.clone()).ok()))
}
pub async fn execute(c: &ApiClient, i: &Identity, q: Query) -> Result<Vec<u8>, ApiError> {
    let b = c
        .request_comment_count(
            i,
            BTreeMap::from([
                ("r".into(), "comments/getcommentsnum".into()),
                ("code".into(), "fc4be23b4e972707f36b8a828a93ba8a".into()),
                ("hash".into(), q.hash.clone()),
            ]),
        )
        .await?;
    request::encode(&convert(&b, &q.hash)?)
}
