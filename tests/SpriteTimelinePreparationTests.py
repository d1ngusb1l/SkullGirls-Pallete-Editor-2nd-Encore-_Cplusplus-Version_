import importlib.util
import io
import tempfile
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
        mapped = preview.map_body_timeline(timeline, fallback_sprite=1)
        self.assertEqual([entry["body_sprite"] for entry in mapped], [6, 6, 6])
        self.assertEqual([entry["duration"] for entry in mapped], [4, 1, 4])

    def test_repeated_sprite_records_remain_distinct_timeline_frames(self):
        timeline = [
            {"sprites": [0, 7, 0], "duration": 4},
            {"sprites": [0, 7, 0], "duration": 1},
        ]
        mapped = preview.map_body_timeline(timeline)
        self.assertEqual(len(mapped), 2)
        self.assertEqual([entry["body_sprite"] for entry in mapped], [6, 6])
        self.assertEqual([entry["duration"] for entry in mapped], [4, 1])

    def test_one_based_references_include_first_and_last_sprite(self):
        timeline=[{"sprites":[0,1,0],"duration":2},{"sprites":[0,3,0],"duration":4}]
        mapped=preview.map_body_timeline(timeline)
        self.assertEqual([f["body_sprite"] for f in mapped],[0,2])
        for frame in mapped:preview.validate_timeline_frame(frame,[{"bank":5}]*3)

    def test_same_tick_command_updates_pose_without_adding_a_hold(self):
        timeline=[{"sprites":[0,8,0],"duration":1,"advance_immediately":True},
                  {"sprites":[0,0,0],"duration":4},
                  {"sprites":[0,9,0],"duration":4}]
        mapped=preview.map_body_timeline(timeline)
        self.assertEqual([(f["body_sprite"],f["duration"]) for f in mapped],[(7,4),(8,4)])
        self.assertEqual([f["source_record"] for f in mapped],[1,2])

    def test_advance_flag_requires_unconditional_constant_assignment(self):
        import struct
        one=int.from_bytes(struct.pack('<f',1),'little')
        node={"opcode":1,"comparison":False,"left":(0,one),"right":(0,0),"destination":(1,0x3a)}
        self.assertTrue(preview.advances_immediately([node]))
        self.assertFalse(preview.advances_immediately([{**node,"comparison":True}]))
        self.assertFalse(preview.advances_immediately([{**node,"comparison":True},node]))
        self.assertFalse(preview.advances_immediately([{**node,"destination":(1,0x3c)}]))
        self.assertFalse(preview.advances_immediately([node,{**node,"left":(0,0)}]))

    def test_serialized_same_tick_assignment_is_retained_as_metadata(self):
        import struct
        expression=bytes([0,0,1,0,1])+struct.pack('<ffB',1,0,0x3a)
        details={}
        duration=preview.read_animation_ticks(preview.Cursor(bytes([0,0,128])+expression+bytes([0,0])),details)
        self.assertEqual(duration,1)
        self.assertTrue(details["advance_immediately"])

    def test_absent_duration_keeps_native_default_tick(self):
        self.assertEqual(preview.read_animation_ticks(preview.Cursor(bytes([0,0,0,0,0]))),1)

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
            preview.validate_timeline_frame({"sprites": [3, 1, 0], "duration": 1}, records)

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

    def test_authored_animation_name_and_numeric_fallback(self):
        self.assertEqual(preview.authored_animation_name(42, "  Idle  "), "Idle")
        self.assertEqual(preview.authored_animation_name(42, ""), "42")

    def test_frame_origin_and_layer_offset_bounds(self):
        layers = [(2, 2, [0], 0, 0, [1, 1, 1])] + [None] * 7
        offsets = [(80, 160)] + [None] * 7
        record = {"bank": 5, "origin": (100, 200), "layers": layers, "offsets": offsets}
        self.assertIsNone(preview.frame_support_reason(record, {5: b"main"}))
        record["offsets"] = [(10000, 160)] + [None] * 7
        self.assertIn("origin", preview.frame_support_reason(record, {5: b"main"}))

    def test_combined_bundle_character_sections_and_invalid_bounds(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "section.bin"
            path.write_bytes(bytes(24))
            characters = [
                ("Filia", path, {}),
                ("Cerebella", path, {}),
            ]
            index = preview.write_bundle_index(characters)
            bundle = index + bytes(48)
            parsed = preview.read_bundle_index(io.BytesIO(bundle), len(bundle))
            self.assertEqual([entry[0] for entry in parsed], ["Filia", "Cerebella"])
            self.assertEqual([entry[2] for entry in parsed], [24, 24])

            malformed = bytearray(bundle)
            malformed[16 + 2 + len("Filia"):16 + 2 + len("Filia") + 8] = (1).to_bytes(8, "little")
            with self.assertRaises(ValueError):
                preview.read_bundle_index(io.BytesIO(malformed), len(malformed))

    def test_local_gfs_asset_lookup_and_malformed_offsets(self):
        name = b"characters/test.foit"
        payload = b"local-data"
        entry_table = len(name).to_bytes(8, "big") + name + len(payload).to_bytes(8, "big") + bytes(4)
        data_offset = 51 + len(entry_table)
        header = bytearray(51)
        header[:4] = data_offset.to_bytes(4, "big")
        header[43:51] = (1).to_bytes(8, "big")
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            archive = folder / "test.gfs"
            archive.write_bytes(header + entry_table + payload)
            self.assertEqual(preview.asset(folder, "test.gfs", "characters/test.foit"), payload)

            header[:4] = (50).to_bytes(4, "big")
            archive.write_bytes(header + entry_table + payload)
            with self.assertRaises(ValueError):
                preview.asset(folder, "test.gfs", "characters/test.foit")


if __name__ == "__main__":
    unittest.main()
