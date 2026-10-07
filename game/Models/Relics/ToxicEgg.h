// Models/Relics/ToxicEgg.cs (IsAllowed only: the relic grab bag asks every relic)
#pragma once

#include "game/Models/RelicModel.h"
#include "game/Models/Relics/EggHelper.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct ToxicEgg : RelicModel {
    SIM_MODEL(ToxicEgg, RelicModel)
    bool TryModifyCardRewardOptionsLate(Player player, CardList& cards, const CardCreationOptionsState& options) const {
        if (player != Owner() || has_flag(options.flags, CardCreationFlags::NoHookUpgrades)) return false;
        egg_helper::UpgradeValidCards(cards, CardType::Skill, *this);
        return true;
    }
    void ModifyMerchantCardCreationResults(Player player, CardList& cards) const {
        if (player == Owner()) egg_helper::UpgradeValidCards(cards, CardType::Skill, *this);
    }
    bool TryModifyCardBeingAddedToDeck(CardModel card, CardModel& newCard) const {
        return egg_helper::TryModifyAddedCard(card, CardType::Skill, *this, newCard);
    }

    bool IsAllowed(RunState runState) const {
        return IsBeforeAct3TreasureChest(runState);
    }
};

}  // namespace sts2
