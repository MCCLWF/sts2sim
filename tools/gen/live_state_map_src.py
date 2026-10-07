#!/usr/bin/env python3
"""Source of data/live_state_map.json: where the C++ port keeps each private
field of a live game model.

The CombatSolver mod reads a real combat out of the game process. Framework
fields (HP, piles, amounts, ...) have named simulator fields; a subclass's own
fields do not: each port in game/Models/** stores them in the POD `extra`
slots of its state record, sometimes re-encoded. This table records that
layout per model so the mod's capture is data-driven:

  E(slot)                    bool/int/enum/integral decimal copied to extra[slot]
  E(slot, plus=k)            extra[slot] = value + k
  E(slot, frm=n)             extra[slot] = n - value          (count-down vs count-up)
  E(slot, zero_when=d)       extra[slot] = 0 if value == d    (port stores "unset" lazily)
  E(slot, has=j)             also extra[j] = 1 when the value is present
                             (non-null, or not the zero_when default)
  E(slot, decimal_bits=True) a decimal as its four 32-bit words in extra[slot..slot+3]
                             (minus="d": the words of value - d)
  E(slot, ref="card")        object reference -> pool index (card/creature/power)
  E(slot, model="relics")    ModelId or canonical model -> dense id (null -> 0)
  V(name, E(slot))           a DynamicVar the game counts in, kept in extra[slot] by the port
  V(name, CANONICAL)         a DynamicVar that only mirrors mapped state for display
  S(name)                    placed by the owning capture component under `name`
  IGNORE(why)                presentation only, or derived from mapped state
  CONST(v)                   must hold this value (test hooks, readonly constants)
  POST_COMBAT(why)           read only by combat-end hooks; not transported (see below)
  RESTING(v, why)            has no simulator counterpart; only its resting value
                             (None / "empty" / int) is representable at an idle root

Every entry is checked two ways: tools/gen/check_live_state_map.py requires
the table to classify every field in data/live_field_inventory.json, and
tests/live/live_capture_diff.py compares captures built from it with the
simulator's own state at the same decision of real combats.

Run to regenerate: tools/gen/live_state_map_src.py
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def E(slot, **options):
    if "frm" in options:
        options["from"] = options.pop("frm")
    return dict({"extra": slot}, **options)


def S(name):
    return {"state": name}


def IGNORE(why):
    return {"ignore": why}


def CONST(value):
    return {"const": value}


def POST_COMBAT(why):
    """CombatManager::CheckWinCondition takes the SafeOutcome boundary before
    EndCombatInternal runs any combat-end hook, and the mod's search stops
    there (CombatSearchState never settles an outcome). A field read only by
    those hooks therefore cannot influence a search from a live root."""
    return {"post_combat": why}


def RESTING(value, why):
    return {"unsupported": why, "resting": value}


CANONICAL = {"canonical": True}


def V(name, encoding):
    """A `var:` key for the tables below."""
    return ("var:" + name, encoding)


ACTIVATING = IGNORE("RelicModel activation pulse (flash/sfx); the counter it accompanies is mapped")
HOVER = IGNORE("hover tip cache")
VFX = IGNORE("visual node")
ANIM = IGNORE("selects an animation state only")
MACHINE_STATE = IGNORE("reference to a state of the move machine, which is compared as a whole")
TEST_HOOK = CONST(-1)  # TestEnergyCostOverride: unset outside the game's own tests
PER_PLAY = "written and cleared inside one card play; an idle root has none in progress"

RELICS = {
    # Orobas.h stores the two option-display snapshots as card references
    # (extra 0/1); ArchaicTooth.h never reads them once the relic is owned.
    "ARCHAIC_TOOTH": {"_serializableStarterCard": IGNORE("option-display snapshot; pickup searches the deck again"),
                      "_serializableAncientCard": IGNORE("option-display snapshot; pickup searches the deck again"),
                      "_extraHoverTips": HOVER},
    "ART_OF_WAR": {"_anyAttacksPlayedLastTurn": E(0), "_anyAttacksPlayedThisTurn": E(1)},
    "BEATING_REMNANT": {"_damageReceivedThisTurn": E(0)},
    "BELT_BUCKLE": {"_dexterityApplied": E(0)},
    "BING_BONG": {"_cardsToSkip": S("bing_bong_cards")},
    "BONE_TEA": {"_combatsLeft": E(0, frm=1)},
    "BOOK_OF_FIVE_RINGS": {"_isActivating": ACTIVATING, "_cardsAdded": E(0)},
    "BRILLIANT_SCARF": {"_cardsPlayedThisTurn": E(0)},
    "BURNING_STICKS": {"_wasUsedThisCombat": E(0)},
    "BYRDPIP": {"_skin": IGNORE("cosmetic skin name")},
    "CENTENNIAL_PUZZLE": {"_usedThisCombat": E(0)},
    "DEMON_TONGUE": {"_triggeredThisTurn": E(0)},
    "DUSTY_TOME": {"_ancientCard": E(0, model="cards"), "_extraHoverTips": HOVER},
    "EMBER_TEA": {"_combatsLeft": E(0, frm=5)},
    "FAKE_HAPPY_FLOWER": {"_isActivating": ACTIVATING, "_turnsSeen": E(0)},
    "FAKE_ORICHALCUM": {"_shouldTrigger": E(0)},
    "FAKE_SNECKO_EYE": {"_testEnergyCostOverride": TEST_HOOK},
    "FAKE_VENERABLE_TEA_SET": {"_gainEnergyInNextCombat": E(0)},
    "FISHING_ROD": {"_combatsSeen": E(0)},
    "FUR_COAT": {"_furCoatActIndex": S("fur_coat_act_index"), "<FurCoatCoordCols>k__BackingField": S("fur_coat_cols"),
                 "<FurCoatCoordRows>k__BackingField": S("fur_coat_rows"), "<FurCoatCoordsSet>k__BackingField": S("fur_coat_set")},
    "GALACTIC_DUST": {"_isActivating": ACTIVATING, "_starsSpent": E(0)},
    "GIRYA": {"_timesLifted": E(0)},
    "GOLDEN_COMPASS": {"_goldenPathAct": E(0, plus=1)},
    "HAPPY_FLOWER": {"_isActivating": ACTIVATING, "_turnsSeen": E(0)},
    "IRON_CLUB": {"_isActivating": ACTIVATING, "_cardsPlayed": E(0)},
    "JOSS_PAPER": {"_isActivating": ACTIVATING, "_cardsExhausted": E(0), "_etherealCount": E(1)},
    "KUNAI": {"_isActivating": ACTIVATING, "_attacksPlayedThisTurn": E(0)},
    "KUSARIGAMA": {"_isActivating": ACTIVATING, "_attacksPlayedThisTurn": E(0)},
    "LASTING_CANDY": {"_isActivating": ACTIVATING, "_combatRewardsSeen": E(0)},
    "LAVA_LAMP": {"_tookDamageThisCombat": E(0)},
    "LAVA_ROCK": {"_hasTriggered": E(0)},
    "LETTER_OPENER": {"_isActivating": ACTIVATING, "_skillsPlayedThisTurn": E(0)},
    "LIZARD_TAIL": {"_wasUsed": E(0)},
    "MAW_BANK": {"_hasItemBeenBought": E(0)},
    "METRONOME": {"_isActivating": ACTIVATING, "_orbsChanneled": E(0)},
    "MINI_REGENT": {"_usedThisTurn": E(0)},
    "MUSIC_BOX": {"_wasUsedThisTurn": E(0), "_cardBeingPlayed": E(1, ref="card")},
    "NUNCHAKU": {"_isActivating": ACTIVATING, "_attacksPlayed": E(0)},
    "ORICHALCUM": {"_shouldTrigger": E(0)},
    "ORNAMENTAL_FAN": {"_isActivating": ACTIVATING, "_attacksPlayedThisTurn": E(0)},
    "PAELS_EYE": {"_usedThisCombat": E(0), "_wasOwnerPartOfLastPlayerTurn": E(1)},
    "PAELS_LEGION": {"_skin": IGNORE("cosmetic skin name"), "_cooldown": E(0), "_triggeredBlockLastTurn": E(3),
                     "_affectedCardPlay": S("paels_legion_play")},
    "PAELS_TEARS": {"_hadLeftoverEnergy": E(0)},
    "PAELS_TOOTH": {"_serializableCards": POST_COMBAT("cards returned by AfterCombatEnd")},
    "PAELS_WING": {"_isActivating": ACTIVATING, "_rewardsSacrificed": E(0)},
    "PEN_NIB": {"_isActivating": ACTIVATING, "_attacksPlayed": E(0), "_attackToDouble": E(1, ref="card")},
    "PENDULUM": {"_isActivating": ACTIVATING, "_turnsSeen": E(0)},
    "PERMAFROST": {"_activatedThisCombat": E(0)},
    "POCKETWATCH": {"_cardsPlayedThisTurn": E(0), "_cardsPlayedLastTurn": E(1)},
    "POLLINOUS_CORE": {"_isActivating": ACTIVATING, "_turnsSeen": E(0)},
    "PUMPKIN_CANDLE": {"_kindleCount": E(0)},
    "RAINBOW_RING": {"_attacksPlayedThisTurn": E(0), "_skillsPlayedThisTurn": E(1), "_powersPlayedThisTurn": E(2),
                     "_activationCountThisTurn": E(3)},
    "RED_SKULL": {"_strengthApplied": E(0)},
    "REGALITE": {"_usedThisTurn": E(0)},
    "RUINED_HELMET": {"_usedThisCombat": E(0)},
    "SEA_GLASS": {"_characterId": E(0, model="characters")},
    "SHURIKEN": {"_isActivating": ACTIVATING, "_attacksPlayedThisTurn": E(0)},
    "SILKEN_TRESS": {"_isUsed": E(0)},
    "SILVER_CRUCIBLE": {"_timesUsed": E(0), "_treasureRoomsEntered": E(1)},
    "SNECKO_EYE": {"_testEnergyCostOverride": TEST_HOOK},
    "STONE_CALENDAR": {"_isActivating": ACTIVATING},
    "SWORD_OF_STONE": {"_elitesDefeated": E(0)},
    "TEA_OF_DISCOURTESY": {"_combatsLeft": E(0, frm=1)},
    "THROWING_AXE": {"_usedThisCombat": E(0)},
    "TOUCH_OF_OROBAS": {"_starterRelic": E(0, model="relics"), "_upgradedRelic": E(1, model="relics"), "_extraHoverTips": HOVER},
    "TOY_BOX": {"_isActivating": ACTIVATING, "_combatsSeen": E(0)},
    "TUNING_FORK": {"_isActivating": ACTIVATING, "_skillsPlayed": E(0)},
    "UNSETTLING_LAMP": {"_triggeringCard": E(0, ref="card"), "_doubledPowers": S("unsettling_lamp_powers"),
                        "_isFinishedTriggering": E(2)},
    "VAMBRACE": {"_triggeringCard": E(0, ref="card"), "_blockGainedThisCombat": E(1)},
    "VELVET_CHOKER": {"_cardsPlayedThisTurn": E(0)},
    "VENERABLE_TEA_SET": {"_gainEnergyInNextCombat": E(0)},
    "WINGED_BOOTS": {"_timesUsed": E(0)},
    "WONGOS_MYSTERY_TICKET": {"_combatsFinished": E(0), "_gaveRelic": E(1)},
}

# Slots the port fills from combat history where the game queries the history
# itself; the capture evaluates the same query (EnvLiveInventory.Derived).
RELIC_DERIVED = {
    # RippleBasin.BeforeSideTurnEnd: no attack card play of the owner finished this turn.
    "RIPPLE_BASIN": {"0": "owner_attack_play_finished_this_turn"},
}

SEGMENT = {"DecimillipedeSegment._starterMoveIdx": E(0), "DecimillipedeSegment._deadState": MACHINE_STATE}
MONSTERS = {
    "AEONGLASS": {"_additionalStrength": E(0), "_witherUpgradeCount": E(1)},
    "AXEBOT": {"_stockOverrideAmount": E(0, has=1), "_shouldPlaySpawnAnimation": ANIM},
    "BOWLBUG_ROCK": {"_isOffBalance": E(0)},
    "CALCIFIED_CULTIST": {"_attackSfxStrength": IGNORE("attack sound parameter")},
    "CEREMONIAL_BEAST": {"_isStunnedByPlowRemoval": E(0), "_isInSecondPhase": E(1), "_inMidCharge": ANIM,
                         "_beastCryState": MACHINE_STATE},
    "CHOMPER": {"_screamFirst": E(0)},
    "CORPSE_SLUG": {"_isRavenous": E(0), "_starterMoveIdx": E(1)},
    "CROSSBOW_RUBY_RAIDER": {"_isCrossbowReloaded": E(0)},
    "CRUSHER": {"_background": VFX},
    "CUBEX_CONSTRUCT": {"_isBurrowed": E(0), "_isCharging": E(1)},
    "DAMP_CULTIST": {"_attackSfxStrength": IGNORE("attack sound parameter")},
    "DECIMILLIPEDE_SEGMENT_BACK": SEGMENT, "DECIMILLIPEDE_SEGMENT_FRONT": SEGMENT, "DECIMILLIPEDE_SEGMENT_MIDDLE": SEGMENT,
    "DEVOTED_SCULPTOR": {"_ritualGain": CONST(9)},
    "FABRICATOR": {"_lastSpawned": E(0, model="monsters", has=1)},
    "FAT_GREMLIN": {"_isAwake": E(0)},
    "FROG_KNIGHT": {"_hasBeetleCharged": E(0)},
    "FUZZY_WURM_CRAWLER": {"_isPuffed": E(0)},
    "GAS_BOMB": {"_hasExploded": E(0)},
    "GREMLIN_MERC": {"_hasSpoken": E(0)},
    "INKLET": {"_middleInklet": E(0)},
    "KIN_FOLLOWER": {"_startsWithDance": E(0)},
    "KIN_PRIEST": {"_speechUsed": IGNORE("gates a one-time speech bubble")},
    "KNOWLEDGE_DEMON": {"_curseOfKnowledgeCounter": E(0), "_isBurnt": E(1)},
    "LAGAVULIN_MATRIARCH": {"_isAwake": IGNORE("gates the wake sound/animation; sleep is AsleepPower"),
                            "_isShellAwake": ANIM, "_sleepingVfx": VFX},
    "LIVING_FOG": {"_bloatAmount": E(0, zero_when=1)},
    "LOUSE_PROGENITOR": {"_curled": E(0)},
    "MECHA_KNIGHT": {"_isWoundUp": ANIM},
    "NIBBIT": {"_isFront": E(0), "_isAlone": E(1)},
    "OWL_MAGISTRATE": {"_isFlying": E(0)},
    "PHANTASMAL_GARDENER": {"_enlargeTriggers": E(0), "<CurrentScale>k__BackingField": IGNORE("visual scale")},
    "PUNCH_CONSTRUCT": {"_startsWithFastPunch": E(0), "_startingHpReduction": E(1)},
    "QUEEN": {"_hasAmalgamDied": E(0), "_amalgam": E(1, ref="creature"), "_burnBrightForMeState": MACHINE_STATE,
              "_enragedState": MACHINE_STATE},
    "ROCKET": {"_background": VFX},
    "SCROLL_OF_BITING": {"_starterMoveIdx": E(0)},
    "SLUMBERING_BEETLE": {"_isAwake": E(0), "_sleepingVfx": VFX},
    "SNEAKY_GREMLIN": {"_isAwake": ANIM},
    "SOUL_FYSH": {"_isInvisible": ANIM},
    "SPINY_TOAD": {"_isSpiny": E(0)},
    "TERROR_EEL": {"_terrorState": MACHINE_STATE},
    "TEST_SUBJECT": {"_deadState": MACHINE_STATE, "_respawns": E(0), "_extraMultiClawCount": E(1)},
    "THE_INSATIABLE": {"_hasLiquified": ANIM},
    "THE_OBSCURA": {"_hasSummoned": ANIM},
    "THIEVING_HOPPER": {"_isHovering": E(0)},
    "TOADPOLE": {"_isFront": E(0)},
    "TOUGH_EGG": {"_hatched": IGNORE("selects the displayed name and death sound"),
                  "_afterHatchedState": MACHINE_STATE,
                  "_isHatched": IGNORE("read once in AfterAddedToRoom; afterwards only animation"),
                  "_hatchPos": IGNORE("visual position")},
    "TUNNELER": {"_isStunned": E(0)},
    "TWO_TAILED_RAT": {"_starterMoveIndex": E(0, plus=1), "_turnsUntilSummonable": E(1, zero_when=2, has=3),
                       "_callForBackupCount": E(2)},
    "VANTOM": {"_scaleTween": VFX},
    "WATERFALL_GIANT": {"_currentPressureGunDamage": E(0), "_steamEruptionDamage": E(1), "_aboutToBlowState": MACHINE_STATE,
                        "_isAboutToBlow": ANIM, "_pressureBuildupIdx": E(2)},
    "WRIGGLER": {"_startStunned": E(0)},
}

CARD_CAPTURE = RESTING("empty", "per-play amount capture: " + PER_PLAY)
POWERS = {
    "ADAPTABLE_POWER": {"data": {"isReviving": E(0)}},
    "AFTERIMAGE_POWER": {"data": {"amountsForPlayedCards": CARD_CAPTURE}},
    "AUTOMATION_POWER": {"data": {"cardsLeft": E(0)}},
    "CALAMITY_POWER": {"data": {"amountsForPlayedCards": CARD_CAPTURE}},
    "CHAINS_OF_BINDING_POWER": {"data": {"boundCardPlayed": E(0)}},
    "CONFUSED_POWER": {"fields": {"_testEnergyCostOverride": TEST_HOOK}},
    "CURL_UP_POWER": {"data": {"playedCard": E(0, ref="card")}},
    "DAMPEN_POWER": {"data": {"casters": S("dampen_casters"), "downgradedCardsToOldUpgradeLevels": S("dampen_downgraded_cards")}},
    "DARK_EMBRACE_POWER": {"data": {"etherealCount": E(0)}},
    "DEMON_FORM_POWER": {"fields": {"_vfx": VFX}},
    "ECHO_FORM_POWER": {"fields": {"_vfx": VFX}},
    "FERAL_POWER": {"data": {"zeroCostAttacksPlayed": E(0)}},
    "GIGANTIFICATION_POWER": {"data": {"commandToModify": RESTING(None, "the attack being modified: " + PER_PLAY)}},
    "HARDENED_SHELL_POWER": {"data": {"damageReceivedThisTurn": S("vars0")}},
    "HELLRAISER_POWER": {"data": {"autoPlayingCards": IGNORE("selects the attack visual of an auto-played card"),
                                  "infiniteAutoPlaysThisTurn": E(0),
                                  "showedCapReachedMessage": IGNORE("suppresses a repeated thought bubble")}},
    "ILLUSION_POWER": {"fields": {"_followUpStateId": S("illusion_follow_up")}, "data": {"isReviving": E(0)}},
    "IMITATION_LEARNING_POWER": {"fields": {"_playerTarget": S("player_target")},
                                 "data": {"cardsAndClones": RESTING("empty", "card/clone pairs: " + PER_PLAY)}},
    "JUGGLING_POWER": {"data": {"attacksPlayedThisTurn": E(0)}},
    "MONOLOGUE_POWER": {"data": {"amountsForPlayedCards": CARD_CAPTURE}},
    "NEMESIS_POWER": {"fields": {"_shouldApplyIntangible": E(0)}},
    "NIGHTMARE_POWER": {"data": {"selectedCard": E(0, ref="card")}},
    "OBLIVION_POWER": {"data": {"amountsForPlayedCards": CARD_CAPTURE}},
    "ORBIT_POWER": {"data": {"energySpent": E(0), "triggerCount": E(1)}},
    "PALE_BLUE_DOT_POWER": {"data": {"alreadyActivatedThisTurn": E(0)}},
    "PANACHE_POWER": {"data": {"alreadyApplied": E(0)}},
    "POSSESS_SPEED_POWER": {"data": {"stolenDexterity": S("stolen_total")}},
    "POSSESS_STRENGTH_POWER": {"data": {"stolenStrength": S("stolen_total")}},
    "REAPER_FORM_POWER": {"fields": {"_vfx": VFX}},
    "REATTACH_POWER": {"data": {"isReviving": E(0)}},
    "RITUAL_POWER": {"fields": {"_wasJustAppliedByEnemy": E(0)}},
    "RUPTURE_POWER": {"data": {"playedCards": CARD_CAPTURE}},
    "SANDPIT_POWER": {"fields": {"_initialAmount": IGNORE("positions the sandpit visual"),
                                 "_initialTargetPosition": IGNORE("positions the sandpit visual")}},
    "SERPENT_FORM_POWER": {"fields": {"_vfx": VFX}, "data": {"amountsForPlayedCards": CARD_CAPTURE}},
    "SKITTISH_POWER": {"data": {"hasGainedBlockThisTurn": E(0)}},
    "SLOTH_POWER": {"fields": {"_cardsPlayedThisTurn": E(0)}},
    "SOULBOUND_POWER": {"fields": {"_isAddingSoul": RESTING(0, "re-entrancy guard held only while its own effect runs")}},
    "STORM_POWER": {"data": {"amountsForPlayedCards": CARD_CAPTURE}},
    "STRANGLE_POWER": {"data": {"amountsForPlayedCards": CARD_CAPTURE}},
    "SUBROUTINE_POWER": {"data": {"amountsForPlayedCards": CARD_CAPTURE}},
    "SURROUNDED_POWER": {"fields": {"_facing": E(0)}},
    "SWIPE_POWER": {"fields": {"_stolenCard": E(0, ref="card")}},
    "TENDER_POWER": {"fields": {"_cardsPlayedThisTurn": E(0)}},
    "VIGOR_POWER": {"data": {"commandToModify": RESTING(None, "the attack being modified: " + PER_PLAY),
                             "amountWhenAttackStarted": E(1)}},
    "VOID_FORM_POWER": {"fields": {"_vfx": VFX}, "data": {"cardsPlayedThisTurn": E(0)}},
    "WITHERING_PRESENCE_POWER": {"vars": {"CardsLeft": E(0)}},
}

MOCK = RESTING(None, "set only by the game's own tests")
MIRROR = "mirrors a DynamicVar, which is captured as a variable override"
CARDS = {
    "BOUNCING_FLASK": {"_vfxTint": IGNORE("visual tint")},
    "CLAW": {"_extraDamageFromClawPlays": S("growth_damage")},
    "DISCOVERY": {"_mockSelectedCard": MOCK},
    "DOWSING": {"_roomsEntered": E(0)},
    "GENETIC_ALGORITHM": {"_currentBlock": IGNORE(MIRROR), "_increasedBlock": E(0)},
    # Guilty.cs mirrors 5 - CombatsSeen into the Combats variable for its text.
    "GUILTY": dict([("_combatsSeen", E(0)), V("Combats", CANONICAL)]),
    "KINGLY_PUNCH": {"_extraDamage": E(0)},
    "MAD_SCIENCE": {"_tinkerTimeType": E(0), "_tinkerTimeRider": E(1), "_mockedChaosCard": MOCK},
    "MAUL": {"_extraDamageFromMaulPlays": E(0)},
    "RAMPAGE": {"_extraDamageFromPlays": S("growth_damage")},
    "REGRET": {"_cardsInHand": E(0)},
    # SovereignBlade.h SaveCurrentValues: the distance from the field initializers
    # (10 damage, 1 repeat): the damage decimal in four words, then the repeats.
    "SOVEREIGN_BLADE": {"_currentDamage": E(0, decimal_bits=True, minus="10"), "_currentRepeats": E(4, plus=-1),
                        "_createdThroughForge": IGNORE("selects the forge visual")},
    "SPLASH": {"_mockGeneratedCard": MOCK},
    "SPOILS_MAP": {"_spoilsActIndex": E(0), "<SpoilsCoord>k__BackingField": S("spoils_coord")},
    "THE_SCYTHE": {"_currentDamage": IGNORE(MIRROR), "_increasedDamage": E(0)},
    "THRASH": {"_extraDamage": S("growth_damage")},
    "UP_MY_SLEEVE": {"_timesPlayedThisCombat": E(0)},
    "WITHER": {"_fakeUpgradeLevel": E(0)},
}
# CardState.enchantment_extra (two slots).
ENCHANTMENTS = {
    "GLAM": {"_usedThisCombat": E(0)},
    "MOMENTUM": {"_extraDamage": E(0)},
    "SLITHER": {"_testEnergyCostOverride": TEST_HOOK},
}
POTIONS = {
    "ASHWATER": {"_tint": IGNORE("potion color")},
    "SNECKO_OIL": {"_testEnergyCostOverride": TEST_HOOK},
}
ORBS = {"DARK_ORB": {"_evokeVal": S("dark_evoke_val")}, "GLASS_ORB": {"_passiveVal": S("glass_passive_val")}}
ENCOUNTERS = {
    "GREMLIN_MERC_NORMAL": {"_goldWasStolen": S("gremlin_merc_gold_was_stolen")},
    "BATTLEWORN_DUMMY_EVENT_V1_ENCOUNTER": {"BattlewornDummyEventEncounter._ranOutOfTime": S("battleworn_dummy_ran_out_of_time")},
    "BATTLEWORN_DUMMY_EVENT_V2_ENCOUNTER": {"BattlewornDummyEventEncounter._ranOutOfTime": S("battleworn_dummy_ran_out_of_time")},
    "BATTLEWORN_DUMMY_EVENT_V3_ENCOUNTER": {"BattlewornDummyEventEncounter._ranOutOfTime": S("battleworn_dummy_ran_out_of_time")},
}


# Creature.StunInternal logic: C# "Class.Method" -> the native registration
# (owner, description) when the port named it differently.
STUN_MOVES = {
    "FlutterPower.StunnedMove": {"owner": "FlutterPower", "name": "stunned_noop"},
}


def wrap(models):
    """Split the per-model dict into its field and variable sections."""
    result = {}
    for entry, fields in models.items():
        item = {"fields": {name: value for name, value in fields.items() if not name.startswith("var:")}}
        variables = {name[4:]: value for name, value in fields.items() if name.startswith("var:")}
        if variables:
            item["vars"] = variables
        result[entry] = item
    return result


def with_derived(models, derived):
    for entry, slots in derived.items():
        models.setdefault(entry, {"fields": {}})["derived"] = slots
    return models


def document():
    return {
        "format": "sts2sim.live_state_map", "version": 1,
        "note": "GENERATED by tools/gen/live_state_map_src.py. Do not edit.",
        "relics": with_derived(wrap(RELICS), RELIC_DERIVED), "monsters": wrap(MONSTERS), "powers": POWERS, "cards": wrap(CARDS),
        "potions": wrap(POTIONS), "orbs": wrap(ORBS), "encounters": wrap(ENCOUNTERS),
        "enchantments": wrap(ENCHANTMENTS), "afflictions": {},
        "stun_moves": STUN_MOVES,
    }


def render():
    doc = document()
    lines = ["{"]
    keys = list(doc)
    for index, key in enumerate(keys):
        value = doc[key]
        comma = "," if index + 1 < len(keys) else ""
        if not isinstance(value, dict):
            lines.append(" %s: %s%s" % (json.dumps(key), json.dumps(value), comma))
            continue
        lines.append(" %s: {" % json.dumps(key))
        entries = sorted(value)
        for position, entry in enumerate(entries):
            lines.append("  %s: %s%s" % (json.dumps(entry), json.dumps(value[entry], sort_keys=True),
                                         "," if position + 1 < len(entries) else ""))
        lines.append(" }" + comma)
    lines.append("}")
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    with open(os.path.join(ROOT, "data", "live_state_map.json"), "w") as handle:
        handle.write(render())
