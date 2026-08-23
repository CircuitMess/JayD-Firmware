#!/usr/bin/env python3
"""Import DJ-library exports into the bounded Jay-D metadata sidecar."""

from __future__ import annotations

import argparse
import binascii
import copy
import hashlib
import json
import os
import re
import sqlite3
import struct
import sys
import urllib.parse
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from decimal import Decimal, InvalidOperation
from fractions import Fraction
from pathlib import Path, PurePosixPath
from typing import Iterable


MAGIC = b"JAYDMETA"
VERSION = 1
SECTION_VERSION = 1
ENDIAN_TAG = 0x4C45
HEADER = struct.Struct("<8sHHHHIIIQQI16s")
SECTION = struct.Struct("<4sHHIIQ")
TRACK = struct.Struct("<16s16s5I6IIQI HBBII24s".replace(" ", ""))
CUE = struct.Struct("<IBBHQQQIQIII8s")
GRID = struct.Struct("<IQQIIiHH4s")
PHRASE = struct.Struct("<IQQIIHH")
PLAYLIST = struct.Struct("<IIII")
PLAYLIST_ENTRY = struct.Struct("<II")
META = struct.Struct("<II")

NO_FRAME = (1 << 64) - 1
MAX_FILE_SIZE = 32 * 1024 * 1024
MAX_SOURCE_SIZE = 256 * 1024 * 1024
MAX_TRACKS = 4096
MAX_CUES_PER_TRACK = 64
MAX_GRID_PER_TRACK = 256
MAX_PHRASES_PER_TRACK = 128
MAX_METADATA_ENTRIES = 8192
MAX_PLAYLISTS = 256
MAX_PLAYLIST_ENTRIES = 65535
MAX_PATH_BYTES = 1024
MAX_STRING_BYTES = 4096
MAX_SAMPLE_RATE = 768000
MAX_DURATION_FRAMES = (1 << 63) - 1
MAX_BPM_MILLI = 999_999
SUPPORTED_AUDIO = {".aac"}

SECTION_ORDER = (b"META", b"TRAK", b"CUES", b"GRID", b"PHRA", b"PLST", b"PLEN", b"STRS")
SECTION_ENTRY_SIZES = {
    b"META": META.size,
    b"TRAK": TRACK.size,
    b"CUES": CUE.size,
    b"GRID": GRID.size,
    b"PHRA": PHRASE.size,
    b"PLST": PLAYLIST.size,
    b"PLEN": PLAYLIST_ENTRY.size,
    b"STRS": 1,
}


class FormatError(ValueError):
    pass


@dataclass
class CuePoint:
    kind: int = 1
    slot: int = 0xFFFF
    position_frames: int | None = None
    length_frames: int | None = None
    position_seconds: Fraction | None = None
    length_seconds: Fraction | None = None
    label: str = ""
    color: int = 0
    flags: int = 0


@dataclass
class GridSegment:
    position_frames: int | None = None
    position_seconds: Fraction | None = None
    bpm_milli: int = 0
    beat_number: int = 0
    confidence: int = 0
    flags: int = 0


@dataclass
class PhraseMarker:
    kind: str
    position_frames: int | None = None
    position_seconds: Fraction | None = None
    confidence: int = 0
    flags: int = 0


@dataclass
class TrackData:
    path: str
    native_id: str
    source: str
    title: str = ""
    artist: str = ""
    album: str = ""
    provenance: str = ""
    sample_rate: int = 0
    duration_frames: int = 0
    bpm_milli: int = 0
    key: int = 0
    rating: int = 0xFF
    play_count: int = 0
    color: int = 0
    cues: list[CuePoint] = field(default_factory=list)
    grid: list[GridSegment] = field(default_factory=list)
    phrases: list[PhraseMarker] = field(default_factory=list)
    source_id: bytes = b""
    fingerprint: bytes = b""


@dataclass
class PlaylistData:
    name: str
    paths: list[str]


@dataclass
class LibraryData:
    tracks: list[TrackData]
    playlists: list[PlaylistData] = field(default_factory=list)
    metadata: dict[str, str] = field(default_factory=dict)
    warnings: list[str] = field(default_factory=list)


def _limited_text(value: object, label: str, maximum: int = MAX_STRING_BYTES) -> str:
    text = "" if value is None else str(value).strip()
    if "\x00" in text:
        raise FormatError(f"{label} contains a NUL byte")
    if len(text.encode("utf-8")) > maximum:
        raise FormatError(f"{label} exceeds {maximum} UTF-8 bytes")
    return text


def _int(value: object, default: int = 0, minimum: int = 0, maximum: int = (1 << 32) - 1) -> int:
    if value in (None, ""):
        return default
    try:
        number = int(value)
    except (TypeError, ValueError) as exc:
        raise FormatError(f"invalid integer {value!r}") from exc
    if not minimum <= number <= maximum:
        raise FormatError(f"integer {number} outside {minimum}..{maximum}")
    return number


def _fraction(value: object, scale: int = 1) -> Fraction | None:
    if value in (None, ""):
        return None
    try:
        decimal = Decimal(str(value))
        if not decimal.is_finite():
            raise ValueError("non-finite value")
        return Fraction(decimal) * scale
    except (InvalidOperation, OverflowError, ValueError, ZeroDivisionError) as exc:
        raise FormatError(f"invalid exact time {value!r}") from exc


def _bpm(value: object) -> int:
    if value in (None, ""):
        return 0
    try:
        result = int((Decimal(str(value)) * 1000).to_integral_value())
    except InvalidOperation as exc:
        raise FormatError(f"invalid BPM {value!r}") from exc
    if not 0 <= result <= MAX_BPM_MILLI:
        raise FormatError(f"BPM {value!r} is outside the supported range")
    return result


