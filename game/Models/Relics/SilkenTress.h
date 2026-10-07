// Models/Relics/SilkenTress.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Models/Events/AllowedHelpers.h"
#include "game/Runs/RunState.h"
namespace sts2 {
struct SilkenTress : RelicModel {
    SIM_MODEL(SilkenTress, RelicModel)
    bool IsUsedUp() const { return extra(0) != 0; }
    void AfterObtained() const { PlayerCmd::LoseGold(Dec(Owner().Gold()), Owner()); }
    bool TryModifyCardRewardOptionsLate(Player player, CardList& cards, const CardCreationOptionsState& options) const {
        if (player != Owner() || IsUsedUp() || !has_flag(options.flags, CardCreationFlags::IsCardReward)) return false;
        for (CardModel& card : cards) {
            if (!event_allowed::BaseCanEnchant(card)) continue;
            CardModel clone = Owner().RunState().CloneCard(card);
            CardCmd::Enchant(EnchantmentId::GLAM, clone, Dec(1));
            card = clone;
        }
        return true;
    }
    void AfterModifyingCardRewardOptions() const {
        if (!IsUsedUp()) { extra(0) = 1; SetStatus(RelicStatus::Disabled); }
    }
};
}
