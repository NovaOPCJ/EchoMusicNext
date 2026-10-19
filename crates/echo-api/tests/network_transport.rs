//! A real loopback HTTP proxy verifies the production API builder, without any
//! upstream DTO fixture, account credential or real remote service.
use echo_api::util::request::{ApiClient, Endpoint, Identity, Origin};
use std::{
    collections::BTreeMap,
    io::{Read, Write},
    net::TcpListener,
    time::{Duration, Instant},
};

#[tokio::test]
async fn api_uses_the_host_transport_for_new_requests() {
    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    let port = listener.local_addr().unwrap().port();
    listener.set_nonblocking(true).unwrap();
    let server = std::thread::spawn(move || {
        let started = Instant::now();
        let mut stream = loop {
            match listener.accept() {
                Ok((stream, _)) => break stream,
                Err(error)
                    if error.kind() == std::io::ErrorKind::WouldBlock
                        && started.elapsed() < Duration::from_secs(5) =>
                {
                    std::thread::sleep(Duration::from_millis(5));
                }
                other => panic!("proxy did not receive the API request: {other:?}"),
            }
        };
        stream.set_nonblocking(false).unwrap();
        stream
            .set_read_timeout(Some(Duration::from_secs(2)))
            .unwrap();
        let mut request = Vec::new();
        let mut buffer = [0; 2048];
        while !request.windows(4).any(|v| v == b"\r\n\r\n") {
            let count = stream.read(&mut buffer).unwrap();
            assert!(count > 0);
            request.extend_from_slice(&buffer[..count]);
        }
        stream
            .write_all(
                b"HTTP/1.1 200 OK\r\nContent-Length: 12\r\nConnection: close\r\n\r\ntransport-ok",
            )
            .unwrap();
        String::from_utf8(request).unwrap()
    });
    let client = ApiClient::new().unwrap();
    assert!(client.set_transport_port(port));
    assert!(!client.set_transport_port(0));
    assert!(client.set_timeout_seconds(2.));
    let identity = Identity {
        userid: 0,
        token: String::new(),
        t1: String::new(),
        mid: "transport-test".into(),
        dfid: "transport-test".into(),
        uuid: "-".into(),
        guid: String::new(),
        mac: String::new(),
        device_name: String::new(),
        render_fingerprint: String::new(),
    };
    let response = client
        .request_effect_catalog(
            &identity,
            Endpoint {
                origin: Origin::EffectsCatalog,
                path: "/transport-test",
                router: None,
                method: reqwest::Method::GET,
                params: BTreeMap::new(),
                body: Vec::new(),
                headers: &[],
            },
        )
        .await
        .unwrap();
    assert_eq!(response, b"transport-ok");
    let request = server.join().unwrap();
    assert!(request.starts_with("GET http://mobilecdngz.kugou.com/transport-test?"));
    assert!(
        !request
            .to_ascii_lowercase()
            .contains("proxy-authorization:")
    );
}

#[tokio::test]
async fn mcloud_transport_connects_only_to_the_cloud_host_and_keeps_secrets_encrypted() {
    // https origins tunnel through the loopback proxy with CONNECT, so the
    // observable facts are the exact tunnel target and the TLS handshake that
    // follows; the query-shape security property is asserted directly by
    // user_cloud::transport_params' unit test.
    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    let port = listener.local_addr().unwrap().port();
    listener.set_nonblocking(true).unwrap();
    let server = std::thread::spawn(move || {
        let started = Instant::now();
        let mut stream = loop {
            match listener.accept() {
                Ok((stream, _)) => break stream,
                Err(error)
                    if error.kind() == std::io::ErrorKind::WouldBlock
                        && started.elapsed() < Duration::from_secs(5) =>
                {
                    std::thread::sleep(Duration::from_millis(5));
                }
                other => panic!("proxy did not receive the cloud request: {other:?}"),
            }
        };
        stream.set_nonblocking(false).unwrap();
        stream
            .set_read_timeout(Some(Duration::from_secs(2)))
            .unwrap();
        let mut head = Vec::new();
        let mut buffer = [0; 2048];
        while !head.windows(4).any(|v| v == b"\r\n\r\n") {
            let count = stream.read(&mut buffer).unwrap();
            assert!(count > 0);
            head.extend_from_slice(&buffer[..count]);
        }
        stream
            .write_all(b"HTTP/1.1 200 Connection established\r\n\r\n")
            .unwrap();
        // The client must now start a TLS handshake (record type 0x16).
        // BSD accepts inherit the listener's nonblocking flag, so the TLS
        // record read retries on WouldBlock with a bounded deadline.
        let mut tls = [0u8; 5];
        let mut read = 0;
        let deadline = Instant::now() + Duration::from_secs(2);
        while read < tls.len() && Instant::now() < deadline {
            match stream.read(&mut tls[read..]) {
                Ok(0) => break,
                Ok(count) => read += count,
                Err(error) if error.kind() == std::io::ErrorKind::WouldBlock => {
                    std::thread::sleep(Duration::from_millis(5));
                }
                Err(error) => panic!("TLS read failed: {error:?}"),
            }
        }
        assert_eq!(read, tls.len(), "TLS handshake missing");
        (String::from_utf8(head).unwrap(), tls)
    });
    let client = ApiClient::new().unwrap();
    assert!(client.set_transport_port(port));
    assert!(client.set_timeout_seconds(2.));
    let identity: Identity = serde_json::from_value(serde_json::json!({
        "userid": 123,
        "token": "fixture-secret",
        "t1": "fixture-refresh",
        "mid": "fixture-mid",
        "dfid": "fixture-dfid"
    }))
    .unwrap();
    let result = echo_api::module::user_cloud::execute(
        &client,
        &identity,
        echo_api::module::user_cloud::Query {
            page: 1,
            page_size: 30,
        },
    )
    .await;
    // The loopback proxy cannot complete a real TLS handshake; the request
    // must fail as a transport error, never be downgraded or retried.
    assert!(result.is_err());
    let (head, tls) = server.join().unwrap();
    assert!(
        head.starts_with("CONNECT mcloudservice.kugou.com:443 HTTP/1.1"),
        "unexpected tunnel head: {}",
        head.lines().next().unwrap_or_default()
    );
    let lower = head.to_ascii_lowercase();
    assert!(lower.contains("host: mcloudservice.kugou.com:443"));
    for excluded in [
        "fixture-secret",
        "fixture-refresh",
        "fixture-mid",
        "fixture-dfid",
    ] {
        assert!(!lower.contains(excluded), "tunnel leaked {excluded}");
    }
    assert_eq!(tls[0], 0x16, "expected a TLS handshake record");
    assert_eq!(tls[1], 0x03, "expected a TLS handshake version");
}
