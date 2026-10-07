// Models/Monsters/LivingFog.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/EncounterModel.h"
#include "game/Models/MonsterModel.h"
#include "game/Models/Powers/SmoggyPower.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct LivingFog : MonsterModel {
    SIM_MODEL(LivingFog, MonsterModel)

    int32_t BloatAmount() const { return extra(0) == 0 ? 1 : extra(0); }
    int32_t AdvancedGasDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 9, 8); }
    int32_t BloatDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 6, 5); }
    int32_t SuperGasBlastDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 9, 8); }
    bool ShouldFadeAfterDeath() const { return false; }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState advanced = b.Move("ADVANCED_GAS_MOVE", SIM_MOVE(LivingFog, AdvancedGasMove),
                                    SingleAttackIntent(AdvancedGasDamage()), CardDebuffIntent());
        MoveState bloat = b.Move("BLOAT_MOVE", SIM_MOVE(LivingFog, BloatMove),
                                  SingleAttackIntent(BloatDamage()), SummonIntent());
        MoveState superBlast = b.Move("SUPER_GAS_BLAST_MOVE", SIM_MOVE(LivingFog, SuperGasBlastMove),
                                      SingleAttackIntent(SuperGasBlastDamage()));
        advanced.SetFollowUpState(bloat);
        bloat.SetFollowUpState(superBlast);
        superBlast.SetFollowUpState(bloat);
        b.Add(advanced); b.Add(superBlast); b.Add(bloat);
        return b.Build(advanced);
    }

    void AdvancedGasMove(const CreatureList& targets) const {
        DamageCmd::Attack(AdvancedGasDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::SMOGGY_POWER, ctx, targets, Dec(1), Creature(), CardModel{});
    }

    void BloatMove(const CreatureList&) const {
        for (int32_t i = 0; i < BloatAmount(); ++i) {
            std::string_view slot = EncounterModel(sim_).GetNextSlot(CombatState());
            if (slot.empty()) break;
            CreatureCmd::Add(MonsterModel::ToMutable(sim_, MonsterId::GAS_BOMB), CombatState(), CombatSide::Enemy, slot);
        }
        DamageCmd::Attack(BloatDamage()).FromMonster(*this).Execute(nullptr);
    }

    void SuperGasBlastMove(const CreatureList&) const {
        DamageCmd::Attack(SuperGasBlastDamage()).FromMonster(*this).Execute(nullptr);
    }
};

}  // namespace sts2
