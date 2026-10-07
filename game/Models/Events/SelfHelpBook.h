#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

namespace self_help_book_detail {
inline bool attack(CardModel c) { return c.Type() == CardType::Attack && !c.Enchantment(); }
inline bool skill(CardModel c) { return c.Type() == CardType::Skill && c.GainsBlock() && !c.Enchantment(); }
inline bool power(CardModel c) { return c.Type() == CardType::Power && !c.Enchantment(); }
}

struct SelfHelpBook : EventModel {
    SIM_MODEL(SelfHelpBook, EventModel)
    EventOptionList GenerateInitialOptions() const {
        bool hasAttack = false, hasSkill = false, hasPower = false;
        for (CardModel c : Owner().Deck().Cards()) { hasAttack |= self_help_book_detail::attack(c); hasSkill |= self_help_book_detail::skill(c); hasPower |= self_help_book_detail::power(c); }
        EventOptionList list;
        if (hasAttack || hasSkill || hasPower) {
            list.push_back(hasAttack ? EventOption(SIM_OPTION(SelfHelpBook, ReadTheBack), InitialOptionKey("READ_THE_BACK")) : EventOption(0, InitialOptionKey("READ_THE_BACK_LOCKED")));
            list.push_back(hasSkill ? EventOption(SIM_OPTION(SelfHelpBook, ReadPassage), InitialOptionKey("READ_PASSAGE")) : EventOption(0, InitialOptionKey("READ_PASSAGE_LOCKED")));
            list.push_back(hasPower ? EventOption(SIM_OPTION(SelfHelpBook, ReadEntireBook), InitialOptionKey("READ_ENTIRE_BOOK")) : EventOption(0, InitialOptionKey("READ_ENTIRE_BOOK_LOCKED")));
        } else list.push_back(EventOption(SIM_OPTION(SelfHelpBook, SkipBook), InitialOptionKey("NO_OPTIONS")));
        return list;
    }
    void ReadTheBack(const EventOptionState&) const { CardList c = CardSelectCmd::FromDeckForEnchantment(Owner(), CardSelectorPrefs(1), self_help_book_detail::attack); if (!c.empty()) CardCmd::Enchant(EnchantmentId::SHARP, c[0], Dec(2)); SetEventFinished(L10NLookup("SELF_HELP_BOOK.pages.READ_THE_BACK.description")); }
    void ReadPassage(const EventOptionState&) const { CardList c = CardSelectCmd::FromDeckForEnchantment(Owner(), CardSelectorPrefs(1), self_help_book_detail::skill); if (!c.empty()) CardCmd::Enchant(EnchantmentId::NIMBLE, c[0], Dec(2)); SetEventFinished(L10NLookup("SELF_HELP_BOOK.pages.READ_PASSAGE.description")); }
    void ReadEntireBook(const EventOptionState&) const { CardList c = CardSelectCmd::FromDeckForEnchantment(Owner(), CardSelectorPrefs(1), self_help_book_detail::power); if (!c.empty()) CardCmd::Enchant(EnchantmentId::SWIFT, c[0], Dec(2)); SetEventFinished(L10NLookup("SELF_HELP_BOOK.pages.READ_ENTIRE_BOOK.description")); }
    void SkipBook(const EventOptionState&) const { SetEventFinished(L10NLookup("SELF_HELP_BOOK.pages.NO_OPTIONS.description")); }
};

}  // namespace sts2
