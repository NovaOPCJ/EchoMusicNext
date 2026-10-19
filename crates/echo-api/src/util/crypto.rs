use crate::error::ApiError;
use aes::cipher::{BlockDecryptMut, BlockEncryptMut, KeyIvInit, block_padding::Pkcs7};
use base64::{Engine, engine::general_purpose::STANDARD};
use md5::{Digest, Md5};
use rsa::rand_core::{OsRng, RngCore};
use rsa::{BigUint, RsaPublicKey, pkcs8::DecodePublicKey, traits::PublicKeyParts};

// RFC PEM line wrapping preserves the protocol public key DER bytes.
const LITE_PUBLIC_KEY: &str = "-----BEGIN PUBLIC KEY-----\nMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDECi0Np2UR87scwrvTr72L6oO0\n1rBbbBPriSDFPxr3Z5syug0O24QyQO8bg27+0+4kBzTBTBOZ/WWU0WryL1JSXRTX\nLgFVxtzIY41Pe7lPOgsfTCn5kZcvKhYKJesKnnJDNr5/abvTGf+rHG3YRwsCHcQ0\n8/q6ifSioBszvb3QiwIDAQAB\n-----END PUBLIC KEY-----\n";

/// CSCC handshake uses PKCS#1 v1.5, unlike the grade portrait's raw RSA.
pub fn lite_pkcs1_base64(plaintext: &[u8]) -> Result<String, ApiError> {
    let key = RsaPublicKey::from_public_key_pem(LITE_PUBLIC_KEY).map_err(|_| ApiError::decode())?;
    let bytes = key
        .encrypt(&mut OsRng, rsa::Pkcs1v15Encrypt, plaintext)
        .map_err(|_| ApiError::encoding())?;
    Ok(STANDARD.encode(bytes))
}

/// This endpoint uses raw RSA with right-side zero padding, not PKCS#1/OAEP.
/// Do not use this protocol-specific encoding for generic cryptographic purposes.
pub fn user_portrait(plaintext: &[u8]) -> Result<String, ApiError> {
    let key = RsaPublicKey::from_public_key_pem(LITE_PUBLIC_KEY).map_err(|error| {
        ApiError::new(
            "internal",
            format!("Cannot load static protocol public key: {error}"),
        )
    })?;
    if plaintext.len() > key.size() {
        return Err(ApiError::new("input", "账号凭证长度超出协议限制"));
    }
    let mut padded = vec![0u8; key.size()];
    padded[..plaintext.len()].copy_from_slice(plaintext);
    let ciphertext = BigUint::from_bytes_be(&padded)
        .modpow(key.e(), key.n())
        .to_bytes_be();
    let mut result = String::with_capacity(key.size() * 2);
    for byte in std::iter::repeat_n(0, key.size() - ciphertext.len()).chain(ciphertext) {
        use std::fmt::Write;
        write!(&mut result, "{byte:02X}")
            .map_err(|_| ApiError::new("internal", "Cannot encode portrait"))?;
    }
    Ok(result)
}

/// verify_user_info uses AES-256-CBC with an ASCII MD5 hex seed and raw RSA.
/// This differs from the SID/EDT AES-128/OAEP envelope.
pub fn verification_envelope(value: &serde_json::Value) -> Result<(String, String), ApiError> {
    let mut entropy = [0u8; 8];
    OsRng.fill_bytes(&mut entropy);
    let seed: String = entropy.iter().map(|b| format!("{b:02x}")).collect();
    parameter_envelope_with_seed(value, &seed, None)
}

/// Password login encrypts the timestamp alongside the key in its RSA envelope.
/// Both variants use the same AES protocol, but their public JSON is distinct.
pub fn login_envelope(
    value: &serde_json::Value,
    timestamp_ms: u64,
) -> Result<(String, String), ApiError> {
    let mut entropy = [0u8; 8];
    OsRng.fill_bytes(&mut entropy);
    let seed: String = entropy.iter().map(|b| format!("{b:02x}")).collect();
    parameter_envelope_with_seed(value, &seed, Some(timestamp_ms))
}

