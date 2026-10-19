//! Exact unsigned JSON numbers/strings. Real playlist ipmap.attr0 uses integer
//! tokens and scientific notation. Never round a copyright bitfield via f64.
use serde::Deserializer;
/// Optional display flags retain an unknown service code as unknown. They do
/// not gate the enclosing response or invent a negative permission fact.
pub fn boolean(value: u32) -> Option<bool> {
    match value {
        0 => Some(false),
        1 => Some(true),
        _ => None,
    }
}
pub fn unsigned_json<'de, D: Deserializer<'de>>(deserializer: D) -> Result<u64, D::Error> {
    crate::util::response::scalar(deserializer)
}
pub fn optional_unsigned_json<'de, D: Deserializer<'de>>(
    deserializer: D,
) -> Result<Option<u64>, D::Error> {
    crate::util::response::optional(deserializer)
}
pub(crate) fn exact_unsigned(number: &serde_json::Number) -> Option<u64> {
    if let Some(value) = number.as_u64() {
        return Some(value);
    }
    // arbitrary_precision retains the original decimal/exponent token here.
    let token = number.to_string();
    if token.starts_with('-') {
        return None;
    }
    let (mantissa, exponent) = match token.split_once(['e', 'E']) {
        Some((m, e)) => (m, e.parse::<i32>().ok()?),
        None => (token.as_str(), 0),
    };
    let fraction = mantissa.split_once('.').map_or(0, |(_, f)| f.len());
    let digits = mantissa.replace('.', "");
    let digits = digits.trim_start_matches('0');
    if digits.is_empty() {
        return Some(0);
    }
    let scale = i64::from(exponent).checked_sub(i64::try_from(fraction).ok()?)?;
    let (digits, zeros) = if scale < 0 {
        let remove = usize::try_from(-scale).ok()?;
        if remove > digits.len() || !digits[digits.len() - remove..].bytes().all(|c| c == b'0') {
            return None;
        }
        (&digits[..digits.len() - remove], 0)
    } else {
        (digits, usize::try_from(scale).ok()?)
    };
    if digits.len().checked_add(zeros)? > 20 {
        return None;
    }
    let value = digits.parse::<u64>().ok()?;
    value.checked_mul(10_u64.checked_pow(u32::try_from(zeros).ok()?)?)
}
#[cfg(test)]
mod tests {
    use super::*;
    use serde::Deserialize;
    #[derive(Deserialize)]
    struct Bits {
        #[serde(deserialize_with = "unsigned_json")]
        attr0: u64,
    }
    #[test]
    fn exact_bitfields_keep_exponents_without_float_rounding() {
        for (wire, expected) in [
            ("2.8162962909184e+14", 281629629091840),
            ("5.6298461514957e+14", 562984615149570),
            ("0.000e20", 0),
            ("1.8446744073709551615e19", u64::MAX),
            ("18446744073709551615", u64::MAX),
            ("\"123\"", 123),
            ("\"1.8446744073709551615e19\"", u64::MAX),
        ] {
            let bits: Bits = serde_json::from_str(&format!("{{\"attr0\":{wire}}}")).unwrap();
            assert_eq!(bits.attr0, expected);
        }
        for wire in [
            "281629629091840.001",
            "1.8446744073709551616e19",
            "1e2000",
            "-1",
            "0.01",
        ] {
            assert!(
                serde_json::from_str::<Bits>(&format!("{{\"attr0\":{wire}}}")).is_err(),
                "accepted {wire}"
            );
        }
    }
}
