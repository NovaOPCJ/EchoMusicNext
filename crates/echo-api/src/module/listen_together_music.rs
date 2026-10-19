//! Music-room endpoints. Wire DTOs follow actual 2026-10-19 captures.
use crate::{
    domain::{cover_url, listen_together::*},
    error::ApiError,
    util::{
        listen_together as protocol,
        listen_together_song::{WireProfile, WireSong},
        request::{self, ApiClient, Identity},
    },
};
use serde::Deserialize;
use serde_json::{Value, json};
use std::collections::BTreeMap;
#[derive(Deserialize)]
#[serde(tag = "action", rename_all = "snake_case", deny_unknown_fields)]
pub enum Query {
    List {
        page: u32,
        page_size: u32,
        tags: String,
    },
    History {
        cursor: u64,
    },
    Detail {
        room_id: String,
    },
    Members {
        room_id: String,
        page_size: u32,
    },
    Initialize {
        room_id: String,
        audios: Vec<AudioRef>,
        progress: Option<InitialPlayback>,
    },
    Sync {
        room_id: String,
    },
    PlaybackUrl {
        room_id: String,
        audio: AudioRef,
    },
    Switch {
        room_id: String,
        audio: AudioRef,
        version: String,
        automatic: bool,
    },
    Player {
        room_id: String,
        command: PlayerCommand,
    },
    Playlist {
        room_id: String,
        cursor: Option<AudioRef>,
    },
    RecentPlaylist {
        room_id: String,
    },
    OrderSong {
        room_id: String,
        audio: AudioRef,
    },
    SongOrders {
        room_id: String,
    },
    RemoveSong {
        room_id: String,
        audio: AudioRef,
        requester_id: String,
    },
    AddSongs {
        room_id: String,
        audios: Vec<AudioRef>,
        version: String,
        requester_id: String,
        progress: Option<InitialPlayback>,
    },
}
#[derive(Deserialize)]
#[serde(tag = "kind", rename_all = "snake_case", deny_unknown_fields)]
pub enum PlayerCommand {
    Mode { mode: u8 },
    Seek { position_ms: u64 },
    Playing { playing: bool },
}
#[derive(Default, Deserialize)]
struct RoomEntry {
    #[serde(default, deserialize_with = "crate::util::response::text")]
    roomid: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    room_theme: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    member_count: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::list")]
    member_list: Vec<WireProfile>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    song_info: Option<WireSong>,
    #[serde(default, deserialize_with = "crate::util::response::object")]
    user_info: WireProfile,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    channel_id: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    channel_name: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    room_bg: String,
    #[serde(flatten)]
    extension: BTreeMap<String, Value>,
}
#[derive(Default, Deserialize)]
struct RoomList {
    #[serde(default, deserialize_with = "crate::util::response::list")]
    list: Vec<RoomEntry>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    is_end: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    last_id: Option<u64>,
}
#[derive(Default, Deserialize)]
struct Switches {
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    chat: Option<u32>,
    #[serde(flatten)]
    extension: BTreeMap<String, Value>,
}
#[derive(Default, Deserialize)]
struct Background {
    #[serde(default, deserialize_with = "crate::util::response::text")]
    bg_img: String,
    #[serde(flatten)]
    extension: BTreeMap<String, Value>,
}
#[derive(Default, Deserialize)]
struct Detail {
    #[serde(default, deserialize_with = "crate::util::response::text")]
    roomid: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    room_theme: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    userid: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    nick_name: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    user_pic: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    online_user_count: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    all_user_count: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    room_privacy: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    room_status: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::object")]
    switch: Switches,
    #[serde(default, deserialize_with = "crate::util::response::object")]
    room_bg_content: Background,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    global_collection_id: String,
    #[serde(flatten)]
    extension: BTreeMap<String, Value>,
}
#[derive(Default, Deserialize)]
struct MemberList {
    #[serde(default, deserialize_with = "crate::util::response::list")]
    member_list: Vec<WireProfile>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    total: Option<u64>,
}
#[derive(Default, Deserialize)]
struct Playlist {
    #[serde(default, deserialize_with = "crate::util::response::list")]
    songs_info: Vec<WireSong>,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    list_version: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    quantity: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    timestamp: Option<u64>,
    #[serde(flatten)]
    extension: BTreeMap<String, Value>,
}
#[derive(Default, Deserialize)]
struct Progress {
    #[serde(default, deserialize_with = "crate::util::response::text")]
    cur_song: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    album_audio_id: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    progress: Option<u64>,
    #[serde(flatten)]
    extension: BTreeMap<String, Value>,
}
#[derive(Default, Deserialize)]
struct Sync {
    #[serde(default, deserialize_with = "crate::util::response::object")]
    progress_info: Progress,
    #[serde(default, deserialize_with = "crate::util::response::list")]
    song_info: Vec<WireSong>,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    list_version: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pause: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    play_mode: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    timestamp: Option<u64>,
    #[serde(flatten)]
    extension: BTreeMap<String, Value>,
}
#[derive(Default, Deserialize)]
struct OrderList {
    #[serde(default, deserialize_with = "crate::util::response::list")]
    list: Vec<Order>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    total: Option<u64>,
}
#[derive(Deserialize)]
struct Order {
    #[serde(default, deserialize_with = "crate::util::response::object")]
    song_info: WireSong,
    #[serde(default, deserialize_with = "crate::util::response::object")]
    user_info: WireProfile,
}
#[derive(Default, Deserialize)]
struct Source {
    #[serde(default, deserialize_with = "crate::util::response::list")]
    url: Vec<String>,
    #[serde(
        default,
        deserialize_with = "crate::util::response::text",
        rename = "extName"
    )]
    extension: String,
    #[serde(
        default,
        deserialize_with = "crate::util::response::optional",
        rename = "timeLength"
    )]
    duration: Option<u64>,
    #[serde(
        default,
        deserialize_with = "crate::util::response::optional",
        rename = "fileSize"
    )]
    size: Option<u64>,
    #[serde(
        default,
        deserialize_with = "crate::util::response::optional",
        rename = "bitRate"
    )]
    bit_rate: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    volume: Option<f64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    volume_gain: Option<f64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    volume_peak: Option<f64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    status: Option<u32>,
    #[serde(flatten)]
    extra: BTreeMap<String, Value>,
}
pub fn rooms(bytes: &[u8]) -> Result<RoomPage, ApiError> {
    let w: RoomList = protocol::decode(protocol::data(bytes)?)?;
    Ok(RoomPage {
        ended: w.is_end.and_then(crate::util::numeric::boolean),
        ended_code: w.is_end,
        cursor: w.last_id,
        items: w
            .list
            .into_iter()
            .map(|r| {
                Ok(Room {
                    id: r.roomid,
                    name: r.room_theme,
                    owner: r.user_info.convert(),
                    member_count: r.member_count,
                    capacity: None,
                    privacy_code: None,
                    chat_code: None,
                    active_code: None,
                    background_url: (!r.room_bg.is_empty()).then(|| cover_url(r.room_bg)),
                    channel_id: r.channel_id,
                    channel_name: r.channel_name,
                    current_song: r.song_info.map(WireSong::convert).transpose()?,
                    members: r
                        .member_list
                        .into_iter()
                        .map(WireProfile::convert)
                        .collect(),
                    extension: r.extension,
                })
            })
            .collect::<Result<_, ApiError>>()?,
    })
}
pub fn detail(bytes: &[u8]) -> Result<Room, ApiError> {
    let w: Detail = protocol::decode(protocol::data(bytes)?)?;
    let mut extension = w.extension;
    if let Some(count) = w.all_user_count {
        extension.insert("all_user_count".into(), json!(count));
    }
    if !w.switch.extension.is_empty() {
        extension.insert(
            "switch".into(),
            serde_json::to_value(w.switch.extension).map_err(|_| ApiError::encoding())?,
        );
    }
    if !w.room_bg_content.extension.is_empty() {
        extension.insert(
            "room_bg_content".into(),
            serde_json::to_value(w.room_bg_content.extension).map_err(|_| ApiError::encoding())?,
        );
    }
    Ok(Room {
        id: w.roomid,
        name: w.room_theme,
        owner: Profile {
            id: w.userid,
            name: w.nick_name,
            avatar_url: (!w.user_pic.is_empty()).then(|| cover_url(w.user_pic)),
            ..Profile::default()
        },
        member_count: w.online_user_count.or(w.all_user_count),
        capacity: None,
        privacy_code: w.room_privacy,
        chat_code: w.switch.chat,
        active_code: w.room_status,
        background_url: (!w.room_bg_content.bg_img.is_empty())
            .then(|| cover_url(w.room_bg_content.bg_img)),
        channel_id: w.global_collection_id,
        channel_name: String::new(),
        current_song: None,
        members: Vec::new(),
        extension,
    })
}
pub fn members(bytes: &[u8]) -> Result<Members, ApiError> {
    let w: MemberList = protocol::decode(protocol::data(bytes)?)?;
    Ok(Members {
        items: w
            .member_list
            .into_iter()
            .map(WireProfile::convert)
            .collect(),
        total: w.total,
    })
}
pub fn playlist(bytes: &[u8]) -> Result<RoomPlaylist, ApiError> {
    let w: Playlist = protocol::decode(protocol::data(bytes)?)?;
    Ok(RoomPlaylist {
        items: w
            .songs_info
            .into_iter()
            .map(WireSong::convert)
            .collect::<Result<_, _>>()?,
        version: w.list_version,
        quantity: w.quantity,
        timestamp_seconds: w.timestamp,
        extension: w.extension,
    })
}
pub fn sync(bytes: &[u8]) -> Result<RoomPlayback, ApiError> {
    let w: Sync = protocol::decode(protocol::data(bytes)?)?;
    let mut extension = w.extension;
    if !w.progress_info.extension.is_empty() {
        extension.insert(
            "progress_info".into(),
            serde_json::to_value(w.progress_info.extension).map_err(|_| ApiError::encoding())?,
        );
    }
    Ok(RoomPlayback {
        provider_extension: extension,
        hash: w.progress_info.cur_song,
        song_id: w.progress_info.album_audio_id,
        position_ms: w
            .progress_info
            .progress
            .map(|v| v.checked_mul(1000).ok_or_else(ApiError::numeric_overflow))
            .transpose()?,
        pause_code: w.pause,
        play_mode_code: w.play_mode,
        timestamp_seconds: w.timestamp,
        version: w.list_version,
        songs: w
            .song_info
            .into_iter()
            .map(WireSong::convert)
            .collect::<Result<_, _>>()?,
    })
}
pub fn orders(bytes: &[u8]) -> Result<SongOrders, ApiError> {
    let w: OrderList = protocol::decode(protocol::data(bytes)?)?;
    Ok(SongOrders {
        items: w
            .list
            .into_iter()
            .map(|v| {
                Ok(SongOrder {
                    song: v.song_info.convert()?,
                    requester: v.user_info.convert(),
                })
            })
            .collect::<Result<_, ApiError>>()?,
        total: w.total,
    })
}
pub fn source(bytes: &[u8]) -> Result<AudioSource, ApiError> {
    let w: Source = protocol::decode(protocol::data(bytes)?)?;
    Ok(AudioSource {
        urls: w.url,
        duration_ms: w
            .duration
            .map(|v| v.checked_mul(1000).ok_or_else(ApiError::numeric_overflow))
            .transpose()?,
        size_bytes: w.size,
        bit_rate_bps: w.bit_rate,
        extension: w.extension,
        volume: w.volume,
        volume_gain: w.volume_gain,
        volume_peak: w.volume_peak,
        status_code: w.status,
        provider_extension: w.extra,
    })
}
fn command(bytes: &[u8]) -> Result<CommandResult, ApiError> {
    #[derive(Default, Deserialize)]
    struct ResultData {
        #[serde(default, deserialize_with = "crate::util::response::text")]
        list_version: String,
    }
    let data = protocol::data(bytes)?;
    let w: ResultData = if data.is_object() {
        protocol::decode(data)?
    } else {
        ResultData::default()
    };
    Ok(CommandResult {
        version: w.list_version,
        ..CommandResult::default()
    })
}
#[derive(Clone, Copy)]
enum ResultKind {
    Rooms,
    Detail,
    Members,
    Playlist,
    Sync,
    Orders,
    Source,
    Command,
}
pub async fn execute(
    client: &ApiClient,
    identity: &Identity,
    q: Query,
) -> Result<Vec<u8>, ApiError> {
    use ResultKind as K;
    use reqwest::Method;
    let (path, method, params, body, kind) = match q {
        Query::List {
            page,
            page_size,
            tags,
        } => (
            "/youth/v2/genting/recommend",
            Method::POST,
            BTreeMap::from([
                ("page".into(), page.saturating_sub(1).to_string()),
                ("pagesize".into(), page_size.to_string()),
                ("loop_pick".into(), "0".into()),
                ("tags".into(), tags),
                ("room_biz".into(), "1009".into()),
            ]),
            Some(json!({"data":[]})),
            K::Rooms,
        ),
        Query::History { cursor } => (
            "/youth/v1/genting/history",
            Method::GET,
            BTreeMap::from([("last_id".into(), cursor.to_string())]),
            None,
            K::Rooms,
        ),
        Query::Detail { room_id } => {
            let mut p = protocol::params(&room_id);
            p.insert("biz".into(), "1009".into());
            (
                "/youth/v1/genting/get_musicroom_info",
                Method::GET,
                p,
                None,
                K::Detail,
            )
        }
        Query::Members { room_id, page_size } => {
            let mut p = protocol::params(&room_id);
            p.extend([
                ("page".into(), "1".into()),
                ("pagesize".into(), page_size.to_string()),
                ("apiver".into(), "3".into()),
            ]);
            (
                "/youth/v1/genting/get_musicroom_member",
                Method::GET,
                p,
                None,
                K::Members,
            )
        }
        Query::Initialize {
            room_id,
            audios,
            progress,
        } => {
            let mut b = json!({"sendall":1,"audios":audios.iter().take(50).map(protocol::audio).collect::<Vec<_>>()});
            if let Some(p) = progress {
                b["progress_info"] = protocol::progress(&p);
            }
            (
                "/youth/v1/genting/init_musicroom",
                Method::POST,
                protocol::params(&room_id),
                Some(b),
                K::Command,
            )
        }
        Query::Sync { room_id } => {
            let mut p = protocol::params(&room_id);
            p.insert("frm".into(), "2".into());
            (
                "/youth/v1/genting/music_sync_player",
                Method::POST,
                p,
                Some(json!({})),
                K::Sync,
            )
        }
        Query::PlaybackUrl { room_id, audio } => {
            let mut b = protocol::auth(identity)?;
            b["roomid"] = json!(room_id);
            b["audio"] = json!({"hash":audio.hash,"mixsongid":audio.song_id});
            (
                "/youth/v1/genting/music_reqcmd",
                Method::POST,
                BTreeMap::from([
                    ("page_id".into(), "147780134".into()),
                    ("ppage_id".into(), "356753938".into()),
                ]),
                Some(b),
                K::Source,
            )
        }
        Query::Switch {
            room_id,
            audio,
            version,
            automatic,
        } => (
            "/youth/v1/genting/music_sw",
            Method::POST,
            protocol::params(&room_id),
            Some(
                json!({"act_type":1,"list_version":version,"is_auto":if automatic{"1"}else{"0"},"audio":{"hash":audio.hash,"mixsongid":audio.song_id}}),
            ),
            K::Command,
        ),
        Query::Player { room_id, command } => {
            let b = match command {
                PlayerCommand::Mode { mode } => json!({"action":1,"play_mode":mode}),
                PlayerCommand::Seek { position_ms } => {
                    json!({"action":2,"progress":position_ms/1000})
                }
                PlayerCommand::Playing { playing } => {
                    json!({"action":3,"pause":if playing{"1"}else{"2"}})
                }
            };
            (
                "/youth/v1/genting/music_player_opr",
                Method::POST,
                protocol::params(&room_id),
                Some(b),
                K::Command,
            )
        }
        Query::Playlist { room_id, cursor } => {
            let mut b = json!({"pagesize":50});
            if let Some(a) = cursor {
                b["audio"] = json!({"hash":a.hash,"mixsongid":a.song_id});
            }
            (
                "/youth/v1/genting/music_fetch_list",
                Method::POST,
                protocol::params(&room_id),
                Some(b),
                K::Playlist,
            )
        }
        // This endpoint signs an empty body, not the JSON object {}.
        Query::RecentPlaylist { room_id } => (
            "/youth/v1/genting/music_recent_list",
            Method::POST,
            protocol::params(&room_id),
            None,
            K::Playlist,
        ),
        Query::OrderSong { room_id, audio } => (
            "/youth/v1/genting/order_song",
            Method::POST,
            protocol::params(&room_id),
            Some(json!({"hash":audio.hash,"mixsongid":audio.song_id})),
            K::Command,
        ),
        Query::SongOrders { room_id } => (
            "/youth/v1/genting/song_order_list",
            Method::GET,
            protocol::params(&room_id),
            None,
            K::Orders,
        ),
        Query::RemoveSong {
            room_id,
            audio,
            requester_id,
        } => (
            "/youth/v1/genting/remove_song",
            Method::POST,
            protocol::params(&room_id),
            Some(json!({"hash":audio.hash,"mixsongid":audio.song_id,"order_userid":requester_id})),
            K::Command,
        ),
        Query::AddSongs {
            room_id,
            audios,
            version,
            requester_id,
            progress,
        } => {
            let mut p = protocol::params(&room_id);
            let mut b = json!({"action":if requester_id.is_empty(){1}else{4},"list_version":version,"sendall":1,"audios":audios.iter().take(50).map(protocol::audio).collect::<Vec<_>>()});
            if !requester_id.is_empty() {
                p.insert("order_userid".into(), requester_id);
                p.insert("source".into(), "1".into());
            }
            if let Some(v) = progress {
                b["progress_info"] = protocol::progress(&v);
            }
            (
                "/youth/v1/genting/music_add",
                Method::POST,
                p,
                Some(b),
                K::Command,
            )
        }
    };
    let bytes = protocol::request(client, identity, path, method, params, body).await?;
    match kind {
        K::Rooms => request::encode(&rooms(&bytes)?),
        K::Detail => request::encode(&detail(&bytes)?),
        K::Members => request::encode(&members(&bytes)?),
        K::Playlist => request::encode(&playlist(&bytes)?),
        K::Sync => request::encode(&sync(&bytes)?),
        K::Orders => request::encode(&orders(&bytes)?),
        K::Source => request::encode(&source(&bytes)?),
        K::Command => request::encode(&command(&bytes)?),
    }
}
