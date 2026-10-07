// Exceptions the simulator throws. Every one of them means "stop": either the
// simulation left what the port covers (Unported, CapacityError), the game's
// own invariant broke (GameError), or the agent has to decide (NeedChoice).
#pragma once

#include <stdexcept>
#include <string>

namespace sts2 {

// A check the C# code performs (InvalidOperationException, ArgumentException,
// ...) failed. The simulation has diverged from the game.
struct GameError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// An original count/reference range or a configured resource budget was
// exceeded. Never truncate a successful state.
struct CapacityError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Execution reached a model member the C# class overrides but the C++ port
// does not implement yet.
struct Unported : std::runtime_error {
    using std::runtime_error::runtime_error;
};

[[noreturn]] inline void game_error(const std::string& what) { throw GameError(what); }

// A bounded observer reached its configured capacity. Pure previews fail
// without publishing a partial successor or modifying the original state.
struct TraceLimitExceeded : GameError { using GameError::GameError; };

// API validation and a feature outside the declared operation contract are
// distinct from a game invariant failing during execution.
struct InvalidRequest : GameError { using GameError::GameError; };
struct UnsupportedOperation : GameError { using GameError::GameError; };

}  // namespace sts2
