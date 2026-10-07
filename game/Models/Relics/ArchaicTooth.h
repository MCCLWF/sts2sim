// Models/Relics/ArchaicTooth.cs. The displayed SerializableCard snapshots are
// prepared by Orobas; the actual pickup searches the current deck again.
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct ArchaicTooth : RelicModel {
    SIM_MODEL(ArchaicTooth, RelicModel)

    static CardId Transcendence(CardId id) {
        switch (id) {
        case CardId::BASH: return CardId::BREAK;
        case CardId::NEUTRALIZE: return CardId::SUPPRESS;
        case CardId::UNLEASH: return CardId::PROTECTOR;
        case CardId::FALLING_STAR: return CardId::METEOR_SHOWER;
        case CardId::DUALCAST: return CardId::QUADCAST;
        default: return CardId::NONE;
        }
    }

    void AfterObtained() const {
        CardModel starter;
        for (CardModel card : Owner().Deck().Cards()) {
            if (Transcendence(card.CardId_()) != CardId::NONE) { starter = card; break; }
        }
        if (!starter) throw GameError("Archaic Tooth starter card is missing");
        CardModel replacement = Owner().RunState().CreateCard(Transcendence(starter.CardId_()), Owner());
        if (starter.CurrentUpgradeLevel() > 0) CardCmd::Upgrade(replacement);
        CardCmd::CloneEnchantment(starter, replacement);
        (void)CardCmd::Transform(starter, replacement);
    }
};

}  // namespace sts2
