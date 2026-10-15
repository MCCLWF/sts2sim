# Orobas / Archaic Tooth 附魔起始牌闭包

## 目标

移除 Orobas 抽到 Archaic Tooth 时对已附魔起始牌的 `Unported`，让预览和领取后实际变形沿用原牌升级、附魔种类、附魔数值与其保存状态。此工作属于先古事件的 run 级可达路径。

## 方法

1. 对照原版 `ArchaicTooth.GetTranscendenceTransformedCard`、`SetupForPlayer` 和 `AfterObtained`：创建超越牌、升级一次、克隆附魔再附着。
2. 在 `CardCmd` 建立通用附魔克隆操作，以 POD 字段复制保存属性；Orobas 的预览与 Archaic Tooth 的领取共用该操作。
3. 用定向事件测试核验升级、附魔和变形；尝试原版 oracle 直接探针，区分 C++ 定向证据与正式整局证据。

## 验收

- 已附魔 Bash 的 Orobas 选项能初始化、预览并在 Archaic Tooth 领取后成为同升级、同附魔的 Break。
- 全量门禁、生成/清单检查和 `git diff --check` 通过；报告列出未覆盖的附魔类型与真实 run 路径。
