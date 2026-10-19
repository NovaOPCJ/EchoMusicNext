//! Live wire evidence: first-page-playback-20261013.json; follows the reference request order.
use crate::{
    domain::Page,
    error::ApiError,
    util::{
        audio_effect::{self, Effect},
        request::{self, ApiClient, Endpoint, Identity, Origin},
    },
};
use serde::Deserialize;
use std::collections::BTreeMap;
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Query {
    pub page: u32,
    pub page_size: u32,
}
pub async fn execute(
    client: &ApiClient,
    identity: &Identity,
    query: Query,
) -> Result<Vec<u8>, ApiError> {
    audio_effect::paging(query.page, query.page_size)?;

    let mut params = BTreeMap::from([
        ("plat".into(), "2".into()),
        ("version".into(), "12460".into()),
        ("apiver".into(), "2".into()),
        ("sort".into(), "1".into()),
        ("classify".into(), "1".into()),
    ]);
    params.insert("page".into(), query.page.to_string());
    params.insert("pagesize".into(), query.page_size.to_string());

    let bytes = client
        .request_effect_catalog(
            identity,
            Endpoint {
                origin: Origin::EffectsCatalog,
                path: "/api/v3/sound/list",
                router: None,
                method: reqwest::Method::GET,
                params,
                body: vec![],
                headers: &[],
            },
        )
        .await?;
    request::encode(&convert(&bytes, query.page, query.page_size)?)
}
pub fn convert(bytes: &[u8], page: u32, size: u32) -> Result<Page<Effect>, ApiError> {
    audio_effect::catalog_page(bytes, page, size, "artist")
}
