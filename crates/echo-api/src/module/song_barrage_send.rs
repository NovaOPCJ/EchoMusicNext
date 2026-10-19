//! Resolve the song hash before one commentsv3/add POST. No implicit retry.
pub use crate::util::comment_write::convert;
use crate::{
    error::ApiError,
    module::song_barrage,
    util::{
        comment::Resource,
        comment_write,
        request::{ApiClient, Identity},
    },
};
use serde::Deserialize;
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
    let resolved = song_barrage::fetch(
        c,
        i,
        &song_barrage::Query {
            hash: q.hash,
            page: 1,
            page_size: 1,
        },
    )
    .await?;
    if resolved.resource_id.is_empty() {
        return Err(ApiError::new("business", "无法根据 hash 解析歌曲弹幕标识"));
    }
    let name = if q.name.is_empty() {
        resolved
            .comments
            .first()
            .map(|r| r.resource_name.clone())
            .unwrap_or_default()
    } else {
        q.name
    };
    comment_write::submit_resolved(
        c,
        i,
        comment_write::Query {
            resource: Resource::Music,
            code: Some("articulossong".into()),
            id: resolved.resource_id,
            name,
            content: content.into(),
            mix_song_id: None,
            comment_id: String::new(),
            thread_id: String::new(),
            parent_id: String::new(),
            reply_user_name: String::new(),
            reply_content: String::new(),
        },
        0,
    )
    .await
}
