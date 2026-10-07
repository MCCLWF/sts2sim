// Models/Relics/FrozenEgg.cs (IsAllowed only: the relic grab bag asks every relic)
#pragma once

#include "game/Models/RelicModel.h"
#include "game/Models/Relics/EggHelper.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct FrozenEgg : RelicModel {
    SIM_MODEL(FrozenEgg, RelicModel)
    bool TryModifyCardRewardOptionsLate(Player player, CardList& cards, const CardCreationOptionsState& options) const {
        if (player != Owner() || has_flag(options.flags, CardCreationFlags::NoHookUpgrades)) return false;
        egg_helper::UpgradeValidCards(cards, CardType::Power, *this);
        return true;
    }
    void ModifyMerchantCardCreationResults(Player player, CardList& cards) const {
        if (player == Owner()) egg_helper::UpgradeValidCards(cards, CardType::Power, *this);
    }
    bool TryModifyCardBeingAddedToDeck(CardModel card, CardModel& newCard) const {
        return egg_helper::TryModifyAddedCard(card, CardType::Power, *this, newCard);
    }

    bool IsAllowed(RunState runState) const {
        return IsBeforeAct3TreasureChest(runState);
    }
};

}  // namespace sts2
