// Independent consumer of the C++ model methods. Compare with the shipped-DLL
// NumericExtremeChecks tool; its output supplies the expected values.
#include <iostream>
#include "engine/json.h"
#include "engine/sim/alloc.h"
#include "env/env.h"
#include "game/All.h"
#include "game/Combat/History/CombatHistory.h"
#include "game/Commands/CardCmd.h"
#include "game/Models/Powers/DampenPower.h"
#include "game/Models/Powers/PoisonPower.h"

using namespace sts2;
int main() {
    try {
        Env root; root.ResetCombat({.seed="NUMERICEXTREMES",.character=CharacterId::IRONCLAD},EncounterId::CULTISTS_NORMAL);
        Player player(&root.sim(),1);
        auto card = [&] { return CardModel(&root.sim(),alloc_card(root.state(),CardId::STRIKE_IRONCLAD,0)); };
        PowerModel power(&root.sim(),alloc_power(root.state(),PowerId::STRENGTH_POWER));
        power.ApplyInternal(player.Creature(),Dec(1),true);
        const int32_t values[]={INT32_MIN,INT32_MIN+1,-32769,-32768,-1,0,1,32767,32768,65535,65536,999999999,INT32_MAX-1,INT32_MAX};
        JsonWriter w; w.begin_object().field("game_version","v0.111.0").field("test_mode",false).key("rows").begin_array();
        for(int32_t v:values) for(int count:{1,2,3}) for(bool overkill:{false,true}) {
            StaticVec<DamageResult,4> hit;
            for(int i=0;i<count;++i) {DamageResult result;result.UnblockedDamage=v;result.OverkillDamage=v;hit.push_back(result);}
            StaticVec<StaticVec<DamageResult,4>,1> hits;hits.push_back(hit);
            bool threw=false;int total=0;
            try {total=SumDamageResults(hits,overkill);} catch(const GameError&) {threw=true;}
            w.begin_object().field("group","damage_sum").field("v",v).field("count",count).field("overkill",overkill).field("threw",threw).field("total",total).end_object();
        }
        {
            auto c=card();
            for(int i=0;i<64;++i) {c.EnergyCost().SetThisTurn(i);c.EnergyCost().AddThisCombat(1);c.SetStarCostThisTurn(1);}
            auto copy=RunState(&root.sim()).CloneCard(c); copy.EnergyCost().EndOfTurnCleanup();copy.EndOfTurnCleanup();
            w.begin_object().field("group","modifier_lists").field("energy_count",static_cast<int>(c.st().cost_modifiers.size())).field("star_count",static_cast<int>(c.st().temporary_star_costs.size()))
                .field("cost",c.EnergyCost().GetWithModifiers(CostModifiers::Local)).field("copy_count",static_cast<int>(copy.st().cost_modifiers.size()))
                .field("copy_cost",copy.EnergyCost().GetWithModifiers(CostModifiers::Local)).field("copy_star",copy.CurrentStarCost()).end_object();
        }
        {
            StaticVec<Creature,3> opponents;
            for(int i=0;i<3;++i) {
                auto opponent=CombatState(&root.sim()).CreateCreature(MonsterModel::ToMutable(&root.sim(),MonsterId::DAMP_CULTIST),CombatSide::Enemy,-1);
                CombatState(&root.sim()).AddCreature(opponent);opponents.push_back(opponent);
            }
            PowerModel accelerants[3];
            for(int i=0;i<3;++i) {accelerants[i]=PowerModel(&root.sim(),alloc_power(root.state(),PowerId::ACCELERANT_POWER));accelerants[i].ApplyInternal(opponents[i],Dec(1),true);}
            PoisonPower poison(&root.sim(),alloc_power(root.state(),PowerId::POISON_POWER));poison.ApplyInternal(player.Creature(),Dec(999999999),true);
            for(int32_t v:values) for(int count:{1,2,3}) {
                for(int i=0;i<3;++i) {opponents[i].st().current_hp=i<count?1:0;accelerants[i].SetAmount(v,true);}
                bool threw=false;int total=0;
                try {total=poison.TriggerCount();} catch(const GameError&) {threw=true;}
                w.begin_object().field("group","poison_sum").field("v",v).field("count",count).field("threw",threw).field("total",total).end_object();
            }
            for(auto opponent:opponents)CombatState(&root.sim()).RemoveCreature(opponent);
            poison.RemoveInternal();
        }
        for(int32_t v:values) {
            CardModel x(&root.sim(),alloc_card(root.state(),CardId::WHIRLWIND,0)); x.EnergyCost().SetCapturedXValue(v);
            auto c=card(); c.st().last_stars_spent=v; c.SetBaseReplayCount(v); c.SetStarCostThisCombat(v);
            c.st().enchantment=EnchantmentId::GOOPY; c.st().enchantment_amount=v;
            auto affliction=AfflictionModel::Pending(&root.sim(),AfflictionId::BOUND); affliction.SetAmount(v);
            power.SetAmount(v,true); auto upgraded=card(); upgraded.EnergyCost().UpgradeBy(v);
            w.begin_object().field("group","fields").field("v",v).field("x",x.EnergyCost().CapturedXValue())
                .field("stars",c.st().last_stars_spent).field("replay",c.BaseReplayCount()).field("star_cost",c.CurrentStarCost())
                .field("enchantment",c.Enchantment().Amount()).field("affliction",affliction.Amount()).field("power",power.Amount())
                .field("upgrade_cost",upgraded.EnergyCost().GetWithModifiers(CostModifiers::Local)).end_object();
        }
        for(int32_t a:values) for(int32_t b:values) for(bool reduce:{false,true}) {
            auto c=card(); c.EnergyCost().SetThisCombat(a); c.EnergyCost().AddThisCombat(b,reduce);
            w.begin_object().field("group","cost").field("a",a).field("b",b).field("reduce",reduce)
                .field("cost",c.EnergyCost().GetWithModifiers(CostModifiers::Local)).end_object();
            auto mixed=card(); mixed.EnergyCost().AddThisTurn(a); mixed.EnergyCost().AddThisCombat(b,reduce);
            int before=mixed.EnergyCost().GetWithModifiers(CostModifiers::Local); mixed.EnergyCost().EndOfTurnCleanup();
            w.begin_object().field("group","expiry").field("a",a).field("b",b).field("reduce",reduce).field("before",before)
                .field("after",mixed.EnergyCost().GetWithModifiers(CostModifiers::Local)).end_object();
            auto relative=card(); relative.EnergyCost().AddThisCombat(a); relative.EnergyCost().AddThisCombat(b,reduce);
            w.begin_object().field("group","relative").field("a",a).field("b",b).field("reduce",reduce)
                .field("cost",relative.EnergyCost().GetWithModifiers(CostModifiers::Local)).end_object();
        }
        for(bool stars:{false,true}) for(int count:{1,2,3}) {
            root.state().combat.player_energy_spent_this_turn=0; root.state().combat.player_stars_gained_this_turn=0;
            auto c=CombatState(&root.sim()).CreateCard(stars?CardId::RADIATE:CardId::HELIX_DRILL,player);
            player.PlayerCombatState().DrawPile().AddInternal(c);
            CombatHistory history(&root.sim());
            for(int i=0;i<count;++i) {
                if(stars) history.StarsModified(CombatState(&root.sim()),999999999,player);
                else history.EnergySpent(CombatState(&root.sim()),999999999,player);
            }
            bool threw=false; int result=0;
            try {result=c.DynamicVars()["CalculatedHits"].Calculate({}).to_int();} catch(const GameError&) {threw=true;}
            w.begin_object().field("group","history_sum").field("stars",stars).field("count",count).field("threw",threw).field("result",result).end_object();
            c.RemoveFromCurrentPile();
        }
        {
            auto original=CombatState(&root.sim()).CreateCard(CardId::STRIKE_IRONCLAD,player); CardCmd::Upgrade(original);
            player.PlayerCombatState().DrawPile().AddInternal(original);
            DampenPower dampen(&root.sim(),alloc_power(root.state(),PowerId::DAMPEN_POWER)); dampen.ApplyInternal(player.Creature(),Dec(1),true);
            dampen.AfterApplied({},{});
            auto clone=CombatState(&root.sim()).CloneCard(original); original.RemoveFromCurrentPile();
            StaticVec<CreatureRef,4> casters;
            for(int i=0;i<40;++i) {
                auto c=MonsterModel::ToMutable(&root.sim(),MonsterId::DAMP_CULTIST).Creature();
                casters.push_back(c.Index()); dampen.AddCaster(c); dampen.AddCaster(c);
            }
            PlayerChoiceContext context;
            for(int i=0;i<39;++i) dampen.AfterDeath(context,Creature(&root.sim(),casters[i]),false,0);
            dampen.AfterRemoved(player.Creature());
            w.begin_object().field("group","dampen").field("casters",static_cast<int32_t>(dampen.st().dampen_casters.size()))
                .field("original",original.CurrentUpgradeLevel()).field("clone",clone.CurrentUpgradeLevel())
                .field("remembered",static_cast<int32_t>(dampen.st().dampen_downgraded_cards.size())).end_object();
        }
        struct PotionCase {bool add; int32_t amount; bool full;};
        const PotionCase cases[]={{true,37,false},{true,-2,false},{false,-2,false},{false,2,false},
            {true,INT32_MAX,false},{false,INT32_MIN,false},{false,2,true},{false,4,false}};
        for(auto item:cases) {
            Env e; new_run(e.state(),{.seed="NUMERICPOTIONS",.character=CharacterId::IRONCLAD}); Player p(&e.sim(),1);
            if(item.full) for(int i=0;i<p.MaxPotionCount();++i) p.AddPotionInternal(PotionModel(&e.sim(),alloc_potion(e.state(),PotionId::BLOCK_POTION,1)),i,true);
            bool threw=false;
            try {if(item.add) p.AddToMaxPotionCount(item.amount); else p.SubtractFromMaxPotionCount(item.amount);}
            catch(const GameError&) {threw=true;}
            w.begin_object().field("group","potions").field("add",item.add).field("amount",item.amount).field("full",item.full)
                .field("threw",threw).field("count",p.MaxPotionCount()).key("occupied").begin_array();
            for(auto potion:p.PotionSlots()) w.value(bool(potion));
            w.end_array().end_object();
        }
        w.end_array().end_object(); std::cout<<w.str()<<'\n'; return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n'; return 1;}
}
