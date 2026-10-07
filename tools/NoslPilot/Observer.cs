using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.CardRewardAlternatives;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Merchant;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Entities.Potions;
using MegaCrit.Sts2.Core.Entities.RestSite;
using MegaCrit.Sts2.Core.Events;
using MegaCrit.Sts2.Core.Map;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.MonsterMoves.Intents;
using MegaCrit.Sts2.Core.Rewards;
using MegaCrit.Sts2.Core.Rooms;
using MegaCrit.Sts2.Core.Runs;

namespace NoslPilot;

// The actor observation nosl.obs v1 (nosl/observation.h) built from what the
// game itself shows, with no simulator and no seed: the same three tables and
// the same legal actions that nosl::Observe builds from an Env. Every value
// read here is on screen or can be inspected by the player; hidden state
// (draw order, RNG, rooms not yet revealed) is never touched: piles are
// counted as multisets and card selections are emitted in content order.
//
// Token order matches nosl::Observe except for the map nodes, which the
// simulator lists in its own creation order; the comparison in Entry.cs
// treats tokens as a multiset.
internal sealed class Obs
{
    public const int GlobalWidth = 40, TokenWidth = 16, ActionWidth = 4;
    public readonly int[] Global = new int[GlobalWidth];
    public readonly List<int[]> Tokens = new();
    public readonly List<int[]> Actions = new();
    public readonly List<int[]> Legal = new();  // {kind, index, target, col, row} per action row
}

internal sealed class ChoiceOption
{
    public int Kind;            // 0 Card, 1 Bundle, 2 Reward, 3 Alternative, 4 Proceed
    public string Key = "";
    public CardModel? Card;
    public int Cards;           // how many cards the option stands for
    public Reward? Reward;
    public bool Enabled = true;
}

internal sealed class Choice
{
    public int Kind;            // 0 CardSelect, 1 Bundle, 2 Option
    public string Source = "", Prompt = "";
    public bool Subset, CanSkip;
    public int Min = 1, Max = 1;
    public List<int> Selected = new();
    public List<ChoiceOption> Options = new();
}

internal static class Observer
{
    // nosl/observation.h
    private const int TPad = 0, THand = 1, TDraw = 2, TDiscard = 3, TExhaust = 4, TDeck = 5, TEnemy = 6, TPower = 7, TRelic = 8, TPotion = 9,
        TMapNext = 10, TMapNode = 11, TReward = 12, TChoiceOption = 13, TShop = 14, TEventOption = 15, TRestOption = 16, TTreasure = 17;
    private const int CType = 0, CCard = 1, CRelic = 2, CPotion = 3, CMonster = 4, CPower = 5, CMiscA = 6, CMiscB = 7, N0 = 8;
    private const int MMapPoint = 1, MRewardType = 17, MShopKind = 33, MRestOption = 41, MChoiceKind = 57, MEnchantment = 64, MAffliction = 96,
        MEvent = 128, MHashed = 256, MHashBuckets = 1024;
    private const int GSchema = 0, GDecision = 1, GCharacter = 2, GAscension = 3, GAct = 4, GActFloor = 5, GTotalFloor = 6, GHp = 7, GMaxHp = 8,
        GBlock = 9, GGold = 10, GInCombat = 11, GEnergy = 12, GMaxEnergy = 13, GStars = 14, GTurn = 15, GRound = 16, GHandCount = 17,
        GDrawCount = 18, GDiscardCount = 19, GExhaustCount = 20, GDeckCount = 21, GRelicCount = 22, GPotionCount = 23, GPotionSlots = 24,
        GBoss = 25, GRoomType = 26, GChoiceKind = 27, GChoiceMin = 28, GChoiceMax = 29, GChoiceSelected = 30, GChoiceSubset = 31,
        GChoicePrompt = 32, GChoiceSource = 33, GChoiceCanSkip = 34, GTokenCount = 35, GActionCount = 36, GEvent = 37;
    // env/operation.h Action::Kind and env/env.h DecisionKind
    public const int AMapMove = 1, APlayCard = 2, AEndTurn = 3, ATakeReward = 4, AProceed = 5, AEventOption = 6, ARestOption = 7, AShopBuy = 8,
        ATreasurePick = 9, AUsePotion = 10, ADiscardPotion = 11, AAnswer = 12;
    public const int DMap = 0, DCombat = 1, DRewards = 2, DEvent = 3, DRestSite = 4, DShop = 5, DTreasure = 6, DChoice = 7;
    private static readonly string[] RestIds = { "HEAL", "SMITH", "MEND", "LIFT", "DIG", "COOK", "CLONE", "HATCH", "KINDLE" };

