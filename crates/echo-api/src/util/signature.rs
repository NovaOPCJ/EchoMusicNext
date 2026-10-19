//! Protocol constants and byte ordering verified against server/util/helper.js.
use md5::{Digest, Md5};
use std::collections::BTreeMap;

pub fn android(params: &BTreeMap<String, String>, body: &[u8]) -> String {
    let salt = b"LnT6xpN3khm36zse0QzvmgTZ3waWdRSA";
    let mut digest = Md5::new();
    digest.update(salt);
    for (key, value) in params {
        digest.update(key.as_bytes());
        digest.update(b"=");
        digest.update(value.as_bytes());
    }
    // Sign exactly the bytes sent; never serialize a POST body twice differently.
    digest.update(body);
    digest.update(salt);
    format!("{:x}", digest.finalize())
}

/// Tracker key uses its own lite salt and includes the exact request hash/mid.
pub fn tracker_key(hash: &str, mid: &str, user_id: u64) -> String {
    let mut digest = Md5::new();
    digest.update(format!(
        "{hash}185672dd44712f60bb1736df5a377e823116{mid}{user_id}"
    ));
    format!("{:x}", digest.finalize())
}

/// H5 endpoints use their own salt, retaining the same exact byte serialization.
pub fn web(params: &BTreeMap<String, String>, body: &[u8]) -> String {
    let salt = b"NVPh5oo715z5DIWAeQlhMDsWXXQV4hwt";
    let mut digest = Md5::new();
    digest.update(salt);
    for (key, value) in params {
        digest.update(key.as_bytes());
        digest.update(b"=");
        digest.update(value.as_bytes());
    }
    digest.update(body);
    digest.update(salt);
    format!("{:x}", digest.finalize())
}

/// Standard/mobile CDN salt; this endpoint ignores the host's lite identity.
pub fn album_cover(params: &BTreeMap<String, String>) -> String {
    let salt = b"OIlwieks28dk2k092lksi2UIkp";
    let mut digest = Md5::new();
    digest.update(salt);
    for (key, value) in params {
        digest.update(key.as_bytes());
        digest.update(b"=");
        digest.update(value.as_bytes());
    }
    digest.update(salt);
    format!("{:x}", digest.finalize())
}

pub fn cloud_key(hash: &str) -> String {
    let mut digest = Md5::new();
    digest.update(format!(
        "musicclound{hash}20026ebd1ac3134c880bda6a2194537843caa0162e2e7"
    ));
    format!("{:x}", digest.finalize())
}
