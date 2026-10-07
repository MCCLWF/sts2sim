# 引擎设计与移植约定

本文是 C++ 引擎的结构说明，也是把 C# 源码移植成 C++ 的**翻译规则**。所有移植工作（人或 AI）都按本文执行。与 [`design.md`](design.md) 冲突时以本文为准。

---

## 1. 总体结构

```
engine/     基础设施，不是逐文件移植：
  sim/      GameState（纯数据）、对象池、Sim 上下文、异常、选择前缀
  （以及已有的 map、run_setup、save_json、containers、json）
game/       C# 源码 src/Core 的逐文件移植，目录结构与 src/Core 完全一致：
            src/Core/Entities/Creatures/Creature.cs → game/Entities/Creatures/Creature.h（.cpp）
            src/Core/Hooks/Hook.cs                  → game/Hooks/Hook.h（.cpp）
            src/Core/Models/Cards/StrikeIronclad.cs → game/Models/Cards/StrikeIronclad.h（.cpp）
  Models/members.def    可覆写成员的 C++ 签名表（Hook 与各基类 virtual 成员）
  Models/registry.def   已移植的具体模型清单（分发用）
```

- **一个 C# 文件对应一个 C++ 文件**，路径、类名、方法名保持一致。这样才能逐文件对照、逐文件跟踪进度。
- C# 的命名空间统一放进 `namespace sts2`。
- 引擎类（`Creature`、`CardModel`、`CombatManager`……）本身也是移植：它们的句柄类就是对应 C# 类的移植。

## 2. 状态：纯数据，克隆就是 memcpy

- 全部可变状态在 `GameState`（`engine/sim/state.h`）里，必须可平凡复制（trivially copyable）：不能有指针、`std::vector`、`std::string`、虚函数。
- **对象池**：卡牌、能力、生物、遗物、药水、充能球各有一个定长池（`Pool<T, N>`）。C# 里的对象引用，在状态里存成池下标（`CardRef`、`PowerRef`、`CreatureRef`……，`uint16_t`，0 表示 null）。
  - 牌组、各牌堆、生物的能力列表，都是 `StaticVec<XxxRef, N>`，顺序与 C# 的 `List<T>` 完全一致。
  - 对象离开战斗（C# 的 `HasBeenRemovedFromState = true`）后，池槽位**不立即回收**，直到战斗结束。原因是 C# 里别处可能还拿着它的引用，并且会去读 `HasBeenRemovedFromState`。
- **每个实例的通用槽位**：C# 子类自己的字段（例如某张卡的计数器、某个能力的 `_internalData`）不单独建结构，而是用实例上的 `extra[k]`（`int32_t`）和 `dvars[k]`（`Dec`）。在具体类里用具名访问器包装：
  ```cpp
  struct Rampage : CardModel {
      using CardModel::CardModel;
      int32_t& TimesPlayed() { return extra(0); }   // C#: private int _timesPlayed
  };
  ```
- 模型的**静态属性**（费用、类型、稀有度、目标类型、`CanonicalVars`、`CanonicalKeywords`、升级差值、怪物血量、遭遇列表……）来自生成的内容表（`content/generated/tables.h`），**不要手写**。

## 3. 句柄：与 C# 同名的类

- `Creature`、`Player`、`CardModel`、`PowerModel`、`MonsterModel`、`RelicModel`、`PotionModel`、`OrbModel`、`EnchantmentModel`、`AfflictionModel`、`CardPile`、`CombatState`、`RunState` 都是**句柄**：
  - 句柄 = `{Sim* sim; uint16_t index;}`，按值传递，只在调用过程中存在，**绝不存进状态**。
  - 可以比较相等（同一个下标即同一个对象），可以为 null（`index == 0`）。转成 bool 表示非 null。
- C# 的属性翻译成同名的无参方法：
  - `card.Owner.Creature.CurrentHp` → `card.Owner().Creature().CurrentHp()`。
  - `base.X` 和 `this.X` 都写成 `X()`。
- setter 写成 `SetX(v)`；或者访问器返回引用，写成 `X() = v`。以句柄类里已有的写法为准。
- 句柄的成员是**非虚**的。可覆写的成员通过第 4 节的分发机制调用。

## 4. 模型行为：句柄子类 + 生成的分发

- 具体模型写成对应基类句柄的子类，放在 `game/Models/<Kind>s/<类名>.h`，只写 C# 里 `override` 的成员（全部是 `const` 方法）。类体以 `SIM_MODEL(类名, 基类)` 开头，它提供基类的构造函数和从基类句柄的转换：
  ```cpp
  // game/Models/Powers/VulnerablePower.h
  struct VulnerablePower : PowerModel {
      SIM_MODEL(VulnerablePower, PowerModel)
      Dec ModifyDamageMultiplicative(Creature target, Dec amount, ValueProp props, Creature dealer,
                                     CardModel cardSource, const CardPlay* cardPlay) const;
      void AfterSideTurnEnd(PlayerChoiceContext& choiceContext, CombatSide side, const CreatureList& participants) const;
  };
  ```
