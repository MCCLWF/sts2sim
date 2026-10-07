#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Entities/Cards/CardPlay.h"
#include "game/Entities/Creatures/DamageResult.h"
#include "game/Models/Monsters/LouseProgenitor.h"
#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct CurlUpPower : PowerModel {
    SIM_MODEL(CurlUpPower, PowerModel)

    void AfterDamageReceived(PlayerChoiceContext&, Creature target, const DamageResult&, ValueProp props, Creature,
                             CardModel cardSource) const {
        if (target != Owner() || !IsPoweredAttack(props) || !cardSource) return;
        if (extra(0) != 0 && extra(0) != cardSource.Index()) return;
        extra(0) = cardSource.Index();
    }

    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* cardPlay) const {
        if (cardPlay == nullptr || extra(0) == 0) return;
        CardModel playedCard(sim_, static_cast<CardRef>(extra(0)));
        if (cardPlay->Card != playedCard) return;

        extra(0) = 0;
        CreatureCmd::GainBlock(Owner(), Dec(Amount()), ValueProp::Unpowered, nullptr);
        if (Owner().Monster() && Owner().Monster().MonsterId_() == MonsterId::LOUSE_PROGENITOR) {
            LouseProgenitor(Owner().Monster()).SetCurled(true);
        }
        PowerCmd::Remove(*this);
    }
};

}  // namespace sts2
