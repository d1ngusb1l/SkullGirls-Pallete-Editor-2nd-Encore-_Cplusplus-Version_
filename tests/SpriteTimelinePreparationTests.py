import importlib.util
import unittest
from pathlib import Path

script = Path(__file__).resolve().parents[1] / "scripts" / "prepare-dll-preview.py"
spec = importlib.util.spec_from_file_location("prepare_dll_preview", script)
preview = importlib.util.module_from_spec(spec)
spec.loader.exec_module(preview)


class SpriteTimelinePreparationTests(unittest.TestCase):
    def test_repeated_timeline_frames_preserve_each_entry(self):
        timeline = [
            {"sprites": [7, 8, 9], "duration": 4},
            {"sprites": [7, 8, 9], "duration": 4},
        ]
        self.assertEqual([entry["sprites"][0] for entry in timeline], [7, 7])
        self.assertEqual([entry["duration"] for entry in timeline], [4, 4])

    def test_timeline_boundaries(self):
        records = [{"bank": 5}, {"bank": 5}, {"bank": 5}]
        preview.validate_timeline_frame({"sprites": [0, 1, 2], "duration": 1}, records)
        preview.validate_timeline_frame({"sprites": [2, 1, 0], "duration": 65535}, records)

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


if __name__ == "__main__":
    unittest.main()
