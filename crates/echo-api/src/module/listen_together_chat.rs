//! Room chat and history, typed independently from the music catalog.
use crate::{
    domain::listen_together::{ChatMessage, ChatPage, CommandResult, Profile},
    error::ApiError,
    util::{
        listen_together as protocol,
        request::{self, ApiClient, Identity},
    },
};
use serde::Deserialize;
use serde_json::json;
use std::collections::BTreeMap;
#[derive(Deserialize)]
#[serde(tag = "action", rename_all = "snake_case", deny_unknown_fields)]
pub enum Query {
    Send {
        room_id: String,
        text: String,
        nickname: String,
        avatar_url: String,
    },
    History {
        room_id: String,
        max_id: String,
    },
}
#[derive(Default, Deserialize)]
struct Payload {
    #[serde(default)]
    list: serde_json::Value,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    is_end: Option<u32>,
}
#[derive(Deserialize)]
struct Message {
    #[serde(default, deserialize_with = "crate::util::response::text")]
    msgid: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    uid: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    addtime: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    is_online: Option<u32>,
    #[serde(
        default,
        deserialize_with = "crate::util::response::optional",
        rename = "type"
    )]
    kind: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::object")]
    message: Content,
    #[serde(flatten)]
    extension: BTreeMap<String, serde_json::Value>,
}
#[derive(Default, Deserialize)]
struct Content {
    #[serde(default, deserialize_with = "crate::util::response::text")]
    alert: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    nickname: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    img: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    msgtype: Option<u32>,
    #[serde(flatten)]
    extension: BTreeMap<String, serde_json::Value>,
}
pub fn convert(bytes: &[u8]) -> Result<ChatPage, ApiError> {
    let w: Payload = protocol::decode(protocol::data(bytes)?)?;
    // Actual empty rooms return list:{}; nonempty rooms return list:[...].
    // Preserve an unknown shape/entry separately rather than reject the page
    // or invent a display message from an opaque provider value.
    let mut unmapped_items = Vec::new();
    let raw = match w.list {
        serde_json::Value::Array(rows) => rows,
        serde_json::Value::Object(ref fields) if fields.is_empty() => Vec::new(),
        serde_json::Value::Null => Vec::new(),
        other => {
            unmapped_items.push(other);
            Vec::new()
        }
    };
    let mut items = Vec::new();
    for value in raw {
        let m: Message = match protocol::decode(value.clone()) {
            Ok(message) => message,
            Err(_) => {
                unmapped_items.push(value);
                continue;
            }
        };
        let mut extension = m.extension;
        if !m.message.extension.is_empty() {
            extension.insert(
                "message".into(),
                serde_json::to_value(m.message.extension).map_err(|_| ApiError::encoding())?,
            );
        }
        items.push(ChatMessage {
            id: m.msgid,
            author: Profile {
                id: m.uid,
                name: m.message.nickname,
                avatar_url: (!m.message.img.is_empty())
                    .then(|| crate::domain::cover_url(m.message.img)),
                ..Profile::default()
            },
            text: m.message.alert,
            sent_at_seconds: m.addtime,
            kind_code: m.kind,
            message_type_code: m.message.msgtype,
            online_code: m.is_online,
            extension,
        });
    }
    Ok(ChatPage {
        items,
        unmapped_items,
        ended: w.is_end.and_then(crate::util::numeric::boolean),
        ended_code: w.is_end,
    })
}
pub async fn execute(
    client: &ApiClient,
    identity: &Identity,
    q: Query,
) -> Result<Vec<u8>, ApiError> {
    let mut body = protocol::auth(identity)?;
    body["biz"] = json!("1009");
    let (path, history) = match q {
        Query::Send {
            room_id,
            text,
            nickname,
            avatar_url,
        } => {
            if text.trim().is_empty() || text.chars().count() > 200 {
                return Err(ApiError::new("input", "消息需为1至200个字符"));
            }
            body["groupid"] = json!(room_id);
            body["message"] =
                json!({"msgtype":801,"nickname":nickname,"img":avatar_url,"alert":text.trim()});
            ("/rmservice/v1/group/chat", false)
        }
        Query::History { room_id, max_id } => {
            body["groupid"] = json!(room_id);
            body["maxid"] = json!(max_id);
            body["pagesize"] = json!("50");
            ("/rmservice/v1/group/msg_history", true)
        }
    };
    let bytes = protocol::request(
        client,
        identity,
        path,
        reqwest::Method::POST,
        BTreeMap::new(),
        Some(body),
    )
    .await?;
    if history {
        request::encode(&convert(&bytes)?)
    } else {
        let data = protocol::data(&bytes)?;
        request::encode(&CommandResult {
            message_id: data
                .get("msgid")
                .map(|v| {
                    if let Some(s) = v.as_str() {
                        s.to_owned()
                    } else {
                        v.to_string()
                    }
                })
                .unwrap_or_default(),
            ..CommandResult::default()
        })
    }
}