    private static readonly Dictionary<(string, string), int> Ids = new();

    private static int Id(string table, string? entry)
    {
        if (string.IsNullOrEmpty(entry)) return 0;
        if (Ids.TryGetValue((table, entry), out int id)) return id;
        id = NativePilot.Id(table, entry);
        if (id < 0) throw new InvalidOperationException($"the content tables have no {table} named {entry}");
        Ids[(table, entry)] = id;
        return id;
    }

    private static uint Fnv(string text)
    {
        uint h = 2166136261u;
        foreach (byte c in System.Text.Encoding.UTF8.GetBytes(text)) { h ^= c; h *= 16777619u; }
        return h;
    }

    private static int Bucket(string text) => text.Length == 0 ? 0 : MHashed + (int)(Fnv(text) % MHashBuckets);

    // A localization key is stored by the simulator as its hash (0 is reserved for "none").
    private static int BucketOfKey(string key)
    {
        if (key.Length == 0) return 0;
        uint h = Fnv(key);
        if (h == 0) h = 1;
        return MHashed + (int)(h % MHashBuckets);
    }

    private static int[] Token(int type)
    {
        int[] t = new int[Obs.TokenWidth];
        t[CType] = type;
        return t;
    }

    private static void SetCard(int[] t, CardModel card)
    {
        t[CCard] = Id("card", card.Id.Entry);
        if (card.Enchantment != null) t[CMiscA] = MEnchantment + Id("enchantment", card.Enchantment.Id.Entry);
        if (card.Affliction != null) t[CMiscB] = MAffliction + Id("affliction", card.Affliction.Id.Entry);
        t[N0] = card.CurrentUpgradeLevel;
        t[N0 + 3] = (int)card.Type;
    }

    private static void SetReward(int[] t, Reward reward)
    {
        int type = reward switch
        {
            CardReward => 1, GoldReward => 2, PotionReward => 3, RelicReward => 4, CardRemovalReward => 5, SpecialCardReward => 6, _ => 0,
        };
        t[CMiscA] = MRewardType + type;
        if (reward is RelicReward { Relic: not null } relic) t[CRelic] = Id("relic", relic.Relic.Id.Entry);
        if (reward is PotionReward { Potion: not null } potion) t[CPotion] = Id("potion", potion.Potion.Id.Entry);
        t[N0 + 1] = reward is GoldReward gold ? Math.Max(0, gold.Amount) : 0;
        t[N0 + 3] = reward.SuccessfullySelected ? 1 : 0;
    }

    private static void PushBag(Obs o, int type, IEnumerable<CardModel> cards)
    {
        SortedDictionary<(int, int, int, int), (int[] token, int count)> bag = new();
        foreach (CardModel card in cards)
        {
            int[] t = Token(type);
            SetCard(t, card);
            (int, int, int, int) key = (t[CCard], t[N0], t[CMiscA], t[CMiscB]);
            bag[key] = bag.TryGetValue(key, out (int[] token, int count) seen) ? (seen.token, seen.count + 1) : (t, 1);
        }
        foreach ((int[] token, int count) in bag.Values)
        {
            token[N0 + 1] = count;
            o.Tokens.Add(token);
        }
    }

