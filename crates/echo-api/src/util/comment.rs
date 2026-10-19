//! Shared comment wire data. Optional facts remain unknown; provider extensions are retained.
use crate::{
    error::ApiError,
    util::request::{self, ApiClient, Identity},
};
use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
#[derive(Clone, Copy, Deserialize, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum Resource {
    Music,
    Album,
    Playlist,
}
impl Resource {
    pub fn code(self) -> &'static str {
        match self {
            Self::Music => "fc4be23b4e972707f36b8a828a93ba8a",
            Self::Album => "94f1792ced1df89aa68a7939eaf2efca",
            Self::Playlist => "ca53b96fe5a1d9c22d71c8f522ef7c4f",
        }
    }
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Query {
    #[serde(default)]
    pub code: Option<String>,
    pub id: String,
    pub page: u32,
    pub page_size: u32,
    #[serde(default)]
    pub category_id: Option<u64>,
    #[serde(default)]
    pub hot_word: Option<String>,
    #[serde(default)]
    pub thread_id: Option<String>,
    #[serde(default)]
    pub mix_song_id: Option<u64>,
    #[serde(default)]
    pub resource: Option<Resource>,
}
#[derive(Default, Deserialize)]
pub struct WireComment {
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub id: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub special_id: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub special_child_id: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub special_child_name: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub song_show_text: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub code: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub tid: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub pid: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub ori_cmt_id: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub content: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub pcontent: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub puser_id: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub user_id: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub user_name: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub user_pic: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub addtime: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub location: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub album_audio_id: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub reply_num: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub comments_num: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub vip_type: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub y_type: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub svip_level: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub is_reply: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub is_publish: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::object")]
    pub like: Likes,
    #[serde(default, deserialize_with = "crate::util::response::object")]
    pub vinfo9: IdentityInfo,
    #[serde(default, deserialize_with = "crate::util::response::object")]
    pub udetail: Decoration,
    #[serde(default, deserialize_with = "crate::util::response::object")]
    pub vipinfo: Membership,
    #[serde(default)]
    pub images: serde_json::Value,
    #[serde(default)]
    pub replys: serde_json::Value,
    #[serde(flatten)]
    pub extension: BTreeMap<String, serde_json::Value>,
}
#[derive(Default, Deserialize, Serialize)]
pub struct Likes {
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub count: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub haslike: Option<bool>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub hasoppose: Option<bool>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub likenum: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub opposenum: Option<u64>,
    #[serde(flatten)]
    pub extension: BTreeMap<String, serde_json::Value>,
}
#[derive(Default, Deserialize, Serialize)]
pub struct IdentityInfo {
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub star_v_status: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub tme_star_status: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub actor_status: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub biz_status: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub cmt_talent_status: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub user_iden: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub singer_status: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub student_status: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub auth_info: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub pic: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub user_label: String,
    #[serde(flatten)]
    pub extension: BTreeMap<String, serde_json::Value>,
}
#[derive(Default, Deserialize, Serialize)]
pub struct Decoration {
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub pendant_url: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub pendant_dynamic: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub pendant_name: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub small_pic_v3: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub word_v3: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub singer_avatar: String,
    #[serde(flatten)]
    pub extension: BTreeMap<String, serde_json::Value>,
}
#[derive(Default, Deserialize, Serialize)]
pub struct Membership {
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub vip_type: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub y_type: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub m_type: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub user_type: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub user_y_type: Option<u32>,
    #[serde(flatten)]
    pub extension: BTreeMap<String, serde_json::Value>,
}
#[derive(Default, Deserialize, Serialize)]
pub struct Image {
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub url: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub width: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub height: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub label: String,
    #[serde(flatten)]
    pub extension: BTreeMap<String, serde_json::Value>,
}
#[derive(Default, Deserialize)]
pub struct WirePage {
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub status: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub err_code: Option<i64>,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub message: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub childrenid: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub current_page: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub count: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub comments_num: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    #[serde(rename = "maxPage")]
    pub max_page: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::list")]
    pub list: Vec<WireComment>,
    #[serde(default, deserialize_with = "crate::util::response::list")]
    pub classify_list: Vec<Category>,
    #[serde(default, deserialize_with = "crate::util::response::list")]
    pub hot_word_list: Vec<HotWord>,
    #[serde(flatten)]
    pub extension: BTreeMap<String, serde_json::Value>,
}
#[derive(Default, Deserialize, Serialize)]
pub struct Category {
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub id: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub label: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub icon: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub cnt: Option<u64>,
    #[serde(flatten)]
    pub extension: BTreeMap<String, serde_json::Value>,
}
#[derive(Default, Deserialize, Serialize)]
pub struct HotWord {
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub content: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub count: Option<u64>,
    #[serde(flatten)]
    pub extension: BTreeMap<String, serde_json::Value>,
}
#[derive(Serialize)]
pub struct Comment {
    pub id: String,
    pub resource_id: String,
    pub category_id: String,
    pub resource_name: String,
    pub display_text: String,
    pub code: String,
    pub thread_id: String,
    pub parent_id: String,
    pub original_id: String,
    pub content: String,
    pub parent_content: String,
    pub parent_user_id: String,
    pub user_id: String,
    pub user_name: String,
    pub avatar_url: String,
    pub created_at: String,
    pub location: String,
    pub mix_song_id: Option<u64>,
    pub reply_count: Option<u64>,
    pub comments_count: Option<u64>,
    pub vip_type: Option<u32>,
    pub young_vip_type: Option<u32>,
    pub svip_level: Option<u32>,
    pub reply_code: Option<u32>,
    pub published_code: Option<u32>,
    pub likes: Likes,
    pub identity: IdentityInfo,
    pub decoration: Decoration,
    pub membership: Membership,
    pub images: Vec<Image>,
    pub previews: Vec<Comment>,
    pub extension: BTreeMap<String, serde_json::Value>,
}
impl From<WireComment> for Comment {
    fn from(mut w: WireComment) -> Self {
        let (images, unmapped_images) = collection(w.images);
        let (previews, unmapped_previews) = collection::<WireComment>(w.replys);
        if let Some(value) = unmapped_images {
            w.extension.insert("unmapped_images".into(), value);
        }
        if let Some(value) = unmapped_previews {
            w.extension.insert("unmapped_previews".into(), value);
        }
        Self {
            id: w.id,
            resource_id: w.special_child_id,
            category_id: w.special_id,
            resource_name: w.special_child_name,
            display_text: w.song_show_text,
            code: w.code,
            thread_id: w.tid,
            parent_id: w.pid,
            original_id: w.ori_cmt_id,
            content: w.content,
            parent_content: w.pcontent,
            parent_user_id: w.puser_id,
            user_id: w.user_id,
            user_name: w.user_name,
            avatar_url: crate::domain::cover_url(w.user_pic),
            created_at: w.addtime,
            location: w.location,
            mix_song_id: w.album_audio_id,
            reply_count: w.reply_num,
            comments_count: w.comments_num,
            vip_type: w.vip_type,
            young_vip_type: w.y_type,
            svip_level: w.svip_level,
            reply_code: w.is_reply,
            published_code: w.is_publish,
            likes: w.like,
            identity: w.vinfo9,
            decoration: w.udetail,
            membership: w.vipinfo,
            images,
            previews: previews.into_iter().map(Into::into).collect(),
            extension: w.extension,
        }
    }
}
#[derive(Serialize)]
pub struct Page {
    pub resource_id: String,
    pub page: Option<u32>,
    pub total: Option<u64>,
    pub reply_total: Option<u64>,
    pub maximum_page: Option<u32>,
    pub service_code: Option<i64>,
    pub message: String,
    pub comments: Vec<Comment>,
    pub singer_comments: Vec<Comment>,
    pub categories: Vec<Category>,
    pub hot_words: Vec<HotWord>,
    pub extension: BTreeMap<String, serde_json::Value>,
}
pub fn convert(bytes: &[u8]) -> Result<Page, ApiError> {
    let mut w: WirePage = request::decode(bytes)?;
    if w.status.is_some_and(|s| s != 1) {
        return Err(ApiError::business(w.err_code.unwrap_or_default()));
    }
    // Singer collections are independent of list and count; retain their provider extensions.
    let singer_list = w
        .extension
        .get("star_cmts")
        .and_then(|v| v.get("list"))
        .cloned()
        .unwrap_or_default();
    let (singers, unmapped) = collection::<WireComment>(singer_list);
    if let Some(value) = unmapped {
        w.extension.insert("unmapped_singer_comments".into(), value);
    }
    Ok(Page {
        resource_id: w.childrenid,
        page: w.current_page,
        total: w.count,
        reply_total: w.comments_num,
        maximum_page: w.max_page,
        service_code: w.err_code,
        message: w.message,
        comments: w.list.into_iter().map(Into::into).collect(),
        singer_comments: singers.into_iter().map(Into::into).collect(),
        categories: w.classify_list,
        hot_words: w.hot_word_list,
        extension: w.extension,
    })
}
pub async fn list(
    client: &ApiClient,
    identity: &Identity,
    query: Query,
    resource: Resource,
    mode: u8,
) -> Result<Vec<u8>, ApiError> {
    request::encode(&fetch(client, identity, query, resource, mode).await?)
}
pub async fn fetch(
    client: &ApiClient,
    identity: &Identity,
    query: Query,
    resource: Resource,
    mode: u8,
) -> Result<Page, ApiError> {
    if query.id.is_empty() || query.page == 0 || query.page_size == 0 {
        return Err(ApiError::new("input", "评论资源或分页无效"));
    }
    let mut params = BTreeMap::from([
        ("need_show_image".into(), "1".into()),
        ("pagesize".into(), query.page_size.to_string()),
        (
            "code".into(),
            if mode == 3 {
                query.code.clone().unwrap_or_else(|| resource.code().into())
            } else {
                resource.code().into()
            },
        ),
    ]);
    let path = match mode {
        1 => {
            params.insert("page".into(), query.page.to_string());
            params.insert(
                "type_id".into(),
                query
                    .category_id
                    .ok_or_else(|| ApiError::new("input", "评论分类无效"))?
                    .to_string(),
            );
            params.insert("sort_method".into(), "2".into());
            "/mcomment/v1/cmt_classify_list"
        }
        2 => {
            params.insert("hot_word".into(), query.hot_word.unwrap_or_default());
            "/mcomment/v1/get_hot_word"
        }
        3 => {
            params.insert("tid".into(), query.thread_id.unwrap_or_default());
            if let Some(mix) = query.mix_song_id {
                params.insert("mixsongid".into(), mix.to_string());
            }
            match resource {
                Resource::Music => "/mcomment/v1/hot_replylist",
                _ => "/m.comment.service/v1/hot_replylist",
            }
        }
        _ => match resource {
            Resource::Music => "/mcomment/v1/cmtlist",
            _ => "/m.comment.service/v1/cmtlist",
        },
    };
    if mode != 1 {
        params.insert("p".into(), query.page.to_string());
    }
    if mode == 0 || mode == 3 {
        params.insert("show_classify".into(), "1".into());
        params.insert("show_hotword_list".into(), "1".into());
    }
    if matches!(resource, Resource::Music) && mode != 3 {
        params.insert("mixsongid".into(), query.id);
    } else {
        params.insert("childrenid".into(), query.id);
    }
    if matches!(resource, Resource::Music) && mode != 3 {
        params.insert("extdata".into(), "0".into());
    }
    if matches!(resource, Resource::Music) && mode == 0 {
        params.insert("ver".into(), "6".into());
    }
    if matches!(resource, Resource::Playlist) && mode == 0 {
        params.insert("content_type".into(), "0".into());
        params.insert("tag".into(), "5".into());
    }
    let bytes = client
        .request(
            identity,
            path,
            None,
            reqwest::Method::POST,
            params,
            Vec::new(),
        )
        .await?;
    convert(&bytes)
}

// This endpoint explicitly encodes absent image/reply arrays as an empty
// string. Other undecodable encodings are preserved, never a page gate.
fn collection<T: serde::de::DeserializeOwned>(
    value: serde_json::Value,
) -> (Vec<T>, Option<serde_json::Value>) {
    match value {
        serde_json::Value::Array(values) => {
            let mut items = Vec::new();
            let mut unmapped = Vec::new();
            for value in values {
                match crate::util::response::from_value(value.clone()) {
                    Ok(item) => items.push(item),
                    Err(_) => unmapped.push(value),
                }
            }
            (
                items,
                if unmapped.is_empty() {
                    None
                } else {
                    Some(serde_json::Value::Array(unmapped))
                },
            )
        }
        serde_json::Value::Null => (Vec::new(), None),
        serde_json::Value::String(s) if s.is_empty() => (Vec::new(), None),
        other => (Vec::new(), Some(other)),
    }
}
