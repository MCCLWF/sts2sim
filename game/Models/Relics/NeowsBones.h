// Models/Relics/NeowsBones.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Commands/RewardsCmd.h"
#include "engine/sim/alloc.h"
#include "game/Commands/RelicCmd.h"
#include "game/Commands/CardPileCmd.h"
namespace sts2 {
struct NeowsBones : RelicModel {
    SIM_MODEL(NeowsBones, RelicModel)
    void AfterObtained() const {
        StaticVec<RelicId, 32> ids;
        for (RelicId id : {RelicId::CURSED_PEARL, RelicId::DOWSING_ROD, RelicId::HEFTY_TABLET, RelicId::LARGE_CAPSULE,
                           RelicId::LEAFY_POULTICE, RelicId::NEOWS_SACRIFICE, RelicId::PRECARIOUS_SHEARS,
                           RelicId::SILKEN_TRESS, RelicId::SILVER_CRUCIBLE,
                           RelicId::ARCANE_SCROLL, RelicId::BOOMING_CONCH, RelicId::FISHING_ROD, RelicId::GOLDEN_PEARL,
                           RelicId::KALEIDOSCOPE, RelicId::LEAD_PAPERWEIGHT, RelicId::LOST_COFFER, RelicId::NEOWS_TORMENT,
                           RelicId::NEW_LEAF, RelicId::PHIAL_HOLSTER, RelicId::PRECISE_SCISSORS, RelicId::SCROLL_BOXES,
                           RelicId::WINGED_BOOTS, RelicId::LAVA_ROCK, RelicId::NEOWS_TALISMAN, RelicId::NUTRITIOUS_OYSTER,
                           RelicId::POMANDER, RelicId::SMALL_CAPSULE, RelicId::STONE_HUMIDIFIER}) ids.push_back(id);
        Owner().PlayerRng().Rewards().Shuffle(ids);
        StaticVec<Reward, 8> rewards;
        for (size_t i = 0; i < ids.size() && i < 2; ++i) {
            RelicRef ref = alloc_relic(*sim().g, ids[i], 0);
            rewards.push_back(Reward::NewRelicReward(Owner(), RelicModel(&sim(), ref)));
        }
        RewardsCmd::OfferCustom(Owner(), rewards);
        StaticVec<CardId, 32> curses;
        for (CardId c : sts2::data(CardPoolId::CURSE_CARD_POOL).cards) if (c == CardId::CLUMSY || c == CardId::DEBT || c == CardId::DECAY || c == CardId::DOUBT || c == CardId::GUILTY || c == CardId::INJURY || c == CardId::NORMALITY || c == CardId::REGRET || c == CardId::SHAME || c == CardId::WRITHE) curses.push_back(c);
        if (!curses.empty()) CardPileCmd::AddCurseToDeck(curses[Owner().RunState().Rng().Niche().next_item_index(static_cast<int32_t>(curses.size()))], Owner());
    }
};
}
