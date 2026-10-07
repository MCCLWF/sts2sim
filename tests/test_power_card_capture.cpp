#include <stdexcept>
#include <tuple>

#include "env/env.h"
#include "env/c_api.h"
#include "env/snapshot_json.h"
#include "env/portable_registry.h"
#include "engine/json.h"
#include "engine/sim/gc.h"
#include "game/All.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/Cards/MadScience.h"
#include "game/Models/Powers/RupturePower.h"
#include "game/Models/Powers/StormPower.h"
#include "game/Models/Powers/SubroutinePower.h"
#include "game/Models/Powers/MonologuePower.h"

using namespace sts2;

static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

// tools/PowerCaptureChecks observes the shipped v0.111.0 private dictionaries
// and Strength values with TestMode=false. These expected values are native
// observations, not simulator output.
static int captured(Env& env, PowerRef power, CardRef card) {
    for (const auto& item : env.state().combat.active_card_play_power_captures)
        if (item.power == power && item.card == card) return item.amount;
    return -1;
}

static void storm_subroutine() {
    Env env;
    env.ResetCombat({.seed = "POWERCAPTURES", .character = CharacterId::DEFECT}, EncounterId::CULTISTS_NORMAL);
    Sim& sim = env.sim();
    Player owner(&sim, 1);
    CombatState combat(&sim);
    PlayerChoiceContext ctx;
    PowerModel s = PowerCmd::Apply(PowerId::STORM_POWER, ctx, owner.Creature(), Dec(3), owner.Creature(), {});
    PowerModel r = PowerCmd::Apply(PowerId::SUBROUTINE_POWER, ctx, owner.Creature(), Dec(5), owner.Creature(), {});
    StormPower storm(s); SubroutinePower subroutine(r);
    MadScience skill(combat.CreateCard(CardId::MAD_SCIENCE, owner));
    skill.SetTinkerTimeType(CardType::Skill); skill.SetTinkerTimeRider(MadScienceRider::Energized);
    CardPlay sp{skill, owner, owner.Creature()};
    const int energy = owner.PlayerCombatState().Energy();
    const auto orbs = owner.PlayerCombatState().st().orbs;
    storm.BeforeCardPlayed(&sp); subroutine.BeforeCardPlayed(&sp);
    storm.AfterCardPlayed(ctx, &sp); subroutine.AfterCardPlayed(ctx, &sp);
    require(skill.TinkerTimeType() == CardType::Skill && skill.TinkerTimeRider() == MadScienceRider::Energized &&
                captured(env, s.Index(), skill.Index()) == -1 && captured(env, r.Index(), skill.Index()) == -1,
            "skill-preserved: Power captures changed MadScience configuration");
    require(owner.PlayerCombatState().Energy() == energy && owner.PlayerCombatState().st().orbs == orbs,
            "non-Power card triggered Storm or Subroutine");

    MadScience card(combat.CreateCard(CardId::MAD_SCIENCE, owner));
    card.SetTinkerTimeType(CardType::Power); card.SetTinkerTimeRider(MadScienceRider::Expertise);
    CardPlay pp{card, owner, owner.Creature()};
    storm.BeforeCardPlayed(&pp); subroutine.BeforeCardPlayed(&pp);
    storm.SetAmount(7, true); subroutine.SetAmount(9, true);
    require(captured(env, s.Index(), card.Index()) == 3 && captured(env, r.Index(), card.Index()) == 5 &&
                card.TinkerTimeType() == CardType::Power && card.TinkerTimeRider() == MadScienceRider::Expertise,
            "captured-before-amount: dictionaries must preserve separate amounts and card fields");
    bool duplicate = false;
    try { storm.BeforeCardPlayed(&pp); } catch (const GameError&) { duplicate = true; }
    require(duplicate, "duplicate source Dictionary.Add was accepted");
    const std::string snapshot = env.ExportSnapshotJson();
    auto restored = Env::ImportSnapshotJson(snapshot);
    require(restored->ExportSnapshotJson() == snapshot, "Power dictionaries did not round-trip");
    for (Env* current : {&env, restored.get()}) {
        Sim& next = current->sim(); Player player(&next, 1);
        CardModel model(&next, card.Index()); CardPlay play{model, player, player.Creature()};
        PlayerChoiceContext context;
        StormPower(&next, s.Index()).AfterCardPlayed(context, &play);
        SubroutinePower(&next, r.Index()).AfterCardPlayed(context, &play);
        require(player.PlayerCombatState().Energy() == energy + 5, "Subroutine used its later amount");
        require(current->state().combat.active_card_play_power_captures.empty(), "finished power retained capture");
    }
    require(env.Hash() == restored->Hash(), "restored Storm/Subroutine successors differ");
}

