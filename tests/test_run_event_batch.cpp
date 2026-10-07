#include <stdexcept>

#include "engine/run_setup.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Commands/PotionCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunManager.h"

using namespace sts2;

static void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

struct Fixture {
    GameState state;
    Sim sim;
    Player player;

    explicit Fixture(const char* seed) : sim(state) {
        new_run(state, NewRunOptions{.seed = seed, .character = CharacterId::IRONCLAD});
        player = Player(&sim, 1);
        state.run.current_act_index = 1;
    }

    EventModel begin(EventId id) {
        EventModel event = EventModel::ToMutable(&sim, id);
        event.BeginEvent(player, false);
        return event;
    }
};

int main() {
    {
        Fixture f("EVTBATCH01");
        EventModel event = f.begin(EventId::AMALGAMATOR);
        require(event.CurrentOptions().size() == 2, "Amalgamator did not expose both source options");
        CardSelectCmd::ScopedDeterministicSelector choose(&f.sim);
        event.ChooseOption(0);
        require(event.IsFinished(), "Amalgamator did not finish after combining strikes");
        int basics = 0, combined = 0;
        for (CardModel card : f.player.Deck().Cards()) {
            basics += card.CardId_() == CardId::STRIKE_IRONCLAD;
            combined += card.CardId_() == CardId::ULTIMATE_STRIKE;
        }
        require(basics == 3 && combined == 1, "Amalgamator deck transition differed from the source");
    }
    {
        Fixture f("EVTBATCH02");
        EventModel event = f.begin(EventId::FIELD_OF_MAN_SIZED_HOLES);
        require(event.CurrentOptions().size() == 2, "Field of Man-Sized Holes options missing");
        CardSelectCmd::ScopedDeterministicSelector choose(&f.sim);
        event.ChooseOption(0);
        int normality = 0;
        for (CardModel card : f.player.Deck().Cards()) normality += card.CardId_() == CardId::NORMALITY;
        require(event.IsFinished() && normality == 1 && f.player.Deck().Cards().size() == 9,
                "Field removal and curse sequence failed");
    }
    {
        Fixture f("EVTBATCH03");
        PlayerCmd::SetGold(200, f.player);
        EventModel event = f.begin(EventId::ZEN_WEAVER);
        require(event.CurrentOptions().size() == 3 && !event.CurrentOptions()[1].is_locked &&
                    event.CurrentOptions()[2].is_locked,
                "Zen Weaver gold gates differ from source");
        event.ChooseOption(0);
        int enlightenment = 0;
        for (CardModel card : f.player.Deck().Cards()) enlightenment += card.CardId_() == CardId::ENLIGHTENMENT;
        require(event.IsFinished() && f.player.Gold() == 150 && enlightenment == 2,
                "Zen Weaver gold and deck transition failed");
    }
    {
        Fixture f("EVTBATCH04");
        require(PotionCmd::TryToProcure(PotionId::FIRE_POTION, f.player, -1).success,
                "Stone fixture could not procure a potion");
        int hp = f.player.Creature().MaxHp();
        EventModel event = f.begin(EventId::STONE_OF_ALL_TIME);
        require(!f.player.CanUseOrRemovePotions() && event.CurrentOptions().size() == 2,
                "Stone did not lock potions at entry");
        event.ChooseOption(0);
        require(event.IsFinished() && f.player.CanUseOrRemovePotions() && f.player.Potions().empty() &&
                    f.player.Creature().MaxHp() == hp + 10 && event.Rng().counter == 2,
                "Stone potion, max HP, RNG or cleanup transition failed");
    }
    {
        Fixture f("EVTBATCH05");
        EventModel event = f.begin(EventId::TANX);
        require(event.CurrentOptions().size() == 3 && event.Rng().counter == 9,
                "Tanx source pool must include Tri-Boomerang for Ironclad's starting attacks");
    }
    {
        Fixture f("EVTBATCH06");
        EventModel event = f.begin(EventId::DOLL_ROOM);
        require(event.CurrentOptions().size() == 3, "Doll Room initial options missing");
        event.ChooseOption(0);
        bool hasDoll = f.player.GetRelic(RelicId::DAUGHTER_OF_THE_WIND) ||
                       f.player.GetRelic(RelicId::MR_STRUGGLES) || f.player.GetRelic(RelicId::BING_BONG);
        require(event.IsFinished() && hasDoll && event.Rng().counter == 1,
                "Doll Room random choice did not obtain exactly one source doll");
    }
    {
        Fixture f("EVTBATCH07");
        for (RelicId id : {RelicId::ANCHOR, RelicId::BAG_OF_PREPARATION, RelicId::MERCURY_HOURGLASS,
                           RelicId::VAJRA, RelicId::ORICHALCUM})
            RelicCmd::Obtain(id, f.player);
        require(EventModel::Canonical(&f.sim, EventId::RELIC_TRADER).IsAllowed(RunState(&f.sim)),
                "Relic Trader fixture does not satisfy the source's five tradable relic gate");
        EventModel event = f.begin(EventId::RELIC_TRADER);
        if (event.CurrentOptions().size() != 3 || event.Rng().counter != 4)
            throw std::runtime_error("Relic Trader options=" + std::to_string(event.CurrentOptions().size()) +
                                     " rng=" + std::to_string(event.Rng().counter) +
                                     " count=" + std::to_string(event.st().extra[0]));
        RelicId old = f.state.relics[static_cast<RelicRef>(event.st().extra[1])].id;
        RelicId offered = static_cast<RelicId>(event.st().extra[4]);
        event.ChooseOption(0);
        require(event.IsFinished() && !f.player.GetRelic(old) && f.player.GetRelic(offered),
                "Relic Trader did not exchange the offered pair");
    }
    {
        Fixture f("EVTBATCH08");
        EventModel event = f.begin(EventId::DOLL_ROOM);
        const int hp = f.player.Creature().CurrentHp();
        event.ChooseOption(1);
        require(f.player.Creature().CurrentHp() == hp - 5 && event.CurrentOptions().size() == 2 &&
                    event.Rng().counter == 2,
                "Doll Room take-time choice did not damage and shuffle as in source");
        for (const EventOptionState& option : event.CurrentOptions()) {
            const RelicId id = static_cast<RelicId>(option.arg);
            const char* title = id == RelicId::BING_BONG ? "Bing Bong" :
                                id == RelicId::DAUGHTER_OF_THE_WIND ? "Daughter of the Wind" :
                                id == RelicId::MR_STRUGGLES ? "Mr. Struggles" : "";
            require(option.text_key == MakeLocKey(title).hash,
                    "Doll Room choice TextKey differs from the source relic title");
        }
        RelicId chosen = static_cast<RelicId>(event.CurrentOptions()[0].arg);
        event.ChooseOption(0);
        require(event.IsFinished() && f.player.GetRelic(chosen), "Doll Room selected relic was not obtained");
    }
    {
        Fixture f("EVTBATCH09");
        EventModel event = f.begin(EventId::THE_LANTERN_KEY);
        const int gold = f.player.Gold();
        require(event.CurrentOptions().size() == 2 &&
                    event.CanonicalEncounter() == EncounterId::MYSTERIOUS_KNIGHT_EVENT_ENCOUNTER,
                "Lantern Key did not initialize its source options or encounter");
        event.ChooseOption(0);
        require(event.IsFinished() && f.player.Gold() == gold + 100,
                "Lantern Key return option did not award 100 gold");
    }
    {
        Fixture f("EVTBATCH10");
        f.state.run.rooms.push_back(RoomType::Event);
        f.state.run.room_event = EventId::THE_LANTERN_KEY;
        EventModel event = f.begin(EventId::THE_LANTERN_KEY);
        event.ChooseOption(1);
        require(event.CurrentOptions().size() == 1 &&
                    event.CurrentOptions()[0].text_key ==
                        MakeLocKey("THE_LANTERN_KEY.pages.KEEP_THE_KEY.options.FIGHT").hash,
                "Lantern Key did not advance to its fight page");
        event.ChooseOption(0);
        require(f.state.run.combat_extra_rewards.size() == 1 &&
                    f.state.rewards[f.state.run.combat_extra_rewards[0]].type == RewardType::SpecialCard &&
                    f.state.combat.encounter == EncounterId::MYSTERIOUS_KNIGHT_EVENT_ENCOUNTER,
                "Lantern Key fight did not carry its special card reward into combat");
    }
}
