//! Resolve the video hash before one write; acceptance is not moderation.
pub use crate::util::comment_write::convert;
use crate::{
    error::ApiError,
    module::video_barrage,
    util::request::{self, ApiClient, Endpoint, Identity, Origin},
};
use serde::Deserialize;
use std::collections::BTreeMap;
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Query {
    pub hash: String,
    pub name: String,
    pub content: String,
}
pub async fn execute(c: &ApiClient, i: &Identity, q: Query) -> Result<Vec<u8>, ApiError> {
    let content = q.content.trim();
    if i.userid == 0 {
        return Err(ApiError::new("authentication", "请先登录后再发送弹幕"));
    }
    if content.is_empty() || content.chars().count() > 100 {
        return Err(ApiError::new("input", "弹幕不能为空，且最多 100 字"));
    }
    let resolved = video_barrage::fetch(
        c,
        i,
        &video_barrage::Query {
            hash: q.hash,
            page: 1,
            page_size: 1,
        },
    )
    .await?;
    if resolved.resource_id.is_empty() {
        return Err(ApiError::new("business", "无法根据 hash 解析视频弹幕标识"));
    }
    let p = BTreeMap::from([
        ("r".into(), "comments/addcomment".into()),
        ("code".into(), "db3664c219a6e350b00ab08d7f723a79".into()),
        ("childrenid".into(), resolved.resource_id),
        ("childrenname".into(), q.name),
        ("ver".into(), "1.02".into()),
        ("content".into(), content.into()),
        ("clientver".into(), "11440".into()),
        ("mid".into(), i.mid.clone()),
        ("clienttoken".into(), i.token.clone()),
        ("kugouid".into(), i.userid.to_string()),
        ("appid".into(), "3116".into()),
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
    request::encode(&convert(&b)?)
}
