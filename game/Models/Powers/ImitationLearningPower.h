#pragma once
#include "game/Commands/CardCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct ImitationLearningPower : PowerModel { SIM_MODEL(ImitationLearningPower, PowerModel)
    void SetPlayerTarget(Player target) const { extra(0) = target.Index(); }
    Player PlayerTarget() const { return extra(0) ? Player(sim_, static_cast<uint8_t>(extra(0))) : Player{}; }
    void BeforeCardPlayed(const CardPlay* play) const {
        if (!play || !PlayerTarget()) throw GameError("ImitationLearningPower applied without a player target");
        if (play->Card.Owner() != PlayerTarget() || play->Card.Type() != CardType::Power || !play->IsFirstInSeries()) return;
        CardModel clone = play->Card.CreateCloneForPlayer(Owner().Player());
        play->Card.st().extra[2] = clone.Index();
    }
    void AfterCardPlayed(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || play->Card.Owner() != PlayerTarget()) return;
        CardRef ref = static_cast<CardRef>(play->Card.st().extra[2]);
        if (!ref) return;
        play->Card.st().extra[2] = 0;
        PowerCmd::Decrement(*this);
        CardCmd::AutoPlay(ctx, CardModel(sim_, ref), Creature{});
    }
};
}
