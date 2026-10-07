#include <array>
#include <vector>

#include "engine/run_setup.h"
#include "game/Entities/Players/Player.h"
#include "game/Models/EventModel.h"

using namespace sts2;

int main() {
    AncientEventProgressState progress;
    progress.SetVisitsAs(EventId::VAKUU, CharacterId::IRONCLAD, 6);
    progress.SetVisitsAs(EventId::VAKUU, CharacterId::SILENT, 23);
    progress.SetVisitsAs(EventId::OROBAS, CharacterId::IRONCLAD, 31);

    GameState state;
    Sim sim(state);
    new_run(state, NewRunOptions{
        .seed = "VAKUU01",
        .character = CharacterId::IRONCLAD,
        .ascension = 0,
        .progress_ancient_stats = progress,
    });
    Player player(&sim, 1);

    std::vector<RelicId> relics_before;
    state.relics.visit_snapshot([](auto, auto) {}, [&](uint16_t, const RelicState& relic) {
        relics_before.push_back(relic.id);
    });

    EventModel event = EventModel::ToMutable(&sim, EventId::VAKUU);
    event.BeginEvent(player, false);
    if (event.DynamicVars()["Visits"].IntValue() != 7) return 1;
    if (state.run.progress_ancient_stats.GetVisitsAs(EventId::VAKUU, CharacterId::SILENT) != 23 ||
        state.run.progress_ancient_stats.GetVisitsAs(EventId::OROBAS, CharacterId::IRONCLAD) != 31) return 2;

    static constexpr std::array<RelicId, 10> kPoolOrder = {
        RelicId::BLOOD_SOAKED_ROSE, RelicId::WHISPERING_EARRING, RelicId::FIDDLE,
        RelicId::PRESERVED_FOG, RelicId::SERE_TALON, RelicId::DISTINGUISHED_CAPE,
        RelicId::CHOICES_PARADOX, RelicId::MUSIC_BOX, RelicId::LORDS_PARASOL, RelicId::JEWELED_MASK,
    };
    std::vector<RelicId> relics_after;
    state.relics.visit_snapshot([](auto, auto) {}, [&](uint16_t, const RelicState& relic) {
        relics_after.push_back(relic.id);
    });
    if (relics_after.size() != relics_before.size() + kPoolOrder.size()) return 3;
    for (size_t i = 0; i < kPoolOrder.size(); ++i) {
        if (relics_after[relics_before.size() + i] != kPoolOrder[i]) return 4;
    }

    static constexpr std::array<RelicId, 3> kPool1 = {
        RelicId::BLOOD_SOAKED_ROSE, RelicId::WHISPERING_EARRING, RelicId::FIDDLE,
    };
    static constexpr std::array<RelicId, 3> kPool2 = {
        RelicId::PRESERVED_FOG, RelicId::SERE_TALON, RelicId::DISTINGUISHED_CAPE,
    };
    static constexpr std::array<RelicId, 4> kPool3 = {
        RelicId::CHOICES_PARADOX, RelicId::MUSIC_BOX, RelicId::LORDS_PARASOL, RelicId::JEWELED_MASK,
    };
    if (event.CurrentOptions().size() != 3 || event.Rng().counter != 7) return 5;
    const std::array pools = {std::span<const RelicId>(kPool1), std::span<const RelicId>(kPool2),
                              std::span<const RelicId>(kPool3)};
    for (size_t i = 0; i < event.CurrentOptions().size(); ++i) {
        const RelicId offered = state.relics[event.CurrentOptions()[i].relic].id;
        bool found = false;
        for (RelicId candidate : pools[i]) found |= candidate == offered;
        if (!found) return 6;
        if (offered == RelicId::SERE_TALON &&
            (event.CurrentOptions()[i].will_kill_player == 0 ||
             event.CurrentOptions()[i].will_kill_value != Dec(9))) return 7;
    }
    return 0;
}
