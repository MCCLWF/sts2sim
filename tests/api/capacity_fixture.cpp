// Test-only caller supplied state. The product ABI does not export this helper.
#include <iostream>
#include "env/env.h"
#include "game/All.h"
#include "game/Runs/RunState.h"

using namespace sts2;
extern "C" const char* sts2_test_capacity_snapshot(int32_t count, int32_t pending) {
    try {
        static std::string payload;
        Env source; new_run(source.state(),{.seed="DYNAMICCAPACITY",.character=CharacterId::IRONCLAD});
        source.state().run.started_with_neow=false;
        source.Step({.kind=Action::StartRun});
        Player player(&source.sim(),1);
        while(player.Deck().Count()<count) {
            auto card=RunState(&source.sim()).CreateCard(CardId::STRIKE_IRONCLAD,player);
            player.Deck().AddInternal(card);
        }
        if(pending) {
            Operation operation; operation.kind=Operation::Kind::ObtainRelic;
            operation.source_hash=source.Hash(); operation.relic_id=static_cast<uint16_t>(RelicId::ASTROLABE);
            source.Execute(operation);
            source.Step(source.LegalActions().front()); // select the original required count
            if(source.decision().kind!=DecisionKind::Choice || source.LegalActions().size()<512)
                throw GameError("large fixture did not reach the card selection");
        }
        payload=source.ExportPortableSnapshotJson();
        return payload.c_str();
    } catch(const std::exception& e) {std::cerr<<"capacity fixture: "<<e.what()<<'\n'; return nullptr;}
      catch(...) {std::cerr<<"capacity fixture: unresolved choice\n"; return nullptr;}
}
