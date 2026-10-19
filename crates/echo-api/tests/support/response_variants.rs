//! Deliberate variants of real captures, not additional provider samples.
use super::{sample, sample_response};
use echo_api::{error::ApiError, module::*};
use serde_json::{Value, json};
fn mapped<T: serde::Serialize>(r: Result<T, ApiError>) -> Result<Value, ApiError> {
    r.map(|v| serde_json::to_value(v).unwrap())
}
fn numeric_strings(value: &mut Value) {
    match value {
        Value::Number(n) => *value = Value::String(n.to_string()),
        Value::Object(o) => o.values_mut().for_each(numeric_strings),
        Value::Array(a) => a.iter_mut().for_each(numeric_strings),
        _ => {}
    }
}
// Compare scalar values while allowing raw provider extension fields to retain
// their actual wire representation. Null vs 0/false remains a real difference.
fn canonical(mut value: Value) -> Value {
    numeric_strings(&mut value);
    value
}
type Converter = fn(&[u8]) -> Result<Value, ApiError>;
fn cases() -> Vec<(&'static str, Vec<u8>, Converter)> {
    let mut cases: Vec<(&str, Vec<u8>, Converter)> = Vec::new();
    macro_rules! case {
        ($module:ident, $fixture:literal) => {
            cases.push((stringify!($module), sample($fixture), |b| {
                mapped($module::convert(b))
            }));
        };
        ($name:literal, $bytes:expr, $convert:expr) => {
            cases.push(($name, $bytes, $convert));
        };
    }
    case!(top_song, "top_song/new-songs-explore-fields-20261016.json");
    case!(
        top_album,
        "top_album/new-albums-explore-fields-20261016.json"
    );
    case!(
        top_playlist,
        "top_playlist/explore-first-category-explore-final-20261016.json"
    );
    case!(
        artist_lists,
        "artist_lists/explore-artists-explore-fields-20261016.json"
    );
    case!(
        playlist_tags,
        "playlist_tags/explore-tags-explore-fields-20261016.json"
    );
    case!(search_hot, "search_hot/success.json");
    case!(search_default, "search_default/success.json");
    case!(search_suggest, "search_suggest/success.json");
    case!(album_detail, "album_detail/success.json");
    case!(
        privilege_lite,
        "privilege_lite/success-rights-20261014.json"
    );
    case!(user_info, "user_info/current-account.json");
    case!(user_grade_info, "user_grade_info/current-account.json");
    case!(user_vip_detail, "user_vip_detail/current-account.json");
    case!(
        user_follow,
        "user_follow/current-account-identity-20261013.json"
    );
    case!(user_fans, "user_fans/current-account.json");
    case!(user_friends, "user_friends/current-account.json");
    case!(user_visitors, "user_visitors/current-account.json");
    case!(user_preference, "user_preference/current-account.json");
    case!(
        user_preference_update,
        "user_preference_update/unchanged-mode.json"
    );
    case!(playlist_add, "playlist_add/created-private.json");
    case!(playlist_update, "playlist_update/updated-info.json");
    case!(playlist_tracks_add, "playlist_tracks_add/added-two.json");
    case!(playlist_tracks_del, "playlist_tracks_del/removed-one.json");
    case!(
        playlist_tracks_sort,
        "playlist_tracks_sort/sorted-reverse.json"
    );
    case!(playlist_sort, "playlist_sort/owned-swapped.json");
    case!(playlist_detail, "playlist_detail/public-list.json");
    case!(
        playlist_track_all_new,
        "playlist_track_all_new/owned-list.json"
    );
    case!(
        krm_audio,
        "krm_audio/playlist-enrichment-title-cover-contract-20261016.json"
    );
    case!(
        "user_detail",
        sample("user_detail/current-account.json"),
        |b| mapped(user_detail::convert(b, 900000))
    );
    case!(
        "login_device",
        sample("login_device/current-account.json"),
        |b| mapped(login_device::convert(b, "current"))
    );
    case!(
        "user_playlist",
        sample("user_playlist/current-account.json"),
        |b| mapped(user_playlist::convert(b, 1, 2))
    );
    case!(
        "playlist_track_all",
        sample("playlist_track_all/public-list.json"),
        |b| mapped(playlist_track_all::convert(b, 1, 2))
    );
    case!("album_songs", sample("album_songs/success.json"), |b| {
        mapped(album_songs::convert(b, 1, 2))
    });
    case!(
        "song_url_auth",
        sample_response("song_url_auth_merge/128.json", 1),
        |b| mapped(song_url_auth::convert(b))
    );
    case!("search_song", sample("search/song.json"), |b| mapped(
        search::convert(search::SearchKind::Song, b)
    ));
    case!("search_album", sample("search/album.json"), |b| mapped(
        search::convert(search::SearchKind::Album, b)
    ));
    case!("search_artist", sample("search/author.json"), |b| mapped(
        search::convert(search::SearchKind::Author, b)
    ));
    case!(
        "search_playlist",
        sample("search/special.json"),
        |b| mapped(search::convert(search::SearchKind::Special, b))
    );
    case!("search_lyric", sample("search/lyric.json"), |b| mapped(
        search::convert(search::SearchKind::Lyric, b)
    ));
    case!("search_mv", sample("search/mv.json"), |b| mapped(
        search::convert(search::SearchKind::Mv, b)
    ));
    case!(
        everyday_recommend,
        "everyday_recommend/daily-current-pages-20261016.json"
    );
    case!(
        everyday_style_recommend,
        "everyday_style_recommend/home-style-default-plus-pages-20261016.json"
    );
    case!(rank_audio, "rank_audio/rank-songs-pages-20261016.json");
    case!(
        ai_recommend_song,
        "ai_recommend_song/free-listen-explore-fields-20261016.json"
    );
    case!(home_discover, "home_discover/home-discover.json");
    case!(
        "home_discover_anonymous",
        sample("home_discover/home-discover-anonymous.json"),
        |b| mapped(home_discover::convert(b))
    );
    case!(rank_top, "rank_top/rank-recommended-pages-20261016.json");
    case!(rank_list, "rank_list/rank-catalog-pages-20261016.json");
    case!(
        effects_match,
        "effects_match/first-page-playback-20261013.json"
    );
    case!(
        "effects_brand",
        sample("effects_brand/first-page-playback-20261013.json"),
        |b| mapped(effects_brand::convert(b, 1, 20))
    );
    case!(
        "effects_brand_detail",
        sample("effects_brand_detail/brand-77-playback-20261013.json"),
        |b| mapped(effects_brand_detail::convert(b, 1, 20))
    );
    case!(
        "effects_artist",
        sample("effects_artist/first-page-playback-20261013.json"),
        |b| mapped(effects_artist::convert(b, 1, 20))
    );
    case!(
        "get_model",
        sample("get_model/first-page-playback-20261013.json"),
        |b| mapped(get_model::convert(b, 1, 20))
    );
    case!(
        "blacklist_songs",
        sample("blacklist_list/current-song-blacklist-20261016.json"),
        |b| mapped(blacklist_list::convert(b, blacklist_list::Label::Song))
    );
    case!(
        "blacklist_singers",
        sample("blacklist_list/current-singer-blacklist-20261016.json"),
        |b| mapped(blacklist_list::convert(b, blacklist_list::Label::Singer))
    );
    case!("user_cloud", sample("user_cloud/page1.json"), |b| mapped(
        user_cloud::convert(b, 1, 30)
    ));
    case!(
        "user_cloud_del",
        sample("user_cloud_del/owned-test-file.json"),
        |b| mapped(user_cloud_del::convert(b))
    );
    case!(
        "user_video_collect",
        sample("user_video_collect/page1.json"),
        |b| mapped(user_video_collect::convert(b, 1, 30))
    );
    case!(
        "user_purchased_songs",
        sample("user_purchased_songs/page1.json"),
        |b| mapped(user_purchased_songs::convert(b, 1, 30))
    );
    case!(
        "user_purchased_albums",
        sample("user_purchased_albums/page1.json"),
        |b| mapped(user_purchased_albums::convert(b, 1, 30))
    );
    cases
}
#[test]
fn exact_numeric_strings_preserve_all_mapped_values_across_modules() {
    let cases = cases();
    let mut errors = Vec::new();
    for (name, bytes, convert) in cases {
        let expected =
            convert(&bytes).unwrap_or_else(|_| panic!("baseline mapping failed: {name}"));
        let mut raw: Value = serde_json::from_slice(&bytes).unwrap();
        numeric_strings(&mut raw);
        match convert(&serde_json::to_vec(&raw).unwrap()) {
            Ok(actual) if canonical(actual.clone()) == canonical(expected) => {}
            Ok(_) => errors.push(format!("{name}: mapped value changed")),
            Err(e) => errors.push(format!("{name}: {}", e.kind)),
        }
    }
    assert!(errors.is_empty(), "{}", errors.join("\n"));
}
#[test]
fn partial_metadata_keeps_every_row_and_does_not_invent_permissions() {
    let mut raw: Value =
        serde_json::from_slice(&sample("top_song/new-songs-explore-fields-20261016.json")).unwrap();
    let count = raw["data"].as_array().unwrap().len();
    for field in [
        "pay_type",
        "status_128",
        "has_obbligato",
        "trans_param",
        "video_id",
    ] {
        raw["data"][0].as_object_mut().unwrap().remove(field);
    }
    raw["data"][0]["bitrate_flac"] = json!({"future":"format"});
    let songs = top_song::convert(&serde_json::to_vec(&raw).unwrap()).unwrap();
    assert_eq!(songs.songs.len(), count);
    let song = serde_json::to_value(&songs.songs[0]).unwrap();
    assert_eq!(song["title"], raw["data"][0]["songname"]);
    assert!(song["metadata"]["payment_type"].is_null());
    assert!(song["metadata"]["has_accompaniment"].is_null());
    assert!(song["metadata"]["resources"][0]["download_status"].is_null());
    let flac = song["metadata"]["resources"]
        .as_array()
        .unwrap()
        .iter()
        .find(|r| r["quality"] == "flac")
        .unwrap();
    assert!(flac["bit_rate_bps"].is_null());
    assert_eq!(flac["hash"], raw["data"][0]["hash_flac"]);

    let mut raw: Value =
        serde_json::from_slice(&sample("user_playlist/current-account.json")).unwrap();
    let count = raw["data"]["info"].as_array().unwrap().len();
    raw["data"]["info"][0]["is_edit"] = json!(99);
    raw["data"]["info"][0]
        .as_object_mut()
        .unwrap()
        .remove("is_pri");
    raw["data"]["info"][0]
        .as_object_mut()
        .unwrap()
        .remove("count");
    let lists = user_playlist::convert(&serde_json::to_vec(&raw).unwrap(), 1, 2).unwrap();
    assert_eq!(lists.page.items.len(), count);
    assert_eq!(lists.page.items[0].editable, None);
    assert_eq!(lists.page.items[0].private, None);
    assert_eq!(lists.page.items[0].song_count, None);
    assert_eq!(lists.page.items[0].flag_codes.editable, Some(99));
    assert_eq!(
        lists.page.items[0].title,
        raw["data"]["info"][0]["name"].as_str().unwrap()
    );

    let mut raw: Value =
        serde_json::from_slice(&sample("login_device/current-account.json")).unwrap();
    let count = raw["data"]["li"].as_array().unwrap().len();
    raw["data"]["li"][0]["t"] = json!("unknown");
    raw["data"]["li"][0]["login_type"] = json!(9123);
    let id = raw["data"]["li"][0]["mid"].as_str().unwrap();
    let result = login_device::convert(&serde_json::to_vec(&raw).unwrap(), "current").unwrap();
    assert_eq!(result.len(), count);
    let row = result.iter().find(|r| r.id == id).unwrap();
    assert_eq!(row.active_at_seconds, None);
    assert_eq!(row.login_type, Some(9123));
    assert_eq!(row.model, raw["data"]["li"][0]["dev"].as_str().unwrap());
}

