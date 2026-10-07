// Models/Cards/MadScience.cs
//
// Mad Science is configured by the TinkerTime event.  The two configuration
// values are instance fields in the original model; they live in CardState's
// four generic extra slots here so clones, snapshots and save/load keep them
// with the card itself.
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/CardModel.h"

namespace sts2 {

enum class MadScienceRider : int32_t {
    None = 0,
    Sapping = 1,
    Violence = 2,
    Choking = 3,
    Energized = 4,
    Wisdom = 5,
    Chaos = 6,
    Expertise = 7,
    Curious = 8,
    Improvement = 9,
};

struct MadScience : CardModel {
    SIM_MODEL(MadScience, CardModel)

    CardType TinkerTimeType() const { return static_cast<CardType>(extra(0)); }
    void SetTinkerTimeType(CardType type) const { extra(0) = static_cast<int32_t>(type); }
    MadScienceRider TinkerTimeRider() const { return static_cast<MadScienceRider>(extra(1)); }
    void SetTinkerTimeRider(MadScienceRider rider) const { extra(1) = static_cast<int32_t>(rider); }

    CardType Type() const { return TinkerTimeType(); }
    sts2::TargetType TargetType() const {
        return TinkerTimeType() == CardType::Attack ? sts2::TargetType::AnyEnemy : sts2::TargetType::Self;
    }
    bool GainsBlock() const { return TinkerTimeType() == CardType::Skill; }

    void OnPlay(PlayerChoiceContext& choiceContext, const CardPlay* cardPlay) const {
        Creature owner = Owner().Creature();
        Creature target = cardPlay ? cardPlay->Target : Creature{};
        switch (TinkerTimeType()) {
        case CardType::Attack:
            if (!target) throw GameError("MadScience cardPlay.Target is null");
            DamageCmd::Attack(DynamicVars().Damage().BaseValue())
                .WithHitCount(TinkerTimeRider() == MadScienceRider::Violence
                                  ? DynamicVars()["ViolenceHits"].IntValue() : 1)
                .FromCard(*this, cardPlay)
                .Targeting(target)
                .Execute(choiceContext);
            break;
        case CardType::Skill:
            CreatureCmd::GainBlock(owner, DynamicVars().Block(), cardPlay);
            break;
        case CardType::Power:
            break;
        default:
            throw GameError("MadScience has an invalid TinkerTime card type");
        }

        switch (TinkerTimeRider()) {
        case MadScienceRider::None:
        case MadScienceRider::Violence:
            return;
        case MadScienceRider::Sapping:
            if (!target) throw GameError("MadScience sapping rider requires an enemy target");
            PowerCmd::Apply(PowerId::WEAK_POWER, choiceContext, target,
                            DynamicVars()["SappingWeak"].BaseValue(), owner, *this);
            PowerCmd::Apply(PowerId::VULNERABLE_POWER, choiceContext, target,
                            DynamicVars()["SappingVulnerable"].BaseValue(), owner, *this);
            return;
        case MadScienceRider::Choking:
            if (!target) throw GameError("MadScience choking rider requires an enemy target");
            PowerCmd::Apply(PowerId::STRANGLE_POWER, choiceContext, target,
                            DynamicVars()["ChokingDamage"].BaseValue(), owner, *this);
            return;
        case MadScienceRider::Energized:
            PlayerCmd::GainEnergy(DynamicVars()["EnergizedEnergy"].IntValue(), Owner());
            return;
        case MadScienceRider::Wisdom:
            CardPileCmd::Draw(choiceContext, DynamicVars()["WisdomCards"].IntValue(), Owner());
            return;
        case MadScienceRider::Chaos: {
            CardList cards = CardFactory::GetForCombat(Owner(), sts2::data(Owner().Character()).card_pool, 1,
                                                       Owner().RunState().Rng().CombatCardGeneration(),
                                                       CardType::None, true);
            if (!cards.empty()) {
                cards.front().SetToFreeThisTurn();
                CardPileCmd::AddGeneratedCardToCombat(cards.front(), PileType::Hand, Owner());
            }
            return;
        }
        case MadScienceRider::Expertise:
            PowerCmd::Apply(PowerId::STRENGTH_POWER, choiceContext, owner,
                            DynamicVars()["ExpertiseStrength"].BaseValue(), owner, *this);
            PowerCmd::Apply(PowerId::DEXTERITY_POWER, choiceContext, owner,
                            DynamicVars()["ExpertiseDexterity"].BaseValue(), owner, *this);
            return;
        case MadScienceRider::Curious:
            PowerCmd::Apply(PowerId::CURIOUS_POWER, choiceContext, owner,
                            DynamicVars()["CuriousReduction"].BaseValue(), owner, *this);
            return;
        case MadScienceRider::Improvement:
            PowerCmd::Apply(PowerId::IMPROVEMENT_POWER, choiceContext, owner, Dec(1), owner, *this);
            return;
        }
        throw GameError("MadScience has an invalid TinkerTime rider");
    }

};

}  // namespace sts2
