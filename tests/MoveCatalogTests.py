import importlib.util
from pathlib import Path
import struct
import unittest

spec = importlib.util.spec_from_file_location("move_catalog", Path(__file__).resolve().parents[1] / "scripts" / "prepare-move-catalog.py")
catalog = importlib.util.module_from_spec(spec)
spec.loader.exec_module(catalog)


class MoveCatalogTests(unittest.TestCase):
    def character(self):
        records = [dict(id=i, tags=[], successor=successor, conditionalTargets=targets, sourceFrames=1)
                   for i, (successor, targets) in enumerate(((1, [3]), (2, []), (3, []), (3, [])))]
        records[0]["tags"] = [catalog.tag_hash("SPECIAL")]
        return dict(character="Beowulf", foitSha256="a" * 64,
                    states=catalog.state_catalog(["Move", "Move_Loop", "Move_End", "Idle"], records))

    def mapping(self, **changes):
        return dict(character="Beowulf", foitSha256="a" * 64,
                    moves=[dict(id="move", label="Whole move", states=["Move", "Move_Loop", "Move_End"])], **changes)

    def test_parser_retains_tags_and_conditional_edges_without_changing_timelines(self):
        condition = bytes(7)  # no condition steps
        first = bytes([2, 0, 1]) + struct.pack("<IHHH", catalog.tag_hash("SPECIAL"), 1, 1, 0)
        first += bytes(2) + struct.pack("<H", 1) + condition
        second = bytes([0, 0]) + struct.pack("<HHH", 0, 1, 0) + bytes(2)
        source = bytes([0]) + struct.pack("<H", 2) + first + second
        metadata = []
        parsed = catalog.preview.read_move_table(catalog.preview.Cursor(source), len(source), 0, move_metadata=metadata)
        original = catalog.preview.read_move_table(catalog.preview.Cursor(source), len(source), 0)
        self.assertEqual(parsed, original)
        self.assertEqual(metadata[0], dict(id=0, tags=[catalog.tag_hash("SPECIAL")], successor=1,
                                           conditionalTargets=[1], sourceFrames=0, timedTicks=0,
                                           conditionalEdges=[dict(target=1,declaredSteps=0,flags=0,hasInputBytes=False)]))

    def test_explicit_membership_covers_all_phases_and_reports_exits(self):
        moves = catalog.resolve_moves(self.character(), self.mapping())
        self.assertEqual(moves[0]["stateIds"], [0, 1, 2])
        self.assertEqual({(e["source"], e["target"]) for e in moves[0]["outgoingBoundaries"]}, {(0, 3), (2, 3)})
        self.assertIn("not verified", moves[0]["validation"])

    def test_reachability_does_not_absorb_idle_or_interruptions(self):
        move = catalog.resolve_moves(self.character(), self.mapping())[0]
        self.assertNotIn(3, move["stateIds"])
        self.assertEqual(self.character()["states"][0]["generalTags"], ["SPECIAL"])

    def test_build_and_character_mismatch_reject(self):
        for key, value in (("foitSha256", "b" * 64), ("character", "Annie")):
            mapping = self.mapping(); mapping[key] = value
            with self.assertRaises(ValueError): catalog.resolve_moves(self.character(), mapping)

    def test_unknown_duplicate_and_ambiguous_states_reject(self):
        for names in (["NotAMove"], ["Move", "Move"], []):
            mapping = self.mapping(); mapping["moves"][0]["states"] = names
            with self.assertRaises(ValueError): catalog.resolve_moves(self.character(), mapping)
        character = self.character(); character["states"][3]["name"] = "Move"
        with self.assertRaises(ValueError): catalog.resolve_moves(character, self.mapping())

    def test_shared_recovery_is_flagged_instead_of_silently_claimed_by_two_moves(self):
        mapping = self.mapping()
        mapping["moves"].append(dict(id="second", label="Second move", states=["Move_End"]))
        with self.assertRaisesRegex(ValueError, "Shared state"):
            catalog.resolve_moves(self.character(), mapping)

    def test_duplicate_identity_and_unknown_fields_reject(self):
        mapping = self.mapping(); mapping["moves"].append(dict(mapping["moves"][0]))
        with self.assertRaises(ValueError): catalog.resolve_moves(self.character(), mapping)
        mapping = self.mapping(); mapping["offset"] = 123
        with self.assertRaises(ValueError): catalog.resolve_moves(self.character(), mapping)

    def test_missing_dispatchers_are_not_filtered_by_preview_support(self):
        character = self.character()
        character["states"][0]["sourceFrames"] = 0
        self.assertEqual(catalog.resolve_moves(character, self.mapping())[0]["stateIds"], [0, 1, 2])

    def test_invalid_edges_reject(self):
        for target in (-1, 2, True):
            with self.assertRaises(ValueError):
                catalog.state_catalog(["Move"], [dict(id=0, tags=[], successor=target,
                                                     conditionalTargets=[], sourceFrames=1)])

    def test_membership_plan_never_fills_unrelated_state_gaps(self):
        ranges = catalog.membership_ranges([9, 3, 2, 8, 6])
        self.assertEqual(ranges, [[2, 3], [6, 6], [8, 9]])
        for state in range(12):
            self.assertEqual(any(low <= state <= high for low, high in ranges), state in {2, 3, 6, 8, 9})
        for invalid in ([], [-1], [65535], [True], [2.5]):
            with self.assertRaises(ValueError): catalog.membership_ranges(invalid)


if __name__ == "__main__":
    unittest.main()
