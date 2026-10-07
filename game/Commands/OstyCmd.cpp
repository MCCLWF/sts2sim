// Commands/OstyCmd.cs
#include "game/Commands/OstyCmd.h"

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Hooks/Hook.h"
#include "game/Models/MonsterModel.h"

namespace sts2::OstyCmd {

Creature Summon(PlayerChoiceContext& choiceContext, Player summoner, Dec amount, AbstractModel source) {
    CombatState combat = summoner.Creature().CombatState();
    amount = Hook::ModifySummonAmount(combat, summoner, amount, source);
    Creature current = summoner.PlayerCombatState().GetPet(MonsterId::OSTY);
    if (amount == Dec(0)) return current;
    Creature osty;
    for (Creature ally : combat.Allies()) {
        if (ally.IsMonster() && ally.Monster().MonsterId_() == MonsterId::OSTY && ally.PetOwner() == summoner) {
            osty = ally;
            break;
        }
    }
    if (current && current.IsAlive()) {
        CreatureCmd::GainMaxHp(current, amount);
    } else {
        bool reviving = static_cast<bool>(osty);
        if (reviving) {
            if (osty.IsAlive()) throw GameError("Osty already alive outside pet list");
            summoner.PlayerCombatState().AddPetInternal(osty);
        } else {
            MonsterModel monster = MonsterModel::ToMutable(&summoner.sim(), MonsterId::OSTY);
            osty = combat.CreateCreature(monster, CombatSide::Player, -1);
            PlayerCmd::AddPet(osty, summoner);
            PowerCmd::Apply(PowerId::DIE_FOR_YOU_POWER, choiceContext, osty, Dec(1), Creature{}, CardModel{});
        }
        CreatureCmd::SetMaxHp(osty, amount);
        CreatureCmd::Heal(osty, amount);
        if (reviving) Hook::AfterOstyRevived(combat, osty);
    }
    // Combat history Summoned is presentation/statistics in the headless engine.
    Hook::AfterSummon(combat, choiceContext, summoner, amount);
    return summoner.PlayerCombatState().GetPet(MonsterId::OSTY);
}

}  // namespace sts2::OstyCmd
