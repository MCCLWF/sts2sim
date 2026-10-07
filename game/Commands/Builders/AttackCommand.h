// Commands/Builders/AttackCommand.cs. A transient builder: it lives on the
// stack of the effect that attacks and is never stored in the state.
// Presentation settings (animations, VFX, SFX, waits) are accepted and ignored,
// so effects can be ported call for call.
#pragma once

#include <functional>
#include <string_view>
#include <vector>

#include "game/All.h"
#include "game/Commands/CreatureCmd.h"

namespace sts2 {

class AttackCommand {
public:
    explicit AttackCommand(Dec damagePerHit) : damagePerHit_(damagePerHit) {}
    explicit AttackCommand(DynamicVar calculatedDamageVar) : damagePerHit_(Dec(-1)), calculatedDamageVar_(calculatedDamageVar) {}

    Creature Attacker() const { return attacker_; }
    AbstractModel ModelSource() const { return modelSource_; }
    const sts2::CardPlay* CardPlay() const { return cardPlay_; }
    CombatSide TargetSide() const { return targetSide_; }
    ValueProp DamageProps() const { return damageProps_; }
    bool IsSingleTargeted() const { return static_cast<bool>(singleTarget_); }
    bool IsMultiTargeted() const { return static_cast<bool>(combatState_); }
    bool IsRandomlyTargeted() const { return isRandomlyTargeted_; }
    const std::vector<std::vector<DamageResult>>& Results() const { return results_; }
    // A power's source commandToModify uses object identity. Keep that claim
    // on this transient command so a nested attack cannot consume its parent.
    PowerRef GigantificationCapture() const { return gigantificationCapture_; }
    void SetGigantificationCapture(PowerRef value) { gigantificationCapture_ = value; }

    AttackCommand& FromCard(CardModel card, const sts2::CardPlay* cardPlay);
    AttackCommand& FromOsty(Creature osty, CardModel card, const sts2::CardPlay* cardPlay);
    AttackCommand& FromMonster(MonsterModel monster);
    AttackCommand& Targeting(Creature target);
    AttackCommand& TargetingAllOpponents(CombatState combatState);
    AttackCommand& TargetingRandomOpponents(CombatState combatState, bool allowDuplicates = true);
    AttackCommand& Unpowered();
    AttackCommand& WithHitCount(int32_t hitCount);
    AttackCommand& BeforeDamage(std::function<void()> beforeDamage);
    AttackCommand& AfterAttackerAnim(std::function<void()> afterAttackerAnim);

    // Presentation only.
    AttackCommand& WithAttackerAnim(std::string_view = {}, float = 0, Creature = {}) { return *this; }
    AttackCommand& WithNoAttackerAnim() { return *this; }
    AttackCommand& WithAttackerFx(std::string_view = {}, std::string_view = {}, std::string_view = {}) { return *this; }
    AttackCommand& WithWaitBeforeHit(float, float) { return *this; }
    AttackCommand& WithHitFx(std::string_view = {}, std::string_view = {}, std::string_view = {}) { return *this; }
    AttackCommand& SpawningHitVfxOnEachCreature() { return *this; }
    AttackCommand& WithHitVfxSpawnedAtBase() { return *this; }
    AttackCommand& OnlyPlayAnimOnce() {
        playOnEveryHit_ = false;
        return *this;
    }

    AttackCommand& Execute(PlayerChoiceContext* choiceContext);
    AttackCommand& Execute(PlayerChoiceContext& choiceContext) { return Execute(&choiceContext); }

    // System code only (C#: "NEVER CALL THIS FROM MODELS").
    void IncrementHitsInternal() { ++hitCount_; }
    void AddResultsInternal(const DamageResults& results);

private:
    friend ExecutionValue FreezeExecutionValue(const AttackCommand&);
    enum class SourceType { None, Card, Monster };
    CreatureList GetPossibleTargets() const;

    Dec damagePerHit_;
    DynamicVar calculatedDamageVar_;
    int32_t hitCount_ = 1;
    SourceType sourceType_ = SourceType::None;
    CombatState combatState_;
    Creature singleTarget_;
    bool doesRandomTargetingAllowDuplicates_ = true;
    std::vector<std::vector<DamageResult>> results_;
    bool playOnEveryHit_ = true;
    std::function<void()> beforeDamage_;
    std::function<void()> afterAttackerAnim_;
    Creature attacker_;
    AbstractModel modelSource_;
    const sts2::CardPlay* cardPlay_ = nullptr;
    CombatSide targetSide_ = CombatSide::Player;
    ValueProp damageProps_ = ValueProp::Move;
    bool isRandomlyTargeted_ = false;
    PowerRef gigantificationCapture_ = 0;
};

}  // namespace sts2
