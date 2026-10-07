#include <stdexcept>
#include <iostream>

#include "engine/run_setup.h"
#include "engine/sim/alloc.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Models/Events/Orobas.h"
#include "game/Runs/RunState.h"

using namespace sts2;

static void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

int main(int argc, char** argv) {
    GameState state;
    Sim sim(state);
    const char* seed = argc == 3 && std::string_view(argv[1]) == "--dump" ? argv[2] : "OROBASENCHANT";
    new_run(state, NewRunOptions{.seed = seed, .character = CharacterId::IRONCLAD});
    Player player(&sim, 1);
    CardModel bash;
    for (CardModel card : player.Deck().Cards())
        if (card.CardId_() == CardId::BASH) { bash = card; break; }
    require(static_cast<bool>(bash), "Orobas fixture needs Ironclad's Bash");
    CardCmd::Upgrade(bash);
    CardCmd::Enchant(EnchantmentId::SHARP, bash, Dec(2));
    bash.st().enchantment_var = Dec(3);
    bash.st().enchantment_extra[0] = 7;

    EventModel event = EventModel::ToMutable(&sim, EventId::OROBAS);
    event.BeginEvent(player, false);
    Orobas orobas(&sim, 1);
    RelicModel tooth(&sim, alloc_relic(state, RelicId::ARCHAIC_TOOTH, 0));
    require(orobas.SetupTooth(tooth), "Orobas did not prepare an enchanted starter card");
    CardModel preview(&sim, static_cast<CardRef>(tooth.st().extra[1]));
    require(preview.CardId_() == CardId::BREAK && preview.CurrentUpgradeLevel() == 1 &&
                preview.Enchantment().EnchantmentId_() == EnchantmentId::SHARP &&
                preview.Enchantment().Amount() == 2 && preview.st().enchantment_var == Dec(3) &&
                preview.st().enchantment_extra[0] == 7,
            "Orobas Archaic Tooth preview lost the starter's upgrade or mutable enchantment facts");

    RelicCmd::Obtain(tooth, player);
    CardModel transformed;
    for (CardModel card : player.Deck().Cards()) {
        require(card.CardId_() != CardId::BASH, "Archaic Tooth left Bash in the deck");
        if (card.CardId_() == CardId::BREAK) transformed = card;
    }
    require(transformed && transformed.CurrentUpgradeLevel() == 1 &&
                transformed.Enchantment().EnchantmentId_() == EnchantmentId::SHARP &&
                transformed.Enchantment().Amount() == 2 &&
                transformed.st().enchantment_var == Dec(3) &&
                transformed.st().enchantment_extra[0] == 7,
            "Archaic Tooth pickup did not transform the enchanted Bash as in the source");
    if (argc == 3 && std::string_view(argv[1]) == "--dump") {
        auto dump = [](CardModel card) {
            std::cout << "{\"id\":\"" << card.data().entry << "\",\"upgrade\":"
                      << card.CurrentUpgradeLevel() << ",\"enchantment\":\""
                      << sts2::data(card.Enchantment().EnchantmentId_()).entry
                      << "\",\"enchantment_amount\":" << card.Enchantment().Amount() << '}';
        };
        std::cout << "{\"seed\":\"" << seed << "\",\"preview\":";
        dump(preview);
        std::cout << ",\"transformed\":";
        dump(transformed);
        std::cout << ",\"bash_remaining\":0}\n";
    }
}
