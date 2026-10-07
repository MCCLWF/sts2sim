using System.Globalization;

namespace CombatSolver;

// This partial declaration can call the upstream private frontier directly.
// All calculations below delegate to the compiled upstream production files.
internal sealed partial class CombatBeamSolver
{
    private sealed class Input(string line)
    {
        private readonly string[] _tokens = line.Split(' ', StringSplitOptions.RemoveEmptyEntries);
        private int _next;
        public string Text() => _tokens[_next++];
        public int Int() => int.Parse(Text(), CultureInfo.InvariantCulture);
        public long Long() => long.Parse(Text(), CultureInfo.InvariantCulture);
        public bool Bool() => Int() != 0;
        public double Double() => double.Parse(Text(), CultureInfo.InvariantCulture);
        public int? NullableInt() { string text = Text(); return text == "n" ? null : int.Parse(text, CultureInfo.InvariantCulture); }
        public SolverTheftPolicy? Theft() { int value = Int(); return value < 0 ? null : (SolverTheftPolicy)value; }
        public RouteQuality Quality() => new(Bool(), Bool(), Int(), Int(), Int(), Int(), Int(),
            NullableInt(), Int(), Int(), Int(), Int(), Double(), Theft()) { RetentionHealthRisk = Long() };
        public TranspositionLabel Label() => new(Int(), Int(), Int(), Int(), Int(), Double());
        public void Done() { if (_next != _tokens.Length) throw new InvalidDataException("trailing oracle input"); }
    }

    private static SolverInterimResult Interim(RouteQuality q) => new(q.Won, q.OutstandingStolenResource,
        q.ProjectedBattleHpLost, q.StrategicHpDeficit, q.PotionStrategicCost,
        q.ProjectedBattlePotionCount, q.EnemyHp, q.Score, q.CombatEndedTurn) {
            Survives = q.Survives, DeathSaveUseCount = q.DeathSaveUseCount,
            GrowthHpCredit = q.GrowthHpCredit, GrowthRewardCount = q.GrowthRewardCount, TheftPolicy = q.TheftPolicy,
        };
    private static string Output(params object[] values) => string.Join(' ', values.Select(value => value switch {
        bool boolean => boolean ? "1" : "0",
        IFormattable formatted => formatted.ToString(null, CultureInfo.InvariantCulture),
        _ => throw new InvalidDataException("unexpected output type"),
    }));