    // Env::LegalActions, append_potion_actions: what the potion popup allows.
    private static void PotionActions(Obs o, Player player, bool inCombat)
    {
        if (!player.CanUseOrRemovePotions) return;
        CombatState? combat = CombatManager.Instance.DebugOnlyGetState();
        for (int slot = 0; slot < player.PotionSlots.Count; slot++)
        {
            PotionModel? potion = player.PotionSlots[slot];
            if (potion == null) continue;
            bool canUse = potion.Usage != PotionUsage.Automatic && (potion.Usage != PotionUsage.CombatOnly || inCombat)
                && potion.PassesCustomUsabilityCheck;
            TargetType type = potion.TargetType;
            Creature self = player.Creature;
            if (canUse && !inCombat)
            {
                if (type is TargetType.Self or TargetType.AnyPlayer)
                {
                    if (potion.IsValidTarget(self)) o.Legal.Add(new[] { AUsePotion, slot, -1, 0, 0 });
                }
                else if (potion.IsValidTarget(null)) o.Legal.Add(new[] { AUsePotion, slot, -1, 0, 0 });
            }
            else if (canUse && type is TargetType.AnyEnemy or TargetType.AnyAlly)
            {
                foreach (Creature target in combat!.Creatures)
                    if (potion.IsValidTarget(target)) o.Legal.Add(new[] { AUsePotion, slot, (int)target.CombatId!.Value, 0, 0 });
            }
            else if (canUse && type is TargetType.Self or TargetType.AnyPlayer)
            {
                if (potion.IsValidTarget(self)) o.Legal.Add(new[] { AUsePotion, slot, (int)self.CombatId!.Value, 0, 0 });
            }
            else if (canUse && potion.IsValidTarget(null)) o.Legal.Add(new[] { AUsePotion, slot, -1, 0, 0 });
            o.Legal.Add(new[] { ADiscardPotion, slot, -1, 0, 0 });
        }
    }

    public static List<MapPoint> MapOptions(RunState run)
    {
        MapPoint? point = run.CurrentMapPoint;
        return point == null
            ? new List<MapPoint> { run.Map.StartingMapPoint }
            : MapTravel.GetTravelablePointsFrom(run, point).OrderBy(p => p.coord.col).ThenBy(p => p.coord.row).ToList();
    }

