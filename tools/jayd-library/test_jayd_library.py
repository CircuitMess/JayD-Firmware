import importlib.util
import json
import sqlite3
import struct
import sys
import tempfile
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("jayd_library.py")
SPEC = importlib.util.spec_from_file_location("jayd_library", MODULE_PATH)
jayd = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
sys.modules[SPEC.name] = jayd
SPEC.loader.exec_module(jayd)


class JayDLibraryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.directory = Path(self.temp.name)
        self.music = self.directory / "Music"
        self.music.mkdir()

    def tearDown(self):
        self.temp.cleanup()

    def write(self, name, text):
        path = self.directory / name
        path.write_text(text, encoding="utf-8")
        return path

    def test_rekordbox_round_trip_and_determinism(self):
        xml = self.write("rekordbox.xml", f"""<?xml version="1.0" encoding="UTF-8"?>
<DJ_PLAYLISTS Version="1.0.0">
  <PRODUCT Name="rekordbox" Version="7.0"/>
  <COLLECTION Entries="1">
    <TRACK TrackID="7" Name="Song" Artist="Artist" Album="Album"
      Location="file://localhost{self.music}/song.wav" AverageBpm="128.125"
      Tonality="8A" Rating="204" PlayCount="3" SampleRate="44100" TotalTime="245.348">
      <TEMPO Inizio="0" Bpm="128.125" Metro="4/4" Battito="1"/>
      <POSITION_MARK Name="Drop" Type="0" Start="1.5" Num="2" Red="255" Green="0" Blue="0"/>
    </TRACK>
  </COLLECTION>
  <PLAYLISTS><NODE Type="0" Name="ROOT"><NODE Type="1" Name="Set">
    <TRACK Key="7"/>
  </NODE></NODE></PLAYLISTS>
</DJ_PLAYLISTS>""")
        library = jayd.import_rekordbox(xml, self.music)
        self.assertEqual(library.tracks[0].duration_frames, 10819847)
        first = jayd.encode(library)
        second = jayd.encode(library)
        self.assertEqual(first, second)
        self.assertEqual(first, jayd.encode(jayd.decode_library(first)))
        summary = jayd.decode(first)
        self.assertEqual(summary["track_count"], 1)
        self.assertEqual(summary["sections"]["TRAK"]["entry_size"], 128)
        self.assertEqual(summary["sections"]["CUES"]["count"], 1)
        self.assertEqual(summary["sections"]["GRID"]["count"], 1)
        cue = jayd.CUE.unpack_from(first, summary["sections"]["CUES"]["offset"])
        grid = jayd.GRID.unpack_from(first, summary["sections"]["GRID"]["offset"])
        self.assertEqual(cue[4], 66150)
        self.assertNotEqual(cue[10], 0)
        self.assertEqual(grid[4], 128125)
        manifest = jayd.conversion_manifest(library)
        self.assertEqual(manifest["entries"][0]["status"], "needs_conversion")
        self.assertEqual(manifest["entries"][0]["target_path"], "song.wav.aac")

    def test_m3u_preserves_order_and_deduplicates_tracks(self):
        m3u = self.write("set.m3u8", """#EXTM3U
#EXTINF:1,Second
sets/two.aac
#EXTINF:1,First
sets/one.aac
sets/two.aac
""")
        library = jayd.finalize(jayd.import_m3u(m3u, None))
        self.assertEqual([track.path for track in library.tracks], ["sets/one.aac", "sets/two.aac"])
        self.assertEqual(library.playlists[0].paths, ["sets/two.aac", "sets/one.aac", "sets/two.aac"])
        self.assertTrue(any("duplicate" in warning for warning in library.warnings))

    def test_traktor_best_effort_exact_time(self):
        nml = self.write("collection.nml", """<?xml version="1.0"?>
<NML VERSION="20"><COLLECTION>
  <ENTRY AUDIO_ID="abc" TITLE="Track" ARTIST="DJ">
    <LOCATION DIR="/:sets/:" FILE="track.aac"/>
    <INFO PLAYTIME_FLOAT="2.5" RANKING="255" KEY="C#m" PLAYCOUNT="4"/>
    <ALBUM TITLE="Album"/>
    <MUSICAL_KEY VALUE="12"/>
    <AUDIO_INFO SAMPLE_RATE="44100"/>
    <TEMPO BPM="120"/>
    <CUE_V2 NAME="Cue" TYPE="0" START="125.5" LEN="0" HOTCUE="1"/>
  </ENTRY>
</COLLECTION></NML>""")
        library = jayd.import_traktor(nml, None)
        self.assertEqual(library.tracks[0].path, "sets/track.aac")
        self.assertEqual(library.tracks[0].album, "Album")
        self.assertNotEqual(library.tracks[0].key, 0)
        self.assertIsNone(library.tracks[0].cues[0].position_frames)
        self.assertEqual(library.tracks[0].cues[0].position_seconds, jayd.Fraction(251, 2000))
        self.assertIn("best-effort", library.warnings[0])
        jayd.decode(jayd.encode(library))

    def test_mixxx_stable_columns_cues_and_opaque_beats_warning(self):
        database = self.directory / "mixxx.sqlite"
        connection = sqlite3.connect(database)
        connection.executescript("""
CREATE TABLE track_locations (id INTEGER PRIMARY KEY, location TEXT);
CREATE TABLE library (
  id INTEGER PRIMARY KEY, location INTEGER, title TEXT, artist TEXT, album TEXT,
  duration REAL, bpm REAL, samplerate INTEGER, timesplayed INTEGER, rating INTEGER,
  key TEXT, color INTEGER, beats BLOB, beats_version TEXT
);
CREATE TABLE cues (
  id INTEGER PRIMARY KEY, track_id INTEGER, type INTEGER, position REAL,
  length REAL, hotcue INTEGER, label TEXT, color INTEGER
);
""")
        connection.execute("INSERT INTO track_locations VALUES (1, ?)", (str(self.music / "mix.aac"),))
        connection.execute(
            "INSERT INTO library VALUES (1,1,'Mix','DJ','LP',180.021333,124.5,48000,2,4,'9B',16711935,?,?)",
            (b"opaque", "BeatGrid-2.0"),
        )
        connection.execute("INSERT INTO cues VALUES (1,1,4,96000,48000,0,'Loop',255)")
        connection.commit()
        connection.close()
        library = jayd.import_mixxx(database, self.music)
        self.assertEqual(library.tracks[0].duration_frames, 8641024)
        self.assertEqual(library.tracks[0].cues[0].position_frames, 48000)
        self.assertEqual(library.tracks[0].cues[0].length_frames, 24000)
        self.assertTrue(any("opaque Mixxx beats BLOB" in warning for warning in library.warnings))
        jayd.decode(jayd.encode(library))

    def test_crc_corruption_and_section_bounds(self):
        data = bytearray(jayd.encode(jayd.LibraryData([
            jayd.TrackData("one.aac", "1", "test")
        ])))
        data[-1] ^= 1
        with self.assertRaisesRegex(jayd.FormatError, "CRC32"):
            jayd.decode(data)

        data = bytearray(jayd.encode(jayd.LibraryData([
            jayd.TrackData("one.aac", "1", "test")
        ])))
        # Move the first section beyond EOF, then repair CRC to reach bounds validation.
        struct.pack_into("<Q", data, jayd.HEADER.size + 16, len(data) + 8)
        struct.pack_into("<I", data, 44, 0)
        struct.pack_into("<I", data, 44, jayd.binascii.crc32(data) & 0xFFFFFFFF)
        with self.assertRaisesRegex(jayd.FormatError, "section bounds"):
            jayd.decode(data)

        data = bytearray(jayd.encode(jayd.LibraryData([
            jayd.TrackData("one.aac", "1", "test")
        ])))
        strings_entry = jayd.HEADER.size + 7 * jayd.SECTION.size
        struct.pack_into("<I", data, strings_entry + 8, 0xFFFFFFFF)
        struct.pack_into("<I", data, 44, 0)
        struct.pack_into("<I", data, 44, jayd.binascii.crc32(data) & 0xFFFFFFFF)
        with self.assertRaisesRegex(jayd.FormatError, "STRS|section shape"):
            jayd.decode(data)

        data = bytearray(jayd.encode(jayd.LibraryData([
            jayd.TrackData("one.aac", "1", "test")
        ])))
        struct.pack_into("<H", data, jayd.HEADER.size + jayd.SECTION.size + 4, 2)
        struct.pack_into("<I", data, 44, 0)
        struct.pack_into("<I", data, 44, jayd.binascii.crc32(data) & 0xFFFFFFFF)
        with self.assertRaisesRegex(jayd.FormatError, "TRAK section version"):
            jayd.decode(data)

        data = bytearray(jayd.encode(jayd.LibraryData([
            jayd.TrackData("one.aac", "1", "test", cues=[
                jayd.CuePoint(position_frames=0)
            ])
        ])))
        summary = jayd._decode_layout(data)
        cue_entry = jayd.HEADER.size + 2 * jayd.SECTION.size
        struct.pack_into("<Q", data, cue_entry + 16, summary["sections"]["TRAK"]["offset"])
        struct.pack_into("<I", data, 44, 0)
        struct.pack_into("<I", data, 44, jayd.binascii.crc32(data) & 0xFFFFFFFF)
        with self.assertRaisesRegex(jayd.FormatError, "overlap"):
            jayd.decode(data)

        with self.assertRaisesRegex(jayd.FormatError, "metadata entry count"):
            jayd.encode(jayd.LibraryData([], metadata={
                str(index): "" for index in range(jayd.MAX_METADATA_ENTRIES + 1)
            }))

    def test_path_traversal_absolute_and_limits(self):
        for invalid in ("../song.aac", "/Music/song.aac", "C:/Music/song.aac", "a/./song.aac"):
            with self.subTest(invalid=invalid), self.assertRaises(jayd.FormatError):
                jayd.normalize_path(invalid)
        self.assertEqual(jayd.normalize_path(str(self.music / "song.aac"), self.music), "song.aac")
        self.assertEqual(
            jayd.normalize_path(f"file://localhost{self.music}/a%23b.aac", self.music),
            "a#b.aac",
        )
        too_many = [
            jayd.TrackData(f"{index}.aac", str(index), "test")
            for index in range(jayd.MAX_TRACKS + 1)
        ]
        with self.assertRaisesRegex(jayd.FormatError, "track count"):
            jayd.encode(jayd.LibraryData(too_many))

    def test_duplicate_tie_break_and_manifest_targets_are_deterministic(self):
        one = jayd.TrackData("same.wav", "1", "test", cues=[
            jayd.CuePoint(position_frames=0, label="A")
        ])
        two = jayd.TrackData("same.wav", "2", "test", cues=[
            jayd.CuePoint(position_frames=0, label="B")
        ])
        self.assertEqual(
            jayd.encode(jayd.LibraryData([one, two])),
            jayd.encode(jayd.LibraryData([two, one])),
        )
        library = jayd.LibraryData([
            jayd.TrackData("same.wav", "1", "test", cues=[
                jayd.CuePoint(position_frames=0, label="B"),
                jayd.CuePoint(position_frames=0, label="A"),
            ]),
            jayd.TrackData("same.wav", "2", "test", cues=[
                jayd.CuePoint(position_frames=0, label="A"),
                jayd.CuePoint(position_frames=0, label="C"),
            ]),
        ])
        self.assertEqual(jayd.encode(library), jayd.encode(library))
        manifest = jayd.conversion_manifest(jayd.LibraryData([
            jayd.TrackData("same.wav", "1", "test"),
            jayd.TrackData("same.mp3", "2", "test"),
            jayd.TrackData("same.wav.aac", "3", "test"),
        ]))
        targets = [entry["target_path"] for entry in manifest["entries"]]
        self.assertEqual(len(targets), len(set(targets)))
        case_manifest = jayd.conversion_manifest(jayd.LibraryData([
            jayd.TrackData("Song.wav", "1", "test"),
            jayd.TrackData("song.wav", "2", "test"),
        ]))
        case_targets = [entry["target_path"].casefold() for entry in case_manifest["entries"]]
        self.assertEqual(len(case_targets), len(set(case_targets)))

    def test_cli_rejects_source_output_or_manifest_aliases(self):
        source = self.write("set.m3u", "track.aac\n")
        self.assertEqual(jayd.main(["m3u", str(source), "-o", str(source)]), 2)
        output = self.directory / "out.jydm"
        self.assertEqual(jayd.main([
            "m3u", str(source), "-o", str(output), "--manifest", str(output)
        ]), 2)
        self.assertEqual(source.read_text(encoding="utf-8"), "track.aac\n")
        self.assertTrue(jayd._paths_alias(
            self.directory / "Missing-OUTPUT",
            self.directory / "missing-output",
        ))

    def test_engine_json_boundary_and_phrase_table(self):
        source = self.write("engine.json", json.dumps({
            "schema": "jayd-engine-interchange-1",
            "producer": "test helper/libdjinterop",
            "tracks": [{
                "source_id": "engine-1",
                "path": "engine/track.aac",
                "sample_rate": 44100,
                "duration_frames": 88200,
                "phrases": [{"position_frames": 0, "kind": "intro", "confidence": 9000}],
            }],
        }))
        library = jayd.import_engine_json(source, None)
        summary = jayd.decode(jayd.encode(library))
        self.assertEqual(summary["sections"]["PHRA"]["count"], 1)

        incomplete = self.write("engine-incomplete.json", json.dumps({
            "schema": "jayd-engine-interchange-1",
            "tracks": [{"path": "engine/track.aac", "cues": [{"kind": "cue"}]}],
        }))
        library = jayd.import_engine_json(incomplete, None)
        self.assertEqual(library.tracks[0].cues, [])
        self.assertTrue(any("without position" in warning for warning in library.warnings))

    def test_decode_rejects_semantic_bounds_and_nul_path(self):
        with self.assertRaisesRegex(jayd.FormatError, "NUL"):
            jayd.encode(jayd.LibraryData([jayd.TrackData("bad\0path.aac", "1", "test")]))
        data = bytearray(jayd.encode(jayd.LibraryData([
            jayd.TrackData("one.aac", "1", "test")
        ])))
        summary = jayd._decode_layout(data)
        struct.pack_into("<I", data, summary["sections"]["TRAK"]["offset"] + 76, jayd.MAX_SAMPLE_RATE + 1)
        struct.pack_into("<I", data, 44, 0)
        struct.pack_into("<I", data, 44, jayd.binascii.crc32(data) & 0xFFFFFFFF)
        with self.assertRaisesRegex(jayd.FormatError, "sample rate"):
            jayd.decode(data)

    def test_traktor_open_key(self):
        self.assertEqual(jayd._key("1d"), jayd._key("8B"))
        self.assertEqual(jayd._key("1m"), jayd._key("8A"))

    def test_duration_rounding_bounds_and_invalid_values(self):
        self.assertEqual(jayd._duration_frames(jayd.Fraction("245.348"), 44100), 10819847)
        self.assertEqual(jayd._duration_frames(jayd.Fraction("180.021333"), 48000), 8641024)
        rate = 48000
        near_bound = jayd.Fraction(2 * jayd.MAX_DURATION_FRAMES - 1, 2 * rate)
        overflow = jayd.Fraction(2 * jayd.MAX_DURATION_FRAMES + 1, 2 * rate)
        self.assertEqual(jayd._duration_frames(near_bound, rate), jayd.MAX_DURATION_FRAMES)
        with self.assertRaisesRegex(jayd.FormatError, "exceeds"):
            jayd._duration_frames(overflow, rate)
        with self.assertRaisesRegex(jayd.FormatError, "negative"):
            jayd._duration_frames(jayd.Fraction(-1), rate)
        for invalid in ("NaN", "Infinity", "-Infinity", "-1"):
            warnings = []
            self.assertEqual(jayd._source_duration(invalid, rate, warnings, "test duration"), 0)
            self.assertEqual(len(warnings), 1)
            self.assertIn("unrepresentable", warnings[0])
        warnings = []
        self.assertEqual(jayd._source_duration("1", 0, warnings, "test duration"), 0)
        self.assertIn("sample rate", warnings[0])

    def test_rational_timeline_bounds(self):
        track = jayd.TrackData(
            "fractional.aac", "1", "test",
            sample_rate=44100,
            duration_frames=110250,
            cues=[
                jayd.CuePoint(position_seconds=jayd.Fraction(5, 2)),
                jayd.CuePoint(
                    kind=2,
                    position_seconds=jayd.Fraction(1),
                    length_seconds=jayd.Fraction(3, 2),
                ),
            ],
            grid=[jayd.GridSegment(position_seconds=jayd.Fraction(5, 2))],
            phrases=[jayd.PhraseMarker("end", position_seconds=jayd.Fraction(5, 2))],
        )
        encoded = jayd.encode(jayd.LibraryData([track]))
        self.assertEqual(jayd.decode(encoded)["track_count"], 1)

        boundary = jayd.TrackData(
            "boundary.aac", "2", "test",
            sample_rate=jayd.MAX_SAMPLE_RATE,
            duration_frames=jayd.MAX_DURATION_FRAMES,
            cues=[jayd.CuePoint(position_seconds=jayd.Fraction(
                jayd.MAX_DURATION_FRAMES, jayd.MAX_SAMPLE_RATE
            ))],
        )
        self.assertEqual(jayd.decode(jayd.encode(jayd.LibraryData([boundary])))["track_count"], 1)

        invalid_positions = [
            ("cue", {"cues": [jayd.CuePoint(position_seconds=jayd.Fraction(110251, 44100))]}),
            ("loop", {"cues": [jayd.CuePoint(
                kind=2,
                position_seconds=jayd.Fraction(1),
                length_seconds=jayd.Fraction(66151, 44100),
            )]}),
            ("beatgrid", {"grid": [jayd.GridSegment(position_seconds=jayd.Fraction(110251, 44100))]}),
            ("phrase", {"phrases": [jayd.PhraseMarker(
                "late", position_seconds=jayd.Fraction(110251, 44100)
            )]}),
            ("64-bit-product-overflow", {"cues": [jayd.CuePoint(position_seconds=jayd.Fraction(
                jayd.NO_FRAME, 0xFFFFFFFF
            ))]}),
        ]
        for label, values in invalid_positions:
            with self.subTest(label=label), self.assertRaisesRegex(jayd.FormatError, "duration"):
                jayd.encode(jayd.LibraryData([jayd.TrackData(
                    f"{label}.aac", label, "test",
                    sample_rate=44100,
                    duration_frames=110250,
                    **values,
                )]))

        malformed = bytearray(encoded)
        summary = jayd._decode_layout(malformed)
        struct.pack_into("<I", malformed, summary["sections"]["CUES"]["offset"] + 32, 0)
        struct.pack_into("<I", malformed, 44, 0)
        struct.pack_into("<I", malformed, 44, jayd.binascii.crc32(malformed) & 0xFFFFFFFF)
        with self.assertRaisesRegex(jayd.FormatError, "invalid rational"):
            jayd.decode(malformed)


if __name__ == "__main__":
    unittest.main()
