// Models/Events/BattlewornDummy.cs (v0.111.0).
#pragma once

#include <algorithm>

#include "engine/sim/alloc.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/RewardsCmd.h"
#include "game/Factories/PotionFactory.h"
#include "game/Factories/RelicFactory.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunManager.h"
#include "game/Runs/RunState.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {

struct BattlewornDummy : EventModel {
    SIM_MODEL(BattlewornDummy, EventModel)

    EventOptionList GenerateInitialOptions() const {
        // Creature.ScaleHpForMultiplayer leaves these values unchanged for
        // the single player supported by this Env.
        auto hp = [](MonsterId id) { return sts2::data(id).min_hp; };
        DynamicVars()["Setting1Hp"].SetBaseValue(Dec(hp(MonsterId::BATTLE_FRIEND_V1)));
        DynamicVars()["Setting2Hp"].SetBaseValue(Dec(hp(MonsterId::BATTLE_FRIEND_V2)));
        DynamicVars()["Setting3Hp"].SetBaseValue(Dec(hp(MonsterId::BATTLE_FRIEND_V3)));

        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(BattlewornDummy, Setting1), InitialOptionKey("SETTING_1")));
        list.push_back(EventOption(SIM_OPTION(BattlewornDummy, Setting2), InitialOptionKey("SETTING_2")));
        list.push_back(EventOption(SIM_OPTION(BattlewornDummy, Setting3), InitialOptionKey("SETTING_3")));
        return list;
    }

    void Setting1(const EventOptionState&) const {
        RunManager::EnterEventCombat(sim_, EncounterId::BATTLEWORN_DUMMY_EVENT_V1_ENCOUNTER, {}, true);
    }
    void Setting2(const EventOptionState&) const {
        RunManager::EnterEventCombat(sim_, EncounterId::BATTLEWORN_DUMMY_EVENT_V2_ENCOUNTER, {}, true);
    }
    void Setting3(const EventOptionState&) const {
        RunManager::EnterEventCombat(sim_, EncounterId::BATTLEWORN_DUMMY_EVENT_V3_ENCOUNTER, {}, true);
    }

    void Resume(AbstractRoom exitedRoom) const {
        if (!exitedRoom.IsCombatRoom()) throw GameError("BattlewornDummy.Resume requires a combat room");
        const EncounterId encounter = exitedRoom.ExitedEncounter();
        if (exitedRoom.EncounterRanOutOfTime()) {
            SetEventFinished(L10NLookup("BATTLEWORN_DUMMY.pages.DEFEAT.description"));
            return;
        }

        SetEventFinished(L10NLookup("BATTLEWORN_DUMMY.pages.VICTORY.description"));
        StaticVec<Reward, 8> rewards;
        switch (encounter) {
        case EncounterId::BATTLEWORN_DUMMY_EVENT_V1_ENCOUNTER: {
            // Character pool followed by the shared pool, selected with the
            // player's Rewards RNG (not the event RNG).
            PotionId id = Owner().PlayerRng().Rewards().NextItem(PotionFactory::GetPotionOptions(Owner()));
            if (id != PotionId::NONE)
                rewards.push_back(Reward::NewPotionReward(Owner(),
                    PotionModel(sim_, alloc_potion(*sim_->g, id, 0))));
            break;
        }
        case EncounterId::BATTLEWORN_DUMMY_EVENT_V2_ENCOUNTER: {
            CardList cards;
            for (CardModel card : Owner().Deck().Cards()) if (card.IsUpgradable()) cards.push_back(card);
            CardPileCmd::StableShuffle(cards, Rng());
            for (size_t i = 0; i < std::min<size_t>(2, cards.size()); ++i) CardCmd::Upgrade(cards[i]);
            break;
        }
        case EncounterId::BATTLEWORN_DUMMY_EVENT_V3_ENCOUNTER: {
            RelicId id = RelicFactory::PullNextRelicFromFront(Owner());
            rewards.push_back(Reward::NewRelicReward(Owner(),
                RelicModel(sim_, alloc_relic(*sim_->g, id, 0))));
            break;
        }
        default: throw GameError("BattlewornDummy.Resume received the wrong encounter");
        }
        for (RewardRef ref : exitedRoom.ExtraRewards()) rewards.push_back(Reward(sim_, ref));
        if (!rewards.empty()) RewardsCmd::OfferCustom(Owner(), rewards);
    }
};

}  // namespace sts2