- `game/Models/<Base>.def`（`tools/gen/gen_members.py` 生成）列出每个基类的可覆写成员的 C++ 签名。它是 X-macro 表，来源是 `data/content.json` 里各基类的 virtual 成员。纯数据的属性（稀有度、类型、`CanonicalVars`……）不在表里，直接读内容表。
- `tools/gen/gen_dispatch.py` 扫描 `game/Models/<Kind>s/*.h` 里定义的类，生成 `game/generated/Dispatch.cpp` 和 `AllModels.h`，不需要手工登记。新增或删除具体类后重跑它。
  - 某个类在 C# 里覆写了某成员、移植类却没声明它时，生成器直接报错（否则会递归回基类分发）。
  - `CardModel.OnUpgrade` 只包含表格增量（`UpgradeValueBy`、`EnergyCost.UpgradeBy`、增删关键词）的卡，由 `CardModel::UpgradeInternal` 按升级表处理，不需要移植 `OnUpgrade`（v0.111.0 的全部卡牌都是这样）。
- 分发：调用 `model.AfterAttack(...)` 时，按以下顺序处理：
  1. 若生成表 `ModelInfo.overrides` 说该类没有覆写这个成员，直接走基类默认实现（`AbstractModel` 里的默认行为，通常是空操作或原样返回输入）。
  2. 若覆写了且类已移植，调用具体类的方法。
  3. 若覆写了但类**还没移植**，抛 `Unported` 异常。**绝不静默跳过。**
- 默认实现照抄 C# 基类（`AbstractModel.cs`、`CardModel.cs`、`PowerModel.cs`……）里的 virtual 方法体。

## 5. async → 同步

| C# | C++ |
|---|---|
| `async Task Foo(...)` | `void Foo(...)` |
| `async Task<T> Foo(...)` | `T Foo(...)` |
| `await X(...)` | `X(...)` |
| `Task.WhenAll(tasks)` | 按列表顺序依次执行（oracle 里就是这个顺序；有分歧时以 oracle 为准） |
| `Task.CompletedTask`、`Task.FromResult(x)` | 什么都不返回，或直接 `return x` |
| `Cmd.Wait`、`Cmd.CustomScaledWait`、`WaitForUnpause` | 删除 |
| `CancellationToken`、`ThrowIfCancellationRequested` | 删除 |

## 6. 玩家选择与前缀重放

- `PlayerChoiceContext` 照样作为参数一路传下去（`PlayerChoiceContext&`），保持签名与 C# 一致，方便逐行对照。
- 所有需要玩家做选择的地方（`CardSelectCmd.*`、事件选项、奖励……）最终都调用 `sim.Choose(request)`：
  - 如果"选择前缀"里有现成答案，就直接返回。
  - 如果没有，就抛出 `NeedChoice{request}`。
- 环境层在静止点保存快照；收到 `NeedChoice` 后，把这个选择暴露给智能体，拿到答案后带着新前缀从快照重新执行。
- 引擎代码里**不要**写任何协程或手写状态机。选择点就是一次函数调用。

## 7. Hook

- `game/Hooks/Hook.cpp` 逐个移植 `Hook.cs` 的分发函数，名字、参数、顺序完全一致。
- 监听者遍历严格照抄 `CombatState.IterateHookListeners`：
  - **顺序**：先我方后敌方。每个生物依次是：它的能力；然后，怪物是怪物模型，玩家是遗物、药水、充能球、各牌堆的卡（每张卡后面跟它的苦难、附魔）。最后是修饰器。
  - **快照**：遍历开始时生成监听者列表，之后状态怎么变都不影响列表本身。
  - **懒检查**：每一项在轮到它时才检查是否仍在战斗中（`Contains`），不在的跳过。
  - **战斗结束抑制**：`IterateCombatHookListeners` 在 `IsOverOrEnding && !IsStarting` 时什么都不产出。只在遍历开始时判断一次。
- 性能：快照时可以跳过"没有覆写该成员"的模型。这不改变语义，因为这些模型的默认实现什么都不做。

## 8. 集合与 LINQ

**顺序就是一切。** 翻译时必须保留 C# 的枚举顺序和"何时生成快照"。

- `foreach (x in list)`：如果循环体可能修改这个列表，C# 会抛异常，所以原代码里一定没有这种写法。直接遍历即可。
- `foreach (x in list.ToList())` 或其他物化的 LINQ 查询：**先拷贝一份**（`StaticVec` 快照），再遍历拷贝。
- 未物化的 LINQ（`Where`/`Select` 直接 `foreach`）是惰性的：每次取下一项时才对**当前**列表求值。只要循环体会修改源列表，就按"每步重新检查"的方式实现。
- `First`、`FirstOrDefault`、`Any`、`All`、`Count(pred)`、`Sum`、`Max` 照抄即可。注意 `Sum<float>` 的累加类型。
- `OrderBy`/`OrderByDescending` 是**稳定**排序，用 `std::stable_sort`。`List.Sort` 是**不稳定**的 introsort，用 `dotnet_sort`。
- `HashSet`、`Dictionary` 的遍历顺序用 `DotnetSet`/插入顺序模拟。只用来做 `Contains` 判断时，用什么结构都可以。
- 随机相关：`rng.NextItem(list)`、`list.StableShuffle(rng)`、`rng.Shuffle(list)` 都有对应的 C++ 版本，参数顺序和抽取次数必须一致。

