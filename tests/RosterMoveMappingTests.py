import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("roster_builder", ROOT / "scripts/build-roster-moves.py")
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


class RosterMoveMappingTests(unittest.TestCase):
    def states(self, names, edges=None, tags=None):
        edges = edges or {}
        return dict(character="Fixture", foitSha256="a" * 64, states=[
            dict(id=i, name=name, successor=edges.get(i, i), conditionalTargets=[], conditionalEdges=[],
                 sourceFrames=2, timedTicks=2, generalTags=(tags or {}).get(i, ["ATTACK", "SPECIAL"]))
            for i, name in enumerate(names)])

    def resolve(self, character, families, untagged=None):
        for state in character["states"]:
            state["tags"] = [builder.catalog.tag_hash(tag) for tag in state["generalTags"]]
        return builder.resolve_character(character, families, untagged)

    def test_reviewed_untagged_finishers_are_distinct_from_parent_stance(self):
        c = self.states(["GrabMode_Stand", "GrabMode_Bulldog_Ground", "GrabMode_Bulldog_Land", "GrabMode_EX_Bulldog_Air", "GrabMode_GringoKilla_Ground"], tags={0:["ATTACK"],1:[],2:[],3:[],4:[]})
        families = [["GrabMode"],["GrabMode_Bulldog","GrabMode_EX_Bulldog"],["GrabMode_GringoKilla"]]
        result = self.resolve(c, families, {"grab-mode-bulldog":"Special", "grab-mode-gringo-killa":"Special"})
        groups={m["id"]:m for m in result["moves"]}
        self.assertEqual(groups["grab-mode"]["stateIds"],[0])
        self.assertEqual(groups["grab-mode-bulldog"]["stateIds"],[1,2,3])
        self.assertEqual(groups["grab-mode-gringo-killa"]["stateIds"],[4])
        self.assertEqual(groups["grab-mode-bulldog"]["category"],"Special")
        self.assertEqual(len(self.resolve(c,families)["moves"]),1)
        with self.assertRaises(ValueError):self.resolve(c,families,{"missing":"Special"})
        with self.assertRaises(ValueError):self.resolve(c,families,{"grab-mode-bulldog":"Typo"})

    def test_normal_phases_are_one_button_selection_without_capturing_other_buttons(self):
        c = self.states(["s.WP", "s.WP_2nd", "s.MP", "j.D+MP", "j.D+MP_Land"])
        result = self.resolve(c, [])
        groups = {m["label"]: m for m in result["moves"]}
        self.assertEqual(groups["s.LP"]["stateIds"], [0, 1])
        self.assertEqual(groups["j.D+MP"]["stateIds"], [3, 4])
        self.assertEqual(groups["s.MP"]["stateIds"], [2])

    def test_longest_alias_keeps_stance_charge_and_individual_attacks_separate(self):
        c = self.states(["DragonStance_Start", "DragonStance_Loop", "DragonStance_Sing", "DragonStance_Sing_Lv2"])
        result = self.resolve(c, [["DragonStance"], ["DragonStance_Sing"]])
        groups = {m["id"]: m["stateIds"] for m in result["moves"]}
        self.assertEqual(groups, {"dragon-stance": [0, 1], "dragon-stance-sing": [2, 3]})

    def test_reverse_and_chain_dispatchers_belong_to_the_actual_air_normal(self):
        c = self.states(["j.WP", "Reverse_j.WP", "NewChain_j.WP"])
        result = self.resolve(c, [])
        self.assertEqual(len(result["moves"]), 1)
        self.assertEqual(result["moves"][0]["stateIds"], [0, 1, 2])

    def test_throw_wrapper_mentioning_a_button_is_not_a_new_normal(self):
        c = self.states(["s.KaraThrow_Back_From_s.WK", "s.Throw_Back", "s.WK"])
        result = self.resolve(c, [])
        self.assertEqual(next(m for m in result["moves"] if m["id"] == "ground-throw")["stateIds"], [0, 1])

    def test_input_cancel_to_another_entry_does_not_absorb_that_move(self):
        c = self.states(["s.First_L", "First_End", "s.Other_L"], {0: 1})
        c["states"][0]["conditionalEdges"] = [dict(target=2, declaredSteps=3, flags=0, hasInputBytes=True)]
        result = self.resolve(c, [["First"], ["Other"]])
        self.assertTrue(all(m["available"] for m in result["moves"]))
        self.assertEqual(next(m for m in result["moves"] if m["id"] == "first")["stateIds"], [0, 1])

    def test_shared_timed_recovery_blocks_both_false_activation_and_missing_tail(self):
        c = self.states(["s.First", "First_End", "Second_Recovery", "s.Second"], {1: 2})
        result = self.resolve(c, [["First"], ["Second"]])
        self.assertTrue(all(not m["available"] for m in result["moves"]))
        self.assertEqual({s["status"] for s in result["attackCoverage"]}, {"shared-phase-review"})

    def test_throw_hit_confirm_steps_do_not_hide_shared_success_phases(self):
        c = self.states(["s.CmdThrow", "CmdThrowSuccess", "s.ArmoredCmdThrow"])
        c["states"][2]["conditionalEdges"] = [dict(target=1, declaredSteps=1, flags=4, hasInputBytes=True)]
        result = self.resolve(c, [["CmdThrow", "CmdThrowSuccess"], ["ArmoredCmdThrow"]])
        self.assertTrue(all(not m["available"] for m in result["moves"]))

    def test_falling_bomb_is_a_move_entry_not_a_fall_recovery(self):
        c = self.states(["s.First", "s.FallingBomb"])
        c["states"][0]["conditionalEdges"] = [dict(target=1, declaredSteps=2, flags=0, hasInputBytes=True)]
        result = self.resolve(c, [["First"], ["FallingBomb"]])
        self.assertTrue(all(m["available"] for m in result["moves"]))

    def test_zero_time_dispatcher_does_not_claim_a_visible_shared_recovery(self):
        c = self.states(["s.First", "Second_ReversalWindow", "s.Second"], {0: 1})
        c["states"][1]["timedTicks"] = 0
        result = self.resolve(c, [["First"], ["Second"]])
        self.assertTrue(all(m["available"] for m in result["moves"]))

    def test_defensive_tech_states_never_join_a_throw(self):
        c = self.states(["s.Throw", "s.ThrowRecovery", "s.ThrowTech", "s.ThrowTeched", "s.Hit"], tags={4: ["HITSTUN"]})
        result = self.resolve(c, [])
        self.assertEqual(result["moves"][0]["stateIds"], [0, 1])
        self.assertEqual([s["status"] for s in result["attackCoverage"]], ["mapped", "mapped", "unmapped", "unmapped"])

    def test_every_character_and_every_available_move_has_a_matching_compiled_entry(self):
        portable = json.loads((ROOT / "PalleteEditor/data01/move-families.json").read_text(encoding="utf-8"))
        expected = {name for name, _ in builder.catalog.preview.PLAYABLE_CHARACTERS}
        self.assertEqual({c["character"] for c in portable["characters"]}, expected)
        compiled = (ROOT / "PalleteEditor/Data/CharacterMoveDefinitions.inl").read_text(encoding="utf-8")
        count = 0
        for c in portable["characters"]:
            self.assertTrue(c["moves"])
            for move in c["moves"]:
                identifier = c["character"].lower() + "/" + move["id"]
                self.assertEqual(compiled.count(json.dumps(identifier) + ","), 1)
                self.assertIn(json.dumps(c["foitSha256"]), compiled)
                count += 1
        self.assertEqual(sum(line.startswith("{") for line in compiled.splitlines()), count)

    def test_roster_cannot_silently_shrink(self):
        with self.assertRaises(ValueError):
            builder.build(dict(format="skullgirls-move-catalog", version=1, characters=[]), dict(version=1, families={}))

    def test_reviewed_beowulf_finishers_are_exposed_with_complete_disjoint_bodies(self):
        portable=json.loads((ROOT / "PalleteEditor/data01/move-families.json").read_text())
        moves={m["id"]:m for c in portable["characters"] if c["character"]=="Beowulf" for m in c["moves"]}
        expected={"grab-mode-bulldog":[[319,329]],"grab-mode-gringo-killa":[[330,337]],"grab-mode-snapback":[[285,289]]}
        compiled=(ROOT / "PalleteEditor/Data/CharacterMoveDefinitions.inl").read_text()
        for identifier,ranges in expected.items():
            line=next(line for line in compiled.splitlines() if line.startswith('{"beowulf/'+identifier+'",'))
            self.assertIn("{{"+str(ranges[0][0])+","+str(ranges[0][1])+"}}",line)
            self.assertEqual(len(moves[identifier]["states"]),sum(b-a+1 for a,b in ranges))
            self.assertFalse(set(moves[identifier]["states"]) & set(moves["grab-mode"]["states"]))

    def test_shared_route_expands_through_loop_and_landing_but_not_cancel_entries(self):
        c = self.states(["s.First", "First_End", "Second_Loop", "Second_Land", "s.Second", "s.Other"], {1:2,2:2,3:5})
        c["states"][2]["conditionalTargets"]=[3,5]
        c["states"][2]["conditionalEdges"]=[dict(target=t,declaredSteps=1,flags=0,hasInputBytes=True) for t in (3,5)]
        reviewed=self.resolve(c,[["First"],["Second"],["Other"]])
        report=builder.context_routes(dict(characters=[reviewed]),dict(characters=[c]))
        moves={m["id"]:m for m in report["characters"][0]["moves"]}
        self.assertEqual(moves["first"]["stateIds"],[0,1,2,3])
        self.assertNotIn(4,moves["first"]["stateIds"])
        self.assertNotIn(5,moves["first"]["stateIds"])
        self.assertTrue(moves["first"]["contextual"] and moves["second"]["contextual"])
        self.assertFalse(moves["other"]["contextual"])
        self.assertIn((2,3),report["characters"][0]["routeEdges"])


if __name__ == "__main__":
    unittest.main()
