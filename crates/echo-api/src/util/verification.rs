//! Transport-level challenge metadata. An event ID plus status 0 or error 20028
//! is required; unrelated business codes never imply an event.
use serde::{Deserialize, Serialize};
use serde_json::Value;

#[derive(Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Challenge {
    pub event_id: String,
}

pub fn challenge(header: Option<&str>, bytes: &[u8]) -> Option<Challenge> {
    let body: Value = serde_json::from_slice(bytes).unwrap_or_default();
    // Choose the first truthy value before converting to string and trimming.
    // Whitespace in a higher-priority field must not select a lower-priority event.
    let candidate = if truthy(&body["ssaCode"]) {
        string(&body["ssaCode"])
    } else if let Some(header) = header.filter(|v| !v.is_empty()) {
        header.to_owned()
    } else if truthy(&body["data"]["event_id"]) {
        string(&body["data"]["event_id"])
    } else {
        return None;
    };
    let event_id = candidate.trim();
    if event_id.is_empty()
        || (number(&body["error_code"]) != Some(20028.0) && number(&body["status"]) != Some(0.0))
    {
        return None;
    }
    Some(Challenge {
        event_id: event_id.into(),
    })
}

// JSON truthiness, string and number conversions for challenge metadata.
// Endpoint response conversion remains separate.
fn truthy(value: &Value) -> bool {
    match value {
        Value::Null => false,
        Value::Bool(value) => *value,
        Value::Number(value) => value.as_f64() != Some(0.0),
        Value::String(value) => !value.is_empty(),
        Value::Array(_) | Value::Object(_) => true,
    }
}
fn string(value: &Value) -> String {
    match value {
        Value::String(value) => value.clone(),
        Value::Array(values) => values
            .iter()
            .map(|value| {
                if value.is_null() {
                    String::new()
                } else {
                    string(value)
                }
            })
            .collect::<Vec<_>>()
            .join(","),
        Value::Object(_) => "[object Object]".into(),
        _ => value.to_string(),
    }
}
fn number(value: &Value) -> Option<f64> {
    match value {
        Value::Null => Some(0.0), // Number(field ?? 0), including a missing field.
        Value::Bool(value) => Some(u8::from(*value).into()),
        Value::Number(value) => value.as_f64(),
        Value::Object(_) => None,
        _ => {
            let text = string(value);
            let text = text.trim();
            if text.is_empty() {
                return Some(0.0);
            }
            for (prefix, radix) in [("0x", 16), ("0o", 8), ("0b", 2)] {
                if text
                    .get(..2)
                    .is_some_and(|value| value.eq_ignore_ascii_case(prefix))
                {
                    return u64::from_str_radix(&text[2..], radix)
                        .ok()
                        .map(|value| value as f64);
                }
            }
            text.parse().ok()
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn transport_metadata_preserves_event_and_does_not_use_local_sid_edt() {
        let bytes = br#"{"status":0,"error_code":20028,"ssaCode":"body-event","sid":"local","edt":"local"}"#;
        assert_eq!(
            challenge(Some(" header-event "), bytes).unwrap().event_id,
            "body-event"
        );
        assert_eq!(challenge(None, bytes).unwrap().event_id, "body-event");
        assert_eq!(
            challenge(None, br#"{"status":0,"data":{"event_id":"nested"}}"#)
                .unwrap()
                .event_id,
            "nested"
        );
        assert!(
            challenge(
                None,
                br#"{"status":1,"data":{"event_id":"not-a-challenge"}}"#
            )
            .is_none()
        );
        assert!(challenge(Some(" "), br#"{}"#).is_none());
    }

    #[test]
    fn event_precedence_and_failure_predicate_match_source() {
        for bytes in [
            br#"{"status":0,"error_code":60045}"#.as_slice(),
            br#"{"status":0,"sid":"simulated","edt":"simulated"}"#,
            br#"{"status":1,"ssaCode":"event"}"#,
            br#"{"status":1,"error_code":60045,"data":{"event_id":"event"}}"#,
            br#"{"status":"invalid","ssaCode":"event"}"#,
        ] {
            assert!(challenge(None, bytes).is_none());
        }
        // 60045 is incidental: any failed response with an actual event works.
        for code in [0, 60045, 60046, 99999] {
            let bytes =
                format!(r#"{{"status":0,"error_code":{code},"data":{{"event_id":"event"}}}}"#);
            assert_eq!(challenge(None, bytes.as_bytes()).unwrap().event_id, "event");
        }
        assert!(challenge(Some("header"), br#"{"status":1}"#).is_none());
        assert_eq!(
            challenge(
                Some(" header "),
                br#"{"status":0,"data":{"event_id":"nested"}}"#
            )
            .unwrap()
            .event_id,
            "header"
        );
        assert_eq!(
            challenge(Some("header"), br#"{"status":1,"error_code":"20028"}"#)
                .unwrap()
                .event_id,
            "header"
        );
        assert_eq!(
            challenge(None, br#"{"status":"0","ssaCode":" event "}"#)
                .unwrap()
                .event_id,
            "event"
        );
        assert_eq!(
            challenge(None, br#"{"ssaCode":"event"}"#).unwrap().event_id,
            "event"
        );
        assert_eq!(
            challenge(None, br#"{"status":null,"data":{"event_id":42}}"#)
                .unwrap()
                .event_id,
            "42"
        );
        assert!(challenge(Some("header"), br#"{"status":0,"ssaCode":" "}"#).is_none());
        assert!(challenge(Some(" "), br#"{"status":0,"data":{"event_id":"nested"}}"#).is_none());
        assert_eq!(
            challenge(
                Some(""),
                br#"{"status":0,"ssaCode":"","data":{"event_id":"nested"}}"#
            )
            .unwrap()
            .event_id,
            "nested"
        );
    }
}
