#include <stdexcept>

#include "engine/run_setup.h"
#include "game/All.h"
#include "game/Commands/PotionCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Runs/RunManager.h"

using namespace sts2;

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void check(bool in_combat, bool sozu, bool occupied) {
    GameState state;
    Sim sim(state);
    new_run(state, NewRunOptions{.seed = "ENTROPICSOZU", .character = CharacterId::SILENT, .ascension = 10});
    Player player(&sim, 1);
    RunManager::SetActInternal(&sim, 0);
    require(PotionCmd::TryToProcure(PotionId::ENTROPIC_BREW, player).success, "could not procure Entropic Brew");
    if (occupied) require(PotionCmd::TryToProcure(PotionId::FIRE_POTION, player).success, "could not procure retained potion");
    // A legal run may acquire Sozu after already carrying these potions.
    if (sozu) RelicCmd::Obtain(RelicId::SOZU, player);
    if (in_combat) RunManager::EnterMapCoordDebug(&sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
    PotionModel brew = player.PotionSlots()[0];
    PotionModel retained = occupied ? player.PotionSlots()[1] : PotionModel{};
    Rng& rng = player.RunState().Rng().CombatPotionGeneration();
    Rng expected = rng;
    // PotionFactory.cs draws rarity then NextItem for each generated potion.
    // EntropicBrew.cs generates once even when Sozu refuses procurement, then breaks.
    const int attempts = sozu ? 1 : static_cast<int>(player.PotionSlots().size()) - static_cast<int>(occupied);
    for (int i = 0; i < attempts; ++i) { expected.NextFloat(); expected.NextInt(1); }
    PlayerChoiceContext context = PlayerChoiceContext::Throwing();
    brew.OnUseWrapper(context, player.Creature());
    require(rng == expected, "Entropic Brew consumed the wrong RNG draws");
    require(!player.Potions().contains(brew), "used Entropic Brew was retained");
    if (occupied) require(player.PotionSlots()[1] == retained, "Entropic Brew changed an occupied slot");
    if (sozu) {
        require(!player.PotionSlots()[0], "Sozu allowed a generated potion");
        require(player.Potions().size() == static_cast<size_t>(occupied), "failed procurement changed potion inventory");
    } else {
        require(!player.HasOpenPotionSlots(), "Entropic Brew did not fill the open slots");
    }
}
}  // namespace

int main() {
    for (bool combat : {false, true})
        for (bool sozu : {false, true})
            for (bool occupied : {false, true}) check(combat, sozu, occupied);
}
