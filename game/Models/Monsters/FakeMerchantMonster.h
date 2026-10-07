// Models/Monsters/FakeMerchantMonster.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct FakeMerchantMonster : MonsterModel {
    SIM_MODEL(FakeMerchantMonster, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);
    SIM_NOT_PORTED(get_TakeDamageSfxType);

    int32_t SwipeDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 15, 13); }
    int32_t ThrowRelicDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 10, 9); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState swipe = b.Move("SWIPE_MOVE", SIM_MOVE(FakeMerchantMonster, SwipeMove), SingleAttackIntent(SwipeDamage()));
        MoveState spew = b.Move("SPEW_COINS_MOVE", SIM_MOVE(FakeMerchantMonster, SpewCoinsMove), MultiAttackIntent(2, 8));
        MoveState throwRelic = b.Move("THROW_RELIC_MOVE", SIM_MOVE(FakeMerchantMonster, ThrowRelicMove), SingleAttackIntent(ThrowRelicDamage()), DebuffIntent());
        MoveState enrage = b.Move("ENRAGE_MOVE", SIM_MOVE(FakeMerchantMonster, EnrageMove), BuffIntent());

        RandomBranchState random = b.RandomBranch("RAND_MOVE");
        random.AddBranch(swipe, MoveRepeatType::CannotRepeat);
        random.AddBranch(spew, MoveRepeatType::CannotRepeat);
        random.AddBranch(throwRelic, MoveRepeatType::CannotRepeat);
        random.AddBranch(enrage, 3, MoveRepeatType::CannotRepeat);
        swipe.SetFollowUpState(random);
        spew.SetFollowUpState(random);
        enrage.SetFollowUpState(random);

        RandomBranchState afterThrow = b.RandomBranch("RAND_ATTACK_MOVE");
        afterThrow.AddBranch(swipe, MoveRepeatType::CannotRepeat);
        afterThrow.AddBranch(spew, MoveRepeatType::CannotRepeat);
        afterThrow.AddBranch(throwRelic, MoveRepeatType::CannotRepeat);
        throwRelic.SetFollowUpState(afterThrow);

        b.Add(swipe);
        b.Add(spew);
        b.Add(throwRelic);
        b.Add(enrage);
        b.Add(random);
        b.Add(afterThrow);
        return b.Build(swipe);
    }

    void SwipeMove(const CreatureList&) const {
        DamageCmd::Attack(SwipeDamage()).FromMonster(*this).Execute(nullptr);
    }
    void SpewCoinsMove(const CreatureList&) const {
        DamageCmd::Attack(2).WithHitCount(8).FromMonster(*this).Execute(nullptr);
    }
    void ThrowRelicMove(const CreatureList& targets) const {
        DamageCmd::Attack(ThrowRelicDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::FRAIL_POWER, ctx, targets, Dec(1), Creature(), CardModel{});
    }
    void EnrageMove(const CreatureList&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(2), Creature(), CardModel{});
    }
};

}  // namespace sts2
