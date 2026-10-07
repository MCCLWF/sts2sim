// Models/Monsters/TorchHeadAmalgam.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"

namespace sts2 {
struct TorchHeadAmalgam : MonsterModel {
    SIM_MODEL(TorchHeadAmalgam, MonsterModel)
    SIM_NOT_PORTED(GenerateAnimator);

    int32_t StrongTackleDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 32, 26); }
    int32_t TackleDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 22, 18); }
    int32_t WeakTackleDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 16, 14); }
    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::MINION_POWER, ctx, Creature(), Dec(1), Creature(), CardModel{});
    }
    void OnDieToDoom() const {}  // Source turns off visual lights.
    void StrongTackleMove(const CreatureList&) const { DamageCmd::Attack(StrongTackleDamage()).FromMonster(*this).Execute(nullptr); }
    void TackleMove(const CreatureList&) const { DamageCmd::Attack(TackleDamage()).FromMonster(*this).Execute(nullptr); }
    void WeakTackleMove(const CreatureList&) const { DamageCmd::Attack(WeakTackleDamage()).FromMonster(*this).Execute(nullptr); }
    void SoulBeamMove(const CreatureList&) const { DamageCmd::Attack(8).WithHitCount(3).FromMonster(*this).OnlyPlayAnimOnce().Execute(nullptr); }
    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        auto strong = b.Move("STRONG_TACKLE_MOVE", SIM_MOVE(TorchHeadAmalgam, StrongTackleMove), SingleAttackIntent(StrongTackleDamage()));
        auto tackle = b.Move("TACKLE_2_MOVE", SIM_MOVE(TorchHeadAmalgam, TackleMove), SingleAttackIntent(TackleDamage()));
        auto beam = b.Move("BEAM_MOVE", SIM_MOVE(TorchHeadAmalgam, SoulBeamMove), MultiAttackIntent(8, 3));
        auto weak3 = b.Move("TACKLE_3_MOVE", SIM_MOVE(TorchHeadAmalgam, WeakTackleMove), SingleAttackIntent(WeakTackleDamage()));
        auto weak4 = b.Move("TACKLE_4_MOVE", SIM_MOVE(TorchHeadAmalgam, WeakTackleMove), SingleAttackIntent(WeakTackleDamage()));
        strong.SetFollowUpState(tackle); tackle.SetFollowUpState(beam); beam.SetFollowUpState(weak3); weak3.SetFollowUpState(weak4); weak4.SetFollowUpState(beam);
        b.Add(strong); b.Add(tackle); b.Add(beam); b.Add(weak3); b.Add(weak4);
        return b.Build(strong);
    }
};
}  // namespace sts2
