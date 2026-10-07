#include <stdexcept>
#include <iostream>

#include "engine/run_setup.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Commands/PotionCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Entities/Merchant/MerchantInventory.h"
#include "game/Models/EventModel.h"
#include "game/Rooms/AbstractRoom.h"
#include "game/Runs/RunFlow.h"
#include "game/Runs/RunManager.h"

using namespace sts2;

static void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

int main(int argc, char** argv) {
    if (argc == 3 && std::string_view(argv[1]) == "--dump") {
        GameState dump_state;
        Sim dump_sim(dump_state);
        new_run(dump_state, NewRunOptions{.seed = argv[2], .character = CharacterId::IRONCLAD});
        Player dump_player(&dump_sim, 1);
        dump_state.run.current_act_index = 1;
        PlayerCmd::SetGold(300, dump_player);
        EventModel dump_event = EventModel::ToMutable(&dump_sim, EventId::FAKE_MERCHANT);
        dump_event.BeginEvent(dump_player, false);
        std::cout << "{\"seed\":\"" << argv[2] << "\",\"event_rng_counter\":"
                  << dump_event.Rng().counter << ",\"shops_rng_counter\":"
                  << dump_player.PlayerRng().Shops().counter << ",\"entries\":[";
        bool first_entry = true;
        for (const MerchantRelicEntryState& entry : dump_state.run.merchant.relics) {
            if (!first_entry) std::cout << ',';
            first_entry = false;
            std::cout << "{\"id\":\"" << RelicModel(&dump_sim, entry.relic).data().entry
                      << "\",\"cost\":" << entry.cost << '}';
        }
        std::cout << "]}\n";
        return 0;
    }
    GameState state;
    Sim sim(state);
    new_run(state, NewRunOptions{.seed = "FAKEMERCHANT", .character = CharacterId::IRONCLAD});
    Player player(&sim, 1);
    state.run.current_act_index = 1;
    state.run.rooms.push_back(RoomType::Event);
    state.run.room_event = EventId::FAKE_MERCHANT;
    EventModel event = EventModel::ToMutable(&sim, EventId::FAKE_MERCHANT);
    event.BeginEvent(player, false);
    RunFlow::AfterRoomEntered(&sim);
    require(state.run.phase == RunPhase::Shop && event.IsFinished() &&
                state.run.merchant.exists && state.run.merchant.relics.size() == 6 &&
                event.Rng().counter == 8 && RunFlow::ShopOptions(&sim).size() == 6,
            "Fake Merchant must expose six shuffled relics through its custom shop screen");

    for (const MerchantRelicEntryState& entry : state.run.merchant.relics) {
        require(entry.relic && entry.cost > 0 &&
                    RelicModel(&sim, entry.relic).MerchantCost() == 50,
                "Fake Merchant relic entry did not use its source merchant price");
    }

    PlayerCmd::SetGold(1000, player);
    RelicRef first = state.run.merchant.relics[0].relic;
    RelicId bought = RelicModel(&sim, first).RelicId_();
    int price = Merchant::Cost(&sim, Merchant::EntryRef{Merchant::EntryRef::Relic, 0});
    require(RunFlow::BuyShopOption(&sim, 0) && player.Gold() == 1000 - price &&
                player.GetRelic(bought) && RunFlow::ShopOptions(&sim).size() == 5,
            "Fake Merchant purchase did not grant the relic and remove it from stock");

    auto procured = PotionCmd::TryToProcure(PotionId::FOUL_POTION, player);
    require(procured.success, "Fake Merchant fixture could not obtain Foul Potion");
    PotionModel foul = player.Potions().back();
    require(foul.PassesCustomUsabilityCheck(), "Foul Potion must be usable in Fake Merchant's custom room");
    RunFlow::UsePotion(&sim, player.GetPotionSlotIndex(foul), -1);
    require(state.run.phase == RunPhase::Combat && state.combat.encounter ==
                EncounterId::FAKE_MERCHANT_EVENT_ENCOUNTER && state.run.rooms.size() == 2 &&
                state.run.combat_extra_rewards.size() == 6 &&
                RelicModel(&sim, state.rewards[state.run.combat_extra_rewards[0]].relic).RelicId_() ==
                    RelicId::FAKE_MERCHANTS_RUG,
            "Foul Potion did not start the Fake Merchant encounter with rug and five unsold relics");
    for (size_t i = 1; i < state.run.combat_extra_rewards.size(); ++i) {
        RelicId id = RelicModel(&sim, state.rewards[state.run.combat_extra_rewards[i]].relic).RelicId_();
        require(id != bought, "Purchased fake relic was incorrectly included in combat rewards");
    }

    {
        GameState relic_state;
        Sim relic_sim(relic_state);
        new_run(relic_state, NewRunOptions{.seed = "FAKERELICS", .character = CharacterId::IRONCLAD});
        Player owner(&relic_sim, 1);
        Creature creature = owner.Creature();
        creature.SetCurrentHpInternal(Dec(40));
        int max_hp = creature.MaxHp();
        RelicCmd::Obtain(RelicId::FAKE_MANGO, owner);
        require(creature.MaxHp() == max_hp + 3, "Fake Mango must grant only three max HP");
        int before_heal = creature.CurrentHp();
        RelicCmd::Obtain(RelicId::FAKE_LEES_WAFFLE, owner);
        require(creature.CurrentHp() == before_heal + 8,
                "Fake Lee's Waffle must heal ten percent of current maximum HP");

        RelicModel tea = RelicCmd::Obtain(RelicId::FAKE_VENERABLE_TEA_SET, owner);
        tea.AfterRoomEntered(AbstractRoom(&relic_sim, RoomType::RestSite));
        require(tea.Status() == RelicStatus::Active,
                "Fake Venerable Tea Set must arm after a rest site");
        RunManager::EnterMapCoordDebug(&relic_sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        require(owner.PlayerCombatState().Energy() == 4 && tea.Status() == RelicStatus::Normal,
                "Fake Venerable Tea Set must grant one energy in the next combat");
    }
}
