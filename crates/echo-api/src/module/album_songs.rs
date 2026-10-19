//! /album/songs: audio_info.duration is already milliseconds. Each record carries
//! an album_audio_id and structured authors; never split base.author_name.
use crate::{
    domain::{ArtistRef, Page, SongSummary, cover_url},
    error::ApiError,
    util::request::{self, ApiClient, Endpoint, Identity},
};
use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
#[derive(Deserialize)]
struct Wire {
    #[serde(deserialize_with = "crate::util::response::scalar")]
    status: u32,
    #[serde(deserialize_with = "crate::util::response::scalar")]
    error_code: i64,
    data: serde_json::Value,
    #[serde(default, deserialize_with = "crate::util::response::object")]
    extra: Extra,
}
#[derive(Deserialize)]
struct Data {
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    total: Option<u64>,
    #[serde(deserialize_with = "crate::util::response::list")]
    songs: Vec<Song>,
}
#[derive(Deserialize, Default)]
struct Extra {
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    disc_cnt: Option<u32>,
}
#[derive(Deserialize)]
struct Song {
    #[serde(default, deserialize_with = "crate::util::response::object")]
    base: Base,
    #[serde(default, deserialize_with = "crate::util::response::object")]
    audio_info: Audio,
    #[serde(default, deserialize_with = "crate::util::response::object")]
    album_info: Album,
    #[serde(default, deserialize_with = "crate::util::response::list")]
    authors: Vec<Author>,
    #[serde(default, deserialize_with = "crate::util::response::object")]
    copyright: Copyright,
    #[serde(default, deserialize_with = "crate::util::response::object")]
    extend: Extend,
}
#[derive(Deserialize, Default)]
struct Base {
    #[serde(default, deserialize_with = "crate::util::response::text")]
    album_id: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    album_audio_id: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    audio_name: String,
}
#[derive(Deserialize, Default)]
struct Audio {
    #[serde(default, deserialize_with = "crate::util::response::text")]
    hash: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    duration: Option<u64>,
}
#[derive(Deserialize, Default)]
struct Album {
    #[serde(default, deserialize_with = "crate::util::response::text")]
    album_name: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    cover: String,
}
#[derive(Deserialize)]
struct Author {
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    author_id: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    author_name: String,
}
#[derive(Deserialize, Default)]
struct Copyright {
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    privilege: Option<u32>,
}
#[derive(Deserialize, Default)]
struct Extend {
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    disc: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    sort: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    has_obbligato: Option<u32>,
}
#[derive(Debug, Serialize)]
pub struct AlbumTrack {
    pub song: SongSummary,
    pub disc: Option<u32>,
    pub position: Option<u32>,
    pub has_accompaniment: Option<bool>,
    pub accompaniment_code: Option<u32>,
}
#[derive(Debug, Serialize)]
pub struct AlbumTracks {
    pub page: Page<AlbumTrack>,
    pub disc_count: Option<u32>,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Query {
    pub id: u64,
    #[serde(default = "first_page")]
    pub page: u32,
    #[serde(default = "default_page_size")]
    pub page_size: u32,
}
fn first_page() -> u32 {
    1
}
fn default_page_size() -> u32 {
    30
}
pub fn convert(bytes: &[u8], page: u32, page_size: u32) -> Result<AlbumTracks, ApiError> {
    let wire: Wire = request::decode(bytes)?;
    request::success(wire.status, wire.error_code)?;
    let data: Data =
        crate::util::response::from_value(wire.data).map_err(|_| ApiError::decode())?;
    let items = data
        .songs
        .into_iter()
        .map(|item| {
            let has_accompaniment = item
                .extend
                .has_obbligato
                .and_then(crate::util::numeric::boolean);
            Ok(AlbumTrack {
                song: SongSummary {
                    id: item.base.album_audio_id.to_string(),
                    hash: item.audio_info.hash,
                    title: item.base.audio_name,
                    artists: item
                        .authors
                        .into_iter()
                        .map(|author| ArtistRef {
                            id: author.author_id,
                            name: author.author_name,
                        })
                        .collect(),
                    album_id: item.base.album_id.to_string(),
                    album_title: Some(item.album_info.album_name),
                    duration_ms: item.audio_info.duration,
                    cover_url: cover_url(item.album_info.cover),
                    privilege: item.copyright.privilege,
                },
                disc: item.extend.disc,
                position: item.extend.sort,
                has_accompaniment,
                accompaniment_code: item.extend.has_obbligato,
            })
        })
        .collect::<Result<_, ApiError>>()?;
    Ok(AlbumTracks {
        page: Page {
            page: Some(page),
            page_size: Some(page_size),
            total: data.total,
            items,
        },
        disc_count: wire.extra.disc_cnt,
    })
}
#[derive(Serialize)]
struct Body {
    album_id: String,
    is_buy: &'static str,
    page: u32,
    pagesize: u32,
}
pub async fn execute(
    client: &ApiClient,
    identity: &Identity,
    query: Query,
) -> Result<Vec<u8>, ApiError> {
    if query.page == 0 || !(1..=100).contains(&query.page_size) {
        return Err(ApiError::new("input", "专辑分页参数无效"));
    }
    let body = request::encode(&Body {
        album_id: query.id.to_string(),
        is_buy: "",
        page: query.page,
        pagesize: query.page_size,
    })?;
    let bytes = client
        .request_endpoint(
            identity,
            Endpoint {
                origin: request::Origin::Gateway,
                path: "/v1/album_audio/lite",
                router: Some("openapi.kugou.com"),
                method: reqwest::Method::POST,
                params: BTreeMap::new(),
                body,
                headers: &[("kg-tid", "255")],
            },
        )
        .await?;
    request::encode(&convert(&bytes, query.page, query.page_size)?)
}
