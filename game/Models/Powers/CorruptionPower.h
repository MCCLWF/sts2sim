#pragma once
#include "game/Models/PowerModel.h"
namespace sts2 {
struct CorruptionPower : PowerModel { SIM_MODEL(CorruptionPower, PowerModel)
bool TryModifyEnergyCostInCombatLate(CardModel card, Dec, Dec& modified) const { if(card.Owner().Creature()!=Owner()||card.Type()!=CardType::Skill) return false; modified=Dec(0); return true; }
CardLocation ModifyCardPlayResultLocation(CardModel card, bool, const ResourceInfo&, CardLocation loc) const { if(card.Owner().Creature()==Owner()&&card.Type()==CardType::Skill) loc.pileType=PileType::Exhaust; return loc; }
void AfterModifyingCardPlayResultLocation(CardModel, CardLocation) const { Flash(); }
}; }
