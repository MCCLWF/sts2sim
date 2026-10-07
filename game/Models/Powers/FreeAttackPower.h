#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct FreeAttackPower : PowerModel { SIM_MODEL(FreeAttackPower, PowerModel)
bool TryModifyEnergyCostInCombatLate(CardModel card, Dec, Dec& modified) const { if(card.Owner().Creature()!=Owner()||card.Type()!=CardType::Attack) return false; CardPile p=card.Pile(); if(!p||(p.Type()!=PileType::Hand&&p.Type()!=PileType::Play)) return false; modified=Dec(0); return true; }
void BeforeCardPlayed(const CardPlay* play) const { if(play&&play->Card.Owner().Creature()==Owner()&&play->Card.Type()==CardType::Attack){ CardPile p=play->Card.Pile(); if(p&&(p.Type()==PileType::Hand||p.Type()==PileType::Play)) PowerCmd::Decrement(*this); } }
}; }
