// The NoSL actor as one object a game-side executor can drive: it holds a
// shadow Env of the run being played, and at each call returns the next
// action of that run — the only legal action when there is one, otherwise an
// action drawn from the policy network on the actor's observation.
//
// What this is and is not (docs/plans/2026-10-04-nosl-q5a-live-pilot.md):
//   - The policy sees exactly nosl::Observe + nosl::ActorFilter, the tables it
//     was trained on. No action is tried, no future is rolled out, and the
//     shadow is stepped only with the action that is returned.
//   - The shadow is kept in step with the real game by seed and by the
//     actions themselves, so the process that hosts it holds the run seed.
//     That is a development arrangement for getting the executor working and
//     for checking a game-side encoder against; the `digest` below is how the
//     executor checks, at every decision, that the two have not drifted.
#pragma once

#include <cstdint>
#include <memory>
#include <random>
#include <string>

#include "env/env.h"
#include "nosl/actor.h"
#include "nosl/observation.h"

namespace sts2::nosl {

struct PilotOptions {
    std::string seed;
    CharacterId character = CharacterId::SILENT;
    int32_t ascension = 10;
    int32_t actor_filter = kActorFilterV1;
    std::string weights;        // python -m nosl.export file
    uint64_t policy_seed = 1;   // action sampling
    bool greedy = false;        // take the most probable action instead of sampling
};

class Pilot {
public:
    explicit Pilot(const PilotOptions& options);

    // The next action as one JSON object, and the shadow advanced by it:
    //   {"step":n,"decision":"Combat","forced":false,
    //    "action":{"kind":"PlayCard","index":2,"target":1,"col":0,"row":0},
    //    "choice":{...}            (decision "Choice" only)
    //    "digest":"...",           (public state before the action, see Digest)
    //    "row":3,"rows":7,"prob":0.61,"value":0.12,"ms":4.1}
    // or, once the run is over, {"step":n,"decision":"Ended","victory":bool,"floor":f,"digest":"..."}.
    // Throws when the shadow cannot continue (unported content, an engine error).
    std::string Next();
    // What Next() will decide about, without deciding it or advancing:
    //   {"step":n,"decision":"Choice","choice":{...},"digest":"..."}
    // The executor uses it to know which screen to wait for.
    std::string Peek() const;

    bool ended() const;
    const Env& env() const { return env_; }
    // The shadow's observation at the pending decision, before the actor filter.
    void Observe(Observation& out) const { nosl::Observe(env_, out); }

    // A one-line summary of what both the shadow and the screen show before a
    // decision; the executor builds the same line from the game's objects.
    //   a<act> f<floor> hp<hp>/<max> g<gold> d<deck> r<relics> p[<potion or - per slot>]
    // and in combat:
    //   | b<block> e<energy> t<turn> h[<card[+n]>,...] dr<n> di<n> ex<n> | m[<monster>:<hp>:<block>,...]
    static std::string Digest(const Env& env);

private:
    Env env_;
    Actor actor_;
    Observation obs_;
    int64_t step_ = 0;
};

}  // namespace sts2::nosl
