// Models/Monsters/ToughEgg.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct ToughEgg : MonsterModel {
    SIM_MODEL(ToughEgg, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);
    SIM_NOT_PORTED(SetupSkins);
    SIM_NOT_PORTED(get_TakeDamageSfxType);

    int32_t HatchlingMinHp() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::ToughEnemies, 20, 19);
    }
    int32_t HatchlingMaxHp() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::ToughEnemies, 23, 22);
    }
    int32_t NibbleDamage() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 5, 4);
    }

    void AfterAddedToRoom() const {
        int32_t hatchTurns = CombatState().CurrentSide() != CombatSide::Enemy ? 1 : 2;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::HATCH_POWER, ctx, Creature(), Dec(hatchTurns), Creature(), CardModel{});
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState hatch = b.Move("HATCH_MOVE", SIM_MOVE(ToughEgg, HatchMove), SummonIntent());
        MoveState nibble = b.Move("NIBBLE_MOVE", SIM_MOVE(ToughEgg, NibbleMove), SingleAttackIntent(NibbleDamage()));
        hatch.SetFollowUpState(nibble);
        nibble.SetFollowUpState(nibble);
        b.Add(hatch);
        b.Add(nibble);
        return b.Build(hatch);
    }

    void HatchMove(const CreatureList&) const {
        PowerCmd::Remove(PowerId::HATCH_POWER, Creature());
        for (PowerModel power : Creature().Powers()) {
            if (power.PowerId_() != PowerId::MINION_POWER) PowerCmd::Remove(power);
        }
        int32_t hp = RunRng().Niche().NextInt(HatchlingMinHp(), HatchlingMaxHp() + 1);
        Dec scaledHp = sts2::Creature::ScaleHpForMultiplayer(Dec(hp), CombatState().Encounter(),
                                                              static_cast<int32_t>(CombatState().Players().size()),
                                                              CombatState().RunState().CurrentActIndex());
        CreatureCmd::SetMaxAndCurrentHp(Creature(), scaledHp);
    }
    void NibbleMove(const CreatureList&) const { DamageCmd::Attack(NibbleDamage()).FromMonster(*this).Execute(nullptr); }
};

}  // namespace sts2
