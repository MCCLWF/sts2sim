// ValueProps/ValuePropExtensions.cs
#pragma once

#include "content/generated/enums.h"

namespace sts2 {

inline bool IsPoweredAttack(ValueProp props) {
    if (has_flag(props, ValueProp::Move)) return !has_flag(props, ValueProp::Unpowered);
    return false;
}

inline bool IsPoweredCardOrMonsterMoveBlock(ValueProp props) {
    if (has_flag(props, ValueProp::Move)) return !has_flag(props, ValueProp::Unpowered);
    return false;
}

inline bool IsCardOrMonsterMove(ValueProp props) { return has_flag(props, ValueProp::Move); }

}  // namespace sts2
