// Entities/Players/Player.cs (single player: index 1)
#include "game/Entities/Players/Player.h"
#include "core/csharp_int.h"

#include "game/All.h"
#include "game/Commands/CreatureCmd.h"

namespace sts2 {

sts2::Creature Player::Creature() const { return sts2::Creature(sim_, st().creature); }
sts2::RunState Player::RunState() const { return sts2::RunState(sim_); }

sts2::PlayerCombatState Player::PlayerCombatState() const {
    return sim_->g->player_combat.exists ? sts2::PlayerCombatState(sim_, index_) : sts2::PlayerCombatState{};
}

StaticVec<RelicModel, kMaxRelics> Player::Relics() const {
    StaticVec<RelicModel, kMaxRelics> out;
    for (RelicRef r : st().relics) out.push_back(RelicModel(sim_, r));
    return out;
}

StaticVec<PotionModel, kMaxPotionSlots> Player::PotionSlots() const {
    StaticVec<PotionModel, kMaxPotionSlots> out;
    for (int i = 0; i < st().max_potion_slots; ++i) out.push_back(PotionModel(sim_, st().potions[i]));
    return out;
}

StaticVec<PotionModel, kMaxPotionSlots> Player::Potions() const {
    StaticVec<PotionModel, kMaxPotionSlots> out;
    for (PotionModel p : PotionSlots()) {
        if (p) out.push_back(p);
    }
    return out;
}

void Player::SetGold(int32_t gold) { st().gold = gold; }

bool Player::HasOpenPotionSlots() const {
    for (PotionModel p : PotionSlots()) {
        if (!p) return true;
    }
    return false;
}

bool Player::CanUseOrRemovePotions() const { return st().can_use_or_remove_potions; }

CardPile Player::Deck() const { return CardPile(sim_, index_, PileType::Deck); }

void Player::AddRelicInternal(RelicModel relic, int32_t index, bool silent) {
    relic.st().owner = index_;
    if (index == -1) {
        st().relics.push_back(relic.Index());
    } else {
        st().relics.insert(static_cast<size_t>(index), relic.Index());
    }
    // RelicObtained: the subscribers with an effect are the pending card
    // rewards (CardReward.OnRelicObtained); the others are UI.
    if (!silent) {
        RewardsSetState& set = sim_->g->run.rewards;
        for (RewardRef r : set.rewards) {
            Reward reward(sim_, r);
            if (reward.Type() == RewardType::Card && reward.st().relic_obtained_subscribed) reward.OnRelicObtained(relic);
        }
    }
}

Player::PotionProcureResult Player::AddPotionInternal(PotionModel potion, int32_t slotIndex, bool silent) {
    PotionProcureResult result;
    if (slotIndex < 0) {
        slotIndex = -1;  // _potionSlots.IndexOf(null)
        for (int i = 0; i < st().max_potion_slots; ++i) {
            if (st().potions[i] == 0) {
                slotIndex = i;
                break;
            }
        }
    }
    if (slotIndex >= 0) {
        if (st().potions[slotIndex] != 0) {
            result.success = false;  // AddPotionFailed is UI
            result.failureReason = PotionProcureFailureReason::TooFull;
            return result;
        }
        potion.st().owner = index_;
        st().potions[slotIndex] = potion.Index();
        result.success = true;  // PotionProcured is UI
    } else {
        result.success = false;
        result.failureReason = PotionProcureFailureReason::TooFull;
    }
    return result;
}

void Player::RemoveRelicInternal(RelicModel relic, bool silent) {
    if (!st().relics.contains(relic.Index())) throw GameError("Player does not have the relic");
    st().relics.remove(relic.Index());
    relic.st().removed = true;  // RelicModel.RemoveInternal: HasBeenRemovedFromState = true
}

void Player::MeltRelicInternal(RelicModel relic) {
    if (!relic.IsWax()) throw GameError("relic is not wax");
    if (relic.IsMelted()) throw GameError("relic is already melted");
    if (!st().relics.contains(relic.Index())) throw GameError("Player does not have the relic");
    relic.st().is_melted = true;
    relic.st().status = RelicStatus::Disabled;
}

RelicModel Player::GetRelic(RelicId id) const {
    for (RelicModel r : Relics()) {
        if (r.RelicId_() == id) return r;
    }
    return {};
}

int32_t Player::GetPotionSlotIndex(PotionModel potion) const {
    for (int i = 0; i < st().max_potion_slots; ++i) {
        if (st().potions[i] == potion.Index()) return i;
    }
    return -1;
}

PotionModel Player::GetPotionAtSlotIndex(int32_t index) const {
    if (index < 0 || index >= st().max_potion_slots) throw GameError("not a valid potion slot index");
    return PotionModel(sim_, st().potions[index]);
}

void Player::AddToMaxPotionCount(int32_t increase) {
    SetMaxPotionCountInternal(int_add(st().max_potion_slots, increase));
}

void Player::SubtractFromMaxPotionCount(int32_t decrease) {
    SetMaxPotionCountInternal(int_sub(st().max_potion_slots, decrease));
}

void Player::SetMaxPotionCountInternal(int32_t newMax) {
    if (newMax > st().max_potion_slots) {
        st().potions.resize(static_cast<size_t>(newMax));
        st().max_potion_slots = newMax;
        return;
    }
    if (newMax >= st().max_potion_slots) return;
    for (int num = st().max_potion_slots - 1; num >= newMax; --num) {
        // List's negative index throws, including an unchecked count overflow.
        if (num < 0) throw GameError("potion slot count produced an invalid List index");
        if (st().potions[num] != 0) {
            int num2 = -1;  // _potionSlots.IndexOf(null)
            for (int i = 0; i < st().max_potion_slots; ++i) {
                if (st().potions[i] == 0) {
                    num2 = i;
                    break;
                }
            }
            if (num2 < newMax) {
                if (num2 < 0) throw GameError("potion slot relocation produced an invalid List index");
                st().potions[num2] = st().potions[num];
            } else {
                DiscardPotionInternal(PotionModel(sim_, st().potions[num]));
            }
        }
        st().potions.pop_back();  // RemoveAt(num)
        st().max_potion_slots = num;
    }
}

void Player::DiscardPotionInternal(PotionModel potion, bool silent) { RemoveUsedPotionInternal(potion); }

void Player::RemoveUsedPotionInternal(PotionModel potion) {
    int32_t num = GetPotionSlotIndex(potion);
    if (num < 0) throw GameError("Tried to remove a potion the player does not have");
    st().potions[num] = 0;
}

// ResetCombatState: PlayerCombatState = new PlayerCombatState(this)
void Player::ResetCombatState() {
    sim_->g->orbs.clear();
    PlayerCombatData& pcs = sim_->g->player_combat;
    pcs = PlayerCombatData{};
    pcs.exists = true;
    pcs.orb_capacity = st().base_orb_slots;  // OrbQueue.AddCapacity(BaseOrbSlotCount)
}

void Player::PopulateCombatState(Rng& rng, sts2::CombatState state) {
    for (CardModel item : Deck().Cards()) {
        CardModel cardModel = state.CloneCard(item);
        cardModel.st().deck_version = item.Index();
        PlayerCombatState().DrawPile().AddInternal(cardModel);
    }
    PlayerCombatState().DrawPile().RandomizeOrderInternal(*this, rng, state);
}

void Player::ReviveBeforeCombatEnd() {
    if (Creature().IsDead()) CreatureCmd::Heal(Creature(), Dec(1));
}

void Player::AfterCombatEnd() {
    Creature().RemoveAllPowersInternalExcept();
    if (PlayerCombatState()) PlayerCombatState().AfterCombatEnd();
    Creature().LoseBlockInternal(Dec(Creature().Block()));
}

void Player::OnSideSwitch() {}

}  // namespace sts2
