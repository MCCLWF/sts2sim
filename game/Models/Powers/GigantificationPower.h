#pragma once

#include "game/Commands/Builders/AttackCommand.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct GigantificationPower : PowerModel {
    SIM_MODEL(GigantificationPower, PowerModel)

    bool Active() const { return extra(0) != 0; }
    void SetActive(bool value) const { extra(0) = value ? 1 : 0; }
    uint32_t CardIndex() const { return static_cast<uint32_t>(extra(1)); }
    void SetCardIndex(uint32_t value) const { extra(1) = static_cast<int32_t>(value); }

    void BeforeAttack(AttackCommand& command) const {
        AbstractModel source = command.ModelSource();
        if (Active() || !source || source.Kind() != ModelKind::Card) return;
        CardModel card(&sim(), source.Index());
        if (card.Owner().Creature() != Owner() || card.Type() != CardType::Attack || !IsPoweredAttack(command.DamageProps())) return;
        SetActive(true);
        SetCardIndex(card.Index());
        command.SetGigantificationCapture(Index());
    }

    Dec ModifyDamageMultiplicative(Creature target, Dec amount, ValueProp props, Creature dealer,
                                   CardModel cardSource, const CardPlay*) const {
        if (!cardSource || cardSource.Owner().Creature() != Owner() || !IsPoweredAttack(props)) return Dec(1);
        if (Active() && cardSource.Index() != CardIndex()) return Dec(1);
        return Dec(3);
    }

    void AfterAttack(PlayerChoiceContext&, AttackCommand& command) const {
        if (!Active() || command.GigantificationCapture() != Index()) return;
        command.SetGigantificationCapture(0);
        SetActive(false);
        SetCardIndex(0);
        PowerCmd::Decrement(*this);
    }
};

}  // namespace sts2