/// Preference protocol encrypts token only, with a seconds/string timestamp
/// in the PKCS#1-v1.5 portrait. This must not reuse login's raw-RSA variant.
pub fn preference_envelope(token: &str, seconds: u64) -> Result<(String, String), ApiError> {
    let mut entropy = [0u8; 8];
    OsRng.fill_bytes(&mut entropy);
    let seed: String = entropy.iter().map(|b| format!("{b:02x}")).collect();
    let key = format!("{:x}", Md5::digest(seed.as_bytes()));
    let encrypted = aes256_hex(
        &serde_json::to_vec(&serde_json::json!({"token":token}))
            .map_err(|_| ApiError::encoding())?,
        key.as_bytes(),
        &key.as_bytes()[16..],
    )?;
    let public =
        serde_json::to_vec(&serde_json::json!({"key":seed,"clienttime":seconds.to_string()}))
            .map_err(|_| ApiError::encoding())?;
    let key = RsaPublicKey::from_public_key_pem(LITE_PUBLIC_KEY).map_err(|_| ApiError::decode())?;
    let portrait = key
        .encrypt(&mut OsRng, rsa::Pkcs1v15Encrypt, &public)
        .map_err(|_| ApiError::new("internal", "Cannot encode preference portrait"))?;
    Ok((
        portrait.iter().map(|b| format!("{b:02x}")).collect(),
        encrypted,
    ))
}

/// Singer follow encrypts singer ID/token and an integer seconds timestamp in PKCS#1 RSA.
pub fn artist_follow_envelope(
    value: &serde_json::Value,
    seconds: u64,
) -> Result<(String, String), ApiError> {
    let mut entropy = [0u8; 8];
    OsRng.fill_bytes(&mut entropy);
    let seed: String = entropy.iter().map(|b| format!("{b:02x}")).collect();
    let key = format!("{:x}", Md5::digest(seed.as_bytes()));
    let encrypted = aes256_hex(
        &request_bytes(value)?,
        key.as_bytes(),
        &key.as_bytes()[16..],
    )?;
    let public = request_bytes(&serde_json::json!({"clienttime":seconds,"key":seed}))?;
    let key = RsaPublicKey::from_public_key_pem(LITE_PUBLIC_KEY).map_err(|_| ApiError::decode())?;
    let portrait = key
        .encrypt(&mut OsRng, rsa::Pkcs1v15Encrypt, &public)
        .map_err(|_| ApiError::encoding())?;
    Ok((
        portrait.iter().map(|b| format!("{b:02x}")).collect(),
        encrypted,
    ))
}
fn request_bytes(value: &serde_json::Value) -> Result<Vec<u8>, ApiError> {
    serde_json::to_vec(value).map_err(|_| ApiError::encoding())
}

fn parameter_envelope_with_seed(
    value: &serde_json::Value,
    seed: &str,
    timestamp_ms: Option<u64>,
) -> Result<(String, String), ApiError> {
    let key = format!("{:x}", Md5::digest(seed.as_bytes()));
    let plaintext = serde_json::to_vec(value).map_err(|_| ApiError::encoding())?;
    let hex = aes256_hex(&plaintext, key.as_bytes(), &key.as_bytes()[16..])?;
    let public = match timestamp_ms {
        Some(time) => serde_json::json!({"clienttime_ms":time,"key":seed}),
        None => serde_json::json!({"key":seed}),
    };
    let public = serde_json::to_vec(&public).map_err(|_| ApiError::encoding())?;
    let pk = crate::util::crypto::user_portrait(&public)?;
    Ok((
        if timestamp_ms.is_some() {
            pk
        } else {
            pk.to_lowercase()
        },
        hex,
    ))
}

