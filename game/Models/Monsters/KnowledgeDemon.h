// Models/Monsters/KnowledgeDemon.cs
#pragma once

#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"

namespace sts2 {
struct KnowledgeDemon : MonsterModel {
    SIM_MODEL(KnowledgeDemon, MonsterModel)
    SIM_NOT_PORTED(GenerateAnimator);

    int32_t CurseOfKnowledgeCounter() const { return extra(0); }
    bool IsBurnt() const { return extra(1) != 0; }
    int32_t SlapDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 18, 17); }
    int32_t PonderDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 13, 11); }
    int32_t KnowledgeOverwhelmingDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 9, 8); }
    int32_t PonderStrength() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 3, 2); }
    void BeforeRemovedFromRoom() const {} // Music parameter only.

    void CurseOfKnowledgeMove(const CreatureList& targets) const {
        int32_t index = CurseOfKnowledgeCounter();
        if (index >= 3) throw GameError("There are no valid Curse of Knowledge sets at this index");
        for (sts2::Creature target : targets) {
            if (target.IsDead()) continue;
            CardId second = index == 0 ? CardId::MIND_ROT : index == 1 ? CardId::SLOTH : CardId::WASTE_AWAY;
            CardModel disintegration = CombatState().CreateCard(CardId::DISINTEGRATION, target.Player());
            disintegration.DynamicVars()["DisintegrationPower"].SetBaseValue(Dec(6 + index));
            CardModel alternative = CombatState().CreateCard(second, target.Player());
            PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
            CardModel chosen = CardSelectCmd::FromChooseACardScreen(ctx, CardList{disintegration, alternative}, target.Player());
            if (!chosen) continue;
            PowerId power = chosen.CardId_() == CardId::DISINTEGRATION ? PowerId::DISINTEGRATION_POWER :
                chosen.CardId_() == CardId::MIND_ROT ? PowerId::MIND_ROT_POWER :
                chosen.CardId_() == CardId::SLOTH ? PowerId::SLOTH_POWER : PowerId::WASTE_AWAY_POWER;
            std::string_view varKey = power == PowerId::DISINTEGRATION_POWER ? "DisintegrationPower" :
                power == PowerId::MIND_ROT_POWER ? "MindRotPower" :
                power == PowerId::SLOTH_POWER ? "SlothPower" : "WasteAwayPower";
            PowerCmd::Apply(power, ctx, target, chosen.DynamicVars()[varKey].BaseValue(), target, chosen);
        }
        ++extra(0);
    }
    void SlapMove(const CreatureList&) const { DamageCmd::Attack(SlapDamage()).FromMonster(*this).Execute(nullptr); }
    void KnowledgeOverwhelmingMove(const CreatureList&) const {
        extra(1) = 1;
        DamageCmd::Attack(KnowledgeOverwhelmingDamage()).WithHitCount(3).FromMonster(*this).OnlyPlayAnimOnce().Execute(nullptr);
    }
    void PonderMove(const CreatureList&) const {
        extra(1) = 0;
        DamageCmd::Attack(PonderDamage()).FromMonster(*this).Execute(nullptr);
        CreatureCmd::Heal(Creature(), Dec(30 * static_cast<int32_t>(CombatState().Players().size())));
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(PonderStrength()), Creature(), CardModel{});
    }
    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        auto curse = b.Move("CURSE_OF_KNOWLEDGE_MOVE", SIM_MOVE(KnowledgeDemon, CurseOfKnowledgeMove), DebuffIntent());
        auto slap = b.Move("SLAP_MOVE", SIM_MOVE(KnowledgeDemon, SlapMove), SingleAttackIntent(SlapDamage()));
        auto overwhelming = b.Move("KNOWLEDGE_OVERWHELMING_MOVE", SIM_MOVE(KnowledgeDemon, KnowledgeOverwhelmingMove), MultiAttackIntent(KnowledgeOverwhelmingDamage(), 3));
        auto ponder = b.Move("PONDER_MOVE", SIM_MOVE(KnowledgeDemon, PonderMove), SingleAttackIntent(PonderDamage()), HealIntent(), BuffIntent());
        auto branch = b.ConditionalBranch("CurseOfKnowledgeBranch");
        curse.SetFollowUpState(slap); slap.SetFollowUpState(overwhelming); overwhelming.SetFollowUpState(ponder); ponder.SetFollowUpState(branch);
        branch.AddState(curse, SIM_COND(KnowledgeDemon, self.CurseOfKnowledgeCounter() < 3));
        branch.AddState(slap, SIM_COND(KnowledgeDemon, self.CurseOfKnowledgeCounter() >= 3));
        b.Add(branch); b.Add(curse); b.Add(slap); b.Add(ponder); b.Add(overwhelming);
        return b.Build(curse);
    }
};
}  // namespace sts2