## 9. 数值

- C# `decimal` 一律用 `Dec`；`int` 用 `int32_t`；`float`、`double` 保持原类型，表达式里的类型转换照抄。
- `(int)decimal` 是向零截断：`Dec::to_int()`。`Math.Round` 是银行家舍入。`Math.Floor`、`Math.Ceiling` 用 `dec_floor`、`dec_ceiling`。
- 字面量：`1.5m` 写成 `Dec::lit(15, 1)`；整数字面量可以直接写 `Dec(3)`。

## 10. C# 事件

- 纯 UI 的事件（`Flashed`、`DisplayAmountChanged`、`EnergyChanged`……）删除。
- 带游戏逻辑的订阅（例如 `pet.Died += OnPetDied`）要保留。在状态里记录"谁订阅了什么"，触发事件时按订阅顺序调用。遇到时先在 `docs/audits/pitfalls.md` 登记，再实现。

## 11. 删除的代码

以下内容全部删除，但**删除前要确认它们不影响状态**。例如 `NCombatRoom.Instance?.GetCreatureNode(x)?.Position` 如果被拿去参与游戏逻辑，就不能删，要登记偏离。
- VFX、SFX、动画（`TriggerAnim`、`VfxCmd`、`SfxCmd`、`NCombatRoom`、`N*` 节点）
- `Log.*`
- 历史记录（`CombatManager.Instance.History.*`）：只在有游戏逻辑读取历史的地方保留。`CombatHistory` 被一些卡牌读取，按需移植。
- 多人同步（`ActionQueueSynchronizer`、`ChecksumTracker`、`NetId`）

## 12. 错误处理

- C# 的 `throw new InvalidOperationException` 这类检查照样保留，改成抛 `GameError`，带上说明。
- 资源预算超限（快照/恢复字节、JSON节点或32位引用/计数范围）抛 `CapacityError`，绝不静默截断；
  `StaticVec` 与游戏对象池本身按需增长，模板容量只用于小状态的内联优化。
- 断言失败意味着模拟器已经偏离游戏，必须让它响亮地失败。

## 13. 移植一个文件的流程

1. 打开 C# 源文件，复制结构：类名、成员顺序、方法名都不改。
2. 按第 5、8、9、11 节逐行翻译。文件开头注释指向 C# 源码位置：`// Models/Cards/Bash.cs`。
3. 具体模型写完后运行 `tools/gen/gen_dispatch.py` 重新生成分发。
4. 写完跑 L3 对拍，覆盖到这个类的战斗必须零分歧。怪物和遭遇可以用 `tests/l3/l3_combat.py --seeds ... --encounter <遭遇>` 单独对拍。
5. 子类字段（C# 的私有字段、`IsFront` 这类属性）放在实例的 `extra[k]` 槽位，用具名访问器包装。遭遇在创建生物之前设置怪物字段时，先用 `MonsterModel::ToMutable` 取得实例，和 C# 的 `ToMutable()` 加属性赋值一一对应。

## 14. M3 环境 API

`Env` 是面向搜索和强化学习的单人决策接口，所有状态仍来自同一个可平凡复制的
`GameState`：

- `LegalActions()` 按当前 `DecisionKind` 返回稳定顺序的完整动作集合；战斗动作包含药水使用/丢弃、手牌动作和结束回合，Choice 的多选先返回数量再返回未选索引。
- `ActionMask()` 按上述稳定动作列表置位并支持长度发现；512 是初始缓冲建议，不是动作数量上限。
- `Clone()` 深复制当前状态、静止点和选择前缀；克隆通过独立 `Sim` 执行，不共享任何池。
- `Hash()` 按生成的完整 POD 字段遍历做稳定哈希，排除 padding、容器未用尾部和池的已释放对象；包含池分配游标及 `DotnetSet` 空闲链次序，因为它们影响未来分配/插入。挂起 Choice 还包含重放静止快照、候选与选择元数据。新增状态字段后运行 `tools/gen/gen_state_fields.py`，构建检查生成结果未过期。哈希不是 RNG 或存档序列化格式，也不是跨版本持久化 ID。
- `Determinize(seed)` 只在没有挂起 Choice 时打乱隐藏的抽牌堆/弃牌堆、未访问房间队列后缀和剩余遗物袋，保持手牌、战场、地图、活动房间和可见观测不变。
- `Observe()` 的整数向量包含版本、决策阶段、玩家公开数值、手牌身份/升级、牌堆数量、敌人生物公开数值和合法动作编码；不包含 RNG 计数器、未来房间队列、抽牌堆/弃牌堆身份。

环境也可通过 `env/c_api.h` 暴露为不依赖 Python 开发头文件的 C ABI，
`python/sts2sim_env.py` 用 `ctypes` 提供单环境、动作 mask 和 `VectorEnv` 包装。
`search/reference_ismcts.*` 是只依赖上述环境接口的参考树搜索，用于验证 clone、随机 rollout 和动作选择契约；它不是最终性能实现。
