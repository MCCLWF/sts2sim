// Commands/Builders/AttackCommand.cs
#include "game/Commands/Builders/AttackCommand.h"

#include "game/Combat/CombatManager.h"
#include "game/Hooks/Hook.h"

namespace sts2 {

CreatureList AttackCommand::GetPossibleTargets() const {
    if (IsSingleTargeted()) return CreatureList{singleTarget_};
    if (IsMultiTargeted()) {
        if (sourceType_ == SourceType::Monster) return combatState_.PlayerCreatures();
        if (!attacker_) throw GameError("We require an attacker to be able to grab its opponents");
        return combatState_.GetOpponentsOf(attacker_);
    }
    throw GameError("No targets set, a Targeting method must be called before Execute");
}

AttackCommand& AttackCommand::FromCard(CardModel card, const sts2::CardPlay* cardPlay) {
    if (attacker_) throw GameError("Attacker has already been set.");
    if (modelSource_) throw GameError("ModelSource has already been set.");
    if (cardPlay != nullptr && cardPlay_ != nullptr) throw GameError("CardPlay has already been set.");
    attacker_ = card.Owner().Creature();
    modelSource_ = card;
    cardPlay_ = cardPlay;
    sourceType_ = SourceType::Card;
    return *this;
}

AttackCommand& AttackCommand::FromOsty(Creature osty, CardModel card, const sts2::CardPlay* cardPlay) {
    if (!osty.IsMonster() || osty.Monster().MonsterId_() != MonsterId::OSTY) throw GameError("Creature is not Osty");
    attacker_ = osty;
    modelSource_ = card;
    cardPlay_ = cardPlay;
    sourceType_ = SourceType::Card;
    return *this;
}

AttackCommand& AttackCommand::FromMonster(MonsterModel monster) {
    if (attacker_) throw GameError("Attacker has already been set.");
    attacker_ = monster.Creature();
    sourceType_ = SourceType::Monster;
    return TargetingAllOpponents(monster.Creature().CombatState());
}

AttackCommand& AttackCommand::Targeting(Creature target) {
    if (singleTarget_) throw GameError("Targets already set.");
    if (combatState_) throw GameError("Already set to target opponents of attacker");
    singleTarget_ = target;
    targetSide_ = target.Side();
    return *this;
}

AttackCommand& AttackCommand::TargetingAllOpponents(CombatState combatState) {
    if (singleTarget_) throw GameError("Targets already set.");
    if (combatState_) throw GameError("Already set to target opponents of attacker");
    if (!attacker_) throw GameError("We require an attacker to be able to grab its opponents");
    combatState_ = combatState;
    targetSide_ = attacker_.Side() == CombatSide::Enemy ? CombatSide::Player : CombatSide::Enemy;
    return *this;
}

AttackCommand& AttackCommand::TargetingRandomOpponents(CombatState combatState, bool allowDuplicates) {
    if (singleTarget_) throw GameError("Targets already set.");
    if (combatState_) throw GameError("Already set to target opponents of attacker");
    if (!attacker_) throw GameError("We require an attacker to be able to grab its opponents");
    combatState_ = combatState;
    isRandomlyTargeted_ = true;
    doesRandomTargetingAllowDuplicates_ = allowDuplicates;
    return *this;
}

AttackCommand& AttackCommand::Unpowered() {
    damageProps_ = damageProps_ | ValueProp::Unpowered;
    return *this;
}

AttackCommand& AttackCommand::WithHitCount(int32_t hitCount) {
    hitCount_ = hitCount;
    return *this;
}

AttackCommand& AttackCommand::BeforeDamage(std::function<void()> beforeDamage) {
    beforeDamage_ = std::move(beforeDamage);
    return *this;
}

AttackCommand& AttackCommand::AfterAttackerAnim(std::function<void()> afterAttackerAnim) {
    afterAttackerAnim_ = std::move(afterAttackerAnim);
    return *this;
}

void AttackCommand::AddResultsInternal(const DamageResults& results) {
    results_.emplace_back(results.begin(), results.end());
}

AttackCommand& AttackCommand::Execute(PlayerChoiceContext* choiceContext) {
    if (!attacker_) throw GameError("No attacker set.");
    CombatState combatState = attacker_.CombatState();
    CombatManager cm(&attacker_.sim());
    if (cm.IsOverOrEnding()) return *this;  // IsLiveCombat() is always true
    if (!combatState) throw GameError("No combat state even though combat is not over.");
    if (attacker_.IsDead()) return *this;
    if (!IsSingleTargeted() && !IsMultiTargeted()) throw GameError("No targets set.");
    ExecutionSourceScope damageSource(attacker_.sim(),
        sourceType_ == SourceType::Monster ? ExecutionDamageSourceKind::MonsterMove
            : ExecutionDamageSourceKind::Card,
        sourceType_ == SourceType::Monster ? attacker_.Monster().data().entry
            : CardModel(&attacker_.sim(), modelSource_.Index()).data().entry);
    PlayerChoiceContext blocking;  // choiceContext ?? new BlockingPlayerChoiceContext()
    PlayerChoiceContext& ctx = choiceContext != nullptr ? *choiceContext : blocking;
    Hook::BeforeAttack(combatState, *this);
    Dec attackCount(Hook::ModifyAttackHitCount(combatState, *this, hitCount_));
    for (int32_t i = 0; Dec(i) < attackCount; ++i) {
        if (attacker_.IsDead()) break;
        CreatureList validTargets;
        for (Creature c : GetPossibleTargets()) {
            if (c.IsAlive()) validTargets.push_back(c);
        }
        if (validTargets.empty()) break;
        if ((playOnEveryHit_ || i == 0) && afterAttackerAnim_) afterAttackerAnim_();
        Creature singleTarget;
        if (!isRandomlyTargeted_) {
            singleTarget = validTargets.size() != 1 ? Creature{} : validTargets[0];
        } else {
            if (!doesRandomTargetingAllowDuplicates_) {
                CreatureList filtered;
                for (Creature c : validTargets) {
                    bool hit = false;
                    for (const auto& r : results_) {
                        for (const DamageResult& d : r) hit |= d.Receiver == c;
                    }
                    if (!hit) filtered.push_back(c);
                }
                validTargets = filtered;
                if (validTargets.empty()) throw GameError("No valid targets for attack with duplicates disallowed.");
            }
            Player owner = attacker_.Player() ? attacker_.Player() : attacker_.PetOwner();
            singleTarget = owner.RunState().Rng().CombatTargets().NextItem(validTargets);
        }
        if (beforeDamage_) beforeDamage_();
        Dec amount = damagePerHit_;
        // Resolve at every hit, after BeforeDamage, with the actual selected
        // single target (null for a multi-target attack), as in the game.
        if (calculatedDamageVar_) amount = calculatedDamageVar_.Calculate(singleTarget);
        CreatureList targets = singleTarget ? CreatureList{singleTarget} : validTargets;
        AddResultsInternal(CreatureCmd::Damage(ctx, targets, amount, damageProps_, attacker_,
                                               CardModel(&attacker_.sim(), modelSource_.Kind() == ModelKind::Card ? modelSource_.Index() : 0),
                                               cardPlay_));
    }
    auto& attack_count = attacker_.sim().g->creatures[attacker_.Index()].attacks_this_turn;
    attack_count = list_count_add(attack_count);
    if (auto* boundary = attacker_.sim().execution_trace) {
        ExecutionEvent event;
        event.kind = ExecutionEventKind::CreatureAttacked;
        event.creature = attacker_.Index();
        for (const auto& hit : results_) {
            for (const DamageResult& result : hit) {
                event.hit_results.push_back(ExecutionHitResult{
                    result.Receiver.Index(), static_cast<int32_t>(result.Props),
                    result.BlockedDamage, result.UnblockedDamage, result.OverkillDamage,
                    result.WasBlockBroken, result.WasFullyBlocked, result.WasTargetKilled});
            }
        }
        event.hit_count = static_cast<int32_t>(event.hit_results.size());
        boundary->Record(event);
    }
    Hook::AfterAttack(combatState, ctx, *this);
    return *this;
}

}  // namespace sts2
