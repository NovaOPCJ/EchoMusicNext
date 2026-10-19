//! Song comment protocol entry.
pub use crate::util::comment::{Query, convert};
use crate::{
    error::ApiError,
    util::{
        comment::{self, Resource},
        request::{ApiClient, Identity},
    },
};
pub async fn execute(
    client: &ApiClient,
    identity: &Identity,
    query: Query,
) -> Result<Vec<u8>, ApiError> {
    let resource = Resource::Music;
    comment::list(client, identity, query, resource, 0).await
}
