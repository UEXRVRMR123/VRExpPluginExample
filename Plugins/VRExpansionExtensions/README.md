# VRExpansionExtensions

`VRExpansionExtensions` 是基于 `VRExpansionPlugin` 的项目级扩展插件。检测、抓取运动状态和 UI 信息展示的完整说明见：

- [DetectableInteractionAndUI.md](Docs/DetectableInteractionAndUI.md)

## 模块

- `VRExpansionExtensions`：Runtime（运行时）模块，支持 Win64、Linux 和 Android。
- `VRExpansionExtensionsEditor`：Editor（编辑器）模块，只在 Win64/Linux 编辑器目标中加载，负责 `UVRExpDetectableComponent` 的 UI 变换标记和临时 Actor 预览。

Runtime（运行时）模块不依赖 `UnrealEd`。Editor（编辑器）模块使用 `UnrealEd` 提供的 `FComponentVisualizer` 接口编辑 World UI 预览，但不依赖引擎 `ComponentVisualizers` 模块。Android/Shipping 不包含编辑器预览代码。

## 推荐配置

对于基于 `AGrippableStaticMeshActor` 的“原地提示、抓取跟随相机、释放独立展示”对象：

1. 添加 `UVRExpGrabbableMotionComponent` 和 `UVRExpDetectableComponent`。
2. 设置 `NormalMotionMode=None`、`ReleaseMotionMode=ReturnToOrigin`。
3. 设置 `GripSourceMode=Auto`；无需填写 `GrabbableMotionSource`。
4. 在 `VRExpDetectableUIInfoTriggerLogic` 中配置 Head 和 Grip 来源开关。
5. 设置 `ReleaseBehavior=UseReleasePresentation`，再配置 Head/Grip/Release 展示位置。

## 兼容性

- `VRExpDetectableUIInfoTriggerLogic` 只使用基础 Head、Grip、Release 和 Motion 来源开关。
- 已删除 `ActivationMode`、`AdvancedRuleSet`、高级规则类型和相关调试字段；这是 Breaking Change（破坏性接口变更）。
- 旧资产中保存的基础来源开关继续生效；自定义高级规则不再迁移或执行。
- `EVRExpUIInfoInteractionSource::Release` 追加在已有枚举末尾。
- 新增释放行为默认 `DeferToCurrentInteraction`。
- 编辑器预览默认 `Disabled`，已有资产不会生成预览对象。

本次未执行 UHT（Unreal Header Tool，Unreal 头文件工具）或编译；需要构建验证时先取得项目维护者确认。
