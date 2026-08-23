# Jay-D library metadata converter

`jayd_library.py` is a read-only, Python-standard-library host tool. It imports
supported DJ-library exports and writes deterministic Jay-D metadata plus an AAC
conversion manifest. It never edits a source library and does not transcode audio.

```sh
python3 tools/jayd-library/jayd_library.py rekordbox export.xml -o library.jydm --root /Music
python3 tools/jayd-library/jayd_library.py m3u set.m3u8 -o library.jydm
python3 tools/jayd-library/jayd_library.py mixxx mixxxdb.sqlite -o library.jydm --root /Music
python3 tools/jayd-library/jayd_library.py traktor collection.nml -o library.jydm --root /Music
python3 tools/jayd-library/jayd_library.py inspect library.jydm
```

Copy the result to `/library.jydm` at the SD-card root for firmware discovery.

Only normalized relative POSIX paths are written. Absolute paths are rejected
unless `--root` maps them to a relative path below that root; traversal, NULs,
foreign `file://` authorities, and Windows drive paths are rejected. Windows
exports should be path-remapped to relative paths before import.

Duplicate normalized paths become one track. The record with more populated
metadata wins; equal records use a lexical tie-break. Tracks and sections are
sorted deterministically, while playlist entry order is retained.

## `JAYDMETA/1` binary format

All integers are unsigned little-endian unless noted. Readers must reject files
over 32 MiB, verify CRC before consuming sections, validate every range, and skip
unknown section types using the directory's byte size. Known sections may grow:
readers use each directory entry's `entry_size` and ignore trailing record bytes.
Known required sections with a version other than `1` are rejected; unknown
section types are optional and skipped by their declared byte range.

The CRC32 is IEEE/zlib CRC32 over the entire file with the header CRC field
(byte offset 44) set to zero.

### Header (64 bytes)

| Offset | Type | Meaning |
| ---: | --- | --- |
| 0 | `char[8]` | `JAYDMETA` magic |
| 8 | `u16` | format version (`1`) |
| 10 | `u16` | endian tag (`0x4c45`, bytes `EL`) |
| 12 | `u16` | header size |
| 14 | `u16` | section-directory entry size |
| 16 | `u32` | section count |
| 20 | `u32` | track count |
| 24 | `u32` | flags; zero in version 1 |
| 28 | `u64` | total file size |
| 36 | `u64` | section-directory offset |
| 44 | `u32` | CRC32 |
| 48 | `byte[16]` | reserved, zero |

Each 24-byte directory entry is `type[4], version u16, entry_size u16,
count u32, byte_size u32, offset u64`. Version 1 writes sections in the order
below, aligned to eight bytes. Order is not semantically significant.

| Type | Entry | Purpose |
| --- | ---: | --- |
| `META` | 8 | provenance/warning key and value string offsets |
| `TRAK` | 128 | fixed track records |
| `CUES` | 64 | cues and loops |
| `GRID` | 40 | sparse tempo/downbeat segments |
| `PHRA` | 32 | optional phrase markers |
| `PLST` | 16 | playlist names and entry spans |
| `PLEN` | 8 | track index and retained playlist position |
| `STRS` | 1 | sorted NUL-terminated UTF-8 strings; offset zero is empty |

String offsets are relative to `STRS`. The first fields in `TRAK` are a
16-byte SHA-256 prefix fingerprint of normalized path/core audio identity and a
16-byte SHA-256 prefix source ID of `adapter + NUL + native ID`. The remaining
fields are:

```
path,title,artist,album,provenance string offsets (5*u32)
cue first/count, grid first/count, phrase first/count (6*u32)
sample_rate u32, duration_frames u64, bpm_milli u32
normalized_key u16, rating u8, flags u8, play_count u32, ARGB color u32
reserved[24]
```

`normalized_key` is chromatic `1=C ... 12=B`, OR `0x100` for minor; zero is
unknown. Rating is `0..5`, or `255` unknown. BPM is fixed-point BPM times 1000.
Durations and frame positions refer to source audio frames, not interleaved
samples. Exact exported duration seconds are converted to the nearest source
frame using deterministic round-half-up arithmetic (ties toward the greater
nonnegative frame); floating-point arithmetic is not used.