    // `rewards`: the set on the rewards screen (Rewards). `choice`: the pending selection (Choice).
    // `shop`: the merchant's inventory (Shop). `treasure`: the chest's relics (Treasure). `restProceed`: whether the
    // rest site shows its proceed button.
    public static Obs Build(RunState run, Player player, int decision, Choice? choice = null, RewardsSet? rewards = null,
        MerchantInventory? shop = null, IReadOnlyList<RelicModel>? treasure = null, bool restProceed = false)
    {
        Obs o = new();
        int[] G = o.Global;
        Creature self = player.Creature;
        PlayerCombatState? pcs = player.PlayerCombatState;
        CombatState? combat = CombatManager.Instance.DebugOnlyGetState();
        bool inCombat = CombatManager.Instance.IsInProgress && pcs != null && combat != null;

        G[GSchema] = 1;
        G[GDecision] = decision;
        G[GCharacter] = Id("character", player.Character.Id.Entry);
        G[GAscension] = run.AscensionLevel;
        G[GAct] = run.CurrentActIndex;
        G[GActFloor] = run.ActFloor;
        G[GTotalFloor] = run.TotalFloor;
        G[GHp] = self.CurrentHp;
        G[GMaxHp] = self.MaxHp;
        G[GBlock] = self.Block;
        G[GGold] = player.Gold;
        G[GInCombat] = inCombat ? 1 : 0;
        G[GDeckCount] = player.Deck.Cards.Count;
        G[GPotionSlots] = player.PotionSlots.Count;
        G[GRoomType] = run.CurrentRoom == null ? 0 : (int)run.CurrentRoom.RoomType;
        G[GBoss] = Id("encounter", run.Act.BossEncounter.Id.Entry);

        foreach (RelicModel relic in player.Relics)
        {
            int[] t = Token(TRelic);
            t[CRelic] = Id("relic", relic.Id.Entry);
            t[N0] = relic.ShowCounter ? relic.DisplayAmount : -1;
            t[N0 + 1] = (int)relic.Status;
            o.Tokens.Add(t);
            G[GRelicCount]++;
        }
        int[] potionToken = Enumerable.Repeat(-1, 16).ToArray();
        for (int slot = 0; slot < player.PotionSlots.Count && slot < 16; slot++)
        {
            PotionModel? potion = player.PotionSlots[slot];
            if (potion == null) continue;
            int[] t = Token(TPotion);
            t[CPotion] = Id("potion", potion.Id.Entry);
            t[N0] = slot;
            potionToken[slot] = o.Tokens.Count;
            o.Tokens.Add(t);
            G[GPotionCount]++;
        }

        List<int> handToken = new();
        Dictionary<int, int> enemyToken = new();  // combat id -> token
        if (inCombat)
        {
            G[GEnergy] = pcs!.Energy;
            G[GMaxEnergy] = pcs.MaxEnergy;
            G[GStars] = pcs.Stars;
            G[GTurn] = pcs.TurnNumber;
            G[GRound] = combat!.RoundNumber;
            G[GHandCount] = pcs.Hand.Cards.Count;
            G[GDrawCount] = pcs.DrawPile.Cards.Count;
            G[GDiscardCount] = pcs.DiscardPile.Cards.Count;
            G[GExhaustCount] = pcs.ExhaustPile.Cards.Count;
            int index = 0;
            foreach (CardModel card in pcs.Hand.Cards)
            {
                int[] t = Token(THand);
                SetCard(t, card);
                t[N0 + 1] = card.EnergyCost.GetWithModifiers(CostModifiers.All);
                t[N0 + 2] = card.GetStarCostWithModifiers();
                t[N0 + 4] = card.CanPlay() ? 1 : 0;
                t[N0 + 5] = index++;
                handToken.Add(o.Tokens.Count);
                o.Tokens.Add(t);
            }
            PushBag(o, TDraw, pcs.DrawPile.Cards);
            PushBag(o, TDiscard, pcs.DiscardPile.Cards);
            PushBag(o, TExhaust, pcs.ExhaustPile.Cards);
            void Powers(Creature owner, int ownerSlot)
            {
                foreach (PowerModel power in owner.Powers)
                {
                    int[] t = Token(TPower);
                    t[CPower] = Id("power", power.Id.Entry);
                    t[N0] = power.Amount;
                    t[N0 + 1] = ownerSlot;
                    o.Tokens.Add(t);
                }
            }
            Powers(self, 0);
            IReadOnlyList<Creature> targets = combat.PlayerCreatures;
            int slot = 0;
            foreach (Creature enemy in combat.Enemies)
            {
                MonsterModel monster = enemy.Monster!;
                int[] t = Token(TEnemy);
                t[CMonster] = Id("monster", monster.Id.Entry);
                t[N0] = enemy.CurrentHp;
                t[N0 + 1] = enemy.MaxHp;
                t[N0 + 2] = enemy.Block;
                t[N0 + 3] = -1;
                foreach (AbstractIntent intent in monster.NextMove.Intents)
                {
                    t[N0 + 5] |= 1 << (int)intent.IntentType;
                    if (intent is AttackIntent attack)
                    {
                        t[N0 + 3] = attack.GetSingleDamage(targets, enemy);
                        t[N0 + 4] = attack is MultiAttackIntent ? attack.Repeats : 1;
                    }
                }
                t[N0 + 6] = slot;
                t[N0 + 7] = enemy.IsAlive ? 1 : 0;
                enemyToken[(int)enemy.CombatId!.Value] = o.Tokens.Count;
                o.Tokens.Add(t);
                Powers(enemy, ++slot);
            }
        }
        else
        {
            PushBag(o, TDeck, player.Deck.Cards);
        }

        List<int> optionToken = new();
        Dictionary<(int, int), int> mapToken = new();
        bool sortable = false;
        switch (decision)
        {
            case DMap:
            {
                ActMap map = run.Map;
                bool started = run.VisitedMapCoords.Count > 0;
                MapPoint? here = started ? map.GetPoint(run.VisitedMapCoords[^1]) : null;
                List<MapPoint> points = map.GetAllMapPoints().ToList();
                foreach (MapPoint special in new[] { map.BossMapPoint, map.StartingMapPoint, map.SecondBossMapPoint })
                    if (special != null && !points.Contains(special)) points.Add(special);
                HashSet<MapPoint> Cone(MapPoint start)
                {
                    HashSet<MapPoint> seen = new() { start };
                    Stack<MapPoint> stack = new();
                    stack.Push(start);
                    while (stack.Count > 0)
                        foreach (MapPoint child in stack.Pop().Children)
                            if (seen.Add(child)) stack.Push(child);
                    return seen;
                }
                HashSet<MapPoint>? reachable = started && here != null ? Cone(here) : null;
                foreach (MapPoint point in points)
                {
                    int[] t = Token(TMapNode);
                    t[CMiscA] = MMapPoint + (int)point.PointType;
                    t[N0] = point.coord.col;
                    t[N0 + 1] = point.coord.row;
                    t[N0 + 2] = run.VisitedMapCoords.Contains(point.coord) ? 1 : 0;
                    t[N0 + 3] = started && here != null && point.coord.Equals(here.coord) ? 1 : 0;
                    foreach (MapPoint child in point.Children)
                        if (child.coord.col >= 0 && child.coord.col < 31) t[N0 + 4] |= 1 << child.coord.col;
                    t[N0 + 5] = !started ? 1 : reachable != null && reachable.Contains(point) ? 1 : 0;
                    o.Tokens.Add(t);
                }
                PotionActions(o, player, false);
                foreach (MapPoint next in MapOptions(run))
                {
                    int[] t = Token(TMapNext);
                    t[N0] = next.coord.col;
                    t[N0 + 1] = next.coord.row;
                    t[CMiscA] = MMapPoint + (int)next.PointType;
                    foreach (MapPoint p in Cone(next))
                    {
                        if (ReferenceEquals(p, next)) continue;
                        switch (p.PointType)
                        {
                            case MapPointType.Elite: t[N0 + 2]++; break;
                            case MapPointType.RestSite: t[N0 + 3]++; break;
                            case MapPointType.Shop: t[N0 + 4]++; break;
                            case MapPointType.Unknown: t[N0 + 5]++; break;
                            case MapPointType.Monster: t[N0 + 6]++; break;
                            case MapPointType.Treasure: t[N0 + 7]++; break;
                        }
                    }
                    mapToken[(next.coord.col, next.coord.row)] = o.Tokens.Count;
                    o.Tokens.Add(t);
                    o.Legal.Add(new[] { AMapMove, -1, -1, next.coord.col, next.coord.row });
                }
                break;
            }
            case DCombat:
            {
                PotionActions(o, player, true);
                for (int i = 0; i < pcs!.Hand.Cards.Count; i++)
                {
                    CardModel card = pcs.Hand.Cards[i];
                    if (!card.CanPlay()) continue;
                    if (card.TargetType is TargetType.AnyEnemy or TargetType.AnyAlly)
                    {
                        foreach (Creature target in combat!.Creatures)
                            if (card.IsValidTarget(target)) o.Legal.Add(new[] { APlayCard, i, (int)target.CombatId!.Value, 0, 0 });
                    }
                    else if (card.IsValidTarget(null)) o.Legal.Add(new[] { APlayCard, i, -1, 0, 0 });
                }
                o.Legal.Add(new[] { AEndTurn, -1, -1, 0, 0 });
                break;
            }
            case DRewards:
            {
                PotionActions(o, player, false);
                for (int i = 0; i < rewards!.Rewards.Count; i++)
                {
                    Reward reward = rewards.Rewards[i];
                    int[] t = Token(TReward);
                    SetReward(t, reward);
                    t[N0] = i;
                    t[N0 + 2] = reward.SuccessfullySelected ? 0 : 1;
                    optionToken.Add(o.Tokens.Count);
                    o.Tokens.Add(t);
                    if (!reward.SuccessfullySelected) o.Legal.Add(new[] { ATakeReward, i, -1, 0, 0 });
                }
                o.Legal.Add(new[] { AProceed, -1, -1, 0, 0 });
                break;
            }
            case DEvent:
            {
                EventModel ev = ((EventRoom)run.CurrentRoom!).LocalMutableEvent;
                int eventId = Id("event", ev.Id.Entry);
                G[GEvent] = eventId;
                PotionActions(o, player, false);
                for (int i = 0; i < ev.CurrentOptions.Count; i++)
                {
                    EventOption option = ev.CurrentOptions[i];
                    int[] t = Token(TEventOption);
                    t[CMiscA] = MEvent + eventId;
                    t[CMiscB] = BucketOfKey(option.TextKey ?? "");
                    if (option.Relic != null) t[CRelic] = Id("relic", option.Relic.Id.Entry);
                    t[N0] = i;
                    t[N0 + 1] = option.IsLocked ? 1 : 0;
                    t[N0 + 2] = option.WillKillPlayer?.Invoke(player) == true ? 1 : 0;
                    t[N0 + 3] = option.IsProceed ? 1 : 0;
                    optionToken.Add(o.Tokens.Count);
                    o.Tokens.Add(t);
                    if (!option.IsLocked) o.Legal.Add(new[] { AEventOption, i, -1, 0, 0 });
                }
                break;
            }
            case DRestSite:
            {
                IReadOnlyList<RestSiteOption> options = ((RestSiteRoom)run.CurrentRoom!).Options;
                PotionActions(o, player, false);
                for (int i = 0; i < options.Count; i++)
                {
                    int[] t = Token(TRestOption);
                    int id = System.Array.IndexOf(RestIds, options[i].OptionId);
                    if (id < 0) throw new InvalidOperationException("unknown rest site option " + options[i].OptionId);
                    t[CMiscA] = MRestOption + id;
                    t[N0] = i;
                    t[N0 + 1] = options[i].IsEnabled ? 1 : 0;
                    optionToken.Add(o.Tokens.Count);
                    o.Tokens.Add(t);
                    if (options[i].IsEnabled) o.Legal.Add(new[] { ARestOption, i, -1, 0, 0 });
                }
                if (options.Count == 0 || restProceed) o.Legal.Add(new[] { AProceed, -1, -1, 0, 0 });
                break;
            }
            case DShop:
            {
                PotionActions(o, player, false);
                int stocked = 0;
                foreach (MerchantEntry entry in shop!.AllEntries)
                {
                    int[] t = Token(TShop);
                    int kind = entry switch
                    {
                        MerchantCardEntry card when shop.CharacterCardEntries.Contains(card) => 0,
                        MerchantCardEntry => 1, MerchantRelicEntry => 2, MerchantPotionEntry => 3, _ => 4,
                    };
                    t[CMiscA] = MShopKind + kind;
                    t[N0] = entry.Cost;
                    t[N0 + 1] = entry.IsStocked ? 1 : 0;
                    t[N0 + 2] = entry.IsStocked && entry.EnoughGold ? 1 : 0;
                    switch (entry)
                    {
                        case MerchantCardEntry card:
                            if (card.CreationResult != null)
                            {
                                t[CCard] = Id("card", card.CreationResult.Card.Id.Entry);
                                t[N0 + 4] = card.CreationResult.Card.CurrentUpgradeLevel;
                            }
                            t[N0 + 3] = card.IsOnSale ? 1 : 0;
                            break;
                        case MerchantRelicEntry relic:
                            if (relic.Model != null) t[CRelic] = Id("relic", relic.Model.Id.Entry);
                            break;
                        case MerchantPotionEntry potion:
                            if (potion.Model != null) t[CPotion] = Id("potion", potion.Model.Id.Entry);
                            break;
                    }
                    if (entry.IsStocked)
                    {
                        optionToken.Add(o.Tokens.Count);
                        if (entry.EnoughGold) o.Legal.Add(new[] { AShopBuy, stocked, -1, 0, 0 });
                        stocked++;
                    }
                    o.Tokens.Add(t);
                }
                o.Legal.Add(new[] { AProceed, -1, -1, 0, 0 });
                break;
            }
            case DTreasure:
            {
                PotionActions(o, player, false);
                for (int i = 0; i < treasure!.Count; i++)
                {
                    int[] t = Token(TTreasure);
                    t[CRelic] = Id("relic", treasure[i].Id.Entry);
                    t[N0] = i;
                    optionToken.Add(o.Tokens.Count);
                    o.Tokens.Add(t);
                    o.Legal.Add(new[] { ATreasurePick, i, -1, 0, 0 });
                }
                o.Legal.Add(new[] { AProceed, -1, -1, 0, 0 });
                break;
            }
            case DChoice:
            {
                Choice c = choice!;
                bool countPrompt = c.Subset && c.Prompt == "count";
                G[GChoiceKind] = 1 + c.Kind;
                G[GChoiceMin] = c.Min;
                G[GChoiceMax] = c.Max;
                G[GChoiceSelected] = c.Selected.Count;
                G[GChoiceSubset] = c.Subset ? (countPrompt ? 2 : 1) : 0;
                G[GChoicePrompt] = Bucket(c.Prompt);
                G[GChoiceSource] = Bucket(c.Source);
                G[GChoiceCanSkip] = c.CanSkip ? 1 : 0;
                // A card selection's backing order can follow a hidden pile order: its options are shown in content order.
                sortable = c.Kind == 0 && !countPrompt;
                List<int[]> options = new();
                for (int i = 0; i < c.Options.Count; i++)
                {
                    ChoiceOption option = c.Options[i];
                    int[] t = Token(TChoiceOption);
                    if (option.Card != null) SetCard(t, option.Card);
                    if (option.Reward != null) SetReward(t, option.Reward);
                    int before = t[CMiscA];
                    t[CMiscA] = MChoiceKind + option.Kind;
                    t[CMiscB] = option.Key.Length == 0 ? before : Bucket(option.Key);
                    t[N0 + 2] = option.Enabled ? 1 : 0;
                    t[N0 + 4] = sortable && option.Kind == 0 ? -1 : i;
                    t[N0 + 5] = option.Cards;
                    t[N0 + 6] = c.Selected.Contains(i) ? 1 : 0;
                    options.Add(t);
                }
                List<int> order = Enumerable.Range(0, options.Count).ToList();
                if (sortable) order = order.OrderBy(i => options[i], TokenOrder.Instance).ToList();  // OrderBy is stable
                optionToken = Enumerable.Repeat(-1, options.Count).ToList();
                foreach (int i in order)
                {
                    optionToken[i] = o.Tokens.Count;
                    o.Tokens.Add(options[i]);
                }
                if (countPrompt)
                {
                    for (int i = Math.Max(0, c.Min); i <= Math.Min(c.Max, c.Options.Count); i++) o.Legal.Add(new[] { AAnswer, i, -1, 0, 0 });
                }
                else
                {
                    for (int i = 0; i < c.Options.Count; i++)
                    {
                        if (c.Subset && c.Selected.Contains(i)) continue;
                        if (!c.Options[i].Enabled) continue;
                        o.Legal.Add(new[] { AAnswer, i, -1, 0, 0 });
                    }
                }
                break;
            }
        }

        // The action table (nosl::Observe): each legal action points at the tokens it acts on.
        List<(int[] row, int[] legal)> rows = new();
        foreach (int[] a in o.Legal)
        {
            int[] row = { a[0], -1, -1, 0 };
            void Target()
            {
                if (a[2] < 0) return;
                if (enemyToken.TryGetValue(a[2], out int token)) row[2] = token;
                else row[3] = a[2];
            }
            switch (a[0])
            {
                case AMapMove: row[1] = mapToken[(a[3], a[4])]; break;
                case APlayCard:
                    if (a[1] >= 0 && a[1] < handToken.Count) row[1] = handToken[a[1]];
                    Target();
                    break;
                case AUsePotion:
                    if (a[1] >= 0 && a[1] < 16) row[1] = potionToken[a[1]];
                    Target();
                    break;
                case ADiscardPotion:
                    if (a[1] >= 0 && a[1] < 16) row[1] = potionToken[a[1]];
                    break;
                case ATakeReward: case AEventOption: case ARestOption: case AShopBuy: case ATreasurePick:
                    row[1] = a[1] >= 0 && a[1] < optionToken.Count ? optionToken[a[1]] : -1;
                    break;
                case AAnswer:
                    if (G[GChoiceSubset] == 2) row[3] = a[1];
                    else row[1] = a[1] >= 0 && a[1] < optionToken.Count ? optionToken[a[1]] : -1;
                    break;
            }
            rows.Add((row, a));
        }
        if (sortable) rows = rows.OrderBy(r => r.row[1]).ToList();  // stable
        o.Legal.Clear();
        foreach ((int[] row, int[] legal) in rows)
        {
            o.Actions.Add(row);
            o.Legal.Add(legal);
        }
        G[GTokenCount] = o.Tokens.Count;
        G[GActionCount] = o.Actions.Count;
        return o;
    }

