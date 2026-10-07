// Differential protocol driver, with no duplicated policy decisions.
#include "search/combatsolver/policy.h"
#include "search/combatsolver/weights.generated.h"

#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace sts2::combatsolver;
namespace {
struct Input {
    std::istringstream stream;
    explicit Input(const std::string& line) : stream(line) {}
    template<class T> T Read() {
        T value;
        if (!(stream >> value)) throw std::runtime_error("invalid policy probe input");
        return value;
    }
    bool Bool() { return Read<int>() != 0; }
    std::optional<int32_t> NullableInt() {
        const auto text = Read<std::string>();
        return text == "n" ? std::nullopt : std::optional<int32_t>(std::stoi(text));
    }
    std::optional<SolverTheftPolicy> Theft() {
        const int value = Read<int>();
        return value < 0 ? std::nullopt : std::optional(static_cast<SolverTheftPolicy>(value));
    }
    double Double() { return std::stod(Read<std::string>()); }
    RouteQuality Quality() {
        RouteQuality q;
        q.Won = Bool(); q.Survives = Bool(); q.DeathSaveUseCount = Read<int32_t>();
        q.OutstandingStolenResource = Read<int32_t>(); q.StrategicHpDeficit = Read<int32_t>();
        q.GrowthHpCredit = Read<int32_t>(); q.GrowthRewardCount = Read<int32_t>();
        q.CombatEndedTurn = NullableInt(); q.ProjectedBattleHpLost = Read<int32_t>();
        q.PotionStrategicCost = Read<int32_t>(); q.ProjectedBattlePotionCount = Read<int32_t>();
        q.EnemyHp = Read<int32_t>(); q.Score = Double(); q.TheftPolicy = Theft();
        q.RetentionHealthRisk = Read<int64_t>();
        return q;
    }
    TranspositionLabel Label() {
        const auto count = Read<int32_t>(), cost = Read<int32_t>(), sold = Read<int32_t>();
        const auto lost = Read<int32_t>(), actions = Read<int32_t>();
        return {count, cost, sold, lost, actions, Double()};
    }
    void Done() {
        std::string extra;
        if (stream >> extra) throw std::runtime_error("trailing policy probe input");
    }
};
void Output(std::ostream& stream) { stream << '\n'; }
template<class T, class... Ts> void Output(std::ostream& stream, T first, Ts... rest) {
    stream << first;
    ((stream << ' ' << rest), ...);
    Output(stream);
}
}  // namespace