def _rating(value: object, scale_255: bool = False) -> int:
    if value in (None, ""):
        return 0xFF
    maximum = 255 if scale_255 else 5
    raw = _int(value, maximum=maximum)
    return min(5, (raw + 25) // 51) if scale_255 else raw


def _color(value: object) -> int:
    if value in (None, ""):
        return 0
    text = str(value).strip()
    if text.startswith("#"):
        text = text[1:]
        if len(text) == 6:
            return 0xFF000000 | int(text, 16)
        if len(text) == 8:
            return int(text, 16)
        raise FormatError(f"invalid color {value!r}")
    number = _int(text)
    return number if number > 0xFFFFFF else 0xFF000000 | number


_KEYS = {
    "C": 1, "C#": 2, "DB": 2, "D": 3, "D#": 4, "EB": 4, "E": 5, "F": 6,
    "F#": 7, "GB": 7, "G": 8, "G#": 9, "AB": 9, "A": 10, "A#": 11,
    "BB": 11, "B": 12,
}
_CAMELOT_PITCH = {
    (1, "B"): 12, (2, "B"): 7, (3, "B"): 2, (4, "B"): 9,
    (5, "B"): 4, (6, "B"): 11, (7, "B"): 6, (8, "B"): 1,
    (9, "B"): 8, (10, "B"): 3, (11, "B"): 10, (12, "B"): 5,
    (1, "A"): 9, (2, "A"): 4, (3, "A"): 11, (4, "A"): 6,
    (5, "A"): 1, (6, "A"): 8, (7, "A"): 3, (8, "A"): 10,
    (9, "A"): 5, (10, "A"): 12, (11, "A"): 7, (12, "A"): 2,
}


def _key(value: object) -> int:
    text = str(value or "").strip().upper().replace("♯", "#").replace("♭", "B")
    if not text:
        return 0
    camelot = re.fullmatch(r"(1[0-2]|[1-9])([AB])", text)
    if camelot:
        number, mode = int(camelot.group(1)), camelot.group(2)
        return _CAMELOT_PITCH[(number, mode)] | (0x100 if mode == "A" else 0)
    open_key = re.fullmatch(r"(1[0-2]|[1-9])([DM])", text)
    if open_key:
        number, mode = int(open_key.group(1)), open_key.group(2)
        camelot_number = ((number + 6) % 12) + 1
        camelot_mode = "A" if mode == "M" else "B"
        return _CAMELOT_PITCH[(camelot_number, camelot_mode)] | (
            0x100 if camelot_mode == "A" else 0
        )
    minor = text.endswith("M") or "MIN" in text
    note = re.match(r"^[A-G](?:#|B)?", text)
    return (_KEYS.get(note.group(0), 0) | (0x100 if minor else 0)) if note else 0


def normalize_path(raw: str, root: Path | None = None) -> str:
    raw = str(raw).strip()
    if raw.lower().startswith("file:"):
        parsed = urllib.parse.urlparse(raw)
        if parsed.scheme.lower() != "file" or parsed.netloc not in ("", "localhost"):
            raise FormatError(f"unsupported file URL authority in {raw!r}")
        raw = urllib.parse.unquote(parsed.path)
    if "\x00" in raw:
        raise FormatError("path contains a NUL byte")
    raw = raw.replace("\\", "/")
    if re.match(r"^[A-Za-z]:/", raw):
        raise FormatError(f"Windows absolute path requires export remapping before import: {raw!r}")
    if raw.startswith("/"):
        if root is None:
            raise FormatError(f"absolute path requires --root: {raw!r}")
        source = Path(raw).resolve(strict=False)
        try:
            raw = source.relative_to(root.resolve(strict=False)).as_posix()
        except ValueError as exc:
            raise FormatError(f"path is outside --root: {raw!r}") from exc
    if any(part in ("", ".", "..") for part in raw.split("/")):
        raise FormatError(f"path must be normalized and relative: {raw!r}")
    path = PurePosixPath(raw)
    if path.is_absolute():
        raise FormatError(f"path must be normalized and relative: {raw!r}")
    normalized = path.as_posix()
    if len(normalized.encode("utf-8")) > MAX_PATH_BYTES:
        raise FormatError(f"path exceeds {MAX_PATH_BYTES} UTF-8 bytes")
    return normalized


def _file_guard(path: Path) -> None:
    size = path.stat().st_size
    if size > MAX_SOURCE_SIZE:
        raise FormatError(f"{path} exceeds the {MAX_SOURCE_SIZE}-byte source limit")


def _time_fields(seconds: Fraction | None, sample_rate: int) -> tuple[int | None, Fraction | None]:
    if seconds is None:
        return None, None
    frames = seconds * sample_rate if sample_rate else None
    if frames is not None and frames.denominator == 1:
        return int(frames), None
    return None, seconds


def _duration_frames(seconds: Fraction | None, sample_rate: int) -> int:
    if seconds is None:
        return 0
    if seconds < 0:
        raise FormatError("duration cannot be negative")
    if not sample_rate:
        if seconds:
            raise FormatError("duration requires a source sample rate")
        return 0
    frames = seconds * sample_rate
    # Exact round-half-up keeps exports deterministic without float error.
    rounded = (2 * frames.numerator + frames.denominator) // (2 * frames.denominator)
    if rounded > MAX_DURATION_FRAMES:
        raise FormatError(f"duration exceeds {MAX_DURATION_FRAMES} source frames")
    return rounded


def _source_duration(
    value: object,
    sample_rate: int,
    warnings: list[str],
    context: str,
) -> int:
    try:
        return _duration_frames(_fraction(value), sample_rate)
    except FormatError as exc:
        warnings.append(f"Skipped unrepresentable {context}: {exc}")
        return 0


def _track(path: str, native_id: object, source: str, **values: object) -> TrackData:
    track = TrackData(path=path, native_id=_limited_text(native_id, "native ID"), source=source)
    for name, value in values.items():
        setattr(track, name, value)
    track.provenance = track.provenance or source
    return track


def import_m3u(path: Path, root: Path | None) -> LibraryData:
    _file_guard(path)
    lines = path.read_text(encoding="utf-8-sig").splitlines()
    tracks: list[TrackData] = []
    playlist_paths: list[str] = []
    pending_title = ""
    for line_number, line in enumerate(lines, 1):
        line = line.strip()
        if not line:
            continue
        if line.startswith("#EXTINF:"):
            pending_title = line.partition(",")[2].strip()
            continue
        if line.startswith("#"):
            continue
        try:
            normalized = normalize_path(line, root)
        except FormatError as exc:
            raise FormatError(f"{path}:{line_number}: {exc}") from exc
        tracks.append(_track(normalized, normalized, "m3u", title=pending_title, provenance="m3u/m3u8"))
        playlist_paths.append(normalized)
        pending_title = ""
    return LibraryData(
        tracks,
        [PlaylistData(_limited_text(path.stem, "playlist name"), playlist_paths)],
        {"adapter": "m3u", "source_format": "M3U/M3U8"},
    )


def import_rekordbox(path: Path, root: Path | None) -> LibraryData:
    _file_guard(path)
    tracks: list[TrackData] = []
    id_to_path: dict[str, str] = {}
    playlist_stack: list[tuple[str, list[str]]] = []
    playlists: list[PlaylistData] = []
    warnings: list[str] = []
    product_version = ""
    try:
        iterator = ET.iterparse(path, events=("start", "end"))
        for event, element in iterator:
            tag = element.tag.rsplit("}", 1)[-1]
            if event == "start" and tag == "NODE":
                playlist_stack.append((_limited_text(element.get("Name"), "playlist name"), []))
                continue
            if event == "end" and tag == "PRODUCT":
                product_version = _limited_text(element.get("Version"), "rekordbox version")
            elif event == "end" and tag == "TRACK" and element.get("Location"):
                normalized = normalize_path(element.get("Location", ""), root)
                sample_rate = _int(element.get("SampleRate"), maximum=MAX_SAMPLE_RATE)
                track = _track(
                    normalized,
                    element.get("TrackID") or normalized,
                    "rekordbox",
                    title=_limited_text(element.get("Name"), "title"),
                    artist=_limited_text(element.get("Artist"), "artist"),
                    album=_limited_text(element.get("Album"), "album"),
                    provenance=f"rekordbox XML {product_version or 'unknown'}",
                    sample_rate=sample_rate,
                    duration_frames=_source_duration(
                        element.get("TotalTime"), sample_rate, warnings, f"duration for {normalized}"
                    ),
                    bpm_milli=_bpm(element.get("AverageBpm")),
                    key=_key(element.get("Tonality")),
                    rating=_rating(element.get("Rating"), scale_255=True),
                    play_count=_int(element.get("PlayCount")),
                    color=_color(element.get("Colour")),
                )
                for child in element:
                    child_tag = child.tag.rsplit("}", 1)[-1]
                    if child_tag == "POSITION_MARK":
                        start = _fraction(child.get("Start"))
                        end = _fraction(child.get("End"))
                        start_frame, start_time = _time_fields(start, sample_rate)
                        length = end - start if start is not None and end is not None and end >= start else None
                        length_frame, length_time = _time_fields(length, sample_rate)
                        kind = 2 if _int(child.get("Type"), maximum=255) == 4 or length else 1
                        track.cues.append(CuePoint(
                            kind=kind,
                            slot=_int(child.get("Num"), default=0xFFFF, maximum=0xFFFF),
                            position_frames=start_frame,
                            length_frames=length_frame,
                            position_seconds=start_time,
                            length_seconds=length_time,
                            label=_limited_text(child.get("Name"), "cue label"),
                            color=_color(child.get("Red") and (
                                (int(child.get("Red", "0")) << 16)
                                | (int(child.get("Green", "0")) << 8)
                                | int(child.get("Blue", "0"))
                            )),
                        ))
                    elif child_tag == "TEMPO":
                        start = _fraction(child.get("Inizio"))
                        start_frame, start_time = _time_fields(start, sample_rate)
                        track.grid.append(GridSegment(
                            position_frames=start_frame,
                            position_seconds=start_time,
                            bpm_milli=_bpm(child.get("Bpm")),
                            beat_number=_int(child.get("Battito"), maximum=32),
                            confidence=10000,
                        ))
                tracks.append(track)
                id_to_path[track.native_id] = normalized
            elif event == "end" and tag == "TRACK" and element.get("Key") and playlist_stack:
                referenced = id_to_path.get(element.get("Key", ""))
                if referenced:
                    playlist_stack[-1][1].append(referenced)
            elif event == "end" and tag == "NODE" and playlist_stack:
                name, paths = playlist_stack.pop()
                if paths:
                    qualified = "/".join([entry[0] for entry in playlist_stack] + [name])
                    playlists.append(PlaylistData(qualified, paths))
            if event == "end" and tag in {"TRACK", "NODE", "PRODUCT", "COLLECTION", "PLAYLISTS"}:
                element.clear()
    except ET.ParseError as exc:
        raise FormatError(f"invalid rekordbox XML: {exc}") from exc
    return LibraryData(
        tracks,
        playlists,
        {"adapter": "rekordbox", "source_format": "DJ_PLAYLISTS XML", "source_version": product_version},
        warnings,
    )


def import_traktor(path: Path, root: Path | None) -> LibraryData:
    _file_guard(path)
    tracks: list[TrackData] = []
    nml_version = ""
    warnings: list[str] = []
    try:
        for event, element in ET.iterparse(path, events=("start", "end")):
            tag = element.tag.rsplit("}", 1)[-1]
            if event == "start" and tag == "NML":
                nml_version = _limited_text(element.get("VERSION"), "NML version")
            if event != "end" or tag != "ENTRY":
                continue
            location = element.find("./LOCATION")
            if location is None:
                element.clear()
                continue
            volume = location.get("VOLUME", "")
            directory = location.get("DIR", "").replace("/:", "/")
            if not volume:
                directory = directory.lstrip("/")
            raw_path = "".join((volume, directory, location.get("FILE", "")))
            normalized = normalize_path(raw_path, root)
            info = element.find("./INFO")
            audio = element.find("./AUDIO_INFO")
            tempo = element.find("./TEMPO")
            album = element.find("./ALBUM")
            musical_key = element.find("./MUSICAL_KEY")
            sample_rate = _int(audio.get("SAMPLE_RATE") if audio is not None else None, maximum=MAX_SAMPLE_RATE)
            duration = (
                (info.get("PLAYTIME_FLOAT") if info is not None else None)
                or (info.get("PLAYTIME") if info is not None else None)
            )
            track = _track(
                normalized,
                element.get("AUDIO_ID") or normalized,
                "traktor",
                title=_limited_text(element.get("TITLE"), "title"),
                artist=_limited_text(element.get("ARTIST"), "artist"),
                album=_limited_text(
                    (album.get("TITLE") if album is not None else None)
                    or (info.get("ALBUM") if info is not None else None),
                    "album",
                ),
                provenance=f"Traktor NML {nml_version or 'unknown'} (best effort)",
                sample_rate=sample_rate,
                duration_frames=_source_duration(
                    duration, sample_rate, warnings, f"duration for {normalized}"
                ),
                bpm_milli=_bpm(tempo.get("BPM") if tempo is not None else None),
                key=_key(info.get("KEY") if info is not None else None),
                rating=_rating(info.get("RANKING") if info is not None else None, scale_255=True),
                play_count=_int(info.get("PLAYCOUNT") if info is not None else None),
                color=_color(info.get("COLOR") if info is not None else None),
            )
            if not track.key and musical_key is not None and musical_key.get("VALUE"):
                warnings.append(
                    f"Skipped unverified Traktor MUSICAL_KEY={musical_key.get('VALUE')} on {normalized}"
                )
            for cue in element.findall("./CUE_V2"):
                start = _fraction(cue.get("START"), scale=Fraction(1, 1000))
                length = _fraction(cue.get("LEN"), scale=Fraction(1, 1000))
                start_frame, start_time = _time_fields(start, sample_rate)
                length_frame, length_time = _time_fields(length, sample_rate)
                cue_type = _int(cue.get("TYPE"), maximum=255)
                if cue_type == 4:
                    track.grid.append(GridSegment(
                        position_frames=start_frame,
                        position_seconds=start_time,
                        bpm_milli=track.bpm_milli,
                        beat_number=1,
                        confidence=5000,
                        flags=1,
                    ))
                else:
                    track.cues.append(CuePoint(
                        kind=2 if length and length > 0 else 1,
                        slot=_int(cue.get("HOTCUE"), default=0xFFFF, minimum=-1, maximum=0xFFFF)
                        if cue.get("HOTCUE") != "-1" else 0xFFFF,
                        position_frames=start_frame,
                        length_frames=length_frame,
                        position_seconds=start_time,
                        length_seconds=length_time,
                        label=_limited_text(cue.get("NAME"), "cue label"),
                    ))
            tracks.append(track)
            element.clear()
    except ET.ParseError as exc:
        raise FormatError(f"invalid Traktor NML: {exc}") from exc
    warnings.append(
        f"Traktor NML VERSION={nml_version or 'unknown'} is best-effort: "
        "Native Instruments publishes no NML schema; verify imported cues and grids."
    )
    return LibraryData(
        tracks,
        metadata={"adapter": "traktor", "source_format": "NML", "source_version": nml_version},
        warnings=warnings,
    )


def _sqlite_columns(connection: sqlite3.Connection, table: str) -> set[str]:
    return {str(row[1]) for row in connection.execute(f'PRAGMA table_info("{table}")')}


def import_mixxx(path: Path, root: Path | None) -> LibraryData:
    _file_guard(path)
    connection = sqlite3.connect(f"file:{urllib.parse.quote(str(path))}?mode=ro", uri=True)
    connection.row_factory = sqlite3.Row
    warnings: list[str] = []
    try:
        tables = {row[0] for row in connection.execute("SELECT name FROM sqlite_master WHERE type='table'")}
        if not {"library", "track_locations"} <= tables:
            raise FormatError("Mixxx database is missing library or track_locations")
        columns = _sqlite_columns(connection, "library")
        wanted = [
            "id", "title", "artist", "album", "duration", "bpm", "samplerate",
            "timesplayed", "rating", "key", "color", "beats", "beats_version",
        ]
        selected = [column for column in wanted if column in columns]
        query = (
            f"SELECT {', '.join('l.' + column for column in selected)}, "
            "tl.location AS path FROM library l "
            "JOIN track_locations tl ON tl.id = l.location "
            "ORDER BY l.id"
        )
        tracks: list[TrackData] = []
        by_id: dict[int, TrackData] = {}
        opaque_beats = 0
        for row in connection.execute(query):
            normalized = normalize_path(row["path"], root)
            sample_rate = _int(row["samplerate"] if "samplerate" in row.keys() else None, maximum=MAX_SAMPLE_RATE)
            duration = row["duration"] if "duration" in row.keys() else None
            track = _track(
                normalized,
                row["id"],
                "mixxx",
                title=_limited_text(row["title"] if "title" in row.keys() else None, "title"),
                artist=_limited_text(row["artist"] if "artist" in row.keys() else None, "artist"),
                album=_limited_text(row["album"] if "album" in row.keys() else None, "album"),
                provenance="Mixxx SQLite",
                sample_rate=sample_rate,
                duration_frames=_source_duration(
                    duration, sample_rate, warnings, f"duration for {normalized}"
                ),
                bpm_milli=_bpm(row["bpm"] if "bpm" in row.keys() else None),
                key=_key(row["key"] if "key" in row.keys() else None),
                rating=_rating(row["rating"] if "rating" in row.keys() else None),
                play_count=_int(row["timesplayed"] if "timesplayed" in row.keys() else None),
                color=_color(row["color"] if "color" in row.keys() else None),
            )
            if "beats" in row.keys() and row["beats"] is not None:
                opaque_beats += 1
            tracks.append(track)
            by_id[int(row["id"])] = track
        if "cues" in tables:
            cue_columns = _sqlite_columns(connection, "cues")
            needed = {"track_id", "type", "position", "length"}
            if needed <= cue_columns:
                optional = [name for name in ("hotcue", "label", "color") if name in cue_columns]
                cue_query = "SELECT track_id,type,position,length"
                cue_query += "".join(f",{name}" for name in optional)
                cue_query += " FROM cues ORDER BY track_id,id"
                for row in connection.execute(cue_query):
                    track = by_id.get(int(row["track_id"]))
                    if track is None or row["position"] is None or float(row["position"]) < 0:
                        continue
                    position_frames = Fraction(Decimal(str(row["position"]))) / 2
                    length_frames = Fraction(Decimal(str(row["length"] or 0))) / 2
                    if position_frames.denominator == 1:
                        frame, position_time = int(position_frames), None
                    elif track.sample_rate:
                        frame, position_time = None, position_frames / track.sample_rate
                    else:
                        frame, position_time = None, None
                        warnings.append(f"Mixxx cue on track {track.native_id} lost sub-frame position without sample rate")
                    if length_frames.denominator == 1:
                        length_frame, length_time = int(length_frames), None
                    elif track.sample_rate:
                        length_frame, length_time = None, length_frames / track.sample_rate
                    else:
                        length_frame, length_time = None, None
                    track.cues.append(CuePoint(
                        kind=2 if int(row["type"]) == 4 else 1,
                        slot=_int(row["hotcue"], default=0xFFFF, minimum=-1, maximum=0xFFFF)
                        if "hotcue" in optional and row["hotcue"] != -1 else 0xFFFF,
                        position_frames=frame,
                        length_frames=length_frame,
                        position_seconds=position_time,
                        length_seconds=length_time,
                        label=_limited_text(row["label"] if "label" in optional else None, "cue label"),
                        color=_color(row["color"] if "color" in optional else None),
                    ))
        if opaque_beats:
            warnings.append(
                f"Skipped opaque Mixxx beats BLOB metadata for {opaque_beats} track(s); "
                "the adapter does not guess at its serialized representation."
            )
        return LibraryData(
            tracks,
            metadata={"adapter": "mixxx", "source_format": "Mixxx SQLite"},
            warnings=warnings,
        )
    finally:
        connection.close()


def import_engine_json(path: Path, root: Path | None) -> LibraryData:
    _file_guard(path)
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise FormatError(f"invalid Engine interchange JSON: {exc}") from exc
    if document.get("schema") != "jayd-engine-interchange-1":
        raise FormatError("Engine JSON schema must be jayd-engine-interchange-1")
    tracks: list[TrackData] = []
    warnings = [_limited_text(warning, "warning") for warning in document.get("warnings", [])]
    for item in document.get("tracks", []):
        normalized = normalize_path(item["path"], root)
        sample_rate = _int(item.get("sample_rate"), maximum=MAX_SAMPLE_RATE)
        track = _track(
            normalized,
            item.get("source_id") or normalized,
            "engine-json",
            title=_limited_text(item.get("title"), "title"),
            artist=_limited_text(item.get("artist"), "artist"),
            album=_limited_text(item.get("album"), "album"),
            provenance=_limited_text(document.get("producer"), "producer"),
            sample_rate=sample_rate,
            duration_frames=_int(item.get("duration_frames"), maximum=MAX_DURATION_FRAMES),
            bpm_milli=_bpm(item.get("bpm")),
            key=_key(item.get("key")),
            rating=_rating(item.get("rating")),
            play_count=_int(item.get("play_count")),
            color=_color(item.get("color")),
        )
        for cue in item.get("cues", []):
            if cue.get("position_frames") is None:
                warnings.append(f"Skipped Engine cue without position on {normalized}")
                continue
            track.cues.append(CuePoint(
                kind=2 if cue.get("kind") == "loop" else 1,
                slot=_int(cue.get("slot"), default=0xFFFF, maximum=0xFFFF),
                position_frames=_int(cue.get("position_frames"), maximum=MAX_DURATION_FRAMES),
                length_frames=_int(cue.get("length_frames"), maximum=MAX_DURATION_FRAMES),
                label=_limited_text(cue.get("label"), "cue label"),
                color=_color(cue.get("color")),
            ))
        for segment in item.get("beatgrid", []):
            if segment.get("position_frames") is None:
                warnings.append(f"Skipped Engine beatgrid segment without position on {normalized}")
                continue
            track.grid.append(GridSegment(
                position_frames=_int(segment.get("position_frames"), maximum=MAX_DURATION_FRAMES),
                bpm_milli=_bpm(segment.get("bpm")),
                beat_number=_int(segment.get("beat_number"), maximum=32),
                confidence=_int(segment.get("confidence"), maximum=10000),
            ))
        for phrase in item.get("phrases", []):
            if phrase.get("position_frames") is None:
                warnings.append(f"Skipped Engine phrase without position on {normalized}")
                continue
            track.phrases.append(PhraseMarker(
                kind=_limited_text(phrase.get("kind"), "phrase kind"),
                position_frames=_int(phrase.get("position_frames"), maximum=MAX_DURATION_FRAMES),
                confidence=_int(phrase.get("confidence"), maximum=10000),
            ))
        tracks.append(track)
    playlists = [
        PlaylistData(
            _limited_text(item.get("name"), "playlist name"),
            [normalize_path(entry, root) for entry in item.get("paths", [])],
        )
        for item in document.get("playlists", [])
    ]
    return LibraryData(
        tracks,
        playlists,
        {
            "adapter": "engine-json",
            "source_format": "jayd-engine-interchange-1",
            "producer": _limited_text(document.get("producer"), "producer"),
        },
        warnings,
    )


def _track_score(track: TrackData) -> tuple[int, str]:
    score = sum(bool(value) for value in (
        track.title, track.artist, track.album, track.sample_rate, track.duration_frames,
        track.bpm_milli, track.key, track.play_count, track.color,
    )) + len(track.cues) + len(track.grid) + len(track.phrases)
    tie = repr(track)
    return score, tie


def _timeline_frames(
    frame: int | None,
    seconds: Fraction | None,
    sample_rate: int,
    required: bool,
    label: str,
) -> Fraction | None:
    if frame is not None:
        if not 0 <= frame <= MAX_DURATION_FRAMES:
            raise FormatError(f"{label} frame position exceeds bounds")
        return Fraction(frame)
    if seconds is None:
        if required:
            raise FormatError(f"{label} has no frame or rational position")
        return None
    if seconds < 0:
        raise FormatError(f"{label} rational position is negative")
    return seconds * sample_rate if sample_rate else None


def _validate_track_timeline(track: TrackData) -> None:
    if track.duration_frames and not track.sample_rate:
        raise FormatError(f"{track.path}: duration frames require a sample rate")

    def position(frame: int | None, seconds: Fraction | None, label: str) -> Fraction | None:
        value = _timeline_frames(frame, seconds, track.sample_rate, True, label)
        if value is not None and track.duration_frames and value > track.duration_frames:
            raise FormatError(f"{label} exceeds track duration")
        return value

    for cue in track.cues:
        cue_position = position(cue.position_frames, cue.position_seconds, f"{track.path}: cue position")
        cue_length = _timeline_frames(
            cue.length_frames, cue.length_seconds, track.sample_rate, False,
            f"{track.path}: cue length",
        )
        if (cue_position is not None and cue_length is not None and track.duration_frames
                and cue_position + cue_length > track.duration_frames):
            raise FormatError(f"{track.path}: cue position and length exceed track duration")
    for segment in track.grid:
        position(segment.position_frames, segment.position_seconds, f"{track.path}: beatgrid position")
    for phrase in track.phrases:
        position(phrase.position_frames, phrase.position_seconds, f"{track.path}: phrase position")


def _decoded_time(
    frame: int,
    numerator: int,
    denominator: int,
    required: bool,
    label: str,
) -> tuple[int | None, Fraction | None]:
    if denominator == 0:
        if numerator or (frame == NO_FRAME and required):
            raise FormatError(f"{label} has an invalid rational position")
        return (None if frame == NO_FRAME else frame), None
    return (None if frame == NO_FRAME else frame), Fraction(numerator, denominator)


def finalize(library: LibraryData) -> LibraryData:
    library = copy.deepcopy(library)
    if len(library.tracks) > MAX_TRACKS:
        raise FormatError(f"track count exceeds {MAX_TRACKS}")
    for track in library.tracks:
        track.cues.sort(key=lambda cue: (
            cue.position_frames if cue.position_frames is not None else NO_FRAME,
            cue.position_seconds or Fraction(0), cue.kind, cue.slot, cue.label,
        ))
        track.grid.sort(key=lambda grid: (
            grid.position_frames if grid.position_frames is not None else NO_FRAME,
            grid.position_seconds or Fraction(0), grid.bpm_milli,
        ))
        track.phrases.sort(key=lambda phrase: (
            phrase.position_frames if phrase.position_frames is not None else NO_FRAME,
            phrase.position_seconds or Fraction(0), phrase.kind,
        ))
    by_path: dict[str, TrackData] = {}
    duplicate_count = 0
    for track in library.tracks:
        track.path = normalize_path(track.path)
        current = by_path.get(track.path)
        if current is None or _track_score(track) > _track_score(current):
            by_path[track.path] = track
        if current is not None:
            duplicate_count += 1
    tracks = sorted(by_path.values(), key=lambda item: item.path.encode("utf-8"))
    for track in tracks:
        if len(track.cues) > MAX_CUES_PER_TRACK:
            raise FormatError(f"{track.path}: cue count exceeds {MAX_CUES_PER_TRACK}")
        if len(track.grid) > MAX_GRID_PER_TRACK:
            raise FormatError(f"{track.path}: beatgrid count exceeds {MAX_GRID_PER_TRACK}")
        if len(track.phrases) > MAX_PHRASES_PER_TRACK:
            raise FormatError(f"{track.path}: phrase count exceeds {MAX_PHRASES_PER_TRACK}")
        if not 0 <= track.sample_rate <= MAX_SAMPLE_RATE:
            raise FormatError(f"{track.path}: sample rate out of range")
        if not 0 <= track.duration_frames <= MAX_DURATION_FRAMES:
            raise FormatError(f"{track.path}: duration frames out of range")
        _validate_track_timeline(track)
        if not track.source_id:
            track.source_id = hashlib.sha256(
                f"{track.source}\0{track.native_id}".encode("utf-8")
            ).digest()[:16]
        if not track.fingerprint:
            identity = json.dumps({
                "path": track.path, "title": track.title, "artist": track.artist,
                "album": track.album, "sample_rate": track.sample_rate,
                "duration_frames": track.duration_frames,
            }, sort_keys=True, ensure_ascii=False, separators=(",", ":"))
            track.fingerprint = hashlib.sha256(identity.encode("utf-8")).digest()[:16]
    path_set = set(by_path)
    playlists: list[PlaylistData] = []
    for playlist in library.playlists:
        kept = [entry for entry in playlist.paths if entry in path_set]
        if kept:
            playlists.append(PlaylistData(playlist.name, kept))
    playlists.sort(key=lambda item: (item.name.encode("utf-8"), tuple(item.paths)))
    if len(playlists) > MAX_PLAYLISTS:
        raise FormatError(f"playlist count exceeds {MAX_PLAYLISTS}")
    if sum(len(item.paths) for item in playlists) > MAX_PLAYLIST_ENTRIES:
        raise FormatError(f"playlist entry count exceeds {MAX_PLAYLIST_ENTRIES}")
    warnings = sorted(set(library.warnings))
    if duplicate_count:
        warnings.append(f"Resolved {duplicate_count} duplicate path(s) by metadata completeness, then lexical tie-break.")
    metadata = {str(key): _limited_text(value, f"metadata {key}") for key, value in library.metadata.items()}
    metadata["sidecar_format"] = "JAYDMETA/1"
    return LibraryData(tracks, playlists, metadata, sorted(warnings))


class StringTable:
    def __init__(self, values: Iterable[str]):
        encoded = sorted({value.encode("utf-8") for value in values if value})
        self.data = bytearray(b"\0")
        self.offsets = {"": 0}
        for value in encoded:
            if len(value) > MAX_STRING_BYTES:
                raise FormatError("string exceeds the sidecar limit")
            self.offsets[value.decode("utf-8")] = len(self.data)
            self.data.extend(value)
            self.data.append(0)

    def offset(self, value: str) -> int:
        return self.offsets[value]


def _fraction_parts(value: Fraction | None) -> tuple[int, int]:
    if value is None:
        return 0, 0
    if value < 0 or value.numerator > NO_FRAME or value.denominator > 0xFFFFFFFF:
        raise FormatError("rational time exceeds sidecar integer bounds")
    return value.numerator, value.denominator


def encode(library: LibraryData) -> bytes:
    library = finalize(library)
    strings: list[str] = []
    for track in library.tracks:
        strings.extend((track.path, track.title, track.artist, track.album, track.provenance))
        strings.extend(cue.label for cue in track.cues)
        strings.extend(phrase.kind for phrase in track.phrases)
    for playlist in library.playlists:
        strings.append(playlist.name)
    for key, value in library.metadata.items():
        strings.extend((key, value))
    for index, warning in enumerate(library.warnings):
        strings.extend((f"warning.{index}", warning))
    string_table = StringTable(strings)

    metadata = dict(library.metadata)
    metadata.update({f"warning.{index}": warning for index, warning in enumerate(library.warnings)})
    if len(metadata) > MAX_METADATA_ENTRIES:
        raise FormatError(f"metadata entry count exceeds {MAX_METADATA_ENTRIES}")
    meta_payload = b"".join(
        META.pack(string_table.offset(key), string_table.offset(value))
        for key, value in sorted(metadata.items())
    )

    cue_payload = bytearray()
    grid_payload = bytearray()
    phrase_payload = bytearray()
    track_payload = bytearray()
    cue_index = grid_index = phrase_index = 0
    for track_index, track in enumerate(library.tracks):
        first_cue, first_grid, first_phrase = cue_index, grid_index, phrase_index
        for cue in track.cues:
            pos_num, pos_den = _fraction_parts(cue.position_seconds)
            len_num, len_den = _fraction_parts(cue.length_seconds)
            cue_payload.extend(CUE.pack(
                track_index, cue.kind, cue.flags, cue.slot,
                cue.position_frames if cue.position_frames is not None else NO_FRAME,
                cue.length_frames if cue.length_frames is not None else NO_FRAME,
                pos_num, pos_den, len_num, len_den,
                string_table.offset(cue.label), cue.color, b"\0" * 8,
            ))
            cue_index += 1
        for segment in track.grid:
            pos_num, pos_den = _fraction_parts(segment.position_seconds)
            grid_payload.extend(GRID.pack(
                track_index,
                segment.position_frames if segment.position_frames is not None else NO_FRAME,
                pos_num, pos_den, segment.bpm_milli, segment.beat_number,
                segment.confidence, segment.flags, b"\0" * 4,
            ))
            grid_index += 1
        for phrase in track.phrases:
            pos_num, pos_den = _fraction_parts(phrase.position_seconds)
            phrase_payload.extend(PHRASE.pack(
                track_index,
                phrase.position_frames if phrase.position_frames is not None else NO_FRAME,
                pos_num, pos_den, string_table.offset(phrase.kind),
                phrase.confidence, phrase.flags,
            ))
            phrase_index += 1
        track_payload.extend(TRACK.pack(
            track.fingerprint, track.source_id,
            string_table.offset(track.path), string_table.offset(track.title),
            string_table.offset(track.artist), string_table.offset(track.album),
            string_table.offset(track.provenance),
            first_cue, len(track.cues), first_grid, len(track.grid),
            first_phrase, len(track.phrases),
            track.sample_rate, track.duration_frames, track.bpm_milli, track.key,
            track.rating, 0, track.play_count, track.color, b"\0" * 24,
        ))

    track_by_path = {track.path: index for index, track in enumerate(library.tracks)}
    playlist_payload = bytearray()
    playlist_entry_payload = bytearray()
    playlist_entry_index = 0
    for playlist in library.playlists:
        playlist_payload.extend(PLAYLIST.pack(
            string_table.offset(playlist.name), playlist_entry_index, len(playlist.paths), 0
        ))
        for position, path in enumerate(playlist.paths):
            playlist_entry_payload.extend(PLAYLIST_ENTRY.pack(track_by_path[path], position))
            playlist_entry_index += 1

    payloads = {
        b"META": bytes(meta_payload),
        b"TRAK": bytes(track_payload),
        b"CUES": bytes(cue_payload),
        b"GRID": bytes(grid_payload),
        b"PHRA": bytes(phrase_payload),
        b"PLST": bytes(playlist_payload),
        b"PLEN": bytes(playlist_entry_payload),
        b"STRS": bytes(string_table.data),
    }
    counts = {
        b"META": len(metadata), b"TRAK": len(library.tracks), b"CUES": cue_index,
        b"GRID": grid_index, b"PHRA": phrase_index, b"PLST": len(library.playlists),
        b"PLEN": playlist_entry_index, b"STRS": len(string_table.data),
    }
    directory_offset = HEADER.size
    offset = HEADER.size + len(SECTION_ORDER) * SECTION.size
    directory = bytearray()
    body = bytearray()
    for section_type in SECTION_ORDER:
        padding = (-offset) % 8
        body.extend(b"\0" * padding)
        offset += padding
        payload = payloads[section_type]
        directory.extend(SECTION.pack(
            section_type, SECTION_VERSION, SECTION_ENTRY_SIZES[section_type], counts[section_type],
            len(payload), offset,
        ))
        body.extend(payload)
        offset += len(payload)
    if offset > MAX_FILE_SIZE:
        raise FormatError(f"sidecar exceeds {MAX_FILE_SIZE} bytes")
    header = HEADER.pack(
        MAGIC, VERSION, ENDIAN_TAG, HEADER.size, SECTION.size, len(SECTION_ORDER),
        len(library.tracks), 0, offset, directory_offset, 0, b"\0" * 16,
    )
    output = bytearray(header + directory + body)
    crc = binascii.crc32(output) & 0xFFFFFFFF
    struct.pack_into("<I", output, 44, crc)
    return bytes(output)


def _decode_layout(data: bytes) -> dict[str, object]:
    if len(data) < HEADER.size:
        raise FormatError("sidecar is shorter than its header")
    (
        magic, version, endian, header_size, directory_entry_size, section_count,
        track_count, flags, file_size, directory_offset, stored_crc, reserved,
    ) = HEADER.unpack_from(data)
    if magic != MAGIC or version != VERSION or endian != ENDIAN_TAG:
        raise FormatError("unsupported magic, version, or endian tag")
    if header_size < HEADER.size or directory_entry_size < SECTION.size:
        raise FormatError("invalid versioned header or directory entry size")
    if file_size != len(data) or file_size > MAX_FILE_SIZE:
        raise FormatError("declared file size is invalid")
    if section_count > 64 or track_count > MAX_TRACKS or flags != 0:
        raise FormatError("header count or flags are unsupported")
    check = bytearray(data)
    struct.pack_into("<I", check, 44, 0)
    if binascii.crc32(check) & 0xFFFFFFFF != stored_crc:
        raise FormatError("CRC32 mismatch")
    if directory_offset < header_size:
        raise FormatError("section directory overlaps header")
    sections: dict[str, dict[str, int]] = {}
    ranges: list[tuple[int, int]] = []
    for index in range(section_count):
        entry_offset = directory_offset + index * directory_entry_size
        if entry_offset + SECTION.size > len(data):
            raise FormatError("section directory exceeds file")
        kind, section_version, entry_size, count, size, offset = SECTION.unpack_from(data, entry_offset)
        end = offset + size
        if offset < directory_offset + section_count * directory_entry_size or end > len(data):
            raise FormatError("section bounds are invalid")
        if any(offset < other_end and end > other_start for other_start, other_end in ranges):
            raise FormatError("sections overlap")
        ranges.append((offset, end))
        name = kind.decode("ascii", "replace")
        sections[name] = {
            "version": section_version, "entry_size": entry_size,
            "count": count, "size": size, "offset": offset,
        }
        expected = SECTION_ENTRY_SIZES.get(kind)
        if expected and section_version != SECTION_VERSION:
            raise FormatError(f"unsupported {name} section version {section_version}")
        if kind == b"META" and count > MAX_METADATA_ENTRIES:
            raise FormatError("META section count exceeds bounds")
        if expected and (entry_size < expected or count * entry_size > size):
            raise FormatError(f"{name} section shape is invalid")
        if kind == b"STRS" and (entry_size != 1 or count != size):
            raise FormatError("STRS section count must equal its byte size")
    if sections.get("TRAK", {}).get("count") != track_count:
        raise FormatError("track counts disagree")
    return {
        "version": version,
        "track_count": track_count,
        "file_size": file_size,
        "crc32": f"{stored_crc:08x}",
        "sections": sections,
    }


def _decode_records(data: bytes, section: dict[str, int], record: struct.Struct) -> list[tuple]:
    return [
        record.unpack_from(data, section["offset"] + index * section["entry_size"])
        for index in range(section["count"])
    ]


def _decode_library(data: bytes, summary: dict[str, object]) -> LibraryData:
    sections = summary["sections"]
    required = {kind.decode("ascii") for kind in SECTION_ORDER}
    if not required <= sections.keys():
        raise FormatError(f"missing required section(s): {', '.join(sorted(required - sections.keys()))}")
    strings_section = sections["STRS"]
    strings = data[strings_section["offset"]:strings_section["offset"] + strings_section["size"]]
    if not strings or strings[0] != 0 or strings[-1] != 0:
        raise FormatError("STRS must begin and end with NUL")

    def read_string(offset: int) -> str:
        if offset >= len(strings):
            raise FormatError("string offset is outside STRS")
        end = strings.find(b"\0", offset)
        if end < 0:
            raise FormatError("string is not NUL terminated")
        try:
            return strings[offset:end].decode("utf-8")
        except UnicodeDecodeError as exc:
            raise FormatError("STRS contains invalid UTF-8") from exc

    meta_records = _decode_records(data, sections["META"], META)
    metadata: dict[str, str] = {}
    warnings: list[tuple[int, str]] = []
    for key_offset, value_offset in meta_records:
        key, value = read_string(key_offset), read_string(value_offset)
        if key in metadata or any(key == f"warning.{index}" for index, _ in warnings):
            raise FormatError(f"duplicate metadata key {key!r}")
        match = re.fullmatch(r"warning\.(\d+)", key)
        if match:
            warnings.append((int(match.group(1)), value))
        else:
            metadata[key] = value

    cue_records = _decode_records(data, sections["CUES"], CUE)
    grid_records = _decode_records(data, sections["GRID"], GRID)
    phrase_records = _decode_records(data, sections["PHRA"], PHRASE)
    track_records = _decode_records(data, sections["TRAK"], TRACK)
    if len(cue_records) > len(track_records) * MAX_CUES_PER_TRACK:
        raise FormatError("global cue count exceeds bounds")
    if len(grid_records) > len(track_records) * MAX_GRID_PER_TRACK:
        raise FormatError("global beatgrid count exceeds bounds")
    if len(phrase_records) > len(track_records) * MAX_PHRASES_PER_TRACK:
        raise FormatError("global phrase count exceeds bounds")
    tracks: list[TrackData] = []
    for track_index, record in enumerate(track_records):
        (
            fingerprint, source_id, path_offset, title_offset, artist_offset, album_offset,
            provenance_offset, cue_first, cue_count, grid_first, grid_count,
            phrase_first, phrase_count, sample_rate, duration_frames, bpm_milli,
            key, rating, flags, play_count, color, reserved,
        ) = record
        if flags or reserved != b"\0" * 24:
            raise FormatError("unsupported track flags or nonzero reserved bytes")
        if sample_rate > MAX_SAMPLE_RATE or duration_frames > MAX_DURATION_FRAMES:
            raise FormatError("track sample rate or duration exceeds bounds")
        if bpm_milli > MAX_BPM_MILLI:
            raise FormatError("track BPM exceeds bounds")
        if rating not in (*range(6), 0xFF):
            raise FormatError("track rating is invalid")
        if key and ((key & 0xFF) not in range(1, 13) or key & ~0x1FF):
            raise FormatError("normalized track key is invalid")
        if cue_count > MAX_CUES_PER_TRACK or cue_first + cue_count > len(cue_records):
            raise FormatError("track cue span is invalid")
        if grid_count > MAX_GRID_PER_TRACK or grid_first + grid_count > len(grid_records):
            raise FormatError("track beatgrid span is invalid")
        if phrase_count > MAX_PHRASES_PER_TRACK or phrase_first + phrase_count > len(phrase_records):
            raise FormatError("track phrase span is invalid")
        track = TrackData(
            path=normalize_path(read_string(path_offset)),
            native_id=source_id.hex(),
            source=metadata.get("adapter", "decoded"),
            title=read_string(title_offset),
            artist=read_string(artist_offset),
            album=read_string(album_offset),
            provenance=read_string(provenance_offset),
            sample_rate=sample_rate,
            duration_frames=duration_frames,
            bpm_milli=bpm_milli,
            key=key,
            rating=rating,
            play_count=play_count,
            color=color,
            source_id=source_id,
            fingerprint=fingerprint,
        )
        for record in cue_records[cue_first:cue_first + cue_count]:
            (
                owner, kind, cue_flags, slot, position_frames, length_frames,
                position_num, position_den, length_num, length_den,
                label_offset, cue_color, cue_reserved,
            ) = record
            if owner != track_index or cue_reserved != b"\0" * 8:
                raise FormatError("cue owner or reserved bytes are invalid")
            cue_position, cue_seconds = _decoded_time(
                position_frames, position_num, position_den, True, "cue position"
            )
            cue_length, cue_length_seconds = _decoded_time(
                length_frames, length_num, length_den, False, "cue length"
            )
            track.cues.append(CuePoint(
                kind=kind,
                slot=slot,
                position_frames=cue_position,
                length_frames=cue_length,
                position_seconds=cue_seconds,
                length_seconds=cue_length_seconds,
                label=read_string(label_offset),
                color=cue_color,
                flags=cue_flags,
            ))
        for record in grid_records[grid_first:grid_first + grid_count]:
            (
                owner, position_frames, position_num, position_den, grid_bpm,
                beat_number, confidence, grid_flags, grid_reserved,
            ) = record
            if owner != track_index or grid_reserved != b"\0" * 4:
                raise FormatError("beatgrid owner or reserved bytes are invalid")
            grid_position, grid_seconds = _decoded_time(
                position_frames, position_num, position_den, True, "beatgrid position"
            )
            track.grid.append(GridSegment(
                position_frames=grid_position,
                position_seconds=grid_seconds,
                bpm_milli=grid_bpm,
                beat_number=beat_number,
                confidence=confidence,
                flags=grid_flags,
            ))
        for record in phrase_records[phrase_first:phrase_first + phrase_count]:
            owner, position_frames, position_num, position_den, kind_offset, confidence, phrase_flags = record
            if owner != track_index:
                raise FormatError("phrase owner is invalid")
            phrase_position, phrase_seconds = _decoded_time(
                position_frames, position_num, position_den, True, "phrase position"
            )
            track.phrases.append(PhraseMarker(
                kind=read_string(kind_offset),
                position_frames=phrase_position,
                position_seconds=phrase_seconds,
                confidence=confidence,
                flags=phrase_flags,
            ))
        _validate_track_timeline(track)
        tracks.append(track)

    entry_records = _decode_records(data, sections["PLEN"], PLAYLIST_ENTRY)
    playlist_records = _decode_records(data, sections["PLST"], PLAYLIST)
    if len(playlist_records) > MAX_PLAYLISTS or len(entry_records) > MAX_PLAYLIST_ENTRIES:
        raise FormatError("playlist count exceeds bounds")
    playlists: list[PlaylistData] = []
    for name_offset, entry_first, entry_count, playlist_flags in playlist_records:
        if playlist_flags or entry_first + entry_count > len(entry_records):
            raise FormatError("playlist flags or entry span are invalid")
        ordered: list[tuple[int, str]] = []
        for track_index, position in entry_records[entry_first:entry_first + entry_count]:
            if track_index >= len(tracks):
                raise FormatError("playlist track index is invalid")
            ordered.append((position, tracks[track_index].path))
        if len({position for position, _ in ordered}) != len(ordered):
            raise FormatError("playlist positions are duplicated")
        playlists.append(PlaylistData(
            read_string(name_offset),
            [path for _, path in sorted(ordered)],
        ))
    if sorted(index for index, _ in warnings) != list(range(len(warnings))):
        raise FormatError("warning metadata indices are not contiguous")
    warning_values = [value for _, value in sorted(warnings)]
    return LibraryData(tracks, playlists, metadata, warning_values)


def decode_library(data: bytes) -> LibraryData:
    summary = _decode_layout(data)
    return _decode_library(data, summary)


def decode(data: bytes) -> dict[str, object]:
    summary = _decode_layout(data)
    _decode_library(data, summary)
    return summary


def conversion_manifest(library: LibraryData) -> dict[str, object]:
    library = finalize(library)
    entries = []
    used_targets: set[str] = set()
    for track in library.tracks:
        if PurePosixPath(track.path).suffix.lower() in SUPPORTED_AUDIO:
            collision_key = track.path.casefold()
            if collision_key in used_targets:
                raise FormatError(f"case-colliding AAC paths are unsafe on the target filesystem: {track.path}")
            used_targets.add(collision_key)
    for track in library.tracks:
        supported = PurePosixPath(track.path).suffix.lower() in SUPPORTED_AUDIO
        target = track.path if supported else f"{track.path}.aac"
        if not supported and target.casefold() in used_targets:
            source = PurePosixPath(track.path)
            target = str(source.with_name(f"{source.stem}.{track.source_id.hex()[:8]}.aac"))
        if target.casefold() in used_targets and not supported:
            raise FormatError(f"unable to create unique conversion target for {track.path}")
        used_targets.add(target.casefold())
        entries.append({
            "source_path": track.path,
            "target_path": target,
            "status": "ready" if supported else "needs_conversion",
            "source_id": track.source_id.hex(),
            "fingerprint": track.fingerprint.hex(),
            "source_sample_rate": track.sample_rate,
            "source_duration_frames": track.duration_frames,
            "target_format": "aac",
            "timeline_contract": "preserve exact source seconds; record target sample rate and frame mapping",
        })
    return {
        "schema": "jayd-conversion-manifest-1",
        "supported_audio_extensions": sorted(SUPPORTED_AUDIO),
        "entries": entries,
    }


def _write_import(library: LibraryData, output: Path, manifest: Path | None) -> None:
    normalized = finalize(library)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(encode(normalized))
    manifest_path = manifest or output.with_suffix(output.suffix + ".convert.json")
    manifest_path.write_text(
        json.dumps(conversion_manifest(normalized), indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    for warning in normalized.warnings:
        print(f"warning: {warning}", file=sys.stderr)
    unsupported = sum(
        PurePosixPath(track.path).suffix.lower() not in SUPPORTED_AUDIO
        for track in normalized.tracks
    )
    print(f"wrote {output}: {len(normalized.tracks)} track(s), {unsupported} need AAC conversion")
    print(f"wrote {manifest_path}")


def _paths_alias(first: Path, second: Path) -> bool:
    if first.resolve(strict=False).as_posix().casefold() == second.resolve(strict=False).as_posix().casefold():
        return True
    try:
        return first.exists() and second.exists() and os.path.samefile(first, second)
    except OSError:
        return False


def _validate_output_paths(source: Path, output: Path, manifest: Path) -> None:
    pairs = (
        ("source", source, "output", output),
        ("source", source, "manifest", manifest),
        ("output", output, "manifest", manifest),
    )
    for first_name, first, second_name, second in pairs:
        if _paths_alias(first, second):
            raise FormatError(f"{first_name} and {second_name} paths must be distinct")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    for command, help_text in (
        ("rekordbox", "import official DJ_PLAYLISTS XML"),
        ("m3u", "import an M3U/M3U8 playlist"),
        ("mixxx", "import a Mixxx SQLite library read-only"),
        ("traktor", "best-effort import of unofficial NML"),
        ("engine-json", "import the documented libdjinterop helper boundary"),
    ):
        child = subparsers.add_parser(command, help=help_text)
        child.add_argument("source", type=Path)
        child.add_argument("-o", "--output", type=Path, required=True)
        child.add_argument("--root", type=Path, help="map absolute source paths under this root")
        child.add_argument("--manifest", type=Path, help="conversion manifest path")
    inspect = subparsers.add_parser("inspect", help="validate and summarize a sidecar")
    inspect.add_argument("source", type=Path)
    return parser


def main(argv: list[str] | None = None) -> int:
    arguments = build_parser().parse_args(argv)
    try:
        if arguments.command == "inspect":
            if arguments.source.stat().st_size > MAX_FILE_SIZE:
                raise FormatError(f"sidecar exceeds {MAX_FILE_SIZE} bytes")
            print(json.dumps(decode(arguments.source.read_bytes()), indent=2, sort_keys=True))
            return 0
        importers = {
            "rekordbox": import_rekordbox,
            "m3u": import_m3u,
            "mixxx": import_mixxx,
            "traktor": import_traktor,
            "engine-json": import_engine_json,
        }
        root = arguments.root.resolve(strict=False) if arguments.root else None
        manifest = arguments.manifest or arguments.output.with_suffix(arguments.output.suffix + ".convert.json")
        _validate_output_paths(arguments.source, arguments.output, manifest)
        library = importers[arguments.command](arguments.source, root)
        _write_import(library, arguments.output, manifest)
        return 0
    except (FormatError, OSError, sqlite3.Error, KeyError, TypeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
