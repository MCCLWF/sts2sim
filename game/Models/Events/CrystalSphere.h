// Models/Events/CrystalSphere.cs and Events/Custom/CrystalSphereEvent/*.cs
#pragma once

#include <array>
#include <string>
#include <vector>

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Commands/RewardsCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Factories/PotionFactory.h"
#include "game/Models/Events/AllowedHelpers.h"
#include "game/Models/EventModel.h"
#include "game/Runs/CardCreationOptions.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct CrystalSphere : EventModel {
    SIM_MODEL(CrystalSphere, EventModel)

    static constexpr int kWidth = 11;
    static constexpr int kCellCount = kWidth * kWidth;
    static constexpr int kItemCount = 15;
    static constexpr int kToolSmall = 1;
    static constexpr int kToolBig = 2;
    static constexpr uint8_t kHiddenEmpty = 0;
    static constexpr uint8_t kClearEmpty = 1;

    enum ItemKind : uint8_t { Relic, Potion, Card, Curse, Gold };
    struct ItemSpec {
        ItemKind kind;
        int width;
        int height;
        int value;  // Potion/Card rarity, or gold amount; unused for relic/curse.
    };

    // The source's item order is part of both placement RNG and reveal order.
    static constexpr std::array<ItemSpec, kItemCount> kItems = {{
        {Relic, 4, 4, 0},
        {Potion, 1, 3, static_cast<int>(PotionRarity::Common)},
        {Potion, 1, 3, static_cast<int>(PotionRarity::Common)},
        {Potion, 2, 2, static_cast<int>(PotionRarity::Rare)},
        {Card, 2, 2, static_cast<int>(CardRarity::Common)},
        {Card, 2, 2, static_cast<int>(CardRarity::Uncommon)},
        {Card, 2, 2, static_cast<int>(CardRarity::Rare)},
        {Curse, 2, 2, 0},
        {Gold, 1, 1, 10},
        {Gold, 1, 1, 10},
        {Gold, 1, 1, 10},
        {Gold, 1, 1, 10},
        {Gold, 1, 1, 10},
        {Gold, 2, 1, 30},
        {Gold, 2, 1, 30},
    }};

    // EventState.extra is reserved for this event while the minigame is active:
    // [0] active, [1] tool, [2] divinations remaining, [3] revealed item bits.
    int32_t& active() const { return extra(0); }
    int32_t& tool() const { return extra(1); }
    int32_t& divinations() const { return extra(2); }
    int32_t& revealed_bits() const { return extra(3); }

    static int cell_index(int x, int y) { return y * kWidth + x; }
    static bool in_bounds(int x, int y) { return x >= 0 && x < kWidth && y >= 0 && y < kWidth; }

    uint8_t& cell(int x, int y) const { return st().crystal_sphere_cells[static_cast<size_t>(cell_index(x, y))]; }
    static bool hidden(uint8_t value) { return value == kHiddenEmpty || (value >= 2 && value <= 16); }
    static int item_index(uint8_t value) {
        if (value >= 2 && value <= 16) return static_cast<int>(value) - 2;
        if (value >= 18 && value <= 32) return static_cast<int>(value) - 18;
        return -1;
    }

    static std::vector<int> horizontal(int x, int y) {
        std::vector<int> result;
        for (int delta : {-1, 1}) if (in_bounds(x + delta, y)) result.push_back(cell_index(x + delta, y));
        return result;
    }
    static std::vector<int> vertical(int x, int y) {
        std::vector<int> result;
        for (int delta : {-1, 1}) if (in_bounds(x, y + delta)) result.push_back(cell_index(x, y + delta));
        return result;
    }
    static std::vector<int> diagonal(int x, int y) {
        std::vector<int> result;
        for (int dx : {-1, 1}) for (int dy : {-1, 1})
            if (in_bounds(x + dx, y + dy)) result.push_back(cell_index(x + dx, y + dy));
        return result;
    }
    static std::vector<int> adjacent(int x, int y) {
        std::vector<int> result = horizontal(x, y);
        auto append = [&](const std::vector<int>& values) { result.insert(result.end(), values.begin(), values.end()); };
        append(vertical(x, y));
        append(diagonal(x, y));
        result.push_back(cell_index(x, y));
        return result;
    }

    void clear_cell(int index) const {
        uint8_t& value = st().crystal_sphere_cells[static_cast<size_t>(index)];
        if (!hidden(value)) return;
        value = value == kHiddenEmpty ? kClearEmpty : static_cast<uint8_t>(value + 16);
    }

    bool can_place(const ItemSpec& item, int x, int y) const {
        for (int dx = 0; dx < item.width; ++dx) for (int dy = 0; dy < item.height; ++dy) {
            if (!in_bounds(x + dx, y + dy) || cell(x + dx, y + dy) != kHiddenEmpty) return false;
        }
        return true;
    }

    bool item_clear(int index) const {
        const ItemSpec& item = kItems[static_cast<size_t>(index)];
        for (int dx = 0; dx < item.width; ++dx) for (int dy = 0; dy < item.height; ++dy) {
            uint8_t value = cell(item_position_x(index) + dx, item_position_y(index) + dy);
            if (value != static_cast<uint8_t>(index + 18)) return false;
        }
        return true;
    }

    // Positions are encoded alongside the board during setup. They are only
    // needed while placing items and are discarded once setup completes.
    int item_position_x(int index) const { return (st().extra[4 + index / 4] >> ((index % 4) * 8)) & 0xf; }
    int item_position_y(int index) const { return st().extra[4 + index / 4] >> ((index % 4) * 8 + 4) & 0xf; }
    void set_item_position(int index, int x, int y) const {
        int slot = 4 + index / 4;
        int shift = (index % 4) * 8;
        st().extra[slot] &= ~(0xff << shift);
        st().extra[slot] |= ((x & 0xf) | ((y & 0xf) << 4)) << shift;
    }

    void RevealItem(int index) const {
        int bit = 1 << index;
        if ((revealed_bits() & bit) != 0) return;
        revealed_bits() |= bit;
        st().crystal_sphere_revealed_order.push_back(static_cast<uint8_t>(index));
        if (kItems[static_cast<size_t>(index)].kind == Curse)
            CardPileCmd::AddCurseToDeck(CardId::DOUBT, Owner());
    }

    bool PlaceItems() const {
        auto& cells = st().crystal_sphere_cells;
        cells.clear();
        for (int i = 0; i < kCellCount; ++i) cells.push_back(kHiddenEmpty);

        // CrystalSphereMinigame's two expansion rounds around the four corners.
        std::vector<int> frontier = {cell_index(0, 0), cell_index(10, 0), cell_index(10, 10), cell_index(0, 10)};
        for (int round = 0; round < 2; ++round) {
            std::vector<int> next = frontier;
            for (int encoded : frontier) {
                int x = encoded % kWidth, y = encoded / kWidth;
                auto h = horizontal(x, y); next.insert(next.end(), h.begin(), h.end());
                auto v = vertical(x, y); next.insert(next.end(), v.begin(), v.end());
            }
            frontier = std::move(next);
        }
        for (int encoded : frontier) clear_cell(encoded);

        for (int index = 0; index < kItemCount; ++index) {
            const ItemSpec& item = kItems[static_cast<size_t>(index)];
            StaticVec<int32_t, kCellCount> candidates;
            // CrystalSphereItem.PlaceItem enumerates x first, then y. The
            // NextItem index depends on this exact candidate order.
            for (int x = 0; x < kWidth; ++x) for (int y = 0; y < kWidth; ++y)
                if (can_place(item, x, y)) candidates.push_back(cell_index(x, y));
            if (candidates.empty()) return false;
            int encoded = Rng().NextItem(candidates);
            int x = encoded % kWidth, y = encoded / kWidth;
            set_item_position(index, x, y);
            for (int dx = 0; dx < item.width; ++dx) for (int dy = 0; dy < item.height; ++dy)
                cell(x + dx, y + dy) = static_cast<uint8_t>(index + 2);
        }
        return true;
    }

    void InitializeMinigame(int count) const {
        active() = 1;
        tool() = kToolBig;
        divinations() = count;
        revealed_bits() = 0;
        st().crystal_sphere_revealed_order.clear();
        for (int i = 4; i < 8; ++i) st().extra[i] = 0;
        // The source keeps previously placed items when PopulateItems fails.
        // This fixed board has no observed failures in the placement corpus;
        // if one occurs, stop explicitly until the retry state is represented.
        if (!PlaceItems()) throw Unported("CrystalSphere placement retry after partial board");
    }

    ChoiceRequest CellRequest() const {
        ChoiceRequest request;
        request.kind = ChoiceKind::Option;
        request.source = "crystal_sphere";
        request.prompt = "CRYSTAL_SPHERE.minigame";
        request.option_count = kCellCount + 2;
        for (int y = 0; y < kWidth; ++y) for (int x = 0; x < kWidth; ++x) {
            ChoiceOption option;
            option.kind = ChoiceOptionKind::Alternative;
            option.key = "CRYSTAL_SPHERE.cell." + std::to_string(x) + "." + std::to_string(y);
            request.options.push_back(std::move(option));
        }
        ChoiceOption small;
        small.kind = ChoiceOptionKind::Alternative;
        small.key = "CRYSTAL_SPHERE.button.DIVINATION_LABEL_SMALL";
        request.options.push_back(std::move(small));
        ChoiceOption big;
        big.kind = ChoiceOptionKind::Alternative;
        big.key = "CRYSTAL_SPHERE.button.DIVINATION_LABEL_BIG";
        request.options.push_back(std::move(big));
        return request;
    }

    void RevealAt(int index) const {
        int x = index % kWidth, y = index / kWidth;
        std::vector<int> cells_to_clear = tool() == kToolBig ? adjacent(x, y) : std::vector<int>{index};
        for (int target : cells_to_clear) {
            uint8_t before = st().crystal_sphere_cells[static_cast<size_t>(target)];
            clear_cell(target);
            int item = item_index(before);
            if (item >= 0 && item_clear(item)) RevealItem(item);
        }
    }

    Reward ItemReward(int index) const {
        const ItemSpec& item = kItems[static_cast<size_t>(index)];
        switch (item.kind) {
        case Gold: {
            Reward reward = Reward::NewGoldReward(Owner(), item.value);
            reward.UseEventRng();
            return reward;
        }
        case Relic: {
            Reward reward = Reward::NewRelicReward(Owner());
            reward.UseEventRng();
            return reward;
        }
        case Potion: {
            StaticVec<PotionId, 96> candidates;
            for (PotionId id : PotionFactory::GetPotionOptions(Owner()))
                if (sts2::data(id).rarity == static_cast<PotionRarity>(item.value)) candidates.push_back(id);
            PotionId id = Rng().NextItem(candidates);
            if (id == PotionId::NONE) throw GameError("CrystalSphere: no potion of requested rarity");
            return Reward::NewPotionReward(Owner(), PotionModel(&sim(), alloc_potion(*sim_->g, id, 0)));
        }
        case Card: {
            // The source constructs CardCreationOptions directly, rather than
            // using ForNonCombatWithUniformOdds (which disables upgrade rolls).
            CardCreationOptionsState options = CardCreationOptions::Make(
                {sts2::data(Owner().Character()).card_pool},
                CardCreationSource::Other, CardRarityOddsType::Uniform);
            options.required_rarity = static_cast<CardRarity>(item.value);
            Reward reward = Reward::NewCardReward(Owner(), options, 3);
            reward.UseEventRng();
            return reward;
        }
        case Curse: break;
        }
        return Reward{};
    }

    void FinishMinigame() const {
        StaticVec<Reward, 16> rewards;
        for (uint8_t index : st().crystal_sphere_revealed_order) {
            if (kItems[index].kind == Curse) continue;
            rewards.push_back(ItemReward(index));
        }
        active() = 0;
        RewardsCmd::OfferCustom(Owner(), rewards);
    }

    void PlayMinigame(int count) const {
        InitializeMinigame(count);
        while (divinations() > 0) {
            int answer = sim_->Choose(CellRequest());
            if (answer == kCellCount) {
                tool() = kToolSmall;
                continue;
            }
            if (answer == kCellCount + 1) {
                tool() = kToolBig;
                continue;
            }
            if (answer < 0 || answer >= kCellCount) throw GameError("CrystalSphere: cell answer out of range");
            --divinations();
            RevealAt(answer);
        }
        FinishMinigame();
    }

    void CalculateVars() const {
        DynamicVars()["UncoverFutureCost"].SetBaseValue(
            DynamicVars()["UncoverFutureCost"].BaseValue() + Dec(Rng().NextInt(1, 50)));
    }

    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(CrystalSphere, UncoverFuture), InitialOptionKey("UNCOVER_FUTURE")));
        list.push_back(EventOption(SIM_OPTION(CrystalSphere, PaymentPlan), InitialOptionKey("PAYMENT_PLAN")));
        return list;
    }

    void UncoverFuture(const EventOptionState&) const {
        PlayerCmd::LoseGold(DynamicVars()["UncoverFutureCost"].BaseValue(), Owner(), GoldLossType::Spent);
        PlayMinigame(DynamicVars()["UncoverFutureProphesizeCount"].IntValue());
        SetEventFinished(L10NLookup("CRYSTAL_SPHERE.pages.FINISH.description"));
    }

    void PaymentPlan(const EventOptionState&) const {
        CardPileCmd::AddCurseToDeck(CardId::DEBT, Owner());
        PlayMinigame(DynamicVars()["PaymentPlanCount"].IntValue());
        SetEventFinished(L10NLookup("CRYSTAL_SPHERE.pages.FINISH.description"));
    }

    template <class Fn> static bool all_players(RunState runState, Fn&& fn) {
        for (Player p : runState.Players()) if (!fn(p)) return false;
        return true;
    }

    bool IsAllowed(RunState runState) const {
        return all_players(runState, [](Player p) { return p.Gold() >= 100; }) && runState.CurrentActIndex() > 0;
    }
};

}  // namespace sts2