static void rupture_buffer() {
    Env env;
    env.ResetCombat({.seed = "RUPTURECAPTURE", .character = CharacterId::IRONCLAD}, EncounterId::CULTISTS_NORMAL);
    Sim& sim = env.sim(); Player owner(&sim, 1); CombatState combat(&sim);
    PlayerChoiceContext ctx;
    PowerModel raw = PowerCmd::Apply(PowerId::RUPTURE_POWER, ctx, owner.Creature(), Dec(2), owner.Creature(), {});
    RupturePower power(raw);
    CardModel a = combat.CreateCard(CardId::HEMOKINESIS, owner), b = combat.CreateCard(CardId::BREAKTHROUGH, owner);
    CardPlay ap{a, owner, owner.Creature()}, bp{b, owner, owner.Creature()};
    power.BeforeCardPlayed(&ap); power.BeforeCardPlayed(&bp);
    DamageResult damage; damage.Receiver = owner.Creature(); damage.UnblockedDamage = 2;
    damage.Props = ValueProp::Move | ValueProp::Unpowered;
    auto receive = [&](CardModel source, const DamageResult& result) {
        power.AfterDamageReceived(ctx, owner.Creature(), result, result.Props, owner.Creature(), source);
    };
    auto check = [&](const char* label, int aa, int bb, int strength) {
        require(captured(env, raw.Index(), a.Index()) == aa && captured(env, raw.Index(), b.Index()) == bb &&
                    owner.Creature().GetPowerAmount(PowerId::STRENGTH_POWER) == strength, label);
    };
    receive(a, damage); check("first-buffered", 2, 0, 0);
    power.SetAmount(3, true); receive(a, damage); receive(b, damage);
    check("nested-distinct-and-current-amount", 5, 3, 0);
    DamageResult blocked = damage; blocked.UnblockedDamage = 0;
    receive(a, blocked); check("blocked-ignored", 5, 3, 0);
    combat.SetCurrentSide(CombatSide::Enemy); receive(a, damage); check("enemy-side-ignored", 5, 3, 0);
    combat.SetCurrentSide(CombatSide::Player); receive({}, damage); check("null-source-immediate", 5, 3, 3);
    const std::string snapshot = env.ExportSnapshotJson(); auto restored = Env::ImportSnapshotJson(snapshot);
    require(restored->ExportSnapshotJson() == snapshot, "Rupture buffers did not round-trip");
    power.AfterCardPlayed(ctx, &ap); check("outer-finish", -1, 3, 8);
    power.AfterCardPlayed(ctx, &bp); power.AfterCardPlayed(ctx, &bp); check("inner-and-duplicate-finish", -1, -1, 11);
    Sim& next = restored->sim(); Player player(&next, 1);
    CardPlay pa{CardModel(&next,a.Index()),player,player.Creature()}, pb{CardModel(&next,b.Index()),player,player.Creature()};
    RupturePower copy(&next,raw.Index()); copy.AfterCardPlayed(ctx,&pa); copy.AfterCardPlayed(ctx,&pb); copy.AfterCardPlayed(ctx,&pb);
    require(env.Hash() == restored->Hash(), "restored Rupture finish differs");
}

