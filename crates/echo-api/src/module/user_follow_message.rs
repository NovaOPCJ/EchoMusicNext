//! History samples contain an empty root list. Nonempty records are retained
//! verbatim as provider extensions until their message fields can be sampled.
use crate::{
    error::ApiError,
    util::{
        relation,
        request::{self, ApiClient, Identity},
    },
};
use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Query {
    pub target: u64,
    pub tag: Option<String>,
}
#[derive(Deserialize)]
struct Wire {
    #[serde(deserialize_with = "crate::util::response::scalar")]
    status: u32,
    #[serde(default, deserialize_with = "crate::util::response::scalar")]
    errcode: i64,
    #[serde(default, deserialize_with = "crate::util::response::list")]
    list: Vec<serde_json::Value>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    is_end: Option<u32>,
}
#[derive(Serialize)]
pub struct History {
    pub records: Vec<serde_json::Value>,
    pub ended: Option<bool>,
}
pub fn convert(bytes: &[u8]) -> Result<History, ApiError> {
    let v: Wire = request::decode(bytes)?;
    request::success(v.status, v.errcode)?;
    Ok(History {
        records: v.list,
        ended: v.is_end.and_then(crate::util::numeric::boolean),
    })
}
pub async fn execute(
    client: &ApiClient,
    identity: &Identity,
    q: Query,
) -> Result<Vec<u8>, ApiError> {
    relation::require_account(identity)?;
    crate::util::cloudlist::id(q.target)?;
    let tag = q
        .tag
        .filter(|s| !s.is_empty())
        .unwrap_or_else(|| format!("chat:{}_{}", q.target, identity.userid));
    let bytes = client
        .request(
            identity,
            "/msg.mobile/v3/msgtag/history",
            None,
            reqwest::Method::GET,
            BTreeMap::from([
                ("filter".into(), "1".into()),
                ("maxid".into(), "0".into()),
                ("pagesize".into(), "30".into()),
                ("tag".into(), tag),
            ]),
            vec![],
        )
        .await?;
    request::encode(&convert(&bytes)?)
}
