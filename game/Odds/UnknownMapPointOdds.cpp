// Odds/UnknownMapPointOdds.cs
#include "game/Odds/UnknownMapPointOdds.h"

#include "game/Hooks/Hook.h"
#include "game/Unlocks/UnlockState.h"

namespace sts2::UnknownMapPointOddsOps {

namespace {

// _nonEventOdds / _baseOdds, in dictionary (insertion) order.
constexpr RoomType kTypes[4] = {RoomType::Monster, RoomType::Elite, RoomType::Treasure, RoomType::Shop};
constexpr float kBase[4] = {0.1f, -1.0f, 0.02f, 0.03f};

float& Odds(sts2::UnknownMapPointOdds& o, int i) {
    switch (i) {
    case 0: return o.monster;
    case 1: return o.elite;
    case 2: return o.treasure;
    default: return o.shop;
    }
}

bool Has(uint16_t set, RoomType t) { return (set >> static_cast<unsigned>(t)) & 1u; }

}  // namespace

float EventOdds(const Sim& sim) {
    sts2::UnknownMapPointOdds o = sim.g->run.odds;
    float sum = 0.0f;  // Values.Where(v > 0).Sum() (float Sum accumulates in double)
    double acc = 0.0;
    for (int i = 0; i < 4; ++i) {
        float v = Odds(o, i);
        if (v > 0.0f) acc += static_cast<double>(v);
    }
    sum = static_cast<float>(acc);
    return std::max(0.0f, 1.0f - sum);
}

RoomType Roll(Sim* sim, uint16_t blacklist) {
    // runState.UnlockState.NumberOfRuns == 0 (the tutorial rolls): never.
    static_assert(UnlockState::NumberOfRuns != 0);
    sts2::UnknownMapPointOdds& odds = sim->g->run.odds;
    uint16_t roomTypes = 0;  // _nonEventOdds.Keys.Append(Event).Except(blacklist).ToHashSet()
    for (RoomType t : kTypes) {
        if (!Has(blacklist, t)) roomTypes |= static_cast<uint16_t>(1u << static_cast<unsigned>(t));
    }
    if (!Has(blacklist, RoomType::Event)) roomTypes |= static_cast<uint16_t>(1u << static_cast<unsigned>(RoomType::Event));
    roomTypes = Hook::ModifyUnknownMapPointRoomTypes(RunState(sim), RoomTypeSet{roomTypes}).bits;
    RoomType roomType = RoomType::Unassigned;
    if (Has(roomTypes, RoomType::Event)) {
        roomType = RoomType::Event;
    } else {  // roomTypes.Order().First()
        for (unsigned t = 0; t < 16; ++t) {
            if ((roomTypes >> t) & 1u) {
                roomType = static_cast<RoomType>(t);
                break;
            }
        }
    }
    float num2 = sim->g->run.rng.UnknownMapPoint().NextFloat();
    float num3 = 0.0f;
    for (int i = 0; i < 4; ++i) {
        float num4 = Odds(odds, i);
        if (Has(roomTypes, kTypes[i]) && !(num4 < 0.0f)) {
            num3 += num4;
            if (num2 <= num3) {
                roomType = kTypes[i];
                break;
            }
        }
    }
    for (int i = 0; i < 4; ++i) {
        if (roomType == kTypes[i]) {
            Odds(odds, i) = kBase[i];
        } else if (Has(roomTypes, kTypes[i])) {
            float num6 = Hook::ModifyOddsIncreaseForUnrolledRoomType(RunState(sim), kTypes[i], kBase[i]);
            Odds(odds, i) += num6;
        }
    }
    return roomType;
}

}  // namespace sts2::UnknownMapPointOddsOps