    private sealed class TokenOrder : IComparer<int[]>
    {
        public static readonly TokenOrder Instance = new();
        public int Compare(int[]? x, int[]? y)
        {
            for (int i = 0; i < x!.Length; i++)
                if (x[i] != y![i]) return x[i].CompareTo(y[i]);
            return 0;
        }
    }

    // The card selection the game hands to the selector, as the simulator words it (CardSelectCmd Select).
    public static Choice CardSelection(IReadOnlyList<CardModel> options, int min, int max, string source, bool countPrompt, List<int> selected)
    {
        Choice c = new() { Kind = 0, Source = source, Prompt = countPrompt ? "count" : "index", Subset = true, CanSkip = min == 0, Min = min, Max = max };
        c.Selected.AddRange(selected);
        foreach (CardModel card in options) c.Options.Add(new ChoiceOption { Kind = 0, Card = card, Cards = 1 });
        return c;
    }

    // The card reward screen (Reward::CardRewardOnSelect): the cards, then Skip / Reroll / Sacrifice.
    public static Choice CardRewardChoice(CardReward reward, IReadOnlyList<CardRewardAlternative> alternatives)
    {
        Choice c = new() { Kind = 0, Source = "CardSelectCmd", Prompt = "CardReward", Subset = false, CanSkip = reward.CanSkip, Min = 0, Max = 1 };
        foreach (CardModel card in reward.Cards) c.Options.Add(new ChoiceOption { Kind = 0, Card = card, Cards = 1 });
        foreach (CardRewardAlternative alternative in alternatives)
            c.Options.Add(new ChoiceOption { Kind = 3, Key = alternative.OptionId, Enabled = alternative.OptionId != "REROLL" || reward.CanReroll });
        return c;
    }

    // A rewards screen inside an event or a rest site (RewardsCmd OfferAndAwait): each reward, then proceed.
    public static Choice NestedRewards(RewardsSet set, bool disallowSkipping)
    {
        Choice c = new() { Kind = 2, Source = "rewards", Prompt = "take a reward, or proceed", Subset = false, CanSkip = false, Min = 1, Max = 1 };
        foreach (Reward reward in set.Rewards) c.Options.Add(new ChoiceOption { Kind = 2, Reward = reward, Enabled = !reward.SuccessfullySelected });
        c.Options.Add(new ChoiceOption { Kind = 4, Enabled = !disallowSkipping });
        return c;
    }
}
