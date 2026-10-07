#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct SpectralKnight : MonsterModel {
    SIM_MODEL(SpectralKnight, MonsterModel)

    int32_t MinInitialHp() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::ToughEnemies, 97, 93); }
    int32_t MaxInitialHp() const { return MinInitialHp(); }
    int32_t SoulSlashDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 17, 15); }
    int32_t SoulFlameDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 4, 3); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState hex = b.Move("HEX", SIM_MOVE(SpectralKnight, HexMove), DebuffIntent());
        MoveState slash = b.Move("SOUL_SLASH", SIM_MOVE(SpectralKnight, SoulSlashMove), SingleAttackIntent(SoulSlashDamage()));
        MoveState flame = b.Move("SOUL_FLAME", SIM_MOVE(SpectralKnight, SoulFlameMove), MultiAttackIntent(SoulFlameDamage(), 3));
        RandomBranchState random = b.RandomBranch("RAND");
        hex.SetFollowUpState(slash);
        slash.SetFollowUpState(random);
        flame.SetFollowUpState(random);
        random.AddBranch(slash, 2);
        random.AddBranch(flame, MoveRepeatType::CannotRepeat);
        b.Add(hex);
        b.Add(slash);
        b.Add(flame);
        b.Add(random);
        return b.Build(hex);
    }

    void HexMove(const CreatureList& targets) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        for (sts2::Creature target : targets)
            PowerCmd::Apply(PowerId::HEX_POWER, ctx, target, Dec(2), this->Creature(), CardModel{});
    }

    void SoulSlashMove(const CreatureList&) const {
        DamageCmd::Attack(SoulSlashDamage()).FromMonster(*this).Execute(nullptr);
    }

    void SoulFlameMove(const CreatureList&) const {
        DamageCmd::Attack(SoulFlameDamage()).WithHitCount(3).FromMonster(*this).Execute(nullptr);
    }
};

}  // namespace sts2
