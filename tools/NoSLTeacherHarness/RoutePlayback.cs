using System.Text.Json.Nodes;
using CombatSolver;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.CardSelection;

// Binding is to the original live objects and exact occurrence/state keys.
// Actions and selections go back through OracleRun's existing legality and
// real action-queue path. The consumer never modifies a card or creature.
internal sealed class RoutePlayback(CombatState root, Player player, IReadOnlyList<PlanAction> actions, int expectedHp)
{
    private readonly Queue<PlanCardChoice> _choices = new();
    public int ActionsReplayed { get; private set; }
    public int ChoicesReplayed { get; private set; }
    public int AutomaticChoicesReplayed { get; private set; }
    public int TerminalHp => player.Creature.CurrentHp;

    public JsonObject Next(CombatState combat, Player actualPlayer)
    {
        if (!ReferenceEquals(combat, root) || !ReferenceEquals(player, actualPlayer))
            throw new InvalidOperationException("Teacher route escaped its frozen combat");
        if (_choices.Count != 0) throw new InvalidOperationException("Teacher action has unconsumed choices");
        if (ActionsReplayed >= actions.Count) throw new InvalidOperationException("Teacher route ended before real combat did");
        PlanAction action = actions[ActionsReplayed];
        if (action.Turn != player.PlayerCombatState!.TurnNumber) throw new InvalidOperationException("Teacher action turn drift");
        foreach (PlanCardChoice choice in action.GetActionChoicesInExecutionOrder()) _choices.Enqueue(choice);
        foreach (PlanCardChoice choice in action.TurnStartChoices ?? []) _choices.Enqueue(choice);
        var script = new JsonObject { ["kind"] = "combat", ["turn"] = action.Turn };
        if (action.Kind == PlanActionKind.PlayCard)
        {
            var hand = player.PlayerCombatState.Hand.Cards.ToList();
            CardModel card = SolverController.FindCardForDeployment(hand, action);
            Creature? target = Target(action, combat);
            if (!card.CanPlay() || !card.IsValidTarget(target)) throw new InvalidOperationException("Teacher card is illegal");
            script["type"] = "play";
            script["hand_index"] = hand.IndexOf(card);
            script["target"] = checked((int?)action.TargetCombatId);
        }
        else if (action.Kind == PlanActionKind.UsePotion)
        {
            PotionModel? potion = action.PotionSlot >= 0 && action.PotionSlot < player.PotionSlots.Count
                ? player.PotionSlots[action.PotionSlot] : null;
            Creature? target = Target(action, combat);
            // PotionModel.EnqueueManualUse(null) resolves the owner's
            // creature when legal (unlike card targeting). OracleRun's
            // action list records that resolved public combat id.
            if (target == null && potion?.IsValidTarget(player.Creature) == true) target = player.Creature;
            if (potion?.Id.Entry != action.PotionId || !potion.IsValidTarget(target))
                throw new InvalidOperationException("Teacher potion identity/target drift");
            script["type"] = "use_potion";
            script["slot"] = action.PotionSlot;
            script["target"] = checked((int?)target?.CombatId);
        }
        else if (action.Kind == PlanActionKind.EndTurn) script["type"] = "end_turn";
        else throw new InvalidOperationException("Unsupported teacher action");
        ActionsReplayed++;
        return script;
    }

    private static Creature? Target(PlanAction action, CombatState combat)
        => action.TargetCombatId is uint id ? combat.Creatures.Single(creature => creature.CombatId == id) : null;

    public IReadOnlyList<int> Select(IReadOnlyList<CardModel> options, int minimum, int maximum)
        => ConsumeSelection(options, minimum, maximum);

    public long? HandSelectionMark(Player actualPlayer)
        => ReferenceEquals(player, actualPlayer) ? ChoicesReplayed : null;

    public void ObserveHandSelection(Player actualPlayer, CardSelectorPrefs prefs,
        IReadOnlyList<CardModel> result, long mark)
    {
        if (!ReferenceEquals(player, actualPlayer)) throw new InvalidOperationException("Automatic selection player drift");
        if (mark != ChoicesReplayed || CombatManager.Instance.IsOverOrEnding) return;
        // A callback already consumed a manual selection. Otherwise the
        // original FromHand returned all candidates (or its empty result).
        if (result.Count == 0 && _choices.Count == 0) return;
        if (result.Count > 0 && (prefs.RequireManualConfirmation || result.Count > prefs.MinSelect))
            throw new InvalidOperationException("Hand selection completed without its required selector callback");
        if (_choices.Count == 0 || _choices.Peek().SourcePile != PileType.Hand)
            throw new InvalidOperationException("Real automatic hand selection has no matching teacher token");
        IReadOnlyList<int> chosen = ConsumeSelection(result, prefs.MinSelect, prefs.MaxSelect);
        if (!chosen.SequenceEqual(Enumerable.Range(0, result.Count)))
            throw new InvalidOperationException("Teacher automatic selection order differs from the real result");
        AutomaticChoicesReplayed++;
    }

    private IReadOnlyList<int> ConsumeSelection(IReadOnlyList<CardModel> options, int minimum, int maximum)
    {
        if (_choices.Count == 0) throw new InvalidOperationException("Real combat requested an unplanned teacher choice");
        PlanCardChoice choice = _choices.Peek();
        IReadOnlyList<CardModel> source = choice.SourcePile == PileType.None ? options : choice.SourcePile.GetPile(player).Cards;
        List<int> selected = [];
        foreach (PlanCardToken token in choice.Cards)
        {
            int index = options.Select((card, index) => (card, index))
                .Where(pair => CardChoiceSupport.MatchesToken(pair.card, token)).Skip(token.OptionOccurrence)
                .Select(pair => pair.index).DefaultIfEmpty(-1).First();
            if (index < 0 || selected.Contains(index)) throw new InvalidOperationException("Teacher selection option identity drift");
            CardModel card = options[index];
            if (!source.Any(candidate => ReferenceEquals(candidate, card))
                || source.TakeWhile(candidate => !ReferenceEquals(candidate, card)).Count(candidate => CardChoiceSupport.MatchesToken(candidate, token)) != token.SourceOccurrence
                || CardChoiceSupport.ChoiceCardKey(card) != token.StateKey)
                throw new InvalidOperationException("Teacher selection source occurrence/full card state drift");
            selected.Add(index);
        }
        int least = Math.Min(minimum, options.Count), most = Math.Min(maximum, options.Count);
        if (most < minimum) most = least;
        if (selected.Count < least || selected.Count > most) throw new InvalidOperationException("Teacher selection count is illegal");
        _choices.Dequeue();
        ChoicesReplayed++;
        return selected;
    }

    public bool Stop()
    {
        bool ended = !CombatManager.Instance.IsInProgress && !CombatManager.Instance.IsEnding
            && !RunManager.Instance.ActionExecutor.IsRunning;
        if (ended) AssertComplete();
        return ended;
    }

    public void AssertComplete()
    {
        if (ActionsReplayed != actions.Count || _choices.Count != 0
            || player.Creature.IsDead || root.Enemies.Any(enemy => enemy.IsAlive) || TerminalHp != expectedHp)
            throw new InvalidOperationException($"Teacher real terminal differs: actions={ActionsReplayed}/{actions.Count}, choices={_choices.Count}, hp={TerminalHp}/{expectedHp}");
    }
}
