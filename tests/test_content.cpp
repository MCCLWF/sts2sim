// Spot checks of the generated content tables against values read from the
// game (see data/content.json), plus structural invariants.
#include <cstdio>
#include <set>
#include <string>

#include "content/generated/tables.h"

using namespace sts2;

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool ok, const std::string& what) {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::printf("  FAIL: %s\n", what.c_str());
    }
}

const ModelInfo* info(std::string_view id) {
    for (const ModelInfo& m : all_model_infos()) {
        if (m.id == id) return &m;
    }
    return nullptr;
}

}  // namespace

int main() {
    // Bash: 2 energy, 8 damage + 2 Vulnerable, upgrades to 10 + 3.
    const CardData& bash = data(CardId::BASH);
    check(bash.type == CardType::Attack && bash.rarity == CardRarity::Basic, "Bash type/rarity");
    check(bash.target == TargetType::AnyEnemy, "Bash target");
    check(bash.pool == CardPoolId::IRONCLAD_CARD_POOL, "Bash pool");
    check(bash.levels.size() == 2 && bash.levels[0].energy_cost == 2, "Bash cost/levels");
    check(bash.vars.size() == 2 && bash.vars[0].name == "Damage" && bash.vars[1].power == PowerId::VULNERABLE_POWER,
          "Bash vars");
    check(bash.levels[0].vars[0] == Dec(8) && bash.levels[1].vars[0] == Dec(10), "Bash damage 8 -> 10");
    check(bash.levels[1].vars[1] == Dec(3), "Bash vulnerable 2 -> 3");

    // Body Slam's upgrade lowers the cost 1 -> 0.
    const CardData& slam = data(CardId::BODY_SLAM);
    check(slam.levels.size() == 2 && slam.levels[0].energy_cost == 1 && slam.levels[1].energy_cost == 0,
          "Body Slam cost upgrade");

    // Vulnerable multiplies by 1.5 (a decimal var).
    const PowerData& vuln = data(PowerId::VULNERABLE_POWER);
    check(vuln.type == PowerType::Debuff && vuln.stack_type == PowerStackType::Counter, "Vulnerable type");
    check(vuln.vars.size() == 1 && vuln.vars[0].base == Dec::lit(15, 1), "Vulnerable 1.5x");

    // Nibbit HP 42-46, 44-48 from ToughEnemies.
    const MonsterData& nib = data(MonsterId::NIBBIT);
    check(nib.min_hp == 42 && nib.max_hp == 46 && nib.min_hp_tough == 44 && nib.max_hp_tough == 48, "Nibbit HP");

    // Ironclad starts with 80 HP, 99 gold, 3 energy, 10 cards, Burning Blood.
    const CharacterData& ic = data(CharacterId::IRONCLAD);
    check(ic.starting_hp == 80 && ic.starting_gold == 99 && ic.max_energy == 3, "Ironclad stats");
    check(ic.starting_deck.size() == 10 && ic.starting_deck[0] == CardId::STRIKE_IRONCLAD, "Ironclad deck");
    check(ic.starting_relics.size() == 1 && ic.starting_relics[0] == RelicId::BURNING_BLOOD, "Ironclad relic");
    check(data(RelicId::BURNING_BLOOD).vars.size() == 1 && data(RelicId::BURNING_BLOOD).vars[0].base == Dec(6),
          "Burning Blood heals 6");

    // Overgrowth has Neow as its ancient and three bosses.
    const ActData& og = data(ActId::OVERGROWTH);
    check(og.ancients.size() == 1 && og.ancients[0] == EventId::NEOW, "Overgrowth ancient");
    check(og.boss.size() == 3 && og.index == 0, "Overgrowth bosses");

    // Override sets: Bash implements OnPlay and OnUpgrade; Burning Blood heals after combat.
    const ModelInfo* bi = info("CARD.BASH");
    check(bi && bi->overrides.has(Member::CardModel_OnPlay) && bi->overrides.has(Member::CardModel_OnUpgrade),
          "Bash overrides");
    check(bi && !bi->overrides.has(Member::AbstractModel_AfterCardPlayed), "Bash does not hook AfterCardPlayed");
    const ModelInfo* bb = info("RELIC.BURNING_BLOOD");
    check(bb && bb->overrides.has(Member::AbstractModel_AfterCombatVictory), "Burning Blood overrides");
    check(bb && bb->source == "Models/Relics/BurningBlood.cs", "Burning Blood source path");

    // Structural invariants.
    for (const CardData& c : all_cards()) {
        check(!c.levels.empty(), std::string(c.entry) + " has levels");
        for (const CardLevel& l : c.levels) check(l.vars.size() == c.vars.size(), std::string(c.entry) + " level vars");
    }
    std::set<std::string_view> ids;
    for (const ModelInfo& m : all_model_infos()) check(ids.insert(m.id).second, std::string(m.id) + " unique");
    check(all_cards().size() == 596 && all_relics().size() == 299 && all_powers().size() == 265, "content counts");

    std::printf("content  %8d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
