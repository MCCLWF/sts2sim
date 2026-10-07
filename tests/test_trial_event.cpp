#include <stdexcept>
#include <string>
#include <memory>

#include "engine/run_setup.h"
#include "game/Commands/CardSelectCmd.h"
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
    EventModel event;

    explicit Fixture(const std::string& seed) : sim(state) {
        new_run(state, NewRunOptions{.seed = seed, .character = CharacterId::IRONCLAD});
        player = Player(&sim, 1);
        event = EventModel::ToMutable(&sim, EventId::TRIAL);
        event.BeginEvent(player, false);
    }

    int curses(CardId id) const {
        int n = 0;
        for (CardModel card : player.Deck().Cards()) n += card.CardId_() == id;
        return n;
    }
};

int main() {
    {
        Fixture f("TRIALREJECT");
        require(f.event.CurrentOptions().size() == 2, "Trial must start with Accept and Reject");
        f.event.ChooseOption(1);
        require(f.event.CurrentOptions().size() == 2 &&
                    f.event.st().description == MakeLocKey("TRIAL.pages.REJECT.description").hash &&
                    f.event.CurrentOptions()[1].will_kill_player != 0,
                "Trial reject page differs from the source");
        try {
            f.event.ChooseOption(1);
            throw std::runtime_error("Trial abandon popup did not request input");
        } catch (const NeedChoice& choice) {
            require(choice.request.kind == ChoiceKind::Option && choice.request.option_count == 2 &&
                        choice.request.source == "NAbandonRunConfirmPopup",
                    "Trial confirmation must use the ordinary Choice prefix");
        }
    }

    bool seen[3] = {};
    for (int i = 0; i < 128 && !(seen[0] && seen[1] && seen[2]); ++i) {
        Fixture f("TRIAL" + std::to_string(i));
        f.event.ChooseOption(0);
        require(f.event.st().description == MakeLocKey("TRIAL.trialFormat").hash &&
                    f.event.CurrentOptions().size() == 2 && f.event.Rng().counter == 1,
                "Trial Accept must draw one event RNG result and show the formatted verdict");
        uint64_t guilty = f.event.CurrentOptions()[0].text_key;
        int kind = guilty == MakeLocKey("TRIAL.pages.MERCHANT.options.GUILTY").hash ? 0 :
                   guilty == MakeLocKey("TRIAL.pages.NOBLE.options.GUILTY").hash ? 1 :
                   guilty == MakeLocKey("TRIAL.pages.NONDESCRIPT.options.GUILTY").hash ? 2 : -1;
        require(kind >= 0, "Trial Accept selected a verdict absent from the source");
        if (seen[kind]) continue;
        seen[kind] = true;
        int old_hp = f.player.Creature().CurrentHp();
        int old_gold = f.player.Gold();
        if (kind == 1) f.player.Creature().SetCurrentHpInternal(Dec(old_hp - 10));
        CardSelectCmd::ScopedDeterministicSelector select(&f.sim);
        int32_t skip_both_rewards[1] = {};
        if (kind == 2) {
            auto before = std::make_unique<GameState>(f.state);
            try {
                f.event.ChooseOption(0);
                throw std::runtime_error("Trial's two rewards did not open a choice");
            } catch (const NeedChoice& choice) {
                require(choice.request.source == "rewards" && choice.request.option_count == 3,
                        "Trial Nondescript guilty must offer two card rewards plus proceed");
                skip_both_rewards[0] = choice.request.option_count - 1;
                f.state = *before;
            }
            f.sim.set_prefix(skip_both_rewards);
        }
        try {
            f.event.ChooseOption(0);
        } catch (const GameError& e) {
            throw std::runtime_error("Trial kind " + std::to_string(kind) + ": " + e.what());
        }
        require(f.event.IsFinished() &&
                    f.event.st().description == MakeLocKey("TRIAL.trialResult").hash,
                "Trial guilty verdict did not finish with the source result page");
        if (kind == 0) require(f.curses(CardId::REGRET) == 1, "Merchant guilty omitted Regret");
        if (kind == 1) require(f.player.Creature().CurrentHp() == old_hp,
                               "Noble guilty did not heal 10 HP");
        if (kind == 2) require(f.curses(CardId::DOUBT) == 1 && f.sim.prefix_consumed(),
                               "Nondescript guilty did not offer both card rewards");
        require(f.player.Gold() == old_gold, "Trial guilty verdict unexpectedly changed gold");

        Fixture innocent("TRIAL" + std::to_string(i));
        innocent.event.ChooseOption(0);
        const int innocent_hp = innocent.player.Creature().CurrentHp();
        const int innocent_gold = innocent.player.Gold();
        const size_t old_deck_size = innocent.player.Deck().Cards().size();
        CardSelectCmd::ScopedDeterministicSelector select_innocent(&innocent.sim);
        innocent.event.ChooseOption(1);
        require(innocent.event.IsFinished() &&
                    innocent.event.st().description == MakeLocKey("TRIAL.trialResult").hash,
                "Trial innocent verdict did not finish with the source result page");
        if (kind == 0) {
            int upgraded = 0;
            for (CardModel card : innocent.player.Deck().Cards()) upgraded += card.CurrentUpgradeLevel() > 0;
            require(innocent.curses(CardId::SHAME) == 1 && upgraded == 2,
                    "Merchant innocent did not add Shame and upgrade two cards");
        }
        if (kind == 1)
            require(innocent.curses(CardId::REGRET) == 1 &&
                        innocent.player.Gold() == innocent_gold + 300,
                    "Noble innocent did not add Regret and gain 300 gold");
        if (kind == 2)
            require(innocent.curses(CardId::DOUBT) == 1 &&
                        innocent.player.Deck().Cards().size() == old_deck_size + 1,
                    "Nondescript innocent did not transform two cards and add Doubt");
        require(innocent.player.Creature().CurrentHp() == innocent_hp,
                "Trial innocent verdict unexpectedly changed HP");
    }
    require(seen[0] && seen[1] && seen[2], "Trial fixture failed to exercise each verdict type");
}
