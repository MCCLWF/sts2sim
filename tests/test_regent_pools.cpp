#include <cstdint>
#include <stdexcept>

#include "engine/run_setup.h"
#include "game/Entities/Players/Player.h"
#include "game/Factories/CardFactory.h"
#include "game/Factories/PotionFactory.h"

using namespace sts2;

namespace {

constexpr uint64_t kAllEpochs = 0x1ffffffffffffffULL;

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

template <class Range, class Value>
bool contains(const Range& range, Value value) {
    for (Value item : range) if (item == value) return true;
    return false;
}

void check_profile(uint64_t epochs, bool regent2, bool regent3, bool regent4,
                   bool regent5, bool regent6, bool regent7) {
    GameState state;
    Sim sim(state);
    NewRunOptions options{.seed = "REGENT_EPOCH_POOL", .character = CharacterId::REGENT};
    options.profile.unlocked_epoch_bits = epochs;
    new_run(state, options);
    Player player(&sim, 1);

    const auto cards = CardFactory::GetUnlockedCards(CardPoolId::REGENT_CARD_POOL, epochs);
    require(contains(cards, CardId::FURNACE) == regent2, "Regent2 card pool gate differed from source");
    require(contains(cards, CardId::BEGONE) == regent5, "Regent5 card pool gate differed from source");
    require(contains(cards, CardId::PATTER) == regent7, "Regent7 card pool gate differed from source");

    const auto& relics = player.st().relic_grab_bag.original;
    require(contains(relics, RelicId::FENCING_MANUAL) == regent3,
            "Regent3 relic pool gate differed from source");
    require(contains(relics, RelicId::MINI_REGENT) == regent6,
            "Regent6 relic pool gate differed from source");

    const auto potions = PotionFactory::GetPotionOptions(player);
    for (PotionId potion : {PotionId::STAR_POTION, PotionId::COSMIC_CONCOCTION, PotionId::KINGS_COURAGE})
        require(contains(potions, potion) == regent4, "Regent4 potion pool gate differed from source");
}

}  // namespace

int main() {
    check_profile(kAllEpochs, true, true, true, true, true, true);
    for (int epoch : {38, 39, 40, 41, 42, 43}) {
        const uint64_t epochs = kAllEpochs & ~(uint64_t{1} << epoch);
        check_profile(epochs, epoch != 38, epoch != 39, epoch != 40,
                      epoch != 41, epoch != 42, epoch != 43);
    }
}
