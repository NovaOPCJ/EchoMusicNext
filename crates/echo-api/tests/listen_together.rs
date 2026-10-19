//! Actual capture regression; synthetic mutations below test optional-field handling only.
use echo_api::{
    module::{
        listen_together_chat as chat, listen_together_music as music, listen_together_room as room,
    },
    util::listen_together,
};
use serde_json::{Value, json};
fn capture(name: &str) -> Vec<u8> {
    let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("../../tests/fixtures/api/listen_together")
        .join(format!("{name}.json"));
    let value: Value = serde_json::from_slice(&std::fs::read(path).unwrap()).unwrap();
    serde_json::to_vec(&value["body"]).unwrap()
}
#[test]
fn actual_rooms_and_nested_song_identity() {
    let list = music::rooms(&capture("music-list")).unwrap();
    assert_eq!(list.items.len(), 4);
    assert_eq!(list.ended, Some(false));
    let first = &list.items[0];
    assert_eq!(first.id, "1564262145997273034");
    assert_eq!(first.owner.id, "1356704987");
    let song = first.current_song.as_ref().unwrap();
    assert_eq!(song.song.id, "878751506");
    assert_eq!(song.song.title, "Woodland Caribou");
    assert_eq!(song.song.album_id, "189879325");
    assert_eq!(song.song.artists[0].id, Some(229704));
    assert_eq!(song.song.artists[0].name, "Panopticon");
}
#[test]
fn actual_room_session_never_exports_room_credential() {
    let created = room::convert(&capture("room-create")).unwrap();
    assert_eq!(created.room_id, "1560000000000000001");
    assert_eq!(created.heartbeat_seconds, Some(60));
    assert!(
        !serde_json::to_string(&created)
            .unwrap()
            .contains("access_token")
    );
    let state = room::convert(&capture("room-state")).unwrap();
    assert_eq!(state.active_code, Some(1));
    assert_eq!(state.capacity, Some(5));
    assert_eq!(state.owner_profile.unwrap().id, "900001");
    let detail = music::detail(&capture("music-detail")).unwrap();
    assert_eq!(detail.privacy_code, Some(2));
    assert_eq!(detail.chat_code, Some(1));
    assert_eq!(detail.owner.id, "900001");
    let members = music::members(&capture("music-members-after")).unwrap();
    assert_eq!(members.total, Some(1));
    assert_eq!(members.items.len(), 1);
    assert_eq!(members.items[0].id, "900001");
    assert_eq!(members.items[0].entered_duration_seconds, Some(0));
    let status = room::convert(&capture("status-after-cleanup")).unwrap();
    assert!(status.room_id.is_empty());
    assert_eq!(status.owner, Some(false));
}
#[test]
fn actual_playlist_keeps_catalog_rights_and_room_facts() {
    let list = music::playlist(&capture("music-playlist")).unwrap();
    assert_eq!(list.version, "1");
    assert_eq!(list.quantity, Some(1));
    assert_eq!(list.items.len(), 1);
    let item = &list.items[0];
    assert_eq!(item.song.duration_ms, Some(730000));
    assert_eq!(item.song.metadata.source_file_name, "Woodland Caribou");
    assert_eq!(
        item.song.album_title.as_deref(),
        Some("Det hjemsøkte hjertet")
    );
    assert!(item.song.cover_url.contains("20260224005511192557"));
    assert_eq!(item.song.artists[0].name, "Panopticon");
    assert_eq!(item.song.artists[0].id, None);
    assert_eq!(item.song.metadata.payment_type, Some(3));
    assert_eq!(
        item.song.metadata.rights.copyright_scope.as_deref(),
        Some("2")
    );
    assert_eq!(item.song.metadata.rights.copyright_grade, Some(5));
    let high = item
        .song
        .metadata
        .resources
        .iter()
        .find(|r| r.quality == "high")
        .unwrap();
    assert_eq!(
        high.hash.as_deref(),
        Some("BE7EE1E176472C1B62A57EA8DA0C6612")
    );
    assert_eq!(high.size_bytes, Some(154495617));
    assert_eq!(high.privilege, Some(10));
    assert_eq!(item.room.can_play_code, Some(1));
    assert_eq!(item.room.genting_code, Some(1));
    assert_eq!(item.room.added_by[0].id, "900001");
    assert_eq!(item.song.metadata.language.as_deref(), Some("英语"));
    assert_eq!(
        item.song.metadata.provider_extension.as_ref().unwrap()["room_trans"]["cid"],
        json!(595641789)
    );
}
#[test]
fn actual_sync_and_chat_keep_zero_codes_and_large_message_id() {
    let sync = music::sync(&capture("music-sync")).unwrap();
    assert_eq!(sync.position_ms, Some(0));
    assert_eq!(sync.pause_code, Some(2));
    assert_eq!(sync.play_mode_code, Some(1));
    assert_eq!(sync.song_id, "878751506");
    assert_eq!(sync.songs[0].room.purchase_code, Some(0));
    assert_eq!(sync.songs[0].room.anchor_play_code, Some(1));
    let messages = chat::convert(&capture("chat-history")).unwrap();
    assert_eq!(messages.items.len(), 1);
    let message = &messages.items[0];
    assert!(message.id.parse::<u64>().unwrap() > 9_007_199_254_740_991);
    assert_eq!(message.author.id, "900001");
    assert_eq!(message.message_type_code, Some(801));
    assert_eq!(message.kind_code, Some(0));
    assert_eq!(message.text, "接口验证消息");
    let source = music::source(&capture("music-playback-url")).unwrap();
    assert_eq!(source.extension, "mp3");
    assert_eq!(source.duration_ms, Some(730000));
    assert_eq!(source.bit_rate_bps, Some(128000));
    assert!(source.urls[0].ends_with(".kgs"));
}
#[test]
fn independent_optional_resources_and_unknown_codes_are_not_rejected() {
    let mut wire: Value = serde_json::from_slice(&capture("music-playlist")).unwrap();
    wire["data"]["songs_info"][0]
        .as_object_mut()
        .unwrap()
        .remove("hash_high");
    wire["data"]["songs_info"][0]["canplay"] = json!(7);
    let list = music::playlist(&serde_json::to_vec(&wire).unwrap()).unwrap();
    assert_eq!(list.items.len(), 1);
    let high = list.items[0]
        .song
        .metadata
        .resources
        .iter()
        .find(|r| r.quality == "high")
        .unwrap();
    assert_eq!(high.hash, None);
    assert_eq!(high.size_bytes, Some(154495617));
    assert_eq!(list.items[0].room.can_play_code, Some(7));
    let error = listen_together::data(
        r#"{"status":0,"errcode":20002,"error":"群组不存在","data":""}"#.as_bytes(),
    )
    .unwrap_err();
    assert_eq!(error.kind, "business");
    assert_eq!(error.code, Some(20002));
    let error = listen_together::data(br#"{"status":0,"error_code":51002,"data":""}"#).unwrap_err();
    assert_eq!(error.kind, "authentication");
}

#[test]
fn actual_pending_order_and_approval_are_not_just_empty_list_success() {
    let orders = music::orders(&capture("music-orders-pending")).unwrap();
    assert_eq!(orders.total, Some(1));
    assert_eq!(orders.items.len(), 1);
    assert_eq!(orders.items[0].requester.id, "900001");
    assert_eq!(orders.items[0].song.song.id, "448844108");
    assert_eq!(orders.items[0].song.song.title, "三十岁的无业游民");
    let mut raw: Value = serde_json::from_slice(&capture("music-orders-pending")).unwrap();
    raw["data"]["list"][0]
        .as_object_mut()
        .unwrap()
        .remove("song_info");
    let unknown = music::orders(&serde_json::to_vec(&raw).unwrap()).unwrap();
    assert_eq!(unknown.items.len(), 1);
    assert!(unknown.items[0].song.song.id.is_empty());
    assert_eq!(unknown.items[0].requester.id, "900001");
    let ack = listen_together::data(&capture("music-approve-order")).unwrap();
    assert_eq!(ack["handled_cnt"], json!(1));
    assert_eq!(ack["list_version"], json!(3));
}

#[test]
fn actual_empty_chat_dictionary_is_not_a_decode_failure() {
    let bytes =
        include_bytes!("../../../tests/fixtures/api/listen_together/chat-empty-history.json");
    let capture: serde_json::Value = serde_json::from_slice(bytes).unwrap();
    let page = echo_api::module::listen_together_chat::convert(
        &serde_json::to_vec(&capture["body"]).unwrap(),
    )
    .unwrap();
    assert!(page.items.is_empty());
    assert!(page.unmapped_items.is_empty());
    assert_eq!(page.ended, Some(true));
    assert_eq!(page.ended_code, Some(1));
    let other = serde_json::json!({"status":1,"data":{"list":{"provider_extension":42}}});
    let page =
        echo_api::module::listen_together_chat::convert(&serde_json::to_vec(&other).unwrap())
            .unwrap();
    assert!(page.items.is_empty());
    assert_eq!(page.unmapped_items, vec![other["data"]["list"].clone()]);
}
