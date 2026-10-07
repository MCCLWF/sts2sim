// Models/Powers/PersonalHivePower.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct PersonalHivePower : PowerModel {
    SIM_MODEL(PersonalHivePower, PowerModel)

    void AfterDamageReceived(PlayerChoiceContext&, Creature target, const DamageResult&, ValueProp props, Creature dealer, CardModel) const {
        if (target != Owner() || !dealer || !IsPoweredAttack(props)) return;
        Player player = dealer.IsMonster() && dealer.Monster().MonsterId_() == MonsterId::OSTY
                             ? dealer.PetOwner()
                             : dealer.Player();
        for (int i = 0; i < Amount(); ++i) {
            CardModel card = CombatState().CreateCard(CardId::DAZED, player);
            // The victim's debuff creates this status, not the damage dealer.
            // Source creator=null matters to RocketPunch and generation hooks.
            CardPileCmd::AddGeneratedCardToCombat(card, PileType::Draw, Player{}, CardPilePosition::Random);
        }
    }
};

}  // namespace sts2
