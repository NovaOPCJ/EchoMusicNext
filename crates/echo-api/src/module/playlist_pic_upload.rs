//! The live image endpoint is account-free HTTP; its HTTPS certificate does not
//! match this hostname. This is the observed upload protocol, not a TLS fallback.
//! Qt passes its local calendar day (the old checksum uses local time, not UTC).
use crate::{
    error::ApiError,
    util::request::{self, ApiClient},
};
use base64::{Engine, engine::general_purpose::STANDARD};
use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;

pub const MAX_PNG_BYTES: usize = 8 * 1024 * 1024;
#[derive(Deserialize)]
#[serde(from = "JsonQuery")]
pub struct Query {
    image: Image,
    local_date: String,
}
enum Image {
    Base64(String),
    Png(Vec<u8>),
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct JsonQuery {
    pub png_base64: String,
    pub local_date: String,
}
impl From<JsonQuery> for Query {
    fn from(value: JsonQuery) -> Self {
        Self {
            image: Image::Base64(value.png_base64),
            local_date: value.local_date,
        }
    }
}
impl Query {
    /// Binary C boundary owns one bounded copy; no base64/JSON image work in Qt.
    pub fn from_png(png: Vec<u8>, local_date: u32) -> Self {
        Self {
            image: Image::Png(png),
            local_date: format!("{local_date:08}"),
        }
    }
}
#[derive(Deserialize)]
struct Wire {
    #[serde(rename = "IsSuccess")]
    #[serde(deserialize_with = "crate::util::response::scalar")]
    success: bool,
    #[serde(rename = "FileName")]
    #[serde(default, deserialize_with = "crate::util::response::text")]
    file_name: String,
}
#[derive(Debug, Serialize)]
pub struct UploadedCover {
    pub file_name: String,
    pub reference: String,
}
pub fn valid_file_name(name: &str) -> bool {
    name.len() <= 255
        && name.strip_suffix(".png").is_some_and(|base| {
            !base.is_empty()
                && base
                    .bytes()
                    .all(|b| b.is_ascii_alphanumeric() || b == b'_' || b == b'-')
        })
}
pub fn convert(bytes: &[u8]) -> Result<UploadedCover, ApiError> {
    let wire: Wire = request::decode(bytes)?;
    if !wire.success {
        return Err(ApiError::new("business", "图片服务拒绝了封面上传"));
    }
    Ok(UploadedCover {
        reference: format!("custom/{}", wire.file_name),
        file_name: wire.file_name,
    })
}
pub use crate::util::image_upload::checksum;
pub async fn execute(client: &ApiClient, query: Query) -> Result<Vec<u8>, ApiError> {
    let key = checksum(&query.local_date)?;
    let png = match query.image {
        Image::Png(png) => png,
        Image::Base64(encoded) => {
            if encoded.len() > MAX_PNG_BYTES.div_ceil(3) * 4 {
                return Err(ApiError::new("input", "封面 PNG 不能超过 8 MB"));
            }
            STANDARD
                .decode(encoded)
                .map_err(|_| ApiError::new("input", "封面 PNG 编码无效"))?
        }
    };
    // Framing only, not image decoding on the GUI or a second bitmap pipeline.
    // In particular a truncated signature never starts an HTTP upload.
    if png.len() > MAX_PNG_BYTES
        || png.len() < 33
        || !png.starts_with(b"\x89PNG\r\n\x1a\n")
        || png[8..12] != 13u32.to_be_bytes()
        || &png[12..16] != b"IHDR"
    {
        return Err(ApiError::new("input", "请选择有效的 PNG 封面"));
    }
    let params = BTreeMap::from([
        ("type".into(), "custom".into()),
        ("extendName".into(), ".png".into()),
        ("md5".into(), key),
        ("jsonResponse".into(), "1".into()),
    ]);
    let response = client.upload_playlist_png(png, params).await?;
    request::encode(&convert(&response.bytes)?)
}
