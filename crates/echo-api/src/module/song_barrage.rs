//! Song barrage uses hash to resolve childrenid, independently of mixsongid.
use crate::{
    error::ApiError,
    util::{
        comment,
        request::{self, ApiClient, Endpoint, Identity, Origin},
    },
};
use serde::Deserialize;
use std::collections::BTreeMap;
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Query {
    pub hash: String,
    pub page: u32,
    pub page_size: u32,
}
pub use crate::util::comment::convert;
pub async fn fetch(c: &ApiClient, i: &Identity, q: &Query) -> Result<comment::Page, ApiError> {
    if !crate::module::song_climax::valid_hash(&q.hash) || q.page == 0 || q.page_size == 0 {
        return Err(ApiError::new("input", "歌曲弹幕标识或分页无效"));
    }
    let t = request::unix_seconds()?;
    let p = BTreeMap::from([
        ("r".into(), "comments/getCommentWithLike".into()),
        ("code".into(), "articulossong".into()),
        ("extdata".into(), q.hash.clone()),
        ("p".into(), q.page.to_string()),
        ("pagesize".into(), q.page_size.to_string()),
        ("kugouid".into(), i.userid.to_string()),
        ("ver".into(), "6".into()),
        ("clienttoken".into(), i.token.clone()),
        ("appid".into(), "3116".into()),
        ("clientver".into(), "11440".into()),
        ("mid".into(), i.mid.clone()),
        ("clienttime".into(), t.to_string()),
        ("key".into(), crate::util::user_protocol::timestamp_key(t)),
        ("uuid".into(), i.uuid.clone()),
        ("dfid".into(), i.dfid.clone()),
    ]);
    let b = c
        .request_comment(
            i,
            Endpoint {
                origin: Origin::Gateway,
                path: "/index.php",
                router: Some("m.comment.service.kugou.com"),
                method: reqwest::Method::GET,
                params: p,
                body: vec![],
                headers: &[],
            },
        )
        .await?;
    convert(&b)
}
pub async fn execute(c: &ApiClient, i: &Identity, q: Query) -> Result<Vec<u8>, ApiError> {
    request::encode(&fetch(c, i, &q).await?)
}
