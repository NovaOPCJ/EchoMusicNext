//! Room facts from 2026-10-19 actual gateway captures. Playback remains host-owned.
use super::discovery::DiscoverySong;
use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::collections::BTreeMap;
#[derive(Default, Debug, Serialize)]
pub struct Profile {
    pub id: String,
    pub name: String,
    pub avatar_url: Option<String>,
    pub entered_at_seconds: Option<u64>,
    pub entered_duration_seconds: Option<u64>,
    pub extension: BTreeMap<String, Value>,
}
#[derive(Debug, Serialize)]
pub struct Room {
    pub id: String,
    pub name: String,
    pub owner: Profile,
    pub member_count: Option<u64>,
    pub capacity: Option<u64>,
    pub privacy_code: Option<u32>,
    pub chat_code: Option<u32>,
    pub active_code: Option<u32>,
    pub background_url: Option<String>,
    pub channel_id: String,
    pub channel_name: String,
    pub current_song: Option<RoomSong>,
    pub members: Vec<Profile>,
    pub extension: BTreeMap<String, Value>,
}
#[derive(Debug, Serialize)]
pub struct RoomPage {
    pub items: Vec<Room>,
    pub ended: Option<bool>,
    pub ended_code: Option<u32>,
    pub cursor: Option<u64>,
}
#[derive(Debug, Serialize)]
pub struct RoomSession {
    pub room_id: String,
    pub owner: Option<bool>,
    pub owner_code: Option<u32>,
    pub joined_at_seconds: Option<u64>,
    pub heartbeat_seconds: Option<u64>,
    pub active_code: Option<u32>,
    pub capacity: Option<u64>,
    pub owner_profile: Option<Profile>,
    pub name: String,
    pub chat_code: Option<u32>,
    pub extension: BTreeMap<String, Value>,
}
#[derive(Debug, Serialize)]
pub struct Members {
    pub items: Vec<Profile>,
    pub total: Option<u64>,
}
#[derive(Debug, Serialize)]
pub struct ChatMessage {
    pub id: String,
    pub author: Profile,
    pub text: String,
    pub sent_at_seconds: Option<u64>,
    pub kind_code: Option<u32>,
    pub message_type_code: Option<u32>,
    pub online_code: Option<u32>,
    pub extension: BTreeMap<String, Value>,
}
#[derive(Debug, Serialize)]
pub struct ChatPage {
    pub items: Vec<ChatMessage>,
    pub unmapped_items: Vec<Value>,
    pub ended: Option<bool>,
    pub ended_code: Option<u32>,
}
#[derive(Default, Debug, Serialize)]
pub struct RoomSongFacts {
    pub original_hash: String,
    pub original_song_id: String,
    pub can_play_code: Option<u32>,
    pub genting_code: Option<u32>,
    pub source_code: Option<u32>,
    pub anchor_play_code: Option<u32>,
    pub purchase_code: Option<u32>,
    pub added_by: Vec<Profile>,
}
#[derive(Debug, Serialize)]
pub struct RoomSong {
    pub song: DiscoverySong,
    pub room: RoomSongFacts,
}
#[derive(Debug, Serialize)]
pub struct RoomPlaylist {
    pub items: Vec<RoomSong>,
    pub version: String,
    pub quantity: Option<u64>,
    pub timestamp_seconds: Option<u64>,
    pub extension: BTreeMap<String, Value>,
}
#[derive(Debug, Serialize)]
pub struct RoomPlayback {
    pub hash: String,
    pub song_id: String,
    pub position_ms: Option<u64>,
    pub pause_code: Option<u32>,
    pub play_mode_code: Option<u32>,
    pub timestamp_seconds: Option<u64>,
    pub version: String,
    pub songs: Vec<RoomSong>,
    pub provider_extension: BTreeMap<String, Value>,
}
#[derive(Debug, Serialize)]
pub struct SongOrder {
    pub song: RoomSong,
    pub requester: Profile,
}
#[derive(Debug, Serialize)]
pub struct SongOrders {
    pub items: Vec<SongOrder>,
    pub total: Option<u64>,
}
#[derive(Debug, Serialize)]
pub struct AudioSource {
    pub urls: Vec<String>,
    pub duration_ms: Option<u64>,
    pub size_bytes: Option<u64>,
    pub bit_rate_bps: Option<u64>,
    pub extension: String,
    pub volume: Option<f64>,
    pub volume_gain: Option<f64>,
    pub volume_peak: Option<f64>,
    pub status_code: Option<u32>,
    pub provider_extension: BTreeMap<String, Value>,
}
#[derive(Default, Debug, Serialize)]
pub struct CommandResult {
    pub version: String,
    pub message_id: String,
}
#[derive(Clone, Debug, Deserialize, Serialize)]
pub struct AudioRef {
    pub hash: String,
    pub song_id: String,
}
#[derive(Clone, Debug, Deserialize)]
pub struct InitialPlayback {
    pub audio: AudioRef,
    pub position_ms: u64,
    pub playing: bool,
    pub play_mode: u8,
}
