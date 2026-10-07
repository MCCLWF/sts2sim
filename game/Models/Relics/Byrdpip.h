// Models/Relics/Byrdpip.cs. Skin selection only affects presentation.
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Models/MonsterModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct ByrdpipRelic : RelicModel {
    SIM_MODEL(ByrdpipRelic, RelicModel)
    bool AddsPet() const { return true; }
    bool HasUponPickupEffect() const { return true; }
    bool SpawnsPets() const { return true; }
    void SummonPet() const {
        CombatState combat = Owner().Creature().CombatState();
        MonsterModel monster = MonsterModel::ToMutable(sim_, MonsterId::BYRDPIP);
        Creature pet = combat.CreateCreature(monster, CombatSide::Player, -1);
        PlayerCmd::AddPet(pet, Owner());
    }
    void AfterObtained() const {
        CardList eggs;
        for (CardModel card : Owner().Deck().Cards())
            if (card.CardId_() == CardId::BYRDONIS_EGG) eggs.push_back(card);
        if (CombatManager(sim_).IsInProgress())
            for (CardModel card : Owner().PlayerCombatState().AllCards())
                if (card.CardId_() == CardId::BYRDONIS_EGG) eggs.push_back(card);
        for (CardModel egg : eggs) {
            CardModel replacement = egg.IsInCombat()
                ? egg.CombatState().CreateCard(CardId::BYRD_SWOOP, Owner())
                : Owner().RunState().CreateCard(CardId::BYRD_SWOOP, Owner());
            CardCmd::Transform(egg, replacement);
        }
        if (CombatManager(sim_).IsInProgress()) SummonPet();
    }
    void BeforeCombatStart() const { SummonPet(); }
};

}  // namespace sts2
