// Entities/RestSite/RestSiteOption.cs and its subclasses (the options a
// single-player run can offer without relics: HealRestSiteOption,
// SmithRestSiteOption), plus the single-player part of
// Multiplayer/Game/RestSiteSynchronizer.cs. The options live in
// RunData::rest_site.
#pragma once

#include <string_view>

#include "game/All.h"

namespace sts2::RestSite {

std::string_view OptionId(RestSiteOptionId option);  // "HEAL", "SMITH", ...
bool IsEnabled(Player player, RestSiteOptionId option);

// RestSiteOption.Generate(player)
StaticVec<RestSiteOptionId, 8> Generate(Player player);

// HealRestSiteOption.GetHealAmount / ExecuteRestSiteHeal
Dec GetHealAmount(Player player);
void ExecuteRestSiteHeal(Player player, bool isMimicked);

// RestSiteSynchronizer
void BeginRestSite(Sim* sim);
bool ChooseOption(Sim* sim, int32_t optionIndex);  // ChooseLocalOption: whether the option succeeded
void BeforeLocalRestSiteExited(Sim* sim);

}  // namespace sts2::RestSite