fn names(value: &Value, result: &mut Vec<Value>) {
    match value {
        Value::Array(values) => values.iter().for_each(|v| names(v, result)),
        Value::Object(o) => {
            for (key, value) in o {
                if matches!(key.as_str(), "title" | "name" | "nickname") {
                    result.push(value.clone());
                } else {
                    names(value, result);
                }
            }
        }
        _ => {}
    }
}
fn vary_optional(value: &mut Value, remove: bool) {
    match value {
        Value::Array(values) => values.iter_mut().for_each(|v| vary_optional(v, remove)),
        Value::Object(o) => {
            let fields = [
                "attr0",
                "attr1",
                "kq_talent",
                "pay_type",
                "fail_process",
                "old_cpy",
                "is_edit",
                "is_pri",
                "privilege",
                "Privilege",
                "IsOriginal",
                "student_status",
                "p_grade",
                "p_grade_point",
                "p_current_point",
                "p_next_grade",
                "p_next_grade_point",
                "d_sec",
                "bitrate",
                "duration",
                "timelen",
                "time_length",
                "status_128",
                "new",
                "t",
                "ver",
                "appid",
                "collectcount",
                "total",
                "pagesize",
                "page",
                "list_count",
                "count",
            ];
            for key in fields {
                if o.contains_key(key) {
                    if remove {
                        o.remove(key);
                    } else {
                        o.insert(key.into(), json!({"variant":"future"}));
                    }
                }
            }
            o.values_mut().for_each(|v| vary_optional(v, remove));
        }
        _ => {}
    }
}
#[test]
fn optional_omissions_and_unknown_values_preserve_names_and_rows_across_modules() {
    let mut errors = Vec::new();
    for (name, bytes, convert) in cases() {
        let baseline = convert(&bytes).unwrap();
        let mut expected_names = Vec::new();
        names(&baseline, &mut expected_names);
        for remove in [false, true] {
            let mut raw: Value = serde_json::from_slice(&bytes).unwrap();
            vary_optional(&mut raw, remove);
            match convert(&serde_json::to_vec(&raw).unwrap()) {
                Ok(result) => {
                    let mut actual_names = Vec::new();
                    names(&result, &mut actual_names);
                    if actual_names != expected_names {
                        errors.push(format!(
                            "{name} remove={remove}: lost or changed named entries"
                        ));
                    }
                }
                Err(e) => errors.push(format!("{name} remove={remove}: {}", e.kind)),
            }
        }
    }
    assert!(errors.is_empty(), "{}", errors.join("\n"));
}

