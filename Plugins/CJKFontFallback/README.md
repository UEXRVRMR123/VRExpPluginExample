# CJKFontFallback

`CJKFontFallback` 是 UE 5.5 的独立 Runtime Plugin（运行时插件）。它只在内存中补充 Editor Slate（编辑器界面）和 UMG（界面系统）的复合字体链，不保存或修改引擎、工程配置和字体资产。

## 行为

- 插件位于 `Plugins/CJKFontFallback`，通过 `EnabledByDefault=true` 自动启用，不需要修改 `.uproject`。
- `PostEngineInit` 阶段校验三份字体的 SHA-256，然后补充 `FCoreStyle`、`/Engine/EngineFonts/Roboto` 和已加载的 Runtime `UFont`。
- 后续通过 `PostLoadMapWithWorld`、Editor `OnAssetLoaded` 和 1 秒低频 Ticker 捕获动态加载的字体。
- 所有字体修改都发生在 Game Thread（游戏线程）；不会调用 `Modify()`、`MarkPackageDirty()` 或 `SavePackage()`。
- 字体缓存只在复合字体实际发生变化时刷新。

## Unicode 17 路由

| 字符区间 | 字体 |
| --- | --- |
| U+3400-U+4DBF | SourceHanSansSC-Regular.otf |
| U+4E00-U+9FFF | SourceHanSansSC-Regular.otf |
| U+F900-U+FA6D | SourceHanSansSC-Regular.otf |
| U+FA70-U+FAD9 | PlangothicP1-Regular.ttf |
| Extension B-F、I | PlangothicP1-Regular.ttf |
| U+2F800-U+2FA1F | PlangothicP1-Regular.ttf |
| Extension G、H、J | PlangothicP2-Regular.ttf |

兼容区按字体实际 cmap（字符映射表）拆分：Source Han Sans 2.005R 覆盖 `F900-FA6D`，Plangothic P1 覆盖剩余已分配的 `FA70-FAD9`。这样不会把 Source Han Sans 不包含的 106 个兼容字错误路由到该字体。

## C++ API（接口）

```cpp
#include "CJKFontFallback.h"

FCJKFontFallback::ApplyToCompositeFont(
    CompositeFont,
    ECJKFontFallbackMergePolicy::PreserveExistingRanges);

FCJKFontFallback::ApplyToFontInfo(FontInfo);
FCJKFontFallback::RefreshLoadedFonts();
```

`ApplyToCompositeFont` 和 `ApplyToFontInfo` 必须在 Game Thread 调用。`RefreshLoadedFonts` 可以从其他线程调用，它会切回 Game Thread。

控制台命令：

```text
CJK.FontFallback.Refresh
```

## 合并规则

- 自动处理普通自定义 `UFont` 时使用 `PreserveExistingRanges`：已有、无 Culture（文化区域限定）的 CJK 子字体范围优先，插件只填补空白。
- `FCoreStyle` 和默认 Roboto 使用 `OverrideTargetRanges`：插件替换重叠的无 Culture 范围。
- 已有带 Culture 的子字体始终保留；Culture 匹配时，UE 自身的优先字体规则继续生效。
- 注入条目通过唯一 Typeface（字族条目）名称和字体路径识别，重复刷新不会重复添加。

## 字体与授权

- Source Han Sans SC 2.005R，Regular，SIL Open Font License 1.1。
- Plangothic V2.9.5795，P1/P2 Regular，SIL Open Font License 1.1。
- 完整许可证位于 `Resources/Licenses`，来源和 SHA-256 位于 `ThirdPartyNotices.txt`。
- 三份字体原始大小合计 49,399,744 字节；最终 Win64/Android 包增量由 UFS/PAK 压缩率决定。

## 限制

- 自动扫描覆盖标准 Runtime `UFont/FCompositeFont` 路径。
- 第三方私有 `FStandaloneCompositeFont`、原始字体文件或无法提供复合字体的 FontFace（字体面）需要主动调用公开 API。
- 只覆盖 Unicode 17 的 CJK 基本区、扩展 A-J 和兼容区，不覆盖 IVS（表意文字异体字选择序列）、PUA（私用区）或未来 Unicode 18 新区块。
- 后备字形统一使用 Regular；原默认字体的 Latin、字号、材质、字重选择等信息不变。
- `FCoreStyle` 适配层通过 `const_cast` 修改 UE 5.5 的共享默认字体，源码中有严格的 UE 5.5 编译期保护；升级引擎后必须先复核该适配层。

## 验证

Automation Test（自动化测试）名称：

```text
CJKFontFallback.Fonts.Unicode17Routing
CJKFontFallback.Fonts.PreserveExistingRanges
```

推荐手工检查 Editor 菜单/资产名称/Details Panel、默认 Roboto UMG、自定义复合 `UFont`、PIE、Win64 独立运行和 Android 真机包。日志中应出现字体校验成功与首次应用记录，不应出现字体哈希失败或 `Last resort fallback font requested`。
