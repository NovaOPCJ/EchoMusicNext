//! Private FM preview, replenishment and feedback share one provider endpoint.
use crate::{
    domain::{
        ArtistRef,
        discovery::{ArtistDetail, AudioResource, DiscoverySong},
    },
    error::ApiError,
    util::{
        catalog_rights,
        recommendation_song::Song,
        request::{self, ApiClient, Identity},
        response::drop_consumed_keys,
        user_protocol,
    },
};
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
use std::collections::BTreeMap;

#[derive(Clone, Copy, Default, Deserialize, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum Mode {
    #[default]
    Normal,
    Small,
    Peak,
    Radio,
}
#[derive(Clone, Copy, Default, Deserialize, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum Action {
    #[default]
    Play,
    Login,
    Garbage,
    CancelGarbage,
    ClickRed,
    CancelRed,
    Download,
    BlackSinger,
    CancelBlackSinger,
    UpdateRecommendSource,
    ChangeSongPool,
}
#[derive(Deserialize, Default)]
#[serde(deny_unknown_fields)]
pub struct Query {
    #[serde(default)]
    pub mode: Mode,
    #[serde(default)]
    pub action: Action,
    #[serde(default)]
    pub song_pool_id: u32,
    #[serde(default)]
    pub remain_songcnt: u32,
    #[serde(default)]
    pub is_overplay: bool,
    #[serde(default)]
    pub hash: String,
    #[serde(default)]
    pub songid: Option<u64>,
    #[serde(default)]
    pub playtime: Option<u64>,
    #[serde(default)]
    pub cur_mark: String,
}
#[derive(Deserialize)]
struct Wire {
    #[serde(deserialize_with = "crate::util::response::scalar")]
    status: u32,
    #[serde(default, deserialize_with = "crate::util::response::scalar")]
    error_code: i64,
    #[serde(default, deserialize_with = "crate::util::response::object")]
    data: Data,
}
#[derive(Deserialize, Default)]
struct Data {
    #[serde(default, deserialize_with = "crate::util::response::list")]
    song_list: Vec<Value>,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    cur_mark: String,
    #[serde(flatten)]
    extension: BTreeMap<String, Value>,
}
#[derive(Deserialize)]
struct FmWireSong {
    #[serde(flatten)]
    song: Song,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    mixsongid: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    author_name: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    cur_mark: String,
    #[serde(default, deserialize_with = "crate::util::response::object")]
    rec_song_info: Recommendation,
    #[serde(default, deserialize_with = "crate::util::response::list")]
    relate_goods: Vec<Good>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    climax_start_time: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    climax_end_time: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    climax_timelength: Option<u64>,
}
#[derive(Deserialize, Default)]
struct Recommendation {
    #[serde(default, deserialize_with = "crate::util::response::text")]
    rec_desc: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    rec_desc2: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    similar_desc: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    alg_path: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    cur_mark: String,
}
#[derive(Deserialize)]
struct Good {
    #[serde(default, deserialize_with = "crate::util::response::text")]
    quality: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    hash: Option<String>,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    albumname: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    privilege: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pay_type: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    fail_process: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    status: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::object")]
    info: GoodInfo,
}
#[derive(Deserialize, Default)]
struct GoodInfo {
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    filesize: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    duration: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    bitrate: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    extname: Option<String>,
}
#[derive(Serialize)]
pub struct PersonalFmSongs {
    pub songs: Vec<FmSong>,
    pub batch_mark: String,
    pub provider_extension: BTreeMap<String, Value>,
}
#[derive(Serialize)]
pub struct FmSong {
    #[serde(flatten)]
    pub song: DiscoverySong,
    pub personal_fm: FmMetadata,
}
#[derive(Serialize)]
pub struct FmMetadata {
    pub mark: String,
    pub similarity: String,
    pub algorithm: String,
    pub climax_start_ms: Option<u64>,
    pub climax_end_ms: Option<u64>,
    pub climax_duration_ms: Option<u64>,
}
pub fn convert(bytes: &[u8]) -> Result<PersonalFmSongs, ApiError> {
    let wire: Wire = request::decode(bytes)?;
    request::success(wire.status, wire.error_code)?;
    let mut songs = Vec::new();
    for mut raw in wire.data.song_list {
        let entry: FmWireSong =
            crate::util::response::from_value(raw.clone()).map_err(|_| ApiError::decode())?;
        let mut song = entry.song.convert()?;
        song.metadata.related_goods_extension = raw.get("relate_goods").cloned();
        if !entry.mixsongid.is_empty() {
            song.song.id = entry.mixsongid;
        }
        if song.song.id.is_empty() {
            song.song.id = song.song.hash.clone();
        }
        if song.song.artists.is_empty() && !entry.author_name.is_empty() {
            song.song.artists.push(ArtistRef {
                id: None,
                name: entry.author_name.clone(),
            });
            song.metadata.artists.push(ArtistDetail {
                id: None,
                name: entry.author_name,
                avatar_url: None,
                publication_code: None,
            });
        }
        if song.song.cover_url.is_empty() {
            song.song.cover_url = song.metadata.rights.cover_url.clone().unwrap_or_default();
        }
        if song.song.album_title.as_deref().is_none_or(str::is_empty) {
            song.song.album_title = entry
                .relate_goods
                .first()
                .map(|good| good.albumname.clone());
        }
        for good in entry.relate_goods {
            song.metadata.resources.push(AudioResource {
                quality: good.quality,
                hash: good.hash,
                size_bytes: good.info.filesize,
                duration_ms: good.info.duration,
                bit_rate_bps: good.info.bitrate.and_then(|b| b.checked_mul(1000)),
                extension: good.info.extname,
                privilege: good.privilege,
                payment_type: good.pay_type,
                failure_code: good.fail_process,
                download_status: good.status,
            });
        }
        // Keep unmapped provider facts without duplicating mapped catalogs or
        // persisting the tracker's ephemeral media authorization.
        drop_consumed_keys(&mut raw, "tracker_info", &["auth"]);
        drop_consumed_keys(&mut raw, "trans_param", catalog_rights::CONSUMED_KEYS);
        drop_consumed_keys(
            &mut raw,
            "rec_song_info",
            &[
                "rec_desc",
                "rec_desc2",
                "similar_desc",
                "alg_path",
                "cur_mark",
            ],
        );
        drop_consumed_keys(
            &mut raw,
            "",
            &[
                "album_audio_id",
                "mixsongid",
                "album_id",
                "hash",
                "songname",
                "album_name",
                "time_length",
                "sizable_cover",
                "privilege",
                "relate_goods",
                "singerinfo",
                "songid",
                "author_name",
                "ori_audio_name",
                "official_songname",
                "suffix_audio_name",
                "album_audio_remark",
                "filename",
                "language",
                "publish_date",
                "is_publish",
                "is_original",
                "has_accompany",
                "pay_type",
                "fail_process",
                "old_cpy",
                "hash_128",
                "hash_192",
                "hash_320",
                "hash_flac",
                "hash_ape",
                "hash_other",
                "filesize_128",
                "filesize_192",
                "filesize_320",
                "filesize_flac",
                "filesize_ape",
                "filesize_other",
                "timelength_320",
                "bitrate",
                "extname",
                "mv_hash",
                "mv_type",
                "tags",
                "ips_tags",
                "rec_copy_write",
                "rec_sub_copy_write",
                "rank_label",
                "cur_mark",
                "climax_start_time",
                "climax_end_time",
                "climax_timelength",
            ],
        );
        song.metadata.provider_extension = raw
            .as_object()
            .filter(|object| !object.is_empty())
            .map(|_| raw.clone());
        let rec = entry.rec_song_info;
        song.metadata.recommendation.headline = rec.rec_desc;
        song.metadata.recommendation.detail = rec.rec_desc2;
        songs.push(FmSong {
            song,
            personal_fm: FmMetadata {
                mark: if rec.cur_mark.is_empty() {
                    entry.cur_mark
                } else {
                    rec.cur_mark
                },
                similarity: rec.similar_desc,
                algorithm: rec.alg_path,
                climax_start_ms: entry.climax_start_time,
                climax_end_ms: entry.climax_end_time,
                climax_duration_ms: entry.climax_timelength,
            },
        });
    }
    Ok(PersonalFmSongs {
        songs,
        batch_mark: wire.data.cur_mark,
        provider_extension: wire.data.extension,
    })
}
pub fn body(identity: &Identity, query: Query, time: u64) -> Result<Value, ApiError> {
    user_protocol::require_account(identity)?;
    if query.song_pool_id > 2 {
        return Err(ApiError::new("input", "无效的 FM 歌曲范围"));
    }
    let mut body = json!({
        "appid":3116,"clienttime":time,"mid":identity.mid,"action":query.action,
        "recommend_source_locked":0,"song_pool_id":query.song_pool_id,"callerid":0,"m_type":1,
        "platform":"ios","area_code":1,"remain_songcnt":query.remain_songcnt,"clientver":11440,
        "is_overplay":u32::from(query.is_overplay),"mode":query.mode,
        "fakem":"ca981cfc583a4c37f28d2d49000013c16a0a","key":user_protocol::timestamp_key(time),
        "userid":identity.userid,"kguid":identity.userid,"token":identity.token,
    });
    for (key, value) in [("hash", query.hash), ("cur_mark", query.cur_mark)] {
        if !value.is_empty() {
            body[key] = Value::String(value);
        }
    }
    if let Some(id) = query.songid.filter(|v| *v != 0) {
        body["songid"] = id.into();
    }
    if let Some(seconds) = query.playtime.filter(|v| *v != 0) {
        body["playtime"] = seconds.into();
    }
    Ok(body)
}
pub async fn execute(
    client: &ApiClient,
    identity: &Identity,
    query: Query,
) -> Result<Vec<u8>, ApiError> {
    let body = request::encode(&body(identity, query, request::unix_milliseconds()?)?)?;
    let bytes = client
        .request(
            identity,
            "/v2/personal_recommend",
            Some("persnfm.service.kugou.com"),
            reqwest::Method::POST,
            BTreeMap::new(),
            body,
        )
        .await?;
    request::encode(&convert(&bytes)?)
}