    public static void Main()
    {
        TranspositionFrontier? frontier = null;
        string? line;
        while ((line = Console.ReadLine()) != null)
        {
            if (line.Length == 0) continue;
            Input input = new(line);
            string result;
            try {
                switch (input.Text()) {
                    case "R": {
                        var projection = (RouteQualityProjection)input.Int();
                        var policy = input.Theft();
                        var candidate = input.Quality(); var current = input.Quality();
                        result = Output(RouteQualityPolicy.Compare(candidate, current, projection, policy)); break;
                    }
                    case "I": {
                        var candidate = Interim(input.Quality()); var current = Interim(input.Quality());
                        result = Output(SolverInterimResultOrdering.IsBetter(candidate, current),
                            SolverInterimResultOrdering.CanPromoteDisplayedResult(candidate, current)); break;
                    }
                    case "V": result = Output(SolverInterimResultOrdering.IsCompleteVictory(
                        input.Int(), input.Bool(), input.Bool(), input.Int())); break;
                    case "D": result = Output(SolverInterimResultOrdering.IsResourceTradeImprovement(
                        input.Int(), input.Int(), input.Int(), input.Int())); break;
                    case "H": {
                        PostCombatRelicHealProfile profile = new(input.Int(), input.Int(), input.Int());
                        int hp = input.Int(), maxHp = input.Int(); bool victory = input.Bool();
                        result = Output(profile.HasAnyHeal, profile.HealFor(hp, maxHp), profile.MonotoneHealFor(hp, maxHp),
                            ActEndingBossPolicy.RankedPostCombatRelicHeal(profile, victory, hp, maxHp)); break;
                    }
                    case "A": {
                        var relief = (BossHpRelief)input.Int(); var act = (BossHpStrategy)input.Int();
                        var final = (BossHpStrategy)input.Int(); int value = input.Int(), lost = input.Int(), deficit = input.Int();
                        int recovered = input.Int(), death = input.Int();
                        result = Output((int)ActEndingBossPolicy.ResolveStrategicHpRelief(relief, act, final),
                            ActEndingBossPolicy.RawHpRequiredForPersistentValue(value, relief),
                            ActEndingBossPolicy.PersistentValueOfRecoveredHp(recovered, relief),
                            ActEndingBossPolicy.StrategicHpDeficit(lost, deficit, recovered, relief, death),
                            ActEndingBossPolicy.DeathSavePremium(death), ActEndingBossPolicy.DeathSaveBeamCost(death)); break;
                    }
                    case "P": {
                        var policy = (SolverPotionPolicy)input.Int();
                        int count = input.Int(), automatic = input.Int(), cost = input.Int(), credit = input.Int();
                        int freeDeficit = input.Int(), deficit = input.Int();
                        bool freeWon = input.Bool(), anyWon = input.Bool(), won = input.Bool(), hasBaseline = input.Bool();
                        int ambergris = input.Int(), maxHp = input.Int(), freeHp = input.Int(), hp = input.Int();
                        var relief = (BossHpRelief)input.Int();
                        result = Output(PotionUsePolicy.RequiredHpSaved(count), PotionUsePolicy.ExplicitUseCount(count, automatic),
                            PotionUsePolicy.AdditionalRequiredUseStrategicHpCost(cost),
                            PotionUsePolicy.ApplyReplacementCredit(cost, count, credit), PotionUsePolicy.HpSaved(freeDeficit, deficit),
                            PotionUsePolicy.SmartRequiredHpSaved(cost, relief), PotionUsePolicy.AmbergrisRequiredHpSaved(maxHp),
                            PotionUsePolicy.EffectiveStrategicHpCost(cost, ambergris, maxHp),
                            PotionUsePolicy.MeetsAmbergrisRestriction(hasBaseline, ambergris, cost, maxHp, freeHp, hp),
                            PotionUsePolicy.IsEligible(policy, count, cost, freeWon, freeDeficit, anyWon, won, deficit)); break;
                    }
                    case "B": {
                        SearchBudgetWindow window = new(input.Int(), input.Long());
                        int minimum = input.Int(), nodes = input.Int(), ms = input.Int(), reserve = input.Int();
                        var profile = SolverSearchProfile.Default with { SecondRankBand = true, BaseScoreOnly = true };
                        var limited = window.Limit(profile, nodes, ms, reserve);
                        result = Output(window.CanStart(minimum), limited.BeamWidth, limited.MaxExpandedNodes,
                            limited.MaxCardBranchesPerNode, limited.MaxPileChoiceBranchesPerAction,
                            limited.MaxHandChoiceBranchesPerAction, limited.SoftTimeBudgetMilliseconds,
                            limited.SecondRankBand, limited.BaseScoreOnly, limited.ReallocatedRefinementPortfolio,
                            limited.StopPortfolioAtHpTarget); break;
                    }
                    case "T0": frontier = new(input.Label()); result = Output(frontier.LabelCount); break;
                    case "T": {
                        if (frontier == null) throw new InvalidDataException("frontier not initialized");
                        bool accepted = frontier.TryAccept(input.Label());
                        result = Output(accepted, frontier.LabelCount); break;
                    }
                    default: throw new InvalidDataException("unknown policy oracle operation");
                }
            } catch (OverflowException) { result = "E_OVERFLOW"; }
            catch (ArgumentOutOfRangeException) { result = "E_ARGUMENT"; }
            input.Done(); Console.WriteLine(result);
        }
    }
}
