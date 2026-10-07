#include <array>
#include <stdexcept>

#include "engine/run_setup.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

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
        state.run.current_act_index = 2;
        state.run.rooms.push_back(RoomType::Event);
        state.run.room_event = EventId::WAR_HISTORIAN_REPY;
        state.run.phase = RunPhase::Event;
        player = Player(&sim, 1);
    }

    void add_key() { CardPileCmd::Add(player.RunState().CreateCard(CardId::LANTERN_KEY, player), PileType::Deck); }
    int key_count() const {
        int count = 0;
        for (CardModel card : player.Deck().Cards()) count += card.CardId_() == CardId::LANTERN_KEY;
        return count;
    }
    EventModel begin() {
        EventModel event = EventModel::ToMutable(&sim, EventId::WAR_HISTORIAN_REPY);
        event.BeginEvent(player, false);
        return event;
    }
    void skip_chest() {
        static constexpr std::array<int32_t, 1> answers = {4};
        sim.set_prefix(answers);
    }
};

int main() {
    {
        Fixture f("REPYONE");
        f.add_key();
        EventModel event = f.begin();
        require(event.CurrentOptions().size() == 2 && f.key_count() == 1,
                "War Historian initial options or key fixture differ from source");
        event.ChooseOption(0);
        require(event.IsFinished() && f.key_count() == 0 && f.state.run.freed_repy &&
                    f.player.GetRelic(RelicId::HISTORY_COURSE),
                "War Historian one-key cage did not consume key and free Repy");
    }
    {
        Fixture f("REPYTWOA");
        f.add_key();
        f.add_key();
        EventModel event = f.begin();
        event.ChooseOption(0);
        require(!event.IsFinished() && f.key_count() == 1 && f.state.run.freed_repy &&
                    event.CurrentOptions().size() == 1 &&
                    event.CurrentOptions()[0].text_key ==
                        MakeLocKey("WAR_HISTORIAN_REPY.pages.INITIAL.options.UNLOCK_CHEST").hash,
                "War Historian cage-first did not offer the second chest");
        f.skip_chest();
        event.ChooseOption(0);
        require(f.sim.prefix_consumed() && event.IsFinished() && f.key_count() == 0,
                "War Historian second chest did not consume remaining keys and finish");
    }
    {
        Fixture f("REPYTWOB");
        f.add_key();
        f.add_key();
        EventModel event = f.begin();
        f.skip_chest();
        event.ChooseOption(1);
        require(f.sim.prefix_consumed() && !event.IsFinished() && f.key_count() == 1 &&
                    !f.state.run.freed_repy && event.CurrentOptions().size() == 1 &&
                    event.CurrentOptions()[0].text_key ==
                        MakeLocKey("WAR_HISTORIAN_REPY.pages.INITIAL.options.UNLOCK_CAGE").hash,
                "War Historian chest-first did not offer the second cage");
        event.ChooseOption(0);
        require(event.IsFinished() && f.key_count() == 0 && f.state.run.freed_repy &&
                    f.player.GetRelic(RelicId::HISTORY_COURSE),
                "War Historian second cage did not consume remaining keys and free Repy");
    }
}
