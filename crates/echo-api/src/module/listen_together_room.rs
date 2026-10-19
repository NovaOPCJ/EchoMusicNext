//! Native room operations. Source request order belongs to the Qt controller.
use crate::{
    domain::listen_together::{Profile, RoomSession},
    error::ApiError,
    util::{
        listen_together as protocol,
        request::{self, ApiClient, Identity},
    },
};
use serde::Deserialize;
use serde_json::{Value, json};
use std::collections::BTreeMap;
#[derive(Deserialize)]
#[serde(tag = "action", rename_all = "snake_case", deny_unknown_fields)]
pub enum Query {
    Create {
        name: String,
        privacy: u8,
        capacity: u32,
    },
    Status {
        #[serde(default)]
        room_id: String,
    },
    Join {
        room_id: String,
    },
    State {
        room_id: String,
    },
    Heartbeat {
        room_id: String,
    },
    Leave {
        room_id: String,
    },
    Dismiss {
        room_id: String,
    },
    UpdateChat {
        room_id: String,
        enabled: bool,
    },
}
#[derive(Default, Deserialize)]
struct WireSession {
    #[serde(default, deserialize_with = "crate::util::response::text")]
    groupid: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    is_owner: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    join_time: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    heartbeat_interval: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    room_state: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    max_member_num: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    owner_uid: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    nickname: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    img: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    introduction: String,
    #[serde(default, deserialize_with = "crate::util::response::object")]
    switch: Switch,
    // Session credentials deliberately never enter a domain response.
    #[serde(default, rename = "access_token")]
    _access_token: Value,
    #[serde(flatten)]
    extension: BTreeMap<String, Value>,
}
#[derive(Default, Deserialize)]
struct Switch {
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    chat: Option<u32>,
    #[serde(flatten)]
    extension: BTreeMap<String, Value>,
}
pub fn convert(bytes: &[u8]) -> Result<RoomSession, ApiError> {
    let value = protocol::data(bytes)?;
    let w: WireSession = if value.is_object() {
        protocol::decode(value)?
    } else {
        WireSession::default()
    };
    let owner = (!w.owner_uid.is_empty()).then(|| Profile {
        id: w.owner_uid,
        name: w.nickname,
        avatar_url: (!w.img.is_empty()).then(|| crate::domain::cover_url(w.img)),
        ..Profile::default()
    });
    let mut extension = w.extension;
    if !w.switch.extension.is_empty() {
        extension.insert(
            "switch".into(),
            serde_json::to_value(w.switch.extension).map_err(|_| ApiError::encoding())?,
        );
    }
    Ok(RoomSession {
        owner_code: w.is_owner,
        room_id: w.groupid,
        owner: w.is_owner.and_then(crate::util::numeric::boolean),
        joined_at_seconds: w.join_time,
        heartbeat_seconds: w.heartbeat_interval,
        active_code: w.room_state,
        capacity: w.max_member_num,
        owner_profile: owner,
        name: w.introduction,
        chat_code: w.switch.chat,
        extension,
    })
}
pub async fn execute(
    client: &ApiClient,
    identity: &Identity,
    q: Query,
) -> Result<Vec<u8>, ApiError> {
    let mut params = BTreeMap::new();
    let (path, body) = match q {
        Query::Create {
            name,
            privacy,
            capacity,
        } => {
            let mut body = protocol::auth(identity)?;
            if name.trim().is_empty() || !matches!(privacy, 1 | 2) || capacity == 0 {
                return Err(ApiError::new("input", "请填写房间名并选择有效权限"));
            }
            body["biz"] = json!("1009");
            body["introduction"] = json!(name.trim());
            body["pass_through_data"] = json!({"room_privacy":privacy,"cp_notice":1,"room_bg_content":serde_json::to_string(&json!({"bg_img":"https://youthimgbssdl.kugou.com/6e9cdcef8d163d06225d8cbeaa2f1ece.JPEG","room_bg_type":"2"})).map_err(|_|ApiError::encoding())?});
            if privacy == 1 {
                body["pass_through_data"]["global_collection_id"] = json!("");
            } else {
                body["capacity"] = json!(capacity);
            }
            ("/rmservice/v1/group/create", body)
        }
        Query::State { room_id } => {
            params.insert("biz".into(), "1009".into());
            ("/rmservice/v1/group/info", json!({"groupid":room_id}))
        }
        Query::UpdateChat { room_id, enabled } => (
            "/rmservice/v1/group/update_info",
            json!({"groupid":room_id,"biz":"1009","switch":{"chat":if enabled{1}else{2}}}),
        ),
        other => {
            let (path, room_id, join) = match other {
                Query::Status { room_id } => ("/rmservice/v1/user/get_status", room_id, false),
                Query::Join { room_id } => ("/rmservice/v1/group/join", room_id, true),
                Query::Heartbeat { room_id } => ("/rmservice/v1/group/heartbeat", room_id, false),
                Query::Leave { room_id } => ("/rmservice/v1/group/leave", room_id, false),
                Query::Dismiss { room_id } => ("/rmservice/v1/group/dismiss", room_id, false),
                _ => unreachable!(),
            };
            let mut body = protocol::auth(identity)?;
            body["biz"] = json!("1009");
            body["groupid"] = json!(room_id);
            if join {
                body["pass_through_data"] = json!({"cp_notice":1});
            }
            (path, body)
        }
    };
    let bytes = protocol::request(
        client,
        identity,
        path,
        reqwest::Method::POST,
        params,
        Some(body),
    )
    .await?;
    request::encode(&convert(&bytes)?)
}
