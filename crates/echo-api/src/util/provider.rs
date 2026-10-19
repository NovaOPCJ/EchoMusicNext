//! Provider HTTP is separate from Kugou signing and account injection. URLs are
//! endpoint-owned, redirects are explicit, and error text never includes URLs.
use crate::error::ApiError;
use reqwest::{Method, Url, header::HeaderMap};
pub struct Response {
    pub bytes: Vec<u8>,
    pub headers: HeaderMap,
}
impl super::request::ApiClient {
    pub(crate) async fn provider_request(
        &self,
        method: Method,
        url: Url,
        headers: &[(&str, &str)],
    ) -> Result<Response, ApiError> {
        if url.scheme() != "https"
            || !matches!(
                url.host_str(),
                Some(
                    "openmobile.qq.com"
                        | "xui.ptlogin2.qq.com"
                        | "api.weixin.qq.com"
                        | "open.weixin.qq.com"
                        | "long.open.weixin.qq.com"
                        | "ssl.ptlogin2.qq.com"
                        | "ptlogin2.qq.com"
                        | "ui.ptlogin2.qq.com"
                        | "graph.qq.com"
                        | "connect.qq.com"
                )
            )
            || url.port().is_some_and(|p| p != 443)
            || !url.username().is_empty()
            || url.password().is_some()
        {
            return Err(ApiError::new("input", "登录服务地址无效"));
        }
        // Inherit the application API timeout. Zero means no global deadline;
        // polling and session owners still cancel their own requests.
        let mut request = self.apply_timeout(self.http().request(method, url));
        for &(key, value) in headers {
            request = request.header(key, value);
        }
        let mut response = request
            .send()
            .await
            .map_err(|error| super::request::transport_error(error, "登录服务网络请求失败"))?;
        if !response.status().is_success() && !response.status().is_redirection() {
            return Err(ApiError::new("http", "登录服务暂时不可用"));
        }
        let headers = response.headers().clone();
        let mut bytes = Vec::new();
        while let Some(chunk) = response
            .chunk()
            .await
            .map_err(|error| super::request::transport_error(error, "登录响应读取失败"))?
        {
            if bytes.len() + chunk.len() > 2 * 1024 * 1024 {
                return Err(ApiError::decode());
            }
            bytes.extend_from_slice(&chunk);
        }
        Ok(Response { bytes, headers })
    }
}
pub const QQ_USER_AGENT: &str = "Mozilla/5.0 (Linux; Android 10; K) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Mobile Safari/537.36";
pub fn url(base: &str, pairs: &[(&str, &str)]) -> Result<Url, ApiError> {
    let mut url = Url::parse(base).map_err(|_| ApiError::decode())?;
    url.query_pairs_mut().extend_pairs(pairs.iter().copied());
    Ok(url)
}
