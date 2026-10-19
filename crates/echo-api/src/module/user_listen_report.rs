//! CSCC playback events. The API runtime owns bounded, memory-only sessions.
use crate::{
    error::ApiError,
    util::{
        crypto,
        request::{self, ApiClient, Identity},
    },
};
use aes::cipher::{BlockDecryptMut, BlockEncryptMut, KeyIvInit, block_padding::Pkcs7};
use base64::{Engine, engine::general_purpose::STANDARD};
use flate2::{Compression, write::ZlibEncoder};
use md5::{Digest, Md5};
use rsa::rand_core::{OsRng, RngCore};
use serde::{Deserialize, Serialize};
use std::{
    collections::BTreeMap,
    io::Write,
    time::{Duration, Instant},
};
const APPKEY: &str = "LnT6xpN3khm36zse0QzvmgTZ3waWdRSA";
#[derive(Clone, Copy, Deserialize, PartialEq)]
#[serde(rename_all = "snake_case")]
pub enum Event {
    Start,
    End,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Query {
    pub event: Event,
    pub mixsongid: u64,
    pub duration_ms: Option<u64>,
    #[serde(default)]
    pub completed: bool,
}
#[derive(Deserialize)]
struct Wire {
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    status: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    errcode: Option<i64>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    error_code: Option<i64>,
    #[serde(default)]
    data: serde_json::Value,
}
#[derive(Deserialize)]
struct Device {
    deviceid: String,
}
#[derive(Deserialize)]
struct SessionData {
    cookie: String,
    #[serde(deserialize_with = "crate::util::response::scalar")]
    clienttime: u64,
    serverstr: String,
}
pub(crate) struct Session {
    data: SessionData,
    seed: String,
    expires: Instant,
}
#[derive(Serialize)]
pub struct Acceptance {
    pub accepted: bool,
}
fn md5(value: impl AsRef<[u8]>) -> String {
    format!("{:x}", Md5::digest(value.as_ref()))
}
fn random_hex() -> String {
    let mut bytes = [0; 16];
    OsRng.fill_bytes(&mut bytes);
    bytes.iter().map(|b| format!("{b:02x}")).collect()
}
fn envelope(bytes: &[u8]) -> Result<Wire, ApiError> {
    let wire: Wire = request::decode(bytes)?;
    let code = wire
        .error_code
        .filter(|code| *code != 0)
        .or(wire.errcode.filter(|code| *code != 0));
    if wire.status == Some(0) || code.is_some() {
        let mut error = ApiError::new("business", "音乐服务拒绝了上报");
        error.code = code;
        return Err(error);
    }
    Ok(wire)
}
pub fn convert(bytes: &[u8]) -> Result<Acceptance, ApiError> {
    envelope(bytes)?;
    Ok(Acceptance { accepted: true })
}
fn pack(bytes: &[u8]) -> Result<Vec<u8>, ApiError> {
    let mut z = ZlibEncoder::new(Vec::new(), Compression::default());
    z.write_all(bytes).map_err(|_| ApiError::encoding())?;
    z.finish().map_err(|_| ApiError::encoding())
}
async fn session(client: &ApiClient, identity: &Identity, uuid: &str) -> Result<Session, ApiError> {
    // EchoMusic's production entry omits optional screen/system overrides.
    #[derive(Serialize)]
    struct DeviceBody<'a> {
        machine: &'a str,
        mid: &'a str,
        uuid: &'a str,
        wh: [u16; 2],
    }
    let body = request::encode(&DeviceBody {
        machine: &identity.device_name,
        mid: &identity.mid,
        uuid,
        wh: [1920, 1080],
    })?;
    let time = request::unix_seconds()?.to_string();
    let sign = md5([
        format!("_t{time}appidand02pbKC7zn{{4U*ydo2M1Rir").as_bytes(),
        &body,
    ]
    .concat());
    let device = envelope(
        &client
            .request_cscc(
                "/v3/qrydid",
                reqwest::Method::POST,
                BTreeMap::from([
                    ("appid".into(), "and02".into()),
                    ("_t".into(), time),
                    ("sign".into(), sign),
                ]),
                body,
            )
            .await?,
    )?;
    let device: Device =
        crate::util::response::from_value(device.data).map_err(|_| ApiError::decode())?;
    if device.deviceid.is_empty() {
        return Err(ApiError::decode());
    }
    let seed = random_hex();
    let portrait = crypto::lite_pkcs1_base64(
        format!(
            "{uuid}\t{seed}\t{}\t{}",
            request::unix_milliseconds()?,
            device.deviceid
        )
        .as_bytes(),
    )?;
    let time = request::unix_seconds()?.to_string();
    let sign = md5(format!("_t{time}appid3116s{portrait}{APPKEY}"));
    let response = envelope(
        &client
            .request_cscc(
                "/v2/gen",
                reqwest::Method::GET,
                BTreeMap::from([
                    ("s".into(), portrait),
                    ("appid".into(), "3116".into()),
                    ("_t".into(), time),
                    ("sign".into(), sign),
                ]),
                vec![],
            )
            .await?,
    )?;
    let encoded: String =
        crate::util::response::from_value(response.data).map_err(|_| ApiError::decode())?;
    let mut encrypted = STANDARD.decode(encoded).map_err(|_| ApiError::decode())?;
    let plaintext =
        cbc::Decryptor::<aes::Aes256>::new_from_slices(seed.as_bytes(), &uuid.as_bytes()[..16])
            .map_err(|_| ApiError::decode())?
            .decrypt_padded_mut::<Pkcs7>(&mut encrypted)
            .map_err(|_| ApiError::decode())?;
    let data: SessionData = request::decode(&plaintext)?;
    if data.cookie.is_empty() || data.serverstr.is_empty() {
        return Err(ApiError::decode());
    }
    Ok(Session {
        data,
        seed,
        expires: Instant::now() + Duration::from_secs(1800),
    })
}
fn event_body(identity: &Identity, uuid: &str, query: &Query) -> Result<Vec<u8>, ApiError> {
    let song = query.mixsongid;
    let mut fields = match query.event {
        Event::Start => format!(
            "type_id=20431&action=play&fo3=3878,4320,4326,5747,5749,7229,7289,7863&spt=0&sty=手动&mixsongid={song}&source=46&ivar1=1&fo=/专辑播放页"
        ),
        Event::End => format!(
            "type_id=4&action=play&fo3=3878,4320,4326,5747,5749,7229,7289,7863&duration={}&svar3=0&svar2=0&fo=我的音乐/主态/自建歌单/RU&sty=手动&state={}&mixsongid={song}&source=46&ivar3=0&type=1&fs=1.0&ivar1=1",
            query.duration_ms.unwrap_or(0),
            if query.completed {
                "完整播放"
            } else {
                "中断播放"
            }
        ),
    };
    fields += &format!(
        "&mid={}&uuid={uuid}&ss1=1&ss2=1&sys=9&mod={}&channelid=18&ip=0.0.0.0&net=1&ver=10597&gitversion=7aa8a76&time={}&userid={}&ss3={}",
        identity.mid,
        identity.device_name,
        request::unix_milliseconds()?,
        identity.userid,
        md5(random_hex())
    );
    let header = format!(
        "4\t1\t{uuid}\t0\t3116\t10597\t18\t9\t{}\t{uuid}\t{}\t0\t000000000000000000000000000000000000\r\n{}\t10048\t0\t{}\t1\t{}\t0\t0\r\n",
        identity.device_name,
        identity.mid,
        fields.len(),
        request::unix_seconds()?,
        identity.userid
    );
    Ok([header.into_bytes(), pack(fields.as_bytes())?].concat())
}
pub async fn execute(
    client: &ApiClient,
    identity: &Identity,
    query: Query,
) -> Result<Vec<u8>, ApiError> {
    // The host persists GUID with separators. Only CSCC's UUID field strips them.
    let uuid = if identity.uuid == "-" || identity.uuid.is_empty() {
        &identity.guid
    } else {
        &identity.uuid
    }
    .replace('-', "");
    if identity.userid == 0
        || identity.token.is_empty()
        || uuid.len() != 32
        || !uuid.bytes().all(|c| c.is_ascii_alphanumeric())
        || identity.mid.is_empty()
        || query.mixsongid == 0
        || (query.event == Event::End && query.duration_ms.is_none())
    {
        return Err(ApiError::new("input", "听歌上报身份或事件不完整"));
    }
    if identity.device_name.is_empty()
        || identity.device_name.chars().count() > 128
        || [&identity.device_name, &identity.mid]
            .iter()
            .any(|s| s.chars().any(|c| c.is_control() || c == '&'))
    {
        return Err(ApiError::new("input", "听歌上报设备资料无效"));
    }
    let key = md5(request::encode(&(
        &uuid,
        &identity.mid,
        identity.userid,
        &identity.token,
        &identity.device_name,
    ))?);
    // Serializes handshake/event submission, coalesces concurrent initialization,
    // and drops cleanly on cancellation. No event is automatically retried.
    let mut sessions = client.listen_sessions.lock().await;
    sessions.retain(|_, s| s.expires > Instant::now());
    if !sessions.contains_key(&key) {
        if sessions.len() >= 256 {
            if let Some(oldest) = sessions
                .iter()
                .min_by_key(|(_, s)| s.expires)
                .map(|(k, _)| k.clone())
            {
                sessions.remove(&oldest);
            }
        }
        let value = session(client, identity, &uuid).await?;
        sessions.insert(key.clone(), value);
    }
    let session = &sessions[&key];
    let plain = event_body(identity, &uuid, &query)?;
    let cipher_key = md5(format!(
        "{uuid}{}{}{}",
        session.data.clienttime, session.seed, session.data.serverstr
    ));
    let packed = pack(&plain)?;
    let mut buffer = vec![0; (packed.len() / 16 + 1) * 16];
    buffer[..packed.len()].copy_from_slice(&packed);
    let encrypted = cbc::Encryptor::<aes::Aes256>::new_from_slices(
        cipher_key.as_bytes(),
        &uuid.as_bytes()[..16],
    )
    .map_err(|_| ApiError::encoding())?
    .encrypt_padded_mut::<Pkcs7>(&mut buffer, packed.len())
    .map_err(|_| ApiError::encoding())?
    .to_vec();
    let mut params = BTreeMap::from([
        ("cookie".into(), session.data.cookie.clone()),
        ("length".into(), plain.len().to_string()),
        ("appid".into(), "3116".into()),
        ("_t".into(), request::unix_seconds()?.to_string()),
    ]);
    let joined: String = params.iter().map(|(k, v)| format!("{k}{v}")).collect();
    params.insert(
        "sign".into(),
        md5([format!("{joined}{APPKEY}").into_bytes(), encrypted.clone()].concat()),
    );
    let response = client
        .request_cscc("/v2/post", reqwest::Method::POST, params, encrypted)
        .await
        .and_then(|bytes| convert(&bytes));
    if response.is_err() {
        sessions.remove(&key);
    }
    request::encode(&response?)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::{io::Read, net::TcpListener};
    #[test]
    #[cfg(feature = "local-api-fixtures")]
    fn genuine_handshake_and_event_samples_keep_business_failures_distinct() {
        let sample: serde_json::Value = serde_json::from_str(include_str!(
            "../../../../tests/fixtures/api/user_listen_report/zero-duration-start-20261018.json"
        ))
        .unwrap();
        let responses = sample["responses"].as_array().unwrap();
        let device: Device =
            serde_json::from_value(responses[0]["payload"]["data"].clone()).unwrap();
        assert!(!device.deviceid.is_empty());
        let data: SessionData =
            serde_json::from_value(responses[1]["payload"]["data"].clone()).unwrap();
        assert_eq!(data.clienttime, 1792306760);
        assert!(
            convert(&serde_json::to_vec(&responses[2]["payload"]).unwrap())
                .unwrap()
                .accepted
        );
        let error = convert(br#"{"status":0,"errcode":1203}"#).err().unwrap();
        assert_eq!(error.code, Some(1203));
        let error = convert(br#"{"status":0,"error_code":0,"errcode":1203}"#)
            .err()
            .unwrap();
        assert_eq!(error.code, Some(1203));
        assert_eq!(convert(br#"{"status":0}"#).err().unwrap().code, None);
    }
    #[tokio::test]
    async fn separated_guid_and_real_binary_event_use_host_transport_without_defaults() {
        let listener = TcpListener::bind("127.0.0.1:0").unwrap();
        let port = listener.local_addr().unwrap().port();
        let server = std::thread::spawn(move || {
            let (mut socket, _) = listener.accept().unwrap();
            socket
                .set_read_timeout(Some(Duration::from_secs(3)))
                .unwrap();
            let mut received = vec![];
            let mut buffer = [0; 4096];
            let (header_end, length) = loop {
                let count = socket.read(&mut buffer).unwrap();
                assert!(count > 0);
                received.extend_from_slice(&buffer[..count]);
                if let Some(end) = received.windows(4).position(|w| w == b"\r\n\r\n") {
                    let headers = String::from_utf8_lossy(&received[..end]);
                    let length = headers
                        .lines()
                        .find_map(|l| {
                            l.to_ascii_lowercase()
                                .strip_prefix("content-length:")
                                .map(|s| s.trim().parse::<usize>().unwrap())
                        })
                        .unwrap();
                    break (end + 4, length);
                }
            };
            while received.len() < header_end + length {
                let count = socket.read(&mut buffer).unwrap();
                assert!(count > 0);
                received.extend_from_slice(&buffer[..count]);
            }
            socket.write_all(b"HTTP/1.1 200 OK\r\nContent-Length: 24\r\nConnection: close\r\n\r\n{\"status\":1,\"errcode\":0}").unwrap();
            (
                String::from_utf8_lossy(&received[..header_end]).into_owned(),
                received[header_end..].to_vec(),
            )
        });
        let client = ApiClient::new().unwrap();
        client.set_transport_port(port);
        client.set_timeout_seconds(3.);
        let identity = Identity {
            userid: 1,
            token: "protocol-test-only".into(),
            t1: String::new(),
            mid: "1234567890".into(),
            dfid: "never-send-dfid".into(),
            uuid: "-".into(),
            guid: "00112233-4455-6677-8899-aabbccddeeff".into(),
            mac: String::new(),
            device_name: "EchoMusicNext".into(),
            render_fingerprint: String::new(),
        };
        let uuid = "00112233445566778899aabbccddeeff";
        let seed = "0123456789abcdef0123456789abcdef";
        let key = md5(request::encode(&(
            &uuid,
            &identity.mid,
            identity.userid,
            &identity.token,
            &identity.device_name,
        ))
        .unwrap());
        client.listen_sessions.lock().await.insert(
            key,
            Session {
                data: SessionData {
                    cookie: "private-session".into(),
                    clienttime: 123,
                    serverstr: "server-key".into(),
                },
                seed: seed.into(),
                expires: Instant::now() + Duration::from_secs(60),
            },
        );
        let result = execute(
            &client,
            &identity,
            Query {
                event: Event::End,
                mixsongid: 5288783,
                duration_ms: Some(4250),
                completed: false,
            },
        )
        .await
        .unwrap();
        assert_eq!(
            serde_json::from_slice::<serde_json::Value>(&result).unwrap()["accepted"],
            true
        );
        let (headers, mut body) = server.join().unwrap();
        assert!(headers.starts_with("POST http://d.kugou.com/v2/post?"));
        for forbidden in [
            "protocol-test-only",
            "never-send-dfid",
            "signature=",
            "x-router:",
        ] {
            assert!(!headers.contains(forbidden));
        }
        assert!(headers.contains("Android9-1070-10597-18-0-CsccPost-wifi"));
        let cipher_key = md5(format!("{uuid}123{seed}server-key"));
        let decrypted = cbc::Decryptor::<aes::Aes256>::new_from_slices(
            cipher_key.as_bytes(),
            &uuid.as_bytes()[..16],
        )
        .unwrap()
        .decrypt_padded_mut::<Pkcs7>(&mut body)
        .unwrap();
        let mut plain = vec![];
        flate2::read::ZlibDecoder::new(decrypted)
            .read_to_end(&mut plain)
            .unwrap();
        let first = plain.windows(2).position(|w| w == b"\r\n").unwrap();
        let second = plain[first + 2..]
            .windows(2)
            .position(|w| w == b"\r\n")
            .unwrap()
            + first
            + 4;
        let mut fields = String::new();
        flate2::read::ZlibDecoder::new(&plain[second..])
            .read_to_string(&mut fields)
            .unwrap();
        assert!(String::from_utf8_lossy(&plain[..first]).contains(uuid));
        assert!(
            fields.contains("duration=4250")
                && fields.contains("state=中断播放")
                && fields.contains("mixsongid=5288783")
        );
        assert!(!fields.contains('-'));
        assert!(fields.contains("mod=EchoMusicNext"));
    }
}
