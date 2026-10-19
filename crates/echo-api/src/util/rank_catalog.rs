//! Catalog fields independently observed in rank_top and rank_list captures.
use crate::{domain::cover_url, error::ApiError};
use serde::{Deserialize, Serialize};
#[derive(Deserialize)]
pub struct WireRank {
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    rankid: Option<u64>,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    rankname: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    imgurl: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    intro: String,
    #[serde(default, deserialize_with = "crate::util::response::text")]
    update_frequency: String,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    ranktype: Option<u32>,
    #[serde(default, deserialize_with = "crate::util::response::optional")]
    rank_id_publish_date: Option<String>,
}
#[derive(Debug, Serialize)]
pub struct Rank {
    pub id: Option<u64>,
    pub title: String,
    pub cover_url: String,
    pub description: String,
    pub update_frequency: String,
    pub published_at: Option<String>,
    pub category: Option<u32>,
}
impl WireRank {
    pub fn convert(self) -> Result<Rank, ApiError> {
        Ok(Rank {
            id: self.rankid,
            title: self.rankname,
            cover_url: cover_url(self.imgurl),
            description: self.intro,
            update_frequency: self.update_frequency,
            published_at: self.rank_id_publish_date,
            category: self.ranktype,
        })
    }
}
