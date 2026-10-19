//! # airplay-client
//!
//! High-level AirPlay 2 sender client.
//!
//! This crate provides:
//! - Simple API for discovering and connecting to AirPlay receivers
//! - Audio streaming from files or raw PCM
//! - Multi-room group management
//! - Volume and playback control
//!
//! ## Example
//!
//! ```ignore
//! use airplay_client::{AirPlayClient, DeviceSelector};
//! use std::path::Path;
//!
//! #[tokio::main]
//! async fn main() -> Result<(), Box<dyn std::error::Error>> {
//!     // Create client and discover devices
//!     let mut client = AirPlayClient::new();
//!     let devices = client.discover(Duration::from_secs(5)).await?;
//!     
//!     // Connect to first device
//!     let device = devices.first().unwrap();
//!     client.connect(device).await?;
//!     
//!     // Stream audio file
//!     client.play_file(Path::new("song.mp3")).await?;
//!     
//!     // Wait for playback
//!     client.wait_for_completion().await?;
//!     
//!     Ok(())
//! }
//! ```

mod builder;
mod client;
mod connection;
mod events;
mod group;
mod playback;
mod raop_connection;
mod stats;

pub use builder::ClientBuilder;
pub use client::AirPlayClient;
pub use connection::{Connection, StreamingParams};
pub use events::{CallbackHandler, ClientEvent, EventHandler, NoOpHandler};
pub use group::{DeviceGroup, GroupMember};
pub use playback::{PlaybackInfo, PlaybackState};
pub use raop_connection::RaopConnection;
pub use stats::{DeviceStatsSnapshot, StatsSnapshot, StreamStats};

// Re-export commonly used types
pub use airplay_core::{AudioFormat, Device, DeviceId, Error, Result, StreamConfig};
pub use airplay_discovery::Discovery;

// Live streaming types for external sources (e.g., Bluetooth audio)
pub use airplay_audio::{LiveAudioDecoder, LiveFrameSender, LivePcmFrame};

// Equalizer types
pub use airplay_audio::eq::MAX_GAIN_DB;
pub use airplay_audio::{EqConfig, EqParams};

// Spatial audio types
pub use airplay_audio::{
    Position, SpatialMixer, SpatialMode, SpatialParams, SpatialSnapshot, SpeakerConfig,
    SpeakerParams,
};

// Timing types needed for group streaming
pub use airplay_timing::ClockOffset;
