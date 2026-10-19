//! Comment reply protocol; response boundaries are defined in util/comment_write.rs.
pub use crate::util::comment_write::{Query, convert};
use crate::{
    error::ApiError,
    util::request::{ApiClient, Identity},
};
pub async fn execute(c: &ApiClient, i: &Identity, q: Query) -> Result<Vec<u8>, ApiError> {
    crate::util::comment_write::execute(c, i, q, 1).await
}
