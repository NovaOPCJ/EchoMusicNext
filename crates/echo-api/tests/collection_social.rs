use echo_api::module;
use serde_json::Value;
fn payload(module: &str, name: &str) -> Vec<u8> {
    let p = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("../../tests/fixtures/api")
        .join(module)
        .join(format!("{name}.json"));
    let v: Value = serde_json::from_slice(&std::fs::read(p).unwrap()).unwrap();
    serde_json::to_vec(&v["responses"][0]["payload"]).unwrap()
}
#[test]
fn collected_playlist_and_album_acknowledge_local_library_ids() {
    for (name, id, previous, version) in [("playlist", 122, 3215, 3216), ("album", 123, 3217, 3218)]
    {
        let ack = module::playlist_add::convert(&payload(
            "playlist_add",
            &format!("social-actions-{name}-add-20261019"),
        ))
        .unwrap();
        assert_eq!(ack.cloud_list_id, id);
        assert_eq!(ack.previous_total_revision, previous);
        assert_eq!(ack.total_revision, version);
        assert_eq!(ack.playlist_count, Some(47));
        let removed = module::playlist_del::convert(
            &payload(
                "playlist_del",
                &format!("social-actions-{name}-remove-20261019"),
            ),
            id,
        )
        .unwrap();
        assert_eq!(removed.cloud_list_id, id);
        assert_eq!(removed.playlist_count, Some(46));
    }
}
#[test]
fn sampled_artist_and_social_mutations() {
    assert_eq!(
        module::artist_follow::convert(&payload("artist_follow", "social-actions-follow-20261019"))
            .unwrap()["following"],
        true
    );
    assert_eq!(
        module::artist_unfollow::convert(&payload(
            "artist_unfollow",
            "social-actions-unfollow-20261019"
        ))
        .unwrap()["following"],
        false
    );
    assert!(
        !module::user_follow_del::convert(&payload("user_follow_del", "restore-original"))
            .unwrap()
            .following
    );
    assert!(
        module::user_follow_add::convert(&payload("user_follow_add", "attempt-mutual")).is_err()
    );
}
#[test]
fn real_empty_histories_and_message_ack() {
    for n in 1..=12 {
        let page = module::user_follow_message::convert(&payload(
            "user_follow_message",
            &format!("social-actions-existing-{n}-20261019"),
        ))
        .unwrap();
        assert!(page.records.is_empty());
        assert_eq!(page.ended, Some(true));
    }
    let ack = module::user_follow_chat::convert(&payload("user_follow_chat", "target")).unwrap();
    assert!(!ack.msgid.is_empty());
    assert!(!ack.tag.is_empty());
    assert_eq!(ack.addtime, Some(1792242093));
    let rejected = module::user_follow_chat::convert(&payload("user_follow_chat", "self"))
        .err()
        .unwrap();
    assert_eq!(rejected.code, Some(2001));
}
