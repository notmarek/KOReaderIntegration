use std::ffi::{c_char, c_int, c_void};

use cjson_rs::Json;

enum ScannerEventType {
    Add,
    Delete,
    Update,
    AddThumb,
    UpdateThumb,
}

struct ScannerEvent {
    event_type: ScannerEventType,
    path: *const c_char,
    lipchandle: *const c_void,
    filename: *const c_char,
    uuid: *const c_char,
    glon: *const c_char,
}

type ScannerEventHandler = extern "C" fn(event: *const ScannerEvent) -> c_int;

#[link(name = "scanner")]
extern "C" {
    fn scanner_post_change(json: *const Json) -> c_int;
    fn scanner_gen_uuid(uuid_out: *mut c_char, buffer_size: c_int) -> c_void;
    fn scanner_get_thumbnail_for_uuid(uuid: *const c_char) -> *const c_char;
    fn scanner_update_ccat(uuid: *const c_char, thumbnail_path: *const c_char) -> c_void;
    fn scanner_delete_ccat_entry(uuid: *const c_char) -> c_void;
    fn getSha1Hash(data: *const c_char) -> *const c_char;
}