static void capture_import_references() {
    Env env;
    env.ResetCombat({.seed="CAPTUREREFS",.character=CharacterId::IRONCLAD},EncounterId::CULTISTS_NORMAL);
    Sim& sim=env.sim();Player player(&sim,1);CombatState combat(&sim);PlayerChoiceContext ctx;
    PowerModel power=PowerCmd::Apply(PowerId::MONOLOGUE_POWER,ctx,player.Creature(),Dec(1),player.Creature(),{});
    CardModel card=combat.CreateCard(CardId::DEFEND_IRONCLAD,player);
    CardPlay play{card,player,player.Creature()};
    // Observe the actual original Before path, not a fabricated capture row.
    MonologuePower(power).BeforeCardPlayed(&play);
    // Dynamic pools have no fixed 192/1024 ceiling. Create one known retired
    // slot so the negative reference case remains inside the current pool
    // extent while still exercising the live-slot validator.
    const uint32_t dead_power = sim.g->powers.alloc();
    sim.g->powers.free(dead_power);
    const uint32_t dead_card = sim.g->cards.alloc();
    sim.g->cards.free(dead_card);
    const auto original=env.ExportSnapshotJson();
    auto local=Env::ImportSnapshotJson(original);
    auto portable=Env::ImportSnapshotJson(env.ExportPortableSnapshotJson());
    require(local->ExportSnapshotJson()==original && portable->ExportSnapshotJson()==original,
            "valid pending Monologue dictionary entry did not survive local/portable restore");

    void* diagnostic=sts2_env_create();
    require(sts2_env_inspect_combat_named(diagnostic,"CAPTUREDIAGNOSTIC","IRONCLAD",0,"CULTISTS_NORMAL",0)==0,
            "public diagnostic source fixture failed");
    auto source_snapshot=[&] {
        require(sts2_env_export_snapshot(diagnostic)==0,"public diagnostic snapshot failed");
        return std::string(sts2_env_last_result(diagnostic));
    };
    const auto source=source_snapshot();
    using namespace snapshot_detail;
    auto encode=[](const Json& value) {JsonWriter w;write_snapshot_json(w,value);return w.str();};
    auto reject=[&](Json document,auto mutate,const char* reason,bool wire=false) {
        Json& logical=wire?document.object.at("logical_state"):document;
        mutate(logical);
        if(wire)document.object.at("wire_hash").text=std::to_string(portable_wire_hash(
            portable_payload(document.at("logical_state"),document.at("registry"))));
        const auto payload=encode(document);void* successor=reinterpret_cast<void*>(1);
        const auto status=sts2_env_import_snapshot(diagnostic,payload.data(),payload.size(),&successor);
        require(status!=0 && successor==nullptr,"public import accepted a corrupt capture reference or pair");
        const std::string detail=sts2_env_last_error(diagnostic);
        if(detail.find(reason)==std::string::npos)
            throw std::runtime_error("capture import expected '"+std::string(reason)+"', got: "+detail);
        require(source_snapshot()==source,"failed capture import changed the complete public source state");
    };
    Json original_json=Parser(original).parse();original_json.object.at("state_hash")=Json{};
    auto captures=[](Json& logical)->Json& {
        return logical.object.at("state").object.at("combat").object.at("active_card_play_power_captures");
    };
    for(auto [field,value,reason]:{
        std::tuple{"power",int64_t{65535},"object reference outside pool"},std::tuple{"card",int64_t{65535},"object reference outside pool"},
        std::tuple{"power",int64_t{0},"null CardPlay Power capture reference"},std::tuple{"card",int64_t{0},"null CardPlay Power capture reference"},
        std::tuple{"power",static_cast<int64_t>(dead_power),"object reference is not live"},std::tuple{"card",static_cast<int64_t>(dead_card),"object reference is not live"}}) {
        reject(original_json,[&](Json& logical){captures(logical).array[0].object.at(field).text=std::to_string(value);},reason);
    }
    auto duplicate=[&](Json& logical){auto& rows=captures(logical).array;rows.push_back(rows[0]);};
    reject(original_json,duplicate,"duplicate CardPlay Power capture pair");
    // Both portable checksums must be valid before reference validation is
    // reached. Export a corrupt clone to calculate its canonical state hash;
    // it never becomes an accepted/imported Env or an original-game fixture.
    auto duplicate_fixture=env.Clone();
    auto& duplicate_rows=duplicate_fixture->sim().g->combat.active_card_play_power_captures;
    duplicate_rows.push_back(duplicate_rows[0]);
    reject(Parser(duplicate_fixture->ExportPortableSnapshotJson()).parse(),[](Json&){},
           "duplicate CardPlay Power capture pair",true);

    // Replay roots receive the same full reference validation. Keep the
    // complete envelope/hash field: the specific FK diagnostic must occur
    // before the whole replay hash mismatch caused by this corruption.
    auto replay_fixture=env.Clone();
    Sim& replay_sim=replay_fixture->sim();Player replay_player(&replay_sim,1);CombatState replay_combat(&replay_sim);
    CardModel discovery=replay_combat.CreateCard(CardId::DISCOVERY,replay_player);CardPileCmd::Add(discovery,PileType::Hand);
    auto choosing=replay_fixture->Predict({.kind=Action::PlayCard,.index=replay_player.PlayerCombatState().Hand().Count()-1});
    require(choosing->decision().kind==DecisionKind::Choice,"capture replay-base fixture did not reach a real Choice");
    reject(Parser(choosing->ExportSnapshotJson()).parse(),[&](Json& logical) {
        auto& rows=logical.object.at("replay_base").object.at("combat").object.at("active_card_play_power_captures").array;
        rows[0].object.at("power").text="65535";
    },"object reference outside pool");
    sts2_env_destroy(diagnostic);

    for(Env* branch:{&env,local.get(),portable.get()}) {
        Sim& next=branch->sim();Player owner(&next,1);PlayerChoiceContext context;
        CardPlay completion{CardModel(&next,card.Index()),owner,owner.Creature()};
        MonologuePower(&next,power.Index()).AfterCardPlayed(context,&completion);
        require(branch->state().combat.active_card_play_power_captures.empty(),"restored capture was not consumed by AfterCardPlayed");
    }
    require(env.ExportSnapshotJson()==local->ExportSnapshotJson() && local->ExportSnapshotJson()==portable->ExportSnapshotJson(),
            "original/local/portable capture successors differ");
}

