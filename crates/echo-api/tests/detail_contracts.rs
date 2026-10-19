//! Upstream response fixtures and derived optional-field robustness cases.
//! Offline variations do not become runtime admission conditions.
use echo_api::{
    module,
    util::{comment, comment_write},
};
use serde_json::{Value, json};
fn payload(module: &str, name: &str) -> Vec<u8> {
    let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("../../tests/fixtures/api")
        .join(module)
        .join(format!("{name}-song-mv-20261018.json"));
    let record: Value = serde_json::from_slice(&std::fs::read(path).unwrap()).unwrap();
    serde_json::to_vec(&record["responses"].as_array().unwrap().last().unwrap()["payload"]).unwrap()
}
#[test]
fn comments_keep_rows_relations_unknowns_and_independent_fields() {
    let first = comment::convert(&payload("comment_music", "detail-music-first")).unwrap();
    assert_eq!(first.comments.len(), 30);
    assert!(!first.categories.is_empty());
    assert_eq!(first.hot_words.len(), 20);
    let item = &first.comments[0];
    assert_eq!(item.resource_id, "154262");
    assert_eq!(item.category_id, "2364");
    assert!(!item.user_id.is_empty());
    assert!(item.likes.count.is_some());
    assert_eq!(item.mix_song_id, Some(32042818));
    assert_eq!(item.display_text, "[REDACTED]");
    assert!(!item.extension.is_empty());
    let category = comment::convert(&payload("comment_music_classify", "detail-classify")).unwrap();
    assert_eq!(category.comments.len(), 20);
    assert_eq!(category.total, Some(72));
    let hot = comment::convert(&payload("comment_music_hotword", "detail-hotword")).unwrap();
    assert_eq!(hot.comments.len(), 30);
    assert!(hot.comments[0].images.is_empty());
    let floor = comment::convert(&payload("comment_floor", "detail-floor-resource-id")).unwrap();
    assert_eq!(floor.comments.len(), 10);
    assert_eq!(floor.reply_total, Some(10));
    assert!(!floor.comments[0].thread_id.is_empty());
    let partial = json!({"status":1,"list":[{"id":7,"content":"partial","reply_num":12,"like":{"count":4}},{"id":8,"images":""}],"count":null});
    let page = comment::convert(&serde_json::to_vec(&partial).unwrap()).unwrap();
    assert_eq!(page.comments.len(), 2);
    assert_eq!(page.comments[0].reply_count, Some(12));
    assert_eq!(page.comments[0].likes.count, Some(4));
    assert_eq!(page.comments[0].mix_song_id, None);
    assert_eq!(page.total, None);
}
#[test]
fn real_comment_singer_probes_keep_ordinary_pages() {
    // These default-filter response fixtures contain no singer collection.
    for name in [
        "detail-singer-comments",
        "detail-singer-gem",
        "detail-singer-xue",
    ] {
        let bytes = payload("comment_music", name);
        let raw: Value = serde_json::from_slice(&bytes).unwrap();
        assert!(raw.get("star_cmts").is_none());
        assert!(raw.get("star_comment").is_none());
        let page = comment::convert(&bytes).unwrap();
        assert_eq!(page.comments.len(), 30);
        assert!(page.total.unwrap() > 30);
        assert!(page.singer_comments.is_empty());
    }
    let bytes = payload("comment_music", "detail-singer-chuntingxue-deng");
    let raw: Value = serde_json::from_slice(&bytes).unwrap();
    let page = comment::convert(&bytes).unwrap();
    assert_eq!(page.comments.len(), 30);
    assert_eq!(page.singer_comments.len(), 1);
    let singer = &page.singer_comments[0];
    let expected = &raw["star_cmts"]["list"][0];
    assert_eq!(singer.id, expected["id"].as_i64().unwrap().to_string());
    assert_eq!(singer.content, expected["content"].as_str().unwrap());
    assert_eq!(singer.reply_count, expected["reply_num"].as_u64());
    assert_eq!(singer.likes.count, expected["like"]["count"].as_u64());
    assert_eq!(
        page.extension["star_cmts"]["title"],
        raw["star_cmts"]["title"]
    );
    assert_eq!(page.total, raw["count"].as_u64());
}
#[test]
fn real_video_versions_resolutions_and_urls() {
    let versions =
        module::kmr_audio_mv::convert(&payload("kmr_audio_mv", "detail-mv-versions")).unwrap();
    assert_eq!(versions.len(), 2);
    assert_eq!(versions[0].id, Some(33958));
    assert_eq!(versions[1].id, Some(756652));
    assert_eq!(versions[0].duration_ms, Some(270003));
    assert_eq!(versions[0].sources.len(), 4);
    assert!(
        versions[0]
            .sources
            .iter()
            .any(|s| s.codec == "H.265" && s.size_bytes == Some(13098344))
    );
    assert_eq!(versions[0].artists[0].id, Some(3520));
    assert_eq!(versions[0].tags.len(), 6);
    let detail = module::video_detail::convert(&payload("video_detail", "detail-video")).unwrap();
    assert_eq!(detail[0].id, Some(33958));
    assert_eq!(detail[0].sources.len(), 4);
    assert_eq!(detail[0].collection_count, Some(4357));
    let b = payload("video_url", "detail-source");
    let wire: Value = serde_json::from_slice(&b).unwrap();
    let hash = wire["data"].as_object().unwrap().keys().next().unwrap();
    let url = module::video_url::convert(&b, hash).unwrap().unwrap();
    assert_eq!(url.size_bytes, Some(21549465));
    assert!(!url.url.is_empty());
    assert_eq!(url.backup_urls.len(), 1);
    let rights =
        module::video_privilege::convert(&payload("video_privilege", "detail-privilege")).unwrap();
    assert_eq!(rights.len(), 1);
    assert_eq!(rights[0].status, Some(1));
    assert_eq!(rights[0].privilege, Some(0));
    // Facts without a playable hash remain available in the domain model.
    let partial = module::kmr_audio_mv::convert(
        br#"{"status":1,"data":[[{"h264":{"sd_filesize":123,"ld_hash":"independent-hash"}}]]}"#,
    )
    .unwrap();
    assert_eq!(partial[0].sources.len(), 1);
    assert_eq!(partial[0].sources[0].quality, "ld");
    assert_eq!(partial[0].sources[0].size_bytes, None);
    assert_eq!(partial[0].encodings[0].facts.sd_filesize, Some(123));
    assert_eq!(partial[0].id, None);
    let ordered = module::kmr_audio_mv::convert(br#"{"status":1,"data":[[{"h264":{"fhd_hash":"a","hd_hash":"b","qhd_hash":"c","sd_hash":"d","ld_hash":"e"}}]]}"#).unwrap();
    assert_eq!(
        ordered[0]
            .sources
            .iter()
            .map(|source| source.quality.as_str())
            .collect::<Vec<_>>(),
        ["fhd", "hd", "qhd", "sd", "ld"]
    );
}
#[test]
fn authors_and_barrage_preserve_actual_fields() {
    let bytes = payload("user_batch_union_vipinfo", "detail-author-products");
    let raw: Value = serde_json::from_slice(&bytes).unwrap();
    let authors = module::user_batch_union_vipinfo::convert(&bytes).unwrap();
    assert_eq!(
        authors.len(),
        raw["data"]["busi_vip"].as_object().unwrap().len()
    );
    assert!(!authors.is_empty());
    for author in authors {
        let rows = raw["data"]["busi_vip"][&author.user_id].as_array().unwrap();
        assert_eq!(author.products.len(), rows.len());
        for (product, row) in author.products.iter().zip(rows) {
            assert_eq!(product.product, row["product_type"].as_str().unwrap());
            assert_eq!(
                product.active_code,
                row["is_vip"].as_u64().map(|value| value as u32)
            );
        }
    }
    let barrage =
        module::video_barrage::convert(&payload("video_barrage", "detail-barrage")).unwrap();
    assert_eq!(barrage.comments.len(), 50);
    assert!(!barrage.resource_id.is_empty());
    assert!(!barrage.comments[0].content.is_empty());
    assert!(!barrage.comments[0].id.is_empty());
}
#[test]
fn achievements_counts_and_reversible_collection() {
    let ranks = module::song_ranking::convert(&payload("song_ranking", "detail-ranking")).unwrap();
    assert_eq!(ranks.info.len(), 3);
    let filtered = module::song_ranking_filter::convert(&payload(
        "song_ranking_filter",
        "detail-ranking-filter",
    ))
    .unwrap();
    assert_eq!(filtered.list.len(), 5);
    let favorites =
        module::favorite_count::convert(&payload("favorite_count", "detail-favorite-count"))
            .unwrap();
    assert_eq!(favorites[0].mixsongid, Some(32042818));
    assert!(favorites[0].count.is_some());
    assert!(
        module::mv_collect::convert(&payload("mv_collect", "detail-collection-add"))
            .unwrap()
            .accepted
    );
    assert!(
        module::mv_collect_del::convert(&payload("mv_collect_del", "detail-collection-remove"))
            .unwrap()
            .accepted
    );
}
#[test]
fn creator_comment_identity_is_not_an_account_cloud_list_id() {
    let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("../../tests/fixtures/api/playlist_detail/public-list-playlist-detail-20261016.json");
    let record: Value = serde_json::from_slice(&std::fs::read(path).unwrap()).unwrap();
    let raw = &record["responses"].as_array().unwrap().last().unwrap()["payload"];
    let details = module::playlist_detail::convert(&serde_json::to_vec(raw).unwrap()).unwrap();
    assert!(!details.is_empty());
    let serialized = serde_json::to_value(&details[0]).unwrap();
    assert!(
        serialized["creator_list_id"]
            .as_u64()
            .is_some_and(|id| id > 0)
    );
    assert!(serialized["creator_id"].as_u64().is_some_and(|id| id > 0));
    assert!(serialized.get("listid").is_none());
}
#[test]
fn real_write_rejections_are_not_moderation_success() {
    assert_eq!(
        comment_write::convert(&payload("comment_album_send", "detail-album-send"))
            .unwrap_err()
            .code,
        Some(60045)
    );
    assert_eq!(
        comment_write::convert(&payload("comment_music_send", "detail-send"))
            .unwrap_err()
            .code,
        Some(60045)
    );
    assert_eq!(
        comment_write::convert(&payload("comment_floor_send", "detail-reply-nonexistent"))
            .unwrap_err()
            .code,
        Some(60046)
    );
    assert!(
        comment_write::convert(&payload("comment_music_del", "detail-delete-nonexistent")).is_err()
    );
}

#[test]
fn song_barrage_uses_its_sampled_pool_and_preserves_identity() {
    for (requested, expected) in [(100, 50), (1, 1)] {
        let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join(format!(
            "../../tests/fixtures/api/song_barrage/playback-{requested}-20261019.json"
        ));
        let record: Value = serde_json::from_slice(&std::fs::read(path).unwrap()).unwrap();
        let bytes = serde_json::to_vec(&record["responses"][0]["payload"]).unwrap();
        let page = module::song_barrage::convert(&bytes).unwrap();
        assert_eq!(page.comments.len(), expected);
        assert_eq!(page.resource_id, "154262");
        assert_eq!(page.total, Some(2767));
        let first = &page.comments[0];
        assert_eq!(first.code, "articulossong");
        assert_eq!(first.mix_song_id, Some(32042818));
        assert!(!first.user_id.is_empty());
        assert!(!first.content.is_empty());
    }
}

#[test]
fn explicit_comment_write_failure_is_not_an_acceptance() {
    let bytes = br#"{"status":1,"err_code":401,"message":"offline rejection"}"#;
    assert!(comment_write::convert(bytes).is_err());
}
