//! Comment write protocol. Successful moderation/creation response mapping
//! is not established; never synthesize a created ID.
use crate::{
    error::ApiError,
    util::{
        comment::Resource,
        request::{self, ApiClient, Endpoint, Identity, Origin},
    },
};
use md5::{Digest, Md5};
use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Query {
    #[serde(default)]
    pub code: Option<String>,
    pub resource: Resource,
    pub id: String,
    #[serde(default)]
    pub mix_song_id: Option<u64>,
    #[serde(default)]
    pub name: String,
    #[serde(default)]
    pub content: String,
    #[serde(default)]
    pub comment_id: String,
    #[serde(default)]
    pub thread_id: String,
    #[serde(default)]
    pub parent_id: String,
    #[serde(default)]
    pub reply_user_name: String,
    #[serde(default)]
    pub reply_content: String,
}
#[derive(Deserialize)]
struct Wire {
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    status: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    err_code: Option<i64>,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    message: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    msg: String,
}
#[derive(Debug, Serialize)]
pub struct Acknowledgement {
    pub accepted: bool,
}
pub fn convert(b: &[u8]) -> Result<Acknowledgement, ApiError> {
    let w: Wire = request::decode(b)?;
    if w.status != Some(1) || w.err_code.is_some_and(|code| code != 0) {
        let mut e = ApiError::business(w.err_code.unwrap_or_default());
        if !w.message.is_empty() {
            e.message = w.message;
        } else if !w.msg.is_empty() {
            e.message = w.msg;
        }
        return Err(e);
    }
    // Only acceptance is consumed. The platform's moderation result is not inferred.
    Ok(Acknowledgement { accepted: true })
}
fn proof(value: &str) -> String {
    format!(
        "{:x}",
        Md5::digest(format!("3116LnT6xpN3khm36zse0QzvmgTZ3waWdRSA11440{value}"))
    )
}
#[derive(Serialize)]
struct Content<'a> {
    content: &'a str,
    #[serde(skip_serializing_if = "Option::is_none")]
    album_audio_id: Option<u64>,
    images: [u8; 0],
}
#[derive(Serialize)]
struct Body<'a> {
    data: Content<'a>,
}
pub async fn execute(
    c: &ApiClient,
    i: &Identity,
    mut q: Query,
    mode: u8,
) -> Result<Vec<u8>, ApiError> {
    if i.userid == 0 {
        return Err(ApiError::new("authentication", "请先登录后再评论"));
    }
    q.content = q.content.trim().into();
    if mode != 2 && (q.content.is_empty() || q.content.chars().count() > 200) {
        return Err(ApiError::new("input", "评论不能为空，且最多 200 字"));
    }
    if mode == 2 && q.comment_id.is_empty() {
        return Err(ApiError::new("input", "评论标识无效"));
    }
    if mode == 1 && q.thread_id.is_empty() {
        return Err(ApiError::new("input", "回复楼层无效"));
    }
    // Resolve music childrenid with a page-one request before sending.
    // It is distinct from special_id and mixsongid.
    let resolve_music = (mode == 0 && matches!(q.resource, Resource::Music))
        || (mode == 2 && q.id.is_empty() && q.mix_song_id.is_some());
    if resolve_music || (mode < 2 && q.name.is_empty()) {
        let resource = if resolve_music {
            Resource::Music
        } else {
            q.resource
        };
        let list = super::comment::Query {
            id: if resolve_music {
                q.mix_song_id
                    .ok_or_else(|| ApiError::new("input", "歌曲标识无效"))?
                    .to_string()
            } else {
                q.id.clone()
            },
            page: 1,
            page_size: 1,
            category_id: None,
            hot_word: None,
            thread_id: if mode == 1 {
                Some(q.thread_id.clone())
            } else {
                None
            },
            mix_song_id: if mode == 1 { q.mix_song_id } else { None },
            resource: Some(resource),
            code: q.code.clone(),
        };
        let resolved =
            super::comment::fetch(c, i, list, resource, if mode == 1 { 3 } else { 0 }).await?;
        if resolve_music {
            q.id = resolved.resource_id;
            if q.id.is_empty() {
                q.id = resolved
                    .comments
                    .first()
                    .map(|row| row.resource_id.clone())
                    .unwrap_or_default();
            }
        }
        if q.name.is_empty() {
            if let Some(first) = resolved.comments.first() {
                q.name = if first.resource_name.is_empty() {
                    first.display_text.clone()
                } else {
                    first.resource_name.clone()
                };
            }
        }
    }
    submit_resolved(c, i, q, mode).await
}
/// Shared native commentsv3/reply/delete protocol after the resource-specific
/// lookup. Song barrage has already resolved its own pool, never music's pool.
pub async fn submit_resolved(
    c: &ApiClient,
    i: &Identity,
    mut q: Query,
    mode: u8,
) -> Result<Vec<u8>, ApiError> {
    if q.id.is_empty() {
        return Err(ApiError::new("input", "评论资源无效"));
    }
    let t = request::unix_seconds()?.to_string();
    let mut p = BTreeMap::from([
        (
            "r".into(),
            match mode {
                1 => "commentsv2/reply",
                2 => "commentsv2/delcomment",
                _ => "commentsv3/add",
            }
            .into(),
        ),
        (
            "code".into(),
            q.code.unwrap_or_else(|| q.resource.code().into()),
        ),
        ("childrenid".into(), q.id),
        ("kugouid".into(), i.userid.to_string()),
        ("ver".into(), "6".into()),
        ("clienttoken".into(), i.token.clone()),
        ("appid".into(), "3116".into()),
        ("clientver".into(), "11440".into()),
        ("mid".into(), i.mid.clone()),
        ("clienttime".into(), t.clone()),
        ("uuid".into(), i.uuid.clone()),
        ("dfid".into(), i.dfid.clone()),
    ]);
    let body = if mode == 0 {
        request::encode(&Body {
            data: Content {
                content: &q.content,
                album_audio_id: q.mix_song_id,
                images: [],
            },
        })?
    } else {
        Vec::new()
    };
    let suffix = if mode == 0 {
        String::from_utf8(body.clone()).map_err(|_| ApiError::encoding())?
    } else {
        String::new()
    };
    p.insert("key".into(), proof(&format!("{t}{}{suffix}", i.mid)));
    if mode != 2 {
        p.insert("childrenname".into(), q.name);
    }
    if mode == 1 {
        if !q.reply_user_name.is_empty()
            && !q.reply_content.is_empty()
            && !q.content.contains("//@")
        {
            q.content
                .push_str(&format!("//@{}:{}", q.reply_user_name, q.reply_content));
        }
        p.insert("content".into(), q.content);
        p.insert("tid".into(), q.thread_id);
        p.insert(
            "is_t".into(),
            if q.parent_id.is_empty() || q.parent_id == "0" {
                "1"
            } else {
                "0"
            }
            .into(),
        );
        p.insert(
            "pid".into(),
            if q.parent_id.is_empty() {
                "0".into()
            } else {
                q.parent_id
            },
        );
    } else if mode == 2 {
        p.insert("cid".into(), q.comment_id);
        if !q.thread_id.is_empty() {
            p.insert("tid".into(), q.thread_id);
        }
    }
    let b = c
        .request_comment(
            i,
            Endpoint {
                origin: Origin::Gateway,
                path: "/index.php",
                router: Some("m.comment.service.kugou.com"),
                method: if mode == 2 {
                    reqwest::Method::GET
                } else {
                    reqwest::Method::POST
                },
                params: p,
                body,
                headers: &[("Content-Type", "application/json; charset=UTF-8")],
            },
        )
        .await?;
    request::encode(&convert(&b)?)
}
