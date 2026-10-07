// Models/Monsters/HauntedShip.cs
#pragma once
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"
namespace sts2 {
struct HauntedShip : MonsterModel {
    SIM_MODEL(HauntedShip, MonsterModel)
    int32_t SwipeDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 14, 13); }
    int32_t StompDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 5, 4); }
    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState swipe = b.Move("SWIPE_MOVE", SIM_MOVE(HauntedShip, SwipeMove), SingleAttackIntent(SwipeDamage()));
        MoveState stomp = b.Move("STOMP_MOVE", SIM_MOVE(HauntedShip, StompMove), MultiAttackIntent(StompDamage(), 3));
        MoveState haunt = b.Move("HAUNT_MOVE", SIM_MOVE(HauntedShip, HauntMove), DebuffIntent(), StatusIntent(5));
        haunt.SetFollowUpState(swipe); swipe.SetFollowUpState(stomp); stomp.SetFollowUpState(swipe);
        b.Add(swipe); b.Add(stomp); b.Add(haunt); return b.Build(haunt);
    }
    void SwipeMove(const CreatureList&) const { DamageCmd::Attack(SwipeDamage()).FromMonster(*this).Execute(nullptr); }
    void StompMove(const CreatureList&) const { DamageCmd::Attack(StompDamage()).WithHitCount(3).FromMonster(*this).Execute(nullptr); }
    void HauntMove(const CreatureList& targets) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::WEAK_POWER, ctx, targets, Dec(3), Creature(), CardModel{});
        CardPileCmd::AddToCombatAndPreview(CardId::DAZED, targets, PileType::Discard, 5, Player{});
    }
};
}
