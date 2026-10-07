// Commands/PotionCmd.cs
#include "game/Commands/PotionCmd.h"

#include "engine/sim/alloc.h"
#include "game/Hooks/Hook.h"

namespace sts2::PotionCmd {

Player::PotionProcureResult TryToProcure(PotionModel potion, Player player, int32_t slotIndex) {
    if (!Hook::ShouldProcurePotion(player.RunState(), player.Creature().CombatState(), potion, player)) {
        return {.success = false, .failureReason = PotionProcureFailureReason::NotAllowed};
    }
    Player::PotionProcureResult result = player.AddPotionInternal(potion, slotIndex);
    if (result.success) {
        // PotionChoices (run history), audio, UI and SaveManager are not modelled.
        Hook::AfterPotionProcured(player.RunState(), player.Creature().CombatState(), potion);
    }
    return result;
}

Player::PotionProcureResult TryToProcure(PotionId potion, Player player, int32_t slotIndex) {
    return TryToProcure(PotionModel(&player.sim(), alloc_potion(*player.sim().g, potion, 0)), player, slotIndex);
}

void Discard(PotionModel potion) {
    Player player = potion.Owner();
    CombatState combatState = player.Creature().CombatState();
    player.DiscardPotionInternal(potion);
    potion.st().removed = true;
    Hook::AfterPotionDiscarded(player.RunState(), combatState, potion);
}

}  // namespace sts2::PotionCmd