/// Sensitive response seed must survive until this request's credential decryption.
/// No Debug/Serialize: the envelope and seed are never logged or persisted.
pub struct ParameterEnvelope {
    pub pk: String,
    pub params: String,
    pub seed: String,
}
pub fn login_parameter_envelope(
    value: &serde_json::Value,
    timestamp_ms: u64,
) -> Result<ParameterEnvelope, ApiError> {
    let mut entropy = [0u8; 8];
    OsRng.fill_bytes(&mut entropy);
    let seed: String = entropy.iter().map(|b| format!("{b:02x}")).collect();
    let (pk, params) = parameter_envelope_with_seed(value, &seed, Some(timestamp_ms))?;
    Ok(ParameterEnvelope { pk, params, seed })
}
pub fn aes256_hex(plaintext: &[u8], key: &[u8], iv: &[u8]) -> Result<String, ApiError> {
    let mut buffer = vec![0; (plaintext.len() / 16 + 1) * 16];
    buffer[..plaintext.len()].copy_from_slice(plaintext);
    let encrypted = cbc::Encryptor::<aes::Aes256>::new_from_slices(key, iv)
        .map_err(|_| ApiError::new("internal", "Invalid protocol AES parameters"))?
        .encrypt_padded_mut::<Pkcs7>(&mut buffer, plaintext.len())
        .map_err(|_| ApiError::new("internal", "Cannot encrypt protocol parameters"))?;
    Ok(encrypted.iter().map(|b| format!("{b:02x}")).collect())
}
fn unhex(value: &str) -> Result<Vec<u8>, ApiError> {
    if value.is_empty()
        || !value.len().is_multiple_of(2)
        || !value.bytes().all(|b| b.is_ascii_hexdigit())
    {
        return Err(ApiError::decode());
    }
    value
        .as_bytes()
        .as_chunks::<2>()
        .0
        .iter()
        .map(|b| {
            let digit = |c: u8| {
                if c.is_ascii_digit() {
                    c - b'0'
                } else {
                    c.to_ascii_lowercase() - b'a' + 10
                }
            };
            Ok((digit(b[0]) << 4) | digit(b[1]))
        })
        .collect()
}
pub fn decrypt_credentials(ciphertext: &str, seed: &str) -> Result<Vec<u8>, ApiError> {
    let key = format!("{:x}", Md5::digest(seed));
    let mut buffer = unhex(ciphertext)?;
    Ok(
        cbc::Decryptor::<aes::Aes256>::new_from_slices(key.as_bytes(), &key.as_bytes()[16..])
            .map_err(|_| ApiError::decode())?
            .decrypt_padded_mut::<Pkcs7>(&mut buffer)
            .map_err(|_| ApiError::decode())?
            .to_vec(),
    )
}
/// Login credentials can be a JSON object or a raw token string.
/// Never log decrypted material.
pub fn decode_login_credentials(
    ciphertext: &str,
    seed: &str,
) -> Result<serde_json::Value, ApiError> {
    let bytes = decrypt_credentials(ciphertext, seed)?;
    let value = match serde_json::from_slice::<serde_json::Value>(&bytes) {
        Ok(value) => value,
        Err(_) => {
            serde_json::Value::String(String::from_utf8(bytes).map_err(|_| ApiError::decode())?)
        }
    };
    if !value.is_object() && !value.is_string() {
        return Err(ApiError::decode());
    }
    Ok(value)
}
/// Shared AES-128/PKCS#1-v1.5 cloud/registration protocol. Binary and base64
/// transport are distinct; the per-request seed never implements Debug/Serialize.
pub struct PlaylistEnvelope {
    pub portrait: String,
    pub body: Vec<u8>,
    seed: String,
}
impl PlaylistEnvelope {
    pub fn new(value: &serde_json::Value, userid: u64, token: &str) -> Result<Self, ApiError> {
        let alphabet = b"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        let mut seed = String::with_capacity(6);
        while seed.len() < 6 {
            let mut entropy = [0u8; 1];
            OsRng.fill_bytes(&mut entropy);
            if entropy[0] < 248 {
                seed.push(alphabet[usize::from(entropy[0] % 62)] as char);
            }
        }
        Self::with_seed(value, userid, token, seed)
    }
    /// Registration uses a lowercase seed and deterministic JSON field order.
    pub fn registration(value: &impl serde::Serialize) -> Result<Self, ApiError> {
        let mut seed = String::with_capacity(6);
        let alphabet = b"0123456789abcdefghijklmnopqrstuvwxyz";
        while seed.len() < 6 {
            let mut entropy = [0u8; 1];
            OsRng.fill_bytes(&mut entropy);
            if entropy[0] < 252 {
                seed.push(alphabet[usize::from(entropy[0] % 36)] as char);
            }
        }
        Self::registration_with_seed(value, seed)
    }
    pub(crate) fn registration_with_seed(
        value: &impl serde::Serialize,
        seed: String,
    ) -> Result<Self, ApiError> {
        let plain = serde_json::to_vec(value).map_err(|_| ApiError::encoding())?;
        Self::with_serialized(&plain, 0, "", seed)
    }
    fn with_seed(
        value: &serde_json::Value,
        userid: u64,
        token: &str,
        seed: String,
    ) -> Result<Self, ApiError> {
        let plain = serde_json::to_vec(value).map_err(|_| ApiError::encoding())?;
        Self::with_serialized(&plain, userid, token, seed)
    }
    fn with_serialized(
        plain: &[u8],
        userid: u64,
        token: &str,
        seed: String,
    ) -> Result<Self, ApiError> {
        let hash = format!("{:x}", Md5::digest(&seed));
        let mut buffer = vec![0; (plain.len() / 16 + 1) * 16];
        buffer[..plain.len()].copy_from_slice(plain);
        let body = cbc::Encryptor::<aes::Aes128>::new_from_slices(
            &hash.as_bytes()[..16],
            &hash.as_bytes()[16..],
        )
        .map_err(|_| ApiError::decode())?
        .encrypt_padded_mut::<Pkcs7>(&mut buffer, plain.len())
        .map_err(|_| ApiError::decode())?
        .to_vec();
        let key =
            RsaPublicKey::from_public_key_pem(LITE_PUBLIC_KEY).map_err(|_| ApiError::decode())?;
        #[derive(serde::Serialize)]
        struct Portrait<'a> {
            aes: &'a str,
            uid: u64,
            token: &'a str,
        }
        let portrait = key
            .encrypt(
                &mut OsRng,
                rsa::Pkcs1v15Encrypt,
                &serde_json::to_vec(&Portrait {
                    aes: &seed,
                    uid: userid,
                    token,
                })
                .map_err(|_| ApiError::encoding())?,
            )
            .map_err(|_| ApiError::new("internal", "Cannot encode registration portrait"))?;
        Ok(Self {
            portrait: portrait.iter().map(|b| format!("{b:02x}")).collect(),
            body,
            seed,
        })
    }
    pub fn base64_body(&self) -> Vec<u8> {
        STANDARD.encode(&self.body).into_bytes()
    }
    pub fn decrypt(&self, bytes: &[u8]) -> Result<Vec<u8>, ApiError> {
        let hash = format!("{:x}", Md5::digest(&self.seed));
        let mut buffer = bytes.to_vec();
        Ok(cbc::Decryptor::<aes::Aes128>::new_from_slices(
            &hash.as_bytes()[..16],
            &hash.as_bytes()[16..],
        )
        .map_err(|_| ApiError::decode())?
        .decrypt_padded_mut::<Pkcs7>(&mut buffer)
        .map_err(|_| ApiError::decode())?
        .to_vec())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn generated_playlist_seed_matches_source_and_remains_fresh() {
        let mut seeds = std::collections::BTreeSet::new();
        for _ in 0..16 {
            let envelope =
                PlaylistEnvelope::registration(&serde_json::json!({"device":"fixture"})).unwrap();
            assert_eq!(envelope.seed.len(), 6);
            assert!(
                envelope
                    .seed
                    .bytes()
                    .all(|v| v.is_ascii_digit() || v.is_ascii_lowercase())
            );
            assert!(seeds.insert(envelope.seed.clone()));
            assert_eq!(
                envelope.decrypt(&envelope.body).unwrap(),
                br#"{"device":"fixture"}"#
            );
        }
    }
    #[test]
    fn cloud_binary_and_base64_match_independent_node_crypto_vector() {
        let envelope = PlaylistEnvelope::with_seed(
            &serde_json::json!({"listid":123,"type":0}),
            123,
            "fixture-token",
            "A1b2C3".into(),
        )
        .unwrap();
        assert_eq!(
            envelope
                .body
                .iter()
                .map(|b| format!("{b:02x}"))
                .collect::<String>(),
            "93a42d45891d448c849f3ab521eae7c6b837287d88d8bbe2426c07329c0278fb"
        );
        assert_eq!(
            envelope.base64_body(),
            b"k6QtRYkdRIyEnzq1Iernxrg3KH2I2LviQmwHMpwCePs="
        );
        assert_eq!(
            envelope.decrypt(&envelope.body).unwrap(),
            br#"{"listid":123,"type":0}"#
        );
        assert_eq!(
            crate::util::cloudlist::keyed_time(1700000000),
            "bad0ee207bf429bd91402b77fc8f7a5b"
        );
        assert_eq!(envelope.portrait.len(), 256);
        assert!(envelope.decrypt(b"{\"status\":0}").is_err());
    }
    #[test]
    fn proof_envelope_uses_distinct_aes256_hex_and_raw_rsa_encoding() {
        let (pk, ciphertext) = parameter_envelope_with_seed(
            &serde_json::json!({"code":"123456"}),
            "0123456789abcdef",
            None,
        )
        .unwrap();
        assert_eq!(
            pk,
            "8f00be7ef062ab4a56be2cce9177273c52ff9c37cd2534deb222cffd3f9d024425e520fef4fde78d48a6661f1814e13ecbb1dce7937417ae356fa21d097e11dd1a9f55b28f94715f9df841a2e8b0413974f157a1c64fc9e7c7c611d4dda8b37e58d27fc7266df185b68b2a2fbcc1e1c2b46242a68409d63d8879772bf7bb4735"
        );
        assert_eq!(
            ciphertext,
            "f6b8a061ddc6d8b1abb7222ae9d44b489e9f3f405c5a336864661bf8af172a65"
        );
    }
    #[test]
    fn refresh_fixed_parameters_and_response_decryption_match_protocol_vectors() {
        let ciphertext = aes256_hex(
            br#"{"clienttime":1700000000,"token":"fixture-token"}"#,
            b"c24f74ca2820225badc01946dba4fdf7",
            b"adc01946dba4fdf7",
        )
        .unwrap();
        assert_eq!(
            ciphertext,
            "8cf37af717d09934ee6ed8faea07ba7163e8b1a89d82e00c04249d4c4acbf7587d71b52fd7df3c4751bc5e5f1854f45a9104f198a855ff8ad71c7ef46aac7c02"
        );
        assert_eq!(
            aes256_hex(
                b"fixture-t1|1700000000123",
                b"5e4ef500e9597fe004bd09a46d8add98",
                b"04bd09a46d8add98"
            )
            .unwrap(),
            "54109b3de6f4ef96c75ee57eddfa19068e695b6a1d1edaa432cff9eaaf6f4d3d"
        );
        assert_eq!(
            decrypt_credentials(
                "f6b8a061ddc6d8b1abb7222ae9d44b489e9f3f405c5a336864661bf8af172a65",
                "0123456789abcdef"
            )
            .unwrap(),
            br#"{"code":"123456"}"#
        );
        assert!(decrypt_credentials("00", "0123456789abcdef").is_err());
        assert!(decrypt_credentials("not-hex", "0123456789abcdef").is_err());
    }
}
