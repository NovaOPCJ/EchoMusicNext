use crate::{
    domain::{
        cover_url,
        discovery::{AudioResource, CatalogRights, EncodedMap, VoiceAd},
    },
    error::ApiError,
    module::privilege_lite::{PreviewRange, QualityMap},
};
use serde::Deserialize;
use std::collections::BTreeMap;

/// trans_param keys consumed by the explicit WireTrans mapping. Leftovers
/// (e.g. cid) stay available for per-endpoint provider_extension capture.
pub const CONSUMED_KEYS: &[&str] = &[
    "qualitymap",
    "classmap",
    "ipmap",
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
    "language",
];
#[derive(Deserialize, Default)]
pub struct WireTrans {
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    qualitymap: Option<QualityMap>,
    #[serde(default, deserialize_with = "crate::util::response::optional_map")]
    classmap: Option<BTreeMap<String, Option<u64>>>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    ipmap: Option<EncodedMap>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    cpy_attr0: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    cpy_map: Option<String>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    cpy_grade: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    cpy_level: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    display: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    display_rate: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pay_block_tpl: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    appid_block: Option<String>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    musicpack_advance: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    is_original: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    hash_offset: Option<PreviewRange>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    hash_multitrack: Option<String>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    union_cover: Option<String>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    ogg_128_hash: Option<String>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    ogg_128_filesize: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    ogg_320_hash: Option<String>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    ogg_320_filesize: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    songname_suffix: Option<String>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    audio_privilege: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    free_for_ad: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    free_limited: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    voice_ad: Option<WireVoiceAd>,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    language: String,
}
#[derive(Deserialize)]
struct WireVoiceAd {
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    start_ms: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    filesize: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    hash: Option<String>,
}
impl WireTrans {
    pub fn title_suffix(&self) -> Option<String> {
        self.songname_suffix.clone()
    }
    /// Audio language carried inside trans_param; empty stays unknown.
    pub fn language(&self) -> Option<String> {
        if self.language.is_empty() {
            None
        } else {
            Some(self.language.clone())
        }
    }
    pub fn convert(self) -> Result<(CatalogRights, Vec<AudioResource>), ApiError> {
        let mut resources = Vec::new();
        for (quality, hash, size) in [
            ("ogg_128", self.ogg_128_hash, self.ogg_128_filesize),
            ("ogg_320", self.ogg_320_hash, self.ogg_320_filesize),
        ] {
            if hash.is_none() && size.is_none() {
                continue;
            }
            resources.push(AudioResource {
                quality: quality.into(),
                hash,
                size_bytes: size,
                duration_ms: None,
                bit_rate_bps: None,
                extension: Some("ogg".into()),
                privilege: None,
                payment_type: None,
                failure_code: None,
                download_status: None,
            });
        }
        Ok((
            CatalogRights {
                quality_map: self.qualitymap,
                classification: self.classmap,
                ip_map: self.ipmap,
                copyright_attributes: self.cpy_attr0,
                copyright_scope: self.cpy_map,
                copyright_grade: self.cpy_grade,
                copyright_level: self.cpy_level,
                display_code: self.display,
                display_rate: self.display_rate,
                payment_template: self.pay_block_tpl,
                blocked_applications: self.appid_block,
                originality_code: self.is_original,
                advance_code: self.musicpack_advance,
                preview: self.hash_offset.filter(|value| !value.is_empty()),
                multitrack_hash: self.hash_multitrack,
                cover_url: self.union_cover.map(cover_url),
                audio_privilege: self.audio_privilege,
                free_for_ad: self.free_for_ad,
                free_limited: self.free_limited,
                voice_ad: self.voice_ad.map(|v| VoiceAd {
                    start_ms: v.start_ms,
                    size_bytes: v.filesize,
                    hash: v.hash,
                }),
            },
            resources,
        ))
    }
}
