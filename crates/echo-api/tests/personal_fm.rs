use echo_api::module::personal_fm;
use serde_json::{Value, json};
fn capture(name: &str) -> Value {
    let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("../../tests/fixtures/api/personal_fm")
        .join(format!("{name}.json"));
    let v: Value = serde_json::from_slice(&std::fs::read(path).unwrap()).unwrap();
    v["responses"][0]["payload"].clone()
}
#[test]
fn sampled_modes_pools_keep_identity_resources_and_recommendation() {
    for name in [
        "preview-normal",
        "preview-small",
        "preview-peak",
        "preview-radio",
        "pool-1",
        "pool-2",
    ] {
        let wire = capture(name);
        let out = personal_fm::convert(&serde_json::to_vec(&wire).unwrap()).unwrap();
        let source = wire["data"]["song_list"].as_array().unwrap();
        assert_eq!(out.songs.len(), source.len());
        for (song, raw) in out.songs.iter().zip(source) {
            assert_eq!(song.song.song.id, raw["mixsongid"]);
            assert_eq!(
                song.song.song.duration_ms,
                raw["time_length"].as_u64().map(|v| v * 1000)
            );
            assert_eq!(song.song.metadata.audio_id, raw["songid"].as_u64());
            assert_eq!(
                song.song.metadata.recommendation.headline,
                raw["rec_song_info"]["rec_desc"]
            );
            assert_eq!(
                song.personal_fm.similarity,
                raw["rec_song_info"]["similar_desc"]
            );
            assert_eq!(
                song.song.metadata.related_goods_extension.as_ref(),
                raw.get("relate_goods")
            );
            assert_eq!(
                song.song.song.album_title.as_ref().unwrap(),
                raw["relate_goods"][0]["albumname"].as_str().unwrap()
            );
            assert!(!song.song.song.cover_url.is_empty());
            assert!(
                song.song
                    .metadata
                    .provider_extension
                    .as_ref()
                    .unwrap()
                    .get("tracker_info")
                    .and_then(|v| v.get("auth"))
                    .is_none()
            );
            for good in raw["relate_goods"].as_array().unwrap() {
                assert!(
                    song.song
                        .metadata
                        .resources
                        .iter()
                        .any(|r| r.hash.as_deref() == good["hash"].as_str()
                            && r.size_bytes == good["info"]["filesize"].as_u64()
                            && r.duration_ms == good["info"]["duration"].as_u64())
                );
            }
        }
    }
}
#[test]
fn optional_fm_data_and_per_track_mark_are_not_invented_from_batch() {
    let mut wire = capture("preview-normal");
    let song = &mut wire["data"]["song_list"][0];
    song["rec_song_info"]["cur_mark"] = json!("actual-track-mark");
    song.as_object_mut().unwrap().remove("time_length");
    song.as_object_mut().unwrap().remove("songid");
    let out = personal_fm::convert(&serde_json::to_vec(&wire).unwrap()).unwrap();
    assert_eq!(out.songs[0].personal_fm.mark, "actual-track-mark");
    assert!(out.songs[0].song.song.duration_ms.is_none());
    assert!(out.songs[0].song.metadata.audio_id.is_none());
    assert!(out.songs[1].personal_fm.mark.is_empty());
    wire["data"]["song_list"] = json!([]);
    assert!(
        personal_fm::convert(&serde_json::to_vec(&wire).unwrap())
            .unwrap()
            .songs
            .is_empty()
    );
}

#[test]
fn feedback_body_uses_audio_identity_and_sampled_protocol_types() {
    let identity: echo_api::util::request::Identity = serde_json::from_value(json!({
        "userid":900001,"token":"test-only-token","mid":"test-only-mid","dfid":"test-only-dfid"
    }))
    .unwrap();
    let query: personal_fm::Query = serde_json::from_value(json!({
        "mode":"small","action":"garbage","song_pool_id":2,"remain_songcnt":4,
        "hash":"00112233445566778899aabbccddeeff","songid":1308303,
        "cur_mark":"track-mark","playtime":12,"is_overplay":false
    }))
    .unwrap();
    let body = personal_fm::body(&identity, query, 1234567890000).unwrap();
    assert_eq!(body["songid"], 1308303);
    assert_eq!(body["song_pool_id"], 2);
    assert_eq!(body["remain_songcnt"], 4);
    assert_eq!(body["mode"], "small");
    assert_eq!(body["action"], "garbage");
    assert_eq!(body["playtime"], 12);
    assert_eq!(body["is_overplay"], 0);
    assert_eq!(body["platform"], "ios");
    assert_eq!(body["appid"], 3116);
    assert_eq!(body["clientver"], 11440);
    assert_eq!(
        body["key"],
        echo_api::util::user_protocol::timestamp_key(1234567890000)
    );
    assert!(body.get("mixsongid").is_none());
}
