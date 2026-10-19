//! SID/EDT protocol fingerprints are generated locally, not issued credentials.
//! Do not log fingerprint material.
use crate::{error::ApiError, util::request::Identity};
use aes::cipher::{BlockEncryptMut, KeyIvInit, block_padding::Pkcs7};
use base64::{Engine, engine::general_purpose::STANDARD};
use md5::{Digest, Md5};
use rsa::{
    Oaep, RsaPublicKey,
    pkcs8::DecodePublicKey,
    rand_core::{OsRng, RngCore},
};

fn random_between(min: u32, max: u32) -> u32 {
    min + OsRng.next_u32() % (max - min + 1)
}
fn jitter(scale: f64) -> f64 {
    (f64::from(OsRng.next_u32()) / f64::from(u32::MAX) - 0.5) * scale
}
fn aes_cbc(plaintext: &[u8], key: &[u8], iv: &[u8]) -> Result<Vec<u8>, ApiError> {
    let mut buffer = vec![0; (plaintext.len() / 16 + 1) * 16];
    buffer[..plaintext.len()].copy_from_slice(plaintext);
    let cipher = cbc::Encryptor::<aes::Aes128>::new_from_slices(key, iv)
        .map_err(|_| ApiError::new("internal", "Invalid fingerprint encryption parameters"))?;
    Ok(cipher
        .encrypt_padded_mut::<Pkcs7>(&mut buffer, plaintext.len())
        .map_err(|_| ApiError::new("internal", "Cannot encrypt fingerprint"))?
        .to_vec())
}

pub struct Fingerprint {
    pub sid: String,
    pub edt: String,
}
pub fn generate(identity: &Identity) -> Result<Fingerprint, ApiError> {
    let sentinel = u32::MAX - random_between(0, 19);
    let mut events = vec![format!("5,0,0:5,{sentinel},0:5,0,0:5,{sentinel},0")];
    let mut elapsed = random_between(5, 20);
    let mut index = 0;
    events.push(format!(
        "6,{elapsed},{index},750,500:6,{sentinel},{index},750,500"
    ));
    index += 1;
    for _ in 0..3 {
        elapsed += random_between(80, 600);
        events.push(format!("5,{elapsed},{index}:5,{sentinel},{index}"));
        index += 1;
    }
    let points = random_between(30, 60);
    let sx = f64::from(random_between(200, 600));
    let sy = f64::from(random_between(200, 500));
    let ex = f64::from(random_between(500, 700));
    let ey = f64::from(random_between(80, 150));
    let c1 = (
        sx + (ex - sx) * 0.3 + jitter(160.0),
        sy + (ey - sy) * 0.2 + jitter(120.0),
    );
    let c2 = (
        sx + (ex - sx) * 0.7 + jitter(120.0),
        sy + (ey - sy) * 0.8 + jitter(80.0),
    );
    for i in 0..=points {
        let t = f64::from(i) / f64::from(points);
        let u = 1.0 - t;
        let scale = (3.0 - t * 2.5).max(0.5);
        let x = (u * u * u * sx
            + 3.0 * u * u * t * c1.0
            + 3.0 * u * t * t * c2.0
            + t * t * t * ex
            + jitter(scale))
        .round();
        let y = (u * u * u * sy
            + 3.0 * u * u * t * c1.1
            + 3.0 * u * t * t * c2.1
            + t * t * t * ey
            + jitter(scale))
        .round();
        elapsed += random_between(8, 50);
        let subindex = i % 2;
        events.push(format!(
            "3,{elapsed},{subindex},{x},{y}:3,{sentinel},{subindex},{x},{y}"
        ));
        if i > 0 && i % 12 == 0 {
            elapsed += random_between(20, 60);
            events.push(format!("5,{elapsed},{index}:5,{sentinel},{index}"));
            index += 1;
        }
    }
    elapsed += random_between(5, 30);
    events.push(format!(
        "3,{elapsed},1,{},{}:3,{sentinel},1,{ex},{ey}",
        (ex + jitter(10.0)).round(),
        (ey + jitter(10.0)).round()
    ));
    let fingerprint = if identity.render_fingerprint.is_empty() {
        OsRng.next_u64().to_string()
    } else {
        identity.render_fingerprint.clone()
    };
    let now = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map_err(|_| ApiError::new("clock", "系统时钟无效"))?
        .as_millis();
    let plaintext = format!(
        "mid={};userid={};dfid={};webgl={fingerprint};webdriver=0;ts={now};data={}",
        identity.mid,
        identity.userid,
        identity.dfid,
        events.join(":")
    );
    let mut entropy = [0u8; 16];
    OsRng.fill_bytes(&mut entropy);
    let key = format!("{:x}", Md5::digest(entropy));
    let key = &key.as_bytes()[..16];
    let edt = STANDARD.encode(aes_cbc(plaintext.as_bytes(), key, b"kugousecurity123")?);
    let public_key = RsaPublicKey::from_public_key_pem(include_str!("verification-public-key.pem"))
        .map_err(|_| ApiError::new("internal", "Cannot load verification public key"))?;
    let sid = STANDARD.encode(
        public_key
            .encrypt(&mut OsRng, Oaep::new::<sha2::Sha256>(), key)
            .map_err(|_| ApiError::new("internal", "Cannot encrypt fingerprint key"))?,
    );
    Ok(Fingerprint { sid, edt })
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn fingerprint_aes_matches_legacy_cryptojs_protocol_golden() {
        let encrypted = aes_cbc(
            b"native-fingerprint-golden",
            b"0123456789abcdef",
            b"kugousecurity123",
        )
        .unwrap();
        assert_eq!(
            STANDARD.encode(encrypted),
            "KRTM4dgOPoV8Yj9lLvQp8wNMJcGgTTx6ruq1m5hHyno="
        );
    }
}
