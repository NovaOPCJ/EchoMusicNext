use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
#[derive(Default, Deserialize)]
pub struct Author {
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub author_id: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub author_name: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub sizable_avatar: String,
    #[serde(flatten)]
    pub extension: BTreeMap<String, serde_json::Value>,
}
#[derive(Serialize)]
pub struct Artist {
    pub id: Option<u64>,
    pub name: String,
    pub avatar_url: String,
    pub extension: BTreeMap<String, serde_json::Value>,
}
#[derive(Serialize)]
pub struct Source {
    pub hash: String,
    pub quality: String,
    pub codec: String,
    pub width: Option<u64>,
    pub height: Option<u64>,
    pub size_bytes: Option<u64>,
    pub bit_rate: Option<u64>,
}
#[derive(Serialize)]
pub struct Video {
    pub audio_fallback_facts: Option<AudioFallbackFacts>,
    pub audio_fallback_eligible: Option<bool>,
    pub id: Option<u64>,
    pub song_id: Option<u64>,
    pub title: String,
    pub artist: String,
    pub artists: Vec<Artist>,
    pub cover_url: String,
    pub description: String,
    pub published_at: String,
    pub duration_ms: Option<u64>,
    pub play_count: Option<u64>,
    pub collection_count: Option<u64>,
    pub download_count: Option<u64>,
    pub recommendation_code: Option<u32>,
    pub sources: Vec<Source>,
    pub encodings: Vec<Encoding>,
    pub tags: Vec<Tag>,
    pub edition: String,
    pub extension: BTreeMap<String, serde_json::Value>,
}
#[derive(Serialize)]
pub struct AudioFallbackFacts {
    pub published_code: Option<u32>,
    pub ugc_code: Option<u32>,
    pub short_code: Option<u32>,
    pub other_code: Option<u32>,
    pub status_code: Option<u32>,
}
#[derive(Serialize)]
pub struct Encoding {
    pub codec: &'static str,
    pub facts: Resolutions,
}
#[derive(Default, Deserialize, Serialize)]
pub struct Tag {
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub tag_id: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub tag_name: String,
    #[serde(flatten)]
    pub extension: BTreeMap<String, serde_json::Value>,
}
#[derive(Default, Deserialize, Serialize)]
pub struct Resolutions {
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub ld_hash: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub ld_width: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub ld_height: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub ld_filesize: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub ld_bitrate: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub sd_hash: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub sd_width: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub sd_height: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub sd_filesize: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub sd_bitrate: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub hd_hash: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub hd_width: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub hd_height: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub hd_filesize: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub hd_bitrate: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub fhd_hash: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub fhd_width: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub fhd_height: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub fhd_filesize: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub fhd_bitrate: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    pub qhd_hash: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub qhd_width: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub qhd_height: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub qhd_filesize: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    pub qhd_bitrate: Option<u64>,
    #[serde(flatten)]
    pub extension: BTreeMap<String, serde_json::Value>,
}
impl Resolutions {
    pub fn sources(&self, codec: &str) -> Vec<Source> {
        let mut out = Vec::new();
        if !self.fhd_hash.is_empty() {
            out.push(Source {
                hash: self.fhd_hash.clone(),
                quality: "fhd".into(),
                codec: codec.into(),
                width: self.fhd_width,
                height: self.fhd_height,
                size_bytes: self.fhd_filesize,
                bit_rate: self.fhd_bitrate,
            });
        }
        if !self.hd_hash.is_empty() {
            out.push(Source {
                hash: self.hd_hash.clone(),
                quality: "hd".into(),
                codec: codec.into(),
                width: self.hd_width,
                height: self.hd_height,
                size_bytes: self.hd_filesize,
                bit_rate: self.hd_bitrate,
            });
        }
        if !self.qhd_hash.is_empty() {
            out.push(Source {
                hash: self.qhd_hash.clone(),
                quality: "qhd".into(),
                codec: codec.into(),
                width: self.qhd_width,
                height: self.qhd_height,
                size_bytes: self.qhd_filesize,
                bit_rate: self.qhd_bitrate,
            });
        }
        if !self.sd_hash.is_empty() {
            out.push(Source {
                hash: self.sd_hash.clone(),
                quality: "sd".into(),
                codec: codec.into(),
                width: self.sd_width,
                height: self.sd_height,
                size_bytes: self.sd_filesize,
                bit_rate: self.sd_bitrate,
            });
        }
        if !self.ld_hash.is_empty() {
            out.push(Source {
                hash: self.ld_hash.clone(),
                quality: "ld".into(),
                codec: codec.into(),
                width: self.ld_width,
                height: self.ld_height,
                size_bytes: self.ld_filesize,
                bit_rate: self.ld_bitrate,
            });
        }
        out
    }
}
