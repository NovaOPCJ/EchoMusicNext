//! Album comment write protocol; response boundaries are defined in util/comment_write.rs.
pub use crate::util::comment_write::{Query, convert};
use crate::{
    error::ApiError,
    util::request::{ApiClient, Identity},
};
pub async fn execute(c: &ApiClient, i: &Identity, mut q: Query) -> Result<Vec<u8>, ApiError> {
    q.resource = crate::util::comment::Resource::Album;
    crate::util::comment_write::execute(c, i, q, 0).await
}
