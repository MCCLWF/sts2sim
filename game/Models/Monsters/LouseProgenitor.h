#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct LouseProgenitor : MonsterModel {
    SIM_MODEL(LouseProgenitor, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);
    SIM_NOT_PORTED(get_AttackSfx);
    SIM_NOT_PORTED(get_DeathSfx);
    SIM_NOT_PORTED(get_TakeDamageSfxType);

    bool Curled() const { return extra(0) != 0; }
    void SetCurled(bool value) const { extra(0) = value; }
    int32_t WebDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 10, 9); }
    int32_t PounceDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 16, 14); }
    int32_t CurlBlock() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::ToughEnemies, 18, 14); }
    int32_t GrowStrength() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 7, 5); }

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::CURL_UP_POWER, ctx, Creature(), Dec(CurlBlock()), Creature(), CardModel{});
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState web = b.Move("WEB_CANNON_MOVE", SIM_MOVE(LouseProgenitor, WebMove), SingleAttackIntent(WebDamage()), DebuffIntent());
        MoveState pounce = b.Move("POUNCE_MOVE", SIM_MOVE(LouseProgenitor, PounceMove), SingleAttackIntent(PounceDamage()));
        MoveState curl = b.Move("CURL_AND_GROW_MOVE", SIM_MOVE(LouseProgenitor, CurlAndGrowMove), DefendIntent(), BuffIntent());
        web.SetFollowUpState(curl);
        curl.SetFollowUpState(pounce);
        pounce.SetFollowUpState(web);
        b.Add(curl);
        b.Add(web);
        b.Add(pounce);
        return b.Build(web);
    }

    void WebMove(const CreatureList& targets) const {
        SetCurled(false);
        DamageCmd::Attack(WebDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::FRAIL_POWER, ctx, targets, Dec(2), Creature(), CardModel{});
    }

    void CurlAndGrowMove(const CreatureList&) const {
        CreatureCmd::GainBlock(Creature(), Dec(CurlBlock()), ValueProp::Move, nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(GrowStrength()), Creature(), CardModel{});
        SetCurled(true);
    }

    void PounceMove(const CreatureList&) const {
        SetCurled(false);
        DamageCmd::Attack(PounceDamage()).FromMonster(*this).Execute(nullptr);
    }
};

}  // namespace sts2
