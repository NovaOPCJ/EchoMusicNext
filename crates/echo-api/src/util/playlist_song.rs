//! Public and cloud playlist track facts. Optional values remain unknown.
//! File and order IDs belong to playlist entries, not album-audio identities.
use crate::{
    domain::{
        cover_url,
        discovery::{ArtistDetail, AudioResource, CatalogRights, VideoReference},
    },
    error::ApiError,
    util::{catalog_rights::WireTrans, request},
};
use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
#[derive(Deserialize)]
pub struct Singer {
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub id: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub name: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    avatar: Option<String>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    publish: Option<u32>,
}
impl Singer {
    /// Cloud author rows project onto the shared singer shape. The provider
    /// author fields are facts; no extension is invented here.
    pub fn from_parts(
        id: Option<u64>,
        name: String,
        avatar: Option<String>,
        publish: Option<u32>,
    ) -> Self {
        Self {
            id,
            name,
            avatar,
            publish,
        }
    }
    pub fn detail(&self) -> ArtistDetail {
        ArtistDetail {
            id: self.id,
            name: self.name.clone(),
            avatar_url: self.avatar.clone().map(cover_url),
            publication_code: self.publish,
        }
    }
}
/// Playlist name is a transport filename. Split only the first spaced
/// separator for display and retain subsequent separators inside the title.
/// Preserve the complete original spelling in metadata.
pub fn display_title(name: &str, _artists: &[Singer]) -> String {
    let title = name.split_once(" - ").map_or(name, |(_, title)| title);
    const EXTENSIONS: [&str; 6] = [".mp3", ".flac", ".wav", ".aac", ".m4a", ".ape"];
    let lower = title.to_ascii_lowercase();
    let end = EXTENSIONS
        .iter()
        .find(|suffix| lower.ends_with(**suffix))
        .map_or(title.len(), |suffix| title.len() - suffix.len());
    title[..end].into()
}
fn filename_artist_name(name: &str, artists: &[Singer]) -> Option<String> {
    if artists.iter().any(|a| !a.name.trim().is_empty()) {
        return None;
    }
    name.split_once(" - ")
        .map(|(artist, _)| artist.trim())
        .filter(|artist| !artist.is_empty())
        .map(str::to_owned)
}
pub fn display_artists(name: &str, artists: &[Singer]) -> Vec<crate::domain::ArtistRef> {
    if let Some(name) = filename_artist_name(name, artists) {
        // A filename supplies presentation only, never a navigable identity.
        return vec![crate::domain::ArtistRef { id: None, name }];
    }
    artists
        .iter()
        .map(|a| crate::domain::ArtistRef {
            id: a.id,
            name: a.name.clone(),
        })
        .collect()
}
#[derive(Deserialize)]
pub struct Facts {
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    audio_id: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    language: Option<String>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    publish_date: Option<String>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    media_old_cpy: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    mvhash: Option<String>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    mvtype: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    mvtrack: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    size: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    bitrate: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    extname: Option<String>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    trans_param: Option<serde_json::Value>,
    #[serde(default, deserialize_with = "crate::util::response::optional_map")]
    tagmap: Option<BTreeMap<String, Option<u64>>>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    remark: Option<String>,
}
#[derive(Debug, Serialize)]
pub struct Metadata {
    pub audio_id: Option<u64>,
    pub album_details_id: Option<u64>,
    pub language: Option<String>,
    pub published_at: Option<String>,
    pub source_file_name: String,
    pub filename_artist_name: Option<String>,
    pub title_suffix: Option<String>,
    pub album_audio_remark: Option<String>,
    pub old_copyright_code: Option<u32>,
    pub payment_type: Option<u32>,
    pub failure_code: Option<u32>,
    pub publication_code: Option<u32>,
    pub has_accompaniment: Option<bool>,
    pub rights: Option<CatalogRights>,
    pub resources: Vec<AudioResource>,
    pub videos: Vec<VideoReference>,
    pub artists: Vec<ArtistDetail>,
    pub genre_map: Option<BTreeMap<String, Option<u64>>>,
    pub related_resources: Vec<RelatedResource>,
    pub downloads: Vec<Download>,
    pub accompaniment_code: Option<u32>,
}
#[derive(Debug, Deserialize, Serialize)]
pub struct RelatedResource {
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub hash: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub size: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub bitrate: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub privilege: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub level: Option<u32>,
}
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct Download {
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub hash: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub status: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub pay_type: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub fail_process: Option<u32>,
}
impl Facts {
    // Flat enrichment entry: each argument is an independent optional wire
    // fact; grouping them would only rename the same list at every caller.
    #[allow(clippy::too_many_arguments)]
    pub fn convert(
        self,
        hash: &str,
        title: &str,
        duration: Option<u64>,
        artists: &[Singer],
        related: Vec<RelatedResource>,
        downloads: &[Download],
        payment: Option<u32>,
        failure: Option<u32>,
        publication: Option<u32>,
        accompaniment: Option<u32>,
    ) -> Result<Metadata, ApiError> {
        // Decode optional enrichment field by field. An unknown container or
        // partial object must not invalidate the enclosing playlist row.
        let trans = match self.trans_param {
            None => None,
            Some(v) if v.as_object().is_some_and(|o| o.is_empty()) => None,
            Some(v) => request::decode_value::<WireTrans>(
                v,
                &[
                    "qualitymap",
                    "classmap",
                    "ipmap",
                    "attr0",
                    "attr1",
                    "bits",
                    "cpy_attr0",
                    "cpy_map",
                    "cpy_grade",
                    "cpy_level",
                    "display",
                    "display_rate",
                    "pay_block_tpl",
                    "appid_block",
                    "musicpack_advance",
                    "is_original",
                    "hash_offset",
                    "hash_multitrack",
                    "union_cover",
                    "ogg_128_hash",
                    "ogg_128_filesize",
                    "ogg_320_hash",
                    "ogg_320_filesize",
                    "songname_suffix",
                    "audio_privilege",
                    "free_for_ad",
                    "free_limited",
                    "voice_ad",
                    "start_ms",
                    "filesize",
                    "hash",
                ],
            )
            .ok(),
        };
        let title_suffix = trans.as_ref().and_then(WireTrans::title_suffix);
        let (rights, mut resources) = match trans {
            Some(v) => {
                let (r, a) = v.convert()?;
                (Some(r), a)
            }
            None => (None, Vec::new()),
        };
        let base_download = downloads.iter().find(|v| v.hash == hash);
        for item in &related {
            let quality = match item.level {
                Some(2) => "128",
                Some(4) => "320",
                Some(5) => "flac",
                Some(6) => "high",
                Some(7) => "level_7",
                _ => "unspecified",
            };
            let download = downloads.iter().find(|v| v.hash == item.hash);
            resources.push(AudioResource {
                quality: quality.into(),
                hash: Some(item.hash.clone()),
                size_bytes: item.size,
                duration_ms: if item.hash == hash { duration } else { None },
                bit_rate_bps: item.bitrate.and_then(|v| v.checked_mul(1000)),
                extension: if item.hash == hash {
                    self.extname.clone()
                } else {
                    None
                },
                privilege: item.privilege,
                payment_type: download.and_then(|v| v.pay_type),
                failure_code: download.and_then(|v| v.fail_process),
                download_status: download.and_then(|v| v.status),
            });
        }
        if self.size.is_some()
            || self.bitrate.is_some()
            || self.extname.is_some()
            || base_download.is_some()
        {
            resources.push(AudioResource {
                quality: self
                    .bitrate
                    .map(|v| v.to_string())
                    .unwrap_or_else(|| "unspecified".into()),
                hash: Some(hash.into()),
                size_bytes: self.size,
                duration_ms: duration,
                bit_rate_bps: self
                    .bitrate
                    .map(|v| v.checked_mul(1000).ok_or_else(ApiError::numeric_overflow))
                    .transpose()?,
                extension: self.extname,
                privilege: None,
                payment_type: payment,
                failure_code: failure,
                download_status: base_download.and_then(|v| v.status),
            });
        }
        let videos = if self.mvhash.is_some() || self.mvtype.is_some() || self.mvtrack.is_some() {
            vec![VideoReference {
                id: None,
                hash: self.mvhash,
                kind: self.mvtype,
                track: self.mvtrack.map(|v| v.to_string()),
                duration_ms: None,
                size_bytes: None,
            }]
        } else {
            Vec::new()
        };
        let has_accompaniment = accompaniment.and_then(crate::util::numeric::boolean);
        Ok(Metadata {
            audio_id: self.audio_id,
            album_details_id: None,
            language: self.language,
            published_at: self.publish_date,
            source_file_name: title.into(),
            filename_artist_name: filename_artist_name(title, artists),
            title_suffix,
            album_audio_remark: self.remark,
            old_copyright_code: self.media_old_cpy,
            payment_type: payment.or_else(|| base_download.and_then(|v| v.pay_type)),
            failure_code: failure.or_else(|| base_download.and_then(|v| v.fail_process)),
            publication_code: publication,
            has_accompaniment,
            rights,
            resources,
            videos,
            artists: artists.iter().map(Singer::detail).collect(),
            genre_map: self.tagmap,
            related_resources: related,
            downloads: downloads.to_vec(),
            accompaniment_code: accompaniment,
        })
    }
}