static void affliction_object_lifetime() {
    Env env;
    env.ResetCombat({.seed="AFFLICTIONOBJECTS",.character=CharacterId::IRONCLAD},EncounterId::CULTISTS_NORMAL);
    Sim& sim=env.sim(); Player player(&sim,1); CombatState combat(&sim);
    CardModel card=combat.CreateCard(CardId::STRIKE_IRONCLAD,player);
    AfflictionModel pending=AfflictionModel::Pending(&sim,AfflictionId::RINGING);
    AfflictionModel second=AfflictionModel::Pending(&sim,AfflictionId::RINGING);
    require(pending && second && pending.Uid()!=0 && pending.Uid()!=second.Uid() && pending.Index()!=second.Index(),
            "pending AfflictionModel instances collapsed to definition identity");
    pending.SetAmount(7);
    require(pending.IsPending() && !pending.HasCard() && pending.Amount()==7,
            "pending AfflictionModel Amount was not independently writable");
    AfflictionModel attached=card.AfflictInternal(pending,Dec(3));
    require(attached.Uid()==pending.Uid() && attached.HasCard() && attached.Card().Index()==card.Index()
                && attached.Amount()==3 && !attached.IsPending(),
            "attaching AfflictionModel changed its object identity or amount");
    CardModel clone=combat.CloneCard(card);
    AfflictionModel cloned=clone.Affliction();
    require(cloned && cloned.HasCard() && cloned.Card().Index()==clone.Index()
                && cloned.Uid()!=attached.Uid() && cloned.Amount()==3,
            "card clone did not deep-clone its AfflictionModel");
    CardModel run_clone=player.RunState().CloneCard(card);
    AfflictionModel run_cloned=run_clone.Affliction();
    require(run_cloned && run_cloned.HasCard() && run_cloned.Card().Index()==run_clone.Index()
                && run_cloned.Uid()!=attached.Uid() && run_cloned.Uid()!=cloned.Uid()
                && run_cloned.Amount()==3,
            "run clone did not deep-clone its AfflictionModel");
    AfflictionModel rejected_affliction=AfflictionModel::Pending(&sim,AfflictionId::TAINTED);
    const auto live_before_rejection=sim.g->afflictions.size();
    require(!CardCmd::Afflict(rejected_affliction, card, Dec(2))
                && sim.g->afflictions.size()+1==live_before_rejection,
            "rejected AfflictionModel operation leaked its pending object");
    card.ClearAfflictionInternal();
    require(!card.Affliction() && !attached.HasCard() && attached.Amount()==3,
            "clearing a card destroyed the detached AfflictionModel too early");
    const std::string snapshot=env.ExportSnapshotJson();
    auto restored=Env::ImportSnapshotJson(snapshot);
    require(restored->ExportSnapshotJson()==snapshot,"AfflictionModel identities did not round-trip");
    require(restored->state().cards[clone.Index()].affliction_instance!=0,
            "attached clone AfflictionModel was lost on snapshot restore");
    const AfflictionRef detached_ref = static_cast<AfflictionRef>(attached.Index() & ~AfflictionModel::kHandle);
    const AfflictionRef cloned_ref = static_cast<AfflictionRef>(cloned.Index() & ~AfflictionModel::kHandle);
    sim.g->run.player.deck.push_back(clone.Index());
    sim.g->run.player.deck.push_back(run_clone.Index());
    env.state().combat.exists=false;
    CollectGarbage(*sim.g);
    require(!sim.g->afflictions.live(detached_ref) && sim.g->afflictions.live(cloned_ref),
            "detached AfflictionModel was not reclaimed without touching a live clone");
}

int main() { storm_subroutine(); rupture_buffer(); capture_import_references(); affliction_object_lifetime(); }
