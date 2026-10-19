//! Shared local-calendar checksum for the provider image service.
use crate::error::ApiError;
use md5::{Digest, Md5};

pub fn checksum(date: &str) -> Result<String, ApiError> {
    if date.len() != 8 || !date.bytes().all(|b| b.is_ascii_digit()) {
        return Err(ApiError::new("input", "图片上传日期无效"));
    }
    let year: u32 = date[..4]
        .parse()
        .map_err(|_| ApiError::new("input", "图片上传日期无效"))?;
    let month: usize = date[4..6]
        .parse()
        .map_err(|_| ApiError::new("input", "图片上传日期无效"))?;
    let day: u32 = date[6..]
        .parse()
        .map_err(|_| ApiError::new("input", "图片上传日期无效"))?;
    let leap = year.is_multiple_of(4) && (!year.is_multiple_of(100) || year.is_multiple_of(400));
    let days = [
        31,
        if leap { 29 } else { 28 },
        31,
        30,
        31,
        30,
        31,
        31,
        30,
        31,
        30,
        31,
    ];
    if year == 0 || !(1..=12).contains(&month) || day == 0 || day > days[month - 1] {
        return Err(ApiError::new("input", "图片上传日期无效"));
    }
    Ok(format!(
        "{:x}",
        Md5::digest(format!("{date}hewry678WEK23D"))
    ))
}
