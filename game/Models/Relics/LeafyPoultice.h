// Models/Relics/LeafyPoultice.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/CardCmd.h"
namespace sts2 {
struct LeafyPoultice : RelicModel {
    SIM_MODEL(LeafyPoultice, RelicModel)
    void AfterObtained() const {
        PlayerChoiceContext context = PlayerChoiceContext::Throwing();
        CreatureCmd::LoseMaxHp(context, Owner().Creature(), Dec(12), false);
        CardModel strike, defend;
        for (CardModel c : Owner().Deck().Cards()) if (c.Rarity() == CardRarity::Basic) { if (!strike && c.HasTag(CardTag::Strike)) strike = c; if (!defend && c.HasTag(CardTag::Defend)) defend = c; }
        std::vector<CardTransformation> t; if (strike) t.emplace_back(strike); if (defend) t.emplace_back(defend);
        if (!t.empty()) CardCmd::Transform(t, &Owner().PlayerRng().Transformations());
    }
};
}