`CUES` stores `track_index, kind(1 cue/2 loop), flags, slot`, frame position and
length, exact rational position and length in seconds, label offset, and ARGB
color. A frame value of `UINT64_MAX` selects its rational `numerator/u32
denominator`; zero denominator means unavailable. `GRID` and `PHRA` use the same
frame-or-rational convention. Grid records contain BPM times 1000, signed beat
number (`1` is a downbeat; zero unknown), confidence `0..10000`, and flags.
Phrase records contain a kind string and confidence.

When source rate and duration are known, frame and rational positions must be
within the source duration. Cue position plus loop length must also fit. Readers
compare these values exactly and must not use overflow-prone fixed-width
cross-multiplication.

Version 1 bounds: 4,096 tracks; 64 cues, 256 grid segments, and 128 phrase
markers per track; 8,192 metadata entries; 256 playlists; 65,535 playlist
entries; 1,024-byte paths; 4,096-byte strings; 768 kHz sample rate; and
signed-63-bit duration frames.

The firmware reader is `src/Metadata/JaydMetadata.{h,cpp}`. It validates the
sidecar with bounded streaming reads, retains only the open file and eight
section descriptors, and exposes lookup by normalized path, fingerprint,
source ID, or track index. `Missing`, `Stale`, `Corrupt`, and `Unsupported`
remain distinct so callers can ignore rejected metadata and continue ordinary
AAC playback.

Run the converter-backed firmware parser fixtures on a host with:

```sh
python3 tools/jayd-library/firmware_reader_self_check.py
```

## Adapter provenance and limits

- **rekordbox** reads only exported `DJ_PLAYLISTS` XML. AlphaTheta/Pioneer
  documents XML playlist import and links the supported tag list at
  <https://rekordbox.com/en/support/developer/> and
  <https://cdn.rekordbox.com/files/20200410160904/xml_format_list.pdf>.
  This tool does not parse `master.db`, Device Library Plus, or OneLibrary.
- **Mixxx** opens SQLite with `mode=ro`. Stable library columns come from
  Mixxx's `trackschema.h`; cue storage and frame semantics come from
  `res/schema.xml` and `cuedao.cpp`:
  <https://github.com/mixxxdj/mixxx/blob/main/src/library/dao/trackschema.h>,
  <https://github.com/mixxxdj/mixxx/blob/main/res/schema.xml>, and
  <https://github.com/mixxxdj/mixxx/blob/main/src/library/dao/cuedao.cpp>.
  The `beats` BLOB is deliberately skipped with a warning rather than decoded.
- **Traktor** NML is best-effort and always warns with its NML version.
  Native Instruments publishes no schema verified by this project. The
  community `traktor-nml-utils` project likewise says its models were inferred
  from samples:
  <https://github.com/wolkenarchitekt/traktor-nml-utils#how-does-it-work>.
- **Engine DJ** databases are not opened. `engine-json` consumes only the
  interchange below, intended for a separately installed helper using
  `libdjinterop`. That community library currently describes Engine as its only
  supported database format:
  <https://xsco.github.io/libdjinterop/>.

No proprietary assets or schema dumps are included.

### Engine helper boundary

The helper writes UTF-8 JSON and this tool remains import-only:

```json
{
  "schema": "jayd-engine-interchange-1",
  "producer": "helper name/version and libdjinterop version",
  "warnings": [],
  "tracks": [{
    "source_id": "stable Engine-native identifier",
    "path": "relative/audio.aac",
    "title": "Title",
    "artist": "Artist",
    "album": "Album",
    "sample_rate": 44100,
    "duration_frames": 100000,
    "bpm": "128.000",
    "key": "8A",
    "rating": 4,
    "play_count": 2,
    "color": "#ff00aa",
    "cues": [{"kind": "cue", "slot": 0, "position_frames": 44100}],
    "beatgrid": [{"position_frames": 0, "bpm": "128", "beat_number": 1, "confidence": 10000}],
    "phrases": [{"position_frames": 0, "kind": "intro", "confidence": 10000}]
  }],
  "playlists": [{"name": "Set", "paths": ["relative/audio.aac"]}]
}
```

## Audio conversion manifest

The firmware's current song browser accepts `.aac` files
([`src/Screens/SongList/SongList.cpp`](../../src/Screens/SongList/SongList.cpp)).
Every import therefore writes deterministic
`<output>.convert.json`. Non-AAC paths are marked `needs_conversion`; the tool
does not transcode. A later converter must preserve each `source_id` and exact
timeline in seconds, validate its AAC output, and record the resulting sample
rate/frame mapping.
