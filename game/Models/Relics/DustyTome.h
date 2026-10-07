// Models/Relics/DustyTome.cs (v0.111.0).
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/Canonical.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct DustyTome : RelicModel {
    SIM_MODEL(DustyTome, RelicModel)

    // The source stores a SerializableCard in the relic.  The simulator's
    // fixed POD relic state stores the canonical card id in the first extra
    // slot; no upgrade/enchantment is present during SetupForPlayer.
    CardId AncientCard() const { return static_cast<CardId>(extra(0)); }

    bool SetupForPlayer(Player player) const {
        StaticVec<CardId, 640> candidates;
        for (CardId id : CardFactory::GetUnlockedCards(
                 sts2::data(player.Character()).card_pool, player.st().profile.unlocked_epoch_bits)) {
            CanonicalCard canonical(&sim(), id);
            CardModel card = canonical.get();
            if (card.Rarity() != CardRarity::Ancient) continue;
            // Archaic Tooth's transformed cards are excluded by the source.
            if (id == CardId::BREAK || id == CardId::SUPPRESS || id == CardId::PROTECTOR ||
                id == CardId::METEOR_SHOWER || id == CardId::QUADCAST)
                continue;
            candidates.push_back(id);
        }
        if (candidates.empty()) return false;
        extra(0) = static_cast<int32_t>(player.PlayerRng().Rewards().NextItem(candidates));
        return true;
    }

    void AfterObtained() const {
        CardId id = AncientCard();
        if (id == CardId::NONE) throw GameError("Dusty Tome has no configured ancient card");
        CardModel card = Owner().RunState().CreateCard(id, Owner());
        CardCmd::Upgrade(card);
        (void)CardPileCmd::Add(card, PileType::Deck);
    }
};

}  // namespace sts2
