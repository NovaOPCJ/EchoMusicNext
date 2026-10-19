//! Song DTOs established from actual room playlist/recent/sync/order captures.
use crate::{
    domain::{
        ArtistRef, cover_url,
        discovery::{ArtistDetail, AudioResource},
        listen_together::{Profile, RoomSong, RoomSongFacts},
    },
    error::ApiError,
};
use serde::Deserialize;
use serde_json::{Value, json};
use std::collections::BTreeMap;
#[derive(Default, Deserialize)]
pub struct WireProfile {
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub userid: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub nick_name: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub user_pic: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub enter_time: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub enter_duration: Option<u64>,
    #[serde(flatten)]
    pub extension: BTreeMap<String, Value>,
}
impl WireProfile {
    pub fn convert(self) -> Profile {
        Profile {
            id: self.userid,
            name: self.nick_name,
            avatar_url: (!self.user_pic.is_empty()).then(|| cover_url(self.user_pic)),
            entered_at_seconds: self.enter_time,
            entered_duration_seconds: self.enter_duration,
            extension: self.extension,
        }
    }
}
#[derive(Default, Deserialize)]
struct Album {
    #[serde(default, deserialize_with = "crate::util::response::text")]
    album_id: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    album_name: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    sizable_cover: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    publish_date: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    is_publish: Option<u32>,
    #[serde(flatten)]
    extension: BTreeMap<String, Value>,
}
#[derive(Deserialize)]
struct Author {
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    author_id: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    author_name: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    sizable_avatar: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    is_publish: Option<u32>,
}
#[derive(Default, Deserialize)]
struct RoomTrans {
    #[serde(flatten)]
    facts: crate::util::catalog_rights::WireTrans,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    language: String,
    #[serde(flatten)]
    extension: BTreeMap<String, Value>,
}
#[derive(Default, Deserialize)]
pub struct WireSong {
    // The already-native catalog primitives match the sampled room fields;
    // room-specific facts and nested album/authors are mapped independently.
    #[serde(default, deserialize_with = "crate::util::response::object")]
    trans_param: RoomTrans,
    #[serde(flatten)]
    catalog: crate::util::recommendation_song::Song,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    mixsongid: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    song_name: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    singername: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    duration: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::object")]
    album_info: Album,
    #[serde(default, deserialize_with = "crate::util::response::list")]
    authors: Vec<Author>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    canplay: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    genting: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    genting_source: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    anchor_play: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    buy_type: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    genting_hash: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    genting_album_audio_id: String,
    #[serde(default, deserialize_with = "crate::util::response::list")]
    user_info: Vec<WireProfile>,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    hash_high: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    hash_super: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    filesize_high: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    filesize_super: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    privilege_128: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    privilege_320: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    privilege_flac: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    privilege_high: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    privilege_super: Option<u32>,
    #[serde(flatten)]
    extension: BTreeMap<String, Value>,
}
impl WireSong {
    pub fn convert(self) -> Result<RoomSong, ApiError> {
        let mut song = self.catalog.convert()?;
        let (rights, resources) = self.trans_param.facts.convert()?;
        song.metadata.rights = rights;
        song.metadata.resources.extend(resources);
        if !self.trans_param.language.is_empty() {
            song.metadata.language = Some(self.trans_param.language);
        }
        if song.song.id.is_empty() {
            song.song.id = self.mixsongid;
        }
        if song.song.title.is_empty() {
            song.song.title = self.song_name;
        }
        if let Some(duration) = self.duration {
            song.song.duration_ms = Some(
                duration
                    .checked_mul(1000)
                    .ok_or_else(ApiError::numeric_overflow)?,
            );
        }
        if !self.album_info.album_id.is_empty() {
            song.song.album_id = self.album_info.album_id;
        }
        if !self.album_info.album_name.is_empty() {
            song.song.album_title = Some(self.album_info.album_name);
        }
        if !self.album_info.sizable_cover.is_empty() {
            song.song.cover_url = cover_url(self.album_info.sizable_cover);
        }
        if !self.album_info.publish_date.is_empty() {
            song.metadata.published_at = Some(self.album_info.publish_date);
        }
        if !self.authors.is_empty() {
            song.metadata.artists = self
                .authors
                .into_iter()
                .map(|a| ArtistDetail {
                    id: a.author_id,
                    name: a.author_name,
                    avatar_url: (!a.sizable_avatar.is_empty()).then(|| cover_url(a.sizable_avatar)),
                    publication_code: a.is_publish,
                })
                .collect();
            song.song.artists = song
                .metadata
                .artists
                .iter()
                .map(|a| ArtistRef {
                    id: a.id,
                    name: a.name.clone(),
                })
                .collect();
        } else if song.song.artists.is_empty() && !self.singername.is_empty() {
            song.song.artists = vec![ArtistRef {
                id: None,
                name: self.singername.clone(),
            }];
            song.metadata.artists = vec![ArtistDetail {
                id: None,
                name: self.singername,
                avatar_url: None,
                publication_code: None,
            }];
        }
        for resource in &mut song.metadata.resources {
            resource.privilege = match resource.quality.as_str() {
                "128" => self.privilege_128,
                "320" => self.privilege_320,
                "flac" => self.privilege_flac,
                _ => resource.privilege,
            };
            if resource.quality == "128" {
                resource.duration_ms = song.song.duration_ms;
            }
        }
        for (quality, hash, size, privilege) in [
            (
                "high",
                self.hash_high,
                self.filesize_high,
                self.privilege_high,
            ),
            (
                "super",
                self.hash_super,
                self.filesize_super,
                self.privilege_super,
            ),
        ] {
            song.metadata.resources.push(AudioResource {
                quality: quality.into(),
                hash: (!hash.is_empty()).then_some(hash),
                size_bytes: size,
                duration_ms: None,
                bit_rate_bps: None,
                extension: None,
                privilege,
                payment_type: None,
                failure_code: None,
                download_status: None,
            });
        }
        // Preserve nested album facts without inventing a publication state for
        // the song. Album is_publish belongs to the album, not to the track.
        song.metadata.provider_extension = Some(
            json!({"room_album":{"publication_code":self.album_info.is_publish,"extension":self.album_info.extension},"room_trans":self.trans_param.extension,"room_extra":self.extension}),
        );
        let room = RoomSongFacts {
            original_hash: if self.genting_hash.is_empty() {
                song.song.hash.clone()
            } else {
                self.genting_hash
            },
            original_song_id: if self.genting_album_audio_id.is_empty() {
                song.song.id.clone()
            } else {
                self.genting_album_audio_id
            },
            can_play_code: self.canplay,
            genting_code: self.genting,
            source_code: self.genting_source,
            anchor_play_code: self.anchor_play,
            purchase_code: self.buy_type,
            added_by: self
                .user_info
                .into_iter()
                .map(WireProfile::convert)
                .collect(),
        };
        Ok(RoomSong { song, room })
    }
}
