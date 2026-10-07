// Commands/RelicCmd.cs (the parts ported so far).
#pragma once

#include "game/All.h"

namespace sts2::RelicCmd {

RelicModel Obtain(RelicModel relic, Player player, int32_t index = -1);
// Obtain<T> / Obtain(canonical): a new instance of `relic`.
RelicModel Obtain(RelicId relic, Player player, int32_t index = -1);
// Remove from the owner's inventory, then run the relic's AfterRemoved hook.
void Remove(RelicModel relic);
// RelicCmd.Replace: remove an existing relic and insert a new instance in its slot.
RelicModel Replace(RelicModel oldRelic, RelicModel replacement);
RelicModel Replace(RelicModel oldRelic, RelicId replacement);

}  // namespace sts2::RelicCmd
