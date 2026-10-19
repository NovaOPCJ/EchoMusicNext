//! Lyric candidate search preserves provider order. Access keys remain
//! ephemeral and stay out of queue persistence and diagnostics.
use crate::{
    error::ApiError,
    util::request::{self, ApiClient, Endpoint, Identity, Origin},
};
use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
#[derive(Deserialize)]
#[serde(untagged)]
pub enum Query {
    Hash(HashQuery),
    Keywords(KeywordQuery),
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct HashQuery {
    pub hash: String,
    pub song_id: u64,
    pub duration_ms: u64,
    pub all_candidates: bool,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct KeywordQuery {
    pub keywords: String,
    pub duration_ms: u64,
    pub all_candidates: bool,
}
impl Query {
    fn params(self) -> BTreeMap<String, String> {
        let (hash, song_id, duration, all, keyword) = match self {
            Self::Hash(q) => (
                q.hash,
                q.song_id,
                q.duration_ms,
                q.all_candidates,
                String::new(),
            ),
            Self::Keywords(q) => (
                String::new(),
                0,
                q.duration_ms,
                q.all_candidates,
                q.keywords.trim().to_owned(),
            ),
        };
        BTreeMap::from([
            ("album_audio_id".into(), song_id.to_string()),
            ("appid".into(), "3116".into()),
            ("clientver".into(), "11440".into()),
            ("duration".into(), duration.to_string()),
            ("hash".into(), hash),
            ("keyword".into(), keyword),
            ("lrctxt".into(), "1".into()),
            ("man".into(), if all { "yes" } else { "no" }.into()),
        ])
    }
}
#[derive(Deserialize)]
struct Wire {
    #[serde(deserialize_with = "crate::util::response::scalar")]
    status: u32,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    candidates: Option<Vec<WireCandidate>>,
}
#[derive(Deserialize)]
struct WireCandidate {
    #[serde(default, deserialize_with = "crate::util::response::text")]
    id: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    accesskey: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    singer: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    nickname: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    song: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    duration: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    product_from: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    language: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    krctype: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    score: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    contenttype: Option<u32>,
}
#[derive(Serialize)]
pub struct Candidate {
    pub source_id: String,
    pub access_key: String,
    pub artist: String,
    pub nickname: String,
    pub title: String,
    pub duration_ms: Option<u64>,
    pub source_label: String,
    pub language: String,
    pub word_timed: Option<bool>,
    pub score: Option<u32>,
    pub scrollable: Option<bool>,
    pub source_format_code: Option<u32>,
    pub content_type_code: Option<u32>,
}
pub fn convert(bytes: &[u8]) -> Result<Vec<Candidate>, ApiError> {
    let wire: Wire = request::decode(bytes)?;
    if wire.status == 404 && wire.candidates.as_ref().is_some_and(Vec::is_empty) {
        return Ok(Vec::new());
    }
    if wire.status != 200 {
        return Err(ApiError::new("business", "歌词搜索失败"));
    }
    let candidates = wire.candidates.unwrap_or_default();
    Ok(candidates
        .into_iter()
        .map(|c| Candidate {
            source_id: c.id,
            access_key: c.accesskey,
            artist: c.singer,
            nickname: c.nickname,
            title: c.song,
            duration_ms: c.duration,
            source_label: c.product_from,
            source_format_code: c.krctype,
            content_type_code: c.contenttype,
            language: c.language,
            word_timed: match c.krctype {
                Some(1) => Some(true),
                Some(2) => Some(false),
                _ => None,
            },
            score: c.score,
            scrollable: match c.contenttype {
                Some(0 | 1) => Some(true),
                Some(2) => Some(false),
                _ => None,
            },
        })
        .collect())
}
pub async fn execute(
    client: &ApiClient,
    identity: &Identity,
    query: Query,
) -> Result<Vec<u8>, ApiError> {
    let bytes = client
        .request_lyric_search(
            identity,
            Endpoint {
                origin: Origin::Lyrics,
                path: "/v1/search",
                router: None,
                method: reqwest::Method::GET,
                params: query.params(),
                body: vec![],
                headers: &[],
            },
        )
        .await?;
    request::encode(&convert(&bytes)?)
}

#[cfg(all(test, feature = "local-api-fixtures"))]
mod tests {
    use super::*;
    #[test]
    fn keyword_search_uses_sampled_transport_and_keeps_provider_order() {
        let query: Query = serde_json::from_str(
            r#"{"keywords":"周杰伦 - 晴天","duration_ms":269000,"all_candidates":true}"#,
        )
        .unwrap();
        let params = query.params();
        assert_eq!(params["keyword"], "周杰伦 - 晴天");
        assert_eq!(params["hash"], "");
        assert_eq!(params["album_audio_id"], "0");
        assert_eq!(params["man"], "yes");
        let best: Query = serde_json::from_str(
            r#"{"keywords":"周杰伦 - 晴天","duration_ms":269000,"all_candidates":false}"#,
        )
        .unwrap();
        assert_eq!(best.params()["man"], "no");
        assert!(
            serde_json::from_str::<Query>(
                r#"{"hash":"abc","song_id":1,"duration_ms":1,"all_candidates":true,"keywords":"x"}"#
            )
            .is_err()
        );
        for (name, count) in [
            ("title-artist", 20),
            ("title-only", 10),
            ("artist-only", 8),
            ("no-match", 0),
        ] {
            let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
                .join("../../tests/fixtures/api/search_lyric")
                .join(format!("keywords-{name}-20261016.json"));
            let sample: serde_json::Value =
                serde_json::from_slice(&std::fs::read(path).unwrap()).unwrap();
            let payload = &sample["responses"][0]["payload"];
            let rows = convert(&serde_json::to_vec(payload).unwrap()).unwrap();
            assert_eq!(rows.len(), count);
            for (row, raw) in rows.iter().zip(payload["candidates"].as_array().unwrap()) {
                assert_eq!(row.source_id, raw["id"].as_str().unwrap());
                assert_eq!(row.title, raw["song"].as_str().unwrap());
            }
        }
    }
    fn capture(name: &str) -> Vec<u8> {
        let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("../../tests/fixtures/api/search_lyric")
            .join(format!("{name}-playback-20261013.json"));
        let sample: serde_json::Value =
            serde_json::from_slice(&std::fs::read(path).unwrap()).unwrap();
        serde_json::to_vec(&sample["responses"][0]["payload"]).unwrap()
    }
    #[test]
    fn genuine_automatic_fallback_modes_distinguish_empty_from_errors() {
        for (name, count) in [
            ("hash-default", 0),
            ("hash-candidates", 0),
            ("keywords-default", 1),
            ("keywords-candidates", 20),
            ("keywords-empty-default", 0),
        ] {
            let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
                .join("../../tests/fixtures/api/search_lyric")
                .join(format!("automatic-fallback-{name}-20261016.json"));
            let sample: serde_json::Value =
                serde_json::from_slice(&std::fs::read(path).unwrap()).unwrap();
            let payload = &sample["responses"][0]["payload"];
            assert_eq!(
                convert(&serde_json::to_vec(payload).unwrap())
                    .unwrap()
                    .len(),
                count
            );
        }
        assert!(convert(br#"{"status":500,"candidates":[]}"#).is_err());
    }
    #[test]
    fn genuine_manual_catalog_has_multiple_formats_and_attribution() {
        let sample: serde_json::Value = serde_json::from_slice(include_bytes!(
            "../../../../tests/fixtures/api/search_lyric/manual-candidates-source-variants2-20261015.json")).unwrap();
        let candidates =
            convert(&serde_json::to_vec(&sample["responses"][0]["payload"]).unwrap()).unwrap();
        assert_eq!(candidates.len(), 20);
        assert!(candidates.iter().any(|c| c.scrollable == Some(false)));
        assert!(candidates.iter().any(|c| c.word_timed == Some(false)));
        assert!(candidates.iter().any(|c| !c.nickname.is_empty()));
    }
    #[test]
    fn genuine_candidates_keep_provider_order() {
        let candidates = convert(&capture("playback-song")).unwrap();
        assert!(!candidates.is_empty());
        assert_eq!(candidates[0].source_id, "137010759");
        assert!(candidates.iter().all(|c| !c.access_key.is_empty()));
    }
    #[test]
    fn genuine_absence_and_bad_parameters_are_distinct() {
        assert!(convert(&capture("not-found")).unwrap().is_empty());
        assert!(convert(&capture("missing")).is_err());
        assert!(convert(br#"{"status":200}"#).unwrap().is_empty());
        assert!(convert(br#"{"status":404}"#).is_err());
    }
}
