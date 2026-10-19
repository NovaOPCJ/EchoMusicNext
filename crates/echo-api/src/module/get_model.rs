//! Live wire evidence: first-page-playback-20261013.json; follows the reference request order.
use crate::{
    domain::Page,
    error::ApiError,
    util::{
        audio_effect::{self, Effect},
        request::{self, ApiClient, Identity},
    },
};
use serde::Deserialize;
use std::collections::BTreeMap;
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Query {
    pub page: u32,
    pub page_size: u32,
    pub sort: u32,
}
pub async fn execute(
    client: &ApiClient,
    identity: &Identity,
    query: Query,
) -> Result<Vec<u8>, ApiError> {
    audio_effect::paging(query.page, query.page_size)?;
    if ![2, 3, 4].contains(&query.sort) {
        return Err(ApiError::new("input", "音效排序无效"));
    }
    let mut params = BTreeMap::from([
        ("super_vip".into(), "1".into()),
        ("sound_ver".into(), "2".into()),
        ("apiver".into(), "3".into()),
        ("classify".into(), "2,3".into()),
        ("plat".into(), "2".into()),
        ("privilege".into(), "1".into()),
    ]);
    params.insert("page".into(), query.page.to_string());
    params.insert("pagesize".into(), query.page_size.to_string());
    params.insert("sort".into(), query.sort.to_string());
    let bytes = client
        .request(
            identity,
            "/ocean/v6/sound/list",
            None,
            reqwest::Method::GET,
            params,
            vec![],
        )
        .await?;
    request::encode(&convert(&bytes, query.page, query.page_size)?)
}
pub fn convert(bytes: &[u8], page: u32, size: u32) -> Result<Page<Effect>, ApiError> {
    audio_effect::catalog_page(bytes, page, size, "market")
}
