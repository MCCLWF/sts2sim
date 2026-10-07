// Models/Monsters/ThievingHopper.cs
#pragma once

#include "engine/sim/alloc.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/Models/Powers/SwipePower.h"

namespace sts2 {
struct ThievingHopper : MonsterModel {
    SIM_MODEL(ThievingHopper, MonsterModel)
    SIM_NOT_PORTED(GenerateAnimator);

    bool IsHovering() const { return extra(0) != 0; }
    void SetIsHovering(bool value) const { extra(0) = value; }
    int32_t TheftDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 19, 17); }
    int32_t HatTrickDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 23, 21); }
    int32_t NabDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 16, 14); }

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::ESCAPE_ARTIST_POWER, ctx, Creature(), Dec(5), Creature(), CardModel{});
    }
    void BeforeRemovedFromRoom() const {}  // Source stops a sound loop.

    static int32_t StealPriority(CardModel card) {
        bool imbued = card.st().enchantment == EnchantmentId::IMBUED;
        if (!imbued && card.Rarity() == CardRarity::Uncommon) return 0;
        if (!imbued && (card.Rarity() == CardRarity::Common || card.Rarity() == CardRarity::Rare || card.Rarity() == CardRarity::Event)) return 1;
        if (!imbued && (card.Rarity() == CardRarity::Basic || card.Rarity() == CardRarity::Quest)) return 2;
        if (card.Rarity() == CardRarity::Ancient || imbued) return 3;
        return 4;
    }
    void ThieveryMove(const CreatureList& targets) const {
        CardList cardsToSteal;
        for (auto target : targets) {
            if (target.IsDead()) continue;
            Player player = target.Player() ? target.Player() : target.PetOwner();
            CardList candidates;
            for (auto card : CardPile::GetCards(player, {PileType::Draw, PileType::Discard})) {
                if (card.DeckVersion()) candidates.push_back(card);
            }
            CardList eligible;
            for (int32_t priority = 0; priority < 4; ++priority) {
                eligible.clear();
                for (auto card : candidates) if (StealPriority(card) == priority) eligible.push_back(card);
                if (!eligible.empty()) break;
            }
            if (eligible.empty()) eligible = candidates;
            if (!eligible.empty()) {
                CardModel card = eligible[static_cast<size_t>(RunRng().CombatCardGeneration().NextInt(static_cast<int32_t>(eligible.size())))];
                CardPileCmd::RemoveFromCombat(card);
                cardsToSteal.push_back(card);
            }
        }
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        for (auto card : cardsToSteal) {
            SwipePower swipe(sim_, alloc_power(*sim_->g, PowerId::SWIPE_POWER));
            swipe.Steal(card);
            PowerCmd::Apply(ctx, swipe, Creature(), Dec(1), Creature(), CardModel{});
        }
        DamageCmd::Attack(TheftDamage()).FromMonster(*this).Execute(nullptr);
    }
    void NabMove(const CreatureList&) const { DamageCmd::Attack(NabDamage()).FromMonster(*this).Execute(nullptr); }
    void HatTrickMove(const CreatureList&) const { DamageCmd::Attack(HatTrickDamage()).FromMonster(*this).Execute(nullptr); }
    void FlutterMove(const CreatureList&) const {
        SetIsHovering(true);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::FLUTTER_POWER, ctx, Creature(), Dec(5), Creature(), CardModel{});
    }
    void EscapeMove(const CreatureList&) const {
        if (IsHovering()) SetIsHovering(false);
        CreatureCmd::Escape(Creature());
    }
    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        auto theft = b.Move("THIEVERY_MOVE", SIM_MOVE(ThievingHopper, ThieveryMove), SingleAttackIntent(TheftDamage()), CardDebuffIntent());
        auto nab = b.Move("NAB_MOVE", SIM_MOVE(ThievingHopper, NabMove), SingleAttackIntent(NabDamage()));
        auto hat = b.Move("HAT_TRICK_MOVE", SIM_MOVE(ThievingHopper, HatTrickMove), SingleAttackIntent(HatTrickDamage()));
        auto flutter = b.Move("FLUTTER_MOVE", SIM_MOVE(ThievingHopper, FlutterMove), BuffIntent());
        auto escape = b.Move("ESCAPE_MOVE", SIM_MOVE(ThievingHopper, EscapeMove), EscapeIntent());
        theft.SetFollowUpState(flutter);
        flutter.SetFollowUpState(hat);
        hat.SetFollowUpState(nab);
        nab.SetFollowUpState(escape);
        escape.SetFollowUpState(escape);
        b.Add(theft); b.Add(nab); b.Add(hat); b.Add(flutter); b.Add(escape);
        return b.Build(theft);
    }
};
}  // namespace sts2
