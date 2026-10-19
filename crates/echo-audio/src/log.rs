//! Decoder URLs can contain signed credentials. Diagnostics keep source kind,
//! timings and errors, but never the URL or authorization query.
pub fn redact_sources(message: &str) -> String {
    let mut out = message.to_owned();
    for prefix in ["https://", "http://"] {
        while let Some(start) = out.find(prefix) {
            let end = out[start..]
                .find(|c: char| c.is_whitespace() || c == '\'' || c == '"')
                .map_or(out.len(), |n| start + n);
            out.replace_range(start..end, "[audio-source]");
        }
    }
    out
}
pub fn info(message: impl AsRef<str>) {
    echo_log::info(redact_sources(message.as_ref()));
}
pub fn debug(message: impl AsRef<str>) {
    echo_log::debug(redact_sources(message.as_ref()));
}
pub fn warn(message: impl AsRef<str>) {
    echo_log::warn(redact_sources(message.as_ref()));
}
pub fn error(message: impl AsRef<str>) {
    echo_log::error(redact_sources(message.as_ref()));
}
#[cfg(test)]
mod tests {
    #[test]
    fn urls_are_removed_without_losing_transport_diagnostics() {
        assert_eq!(
            super::redact_sources("failed source 'https://host/audio?token=secret' after 123ms"),
            "failed source '[audio-source]' after 123ms"
        );
    }
}