#[test]
fn missing_device_presentation_and_missing_authorization_have_separate_meanings() {
    let mut raw: Value =
        serde_json::from_slice(&sample("login_device/current-account.json")).unwrap();
    let count = raw["data"]["li"].as_array().unwrap().len();
    for key in ["dev", "loc", "app", "mt"] {
        raw["data"]["li"][0].as_object_mut().unwrap().remove(key);
    }
    let result = login_device::convert(&serde_json::to_vec(&raw).unwrap(), "current").unwrap();
    assert_eq!(result.len(), count);
    let id = raw["data"]["li"][0]["mid"].as_str().unwrap();
    assert!(result.iter().find(|r| r.id == id).unwrap().model.is_empty());
    let mut raw: Value =
        serde_json::from_slice(&sample_response("song_url_auth_merge/128.json", 0)).unwrap();
    raw["data"].as_object_mut().unwrap().remove("auth");
    assert_eq!(
        song_auth::convert(&serde_json::to_vec(&raw).unwrap())
            .err()
            .unwrap()
            .kind,
        "authentication"
    );
}

#[test]
fn unknown_replacement_entries_do_not_erase_neighboring_ids_or_reject_the_ack() {
    let mut raw: Value =
        serde_json::from_slice(&sample("playlist_tracks_add/added-two.json")).unwrap();
    raw["data"]["del_fileids"] = json!([12,"13",{"future":1},null]);
    let result = playlist_tracks_add::convert(&serde_json::to_vec(&raw).unwrap()).unwrap();
    assert_eq!(
        result.replaced_file_ids,
        Some(vec![Some(12), Some(13), None, None])
    );
    assert_eq!(
        result.outcomes.unwrap().len(),
        raw["data"]["info"].as_array().unwrap().len()
    );
}

#[test]
fn optional_nested_metadata_does_not_gate_entities_or_real_business_errors() {
    let mut raw: Value = serde_json::from_slice(&sample(
        "artist_lists/explore-artists-explore-fields-20261016.json",
    ))
    .unwrap();
    let expected = artist_lists::convert(&serde_json::to_vec(&raw).unwrap()).unwrap();
    raw["data"].as_object_mut().unwrap().remove("enu_list");
    let result = artist_lists::convert(&serde_json::to_vec(&raw).unwrap()).unwrap();
    assert_eq!(result.groups.len(), expected.groups.len());
    for (a, b) in result.groups.iter().zip(expected.groups) {
        assert_eq!(a.artists.len(), b.artists.len());
    }
    assert!(result.type_filters.is_empty());
    let failure = serde_json::to_vec(&json!({"status":0,"error_code":123,"data":null})).unwrap();
    let error = song_auth::convert(&failure).err().unwrap();
    assert_eq!(error.kind, "business");
    assert_eq!(error.code, Some(123));
}
