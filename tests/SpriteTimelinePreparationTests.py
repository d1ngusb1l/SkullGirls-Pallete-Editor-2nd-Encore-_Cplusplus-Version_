import importlib.util
import unittest
from pathlib import Path

script = Path(__file__).resolve().parents[1] / "scripts" / "prepare-dll-preview.py"
spec = importlib.util.spec_from_file_location("prepare_dll_preview", script)
preview = importlib.util.module_from_spec(spec)
spec.loader.exec_module(preview)


class SpriteTimelinePreparationTests(unittest.TestCase):
    def test_body_sprite_uses_slot_one_and_holds_pose_on_non_sprite_frames(self):
        timeline = [
            {"sprites": [0, 7, 0], "duration": 4},
            {"sprites": [0, 0, 0], "duration": 1},
            {"sprites": [0, 7, 0], "duration": 4},
        ]
        mapped = preview.map_body_timeline(timeline, fallback_sprite=2)
        self.assertEqual([entry["body_sprite"] for entry in mapped], [7, 7, 7])
        self.assertEqual([entry["duration"] for entry in mapped], [4, 1, 4])

    def test_repeated_sprite_records_remain_distinct_timeline_frames(self):
        timeline = [
            {"sprites": [0, 7, 0], "duration": 4},
            {"sprites": [0, 7, 0], "duration": 1},
        ]
        mapped = preview.map_body_timeline(timeline)
        self.assertEqual(len(mapped), 2)
        self.assertEqual([entry["body_sprite"] for entry in mapped], [7, 7])
        self.assertEqual([entry["duration"] for entry in mapped], [4, 1])

    def test_timeline_boundaries(self):
        records = [{"bank": 5} for _ in range(3)]
        for mapped in preview.map_body_timeline([
            {"sprites": [0, 0, 0], "duration": 1},
            {"sprites": [0, 2, 0], "duration": 65535},
        ]):
            preview.validate_timeline_frame(mapped, records)

    def test_invalid_sprite_reference_is_rejected(self):
        records = [{"bank": 5}, {"bank": 5}]
        with self.assertRaises(ValueError):
            preview.validate_timeline_frame({"sprites": [2, 1, 0], "duration": 1}, records)

    def test_invalid_duration_is_rejected(self):
        records = [{"bank": 5}]
        with self.assertRaises(ValueError):
            preview.validate_timeline_frame({"sprites": [0, 0, 0], "duration": 0}, records)

    def test_asset_bank_selection(self):
        arts = {4: "pt", 5: "main", 6: "pt"}
        self.assertEqual(preview.resolve_art_bank(4, arts), "pt")
        self.assertEqual(preview.resolve_art_bank(5, arts), "main")
        self.assertEqual(preview.resolve_art_bank(6, arts), "pt")
        with self.assertRaises(ValueError):
            preview.resolve_art_bank(0, arts)

    def test_game_rle_layer_decode_and_bounds(self):
        source = bytes([2, 1, 2, 3, 0x82, 9])
        self.assertEqual(preview.unpack(source, 6), bytes([1, 2, 3, 9, 9, 9]))
        with self.assertRaises(ValueError):
            preview.unpack(bytes([0x82]), 3)
        with self.assertRaises(ValueError):
            preview.unpack(bytes([0x80]), 3)


if __name__ == "__main__":
    unittest.main()