int main() {
    std::unique_ptr<TranspositionFrontier> frontier;
    std::string line;
    size_t lineNumber = 0;
    while (std::getline(std::cin, line)) {
        ++lineNumber;
        if (line.empty()) continue;
        Input in(line);
        std::ostringstream out;
        try {
            const auto op = in.Read<std::string>();
            if (op == "R") {
                const auto projection = static_cast<RouteQualityProjection>(in.Read<int>());
                const auto policy = in.Theft();
                const auto candidate = in.Quality(), current = in.Quality();
                Output(out, RouteQualityPolicy::Compare(candidate, current, projection, policy));
            } else if (op == "I") {
                const auto candidate = in.Quality(), current = in.Quality();
                Output(out, SolverInterimResultOrdering::IsBetter(candidate, current),
                    SolverInterimResultOrdering::CanPromoteDisplayedResult(candidate, current));
            } else if (op == "V") {
                const int count = in.Read<int>();
                const bool allDead = in.Bool(), playerDead = in.Bool();
                Output(out, SolverInterimResultOrdering::IsCompleteVictory(count, allDead, playerDead, in.Read<int>()));
            } else if (op == "D") {
                const int cd = in.Read<int>(), cc = in.Read<int>(), rd = in.Read<int>(), rc = in.Read<int>();
                Output(out, SolverInterimResultOrdering::IsResourceTradeImprovement(cd, cc, rd, rc));
            } else if (op == "H") {
                PostCombatRelicHealProfile p{in.Read<int>(), in.Read<int>(), in.Read<int>()};
                const int hp = in.Read<int>(), maxHp = in.Read<int>();
                const bool victory = in.Bool();
                Output(out, p.HasAnyHeal(), p.HealFor(hp, maxHp), p.MonotoneHealFor(hp, maxHp),
                    ActEndingBossPolicy::RankedPostCombatRelicHeal(p, victory, hp, maxHp));
            } else if (op == "A") {
                const auto relief = static_cast<BossHpRelief>(in.Read<int>());
                const auto act = static_cast<BossHpStrategy>(in.Read<int>());
                const auto final = static_cast<BossHpStrategy>(in.Read<int>());
                const int value = in.Read<int>(), lost = in.Read<int>(), deficit = in.Read<int>();
                const int recovered = in.Read<int>(), death = in.Read<int>();
                Output(out, static_cast<int>(ActEndingBossPolicy::ResolveStrategicHpRelief(relief, act, final)),
                    ActEndingBossPolicy::RawHpRequiredForPersistentValue(value, relief),
                    ActEndingBossPolicy::PersistentValueOfRecoveredHp(recovered, relief),
                    ActEndingBossPolicy::StrategicHpDeficit(lost, deficit, recovered, relief, death),
                    ActEndingBossPolicy::DeathSavePremium(death), ActEndingBossPolicy::DeathSaveBeamCost(death));
            } else if (op == "P") {
                const auto policy = static_cast<SolverPotionPolicy>(in.Read<int>());
                const int count = in.Read<int>(), automatic = in.Read<int>(), cost = in.Read<int>();
                const int credit = in.Read<int>(), freeDeficit = in.Read<int>(), deficit = in.Read<int>();
                const bool freeWon = in.Bool(), anyWon = in.Bool(), won = in.Bool(), hasBaseline = in.Bool();
                const int ambergris = in.Read<int>(), maxHp = in.Read<int>(), freeHp = in.Read<int>(), hp = in.Read<int>();
                const auto relief = static_cast<BossHpRelief>(in.Read<int>());
                Output(out, PotionUsePolicy::RequiredHpSaved(count), PotionUsePolicy::ExplicitUseCount(count, automatic),
                    PotionUsePolicy::AdditionalRequiredUseStrategicHpCost(cost),
                    PotionUsePolicy::ApplyReplacementCredit(cost, count, credit), PotionUsePolicy::HpSaved(freeDeficit, deficit),
                    PotionUsePolicy::SmartRequiredHpSaved(cost, relief), PotionUsePolicy::AmbergrisRequiredHpSaved(maxHp),
                    PotionUsePolicy::EffectiveStrategicHpCost(cost, ambergris, maxHp),
                    PotionUsePolicy::MeetsAmbergrisRestriction(hasBaseline, ambergris, cost, maxHp, freeHp, hp),
                    PotionUsePolicy::IsEligible(policy, count, cost, freeWon, freeDeficit, anyWon, won, deficit));
            } else if (op == "B") {
                SearchBudgetWindow window{in.Read<int>(), in.Read<int64_t>()};
                const int minimum = in.Read<int>(), nodes = in.Read<int>(), ms = in.Read<int>(), reserve = in.Read<int>();
                auto profile = SolverSearchProfile{};
                profile.SecondRankBand = true; profile.BaseScoreOnly = true;
                const auto limited = window.Limit(profile, nodes, ms, reserve);
                Output(out, window.CanStart(minimum), limited.BeamWidth, limited.MaxExpandedNodes,
                    limited.MaxCardBranchesPerNode, limited.MaxPileChoiceBranchesPerAction,
                    limited.MaxHandChoiceBranchesPerAction, limited.SoftTimeBudgetMilliseconds,
                    limited.SecondRankBand, limited.BaseScoreOnly, limited.ReallocatedRefinementPortfolio,
                    limited.StopPortfolioAtHpTarget);
            } else if (op == "T0") {
                frontier = std::make_unique<TranspositionFrontier>(in.Label());
                Output(out, frontier->LabelCount());
            } else if (op == "T") {
                if (!frontier) throw std::runtime_error("frontier was not initialized");
                const bool accepted = frontier->TryAccept(in.Label());
                Output(out, accepted, frontier->LabelCount());
            } else {
                throw std::runtime_error("unknown policy probe operation");
            }
            in.Done();
            std::cout << out.str();
        } catch (const std::overflow_error&) {
            in.Done(); std::cout << "E_OVERFLOW\n";
        } catch (const std::invalid_argument&) {
            in.Done(); std::cout << "E_ARGUMENT\n";
        } catch (const std::exception& error) {
            std::cerr << "policy probe line " << lineNumber << ": " << error.what() << '\n';
            return 2;
        }
    }
    return 0;
}
