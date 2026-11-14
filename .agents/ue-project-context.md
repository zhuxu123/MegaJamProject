# UE Project Context

更新日期：2026-11-14。根据当前仓库生成，供随项目打包的 UE Skills 读取。完整操作、架构契约和协作约定见 [AGENTS.md](../AGENTS.md)；本文仅补充技能需要的环境与系统概览。未确认事项明确标注，不作为新增需求。

## Engine & Project Overview

- 项目：`MegaJamProject`，游戏《营销达人》，通过拍照、选图、顺序、裁切和标题影响事件解读。
- 引擎：本机验证 UE **5.8.4**，项目关联 `5.8`。引擎发行渠道、是否存在外部引擎修改未在仓库中确认。
- 平台：已验证 Windows / Win64 Editor；正式发布平台和打包验收尚未确定。配置中的其他平台段落不能视为已验证支持。
- 当前入口：`/Game/levels/Backstage.Backstage`；单关卡、单玩家回合原型。

## Module Structure

| 模块 | 类型 | 职责 |
| --- | --- | --- |
| `MegaJamProject` | Runtime / Default 加载 | 游戏专属 C++ 桥接、3D 舞台与渲染 |

- Public dependencies：`Core`、`CoreUObject`、`Engine`、`InputCore`、`EnhancedInput`。
- Private dependency：`AngelscriptRuntime`。
- 游戏脚本位于 `Script/Marketing/`；当前原生源文件平铺于 `Source/MegaJamProject/`。
- 不把原生模块与插件各自的 Runtime / Editor / Test 模块混为一谈；插件模块清单查看各 `.uplugin`。

## Plugin Dependencies

- 项目显式启用 `Angelscript`、`AngelscriptGameplayTags`、`AngelscriptGAS`，源码均随仓库提供。
- `AngelscriptGAS` 依赖 `AngelscriptGameplayTags` 和引擎 `GameplayAbilities`。
- `ModelingToolsEditorMode` 仅 Editor 目标启用。
- 当前玩法未实现 GAS Ability / AttributeSet；存在插件依赖不代表已采用该架构。
- 项目存在 CommonUI 配置片段，但当前 HUD 是 Canvas 绘制；不据此推断已完成 CommonUI/UMG 接入。
- 没有在本次扫描中确认额外 Fab 付费插件依赖；具体许可证以随源码保留的声明为准。

## Coding Conventions

- 沿用 UE 的 `A/U/F/E` 等类型前缀；C++ 头文件使用 `#pragma once`。
- 保留各文件现有格式，不做无关全局重排。交流与玩法文案使用中文，代码符号使用英文。
- AS 生命周期覆写使用 `UFUNCTION(BlueprintOverride)`，测试标记使用 `UFUNCTION(meta=(AngelscriptTest))`。
- 手工原生绑定的签名字符串明确使用 `float32/float64`；不能直接照抄 AS 的 `float` 写法。
- UObject 引用按生命周期使用 `UPROPERTY/TObjectPtr` 等机制；先查本项目模式再扩展。
- 舞台资产加载使用 `ensure` 检查；尚无要求全项目统一断言风格或新增日志分类的约定。
- 技能示例是参考，不能假定其中的 C++ API 已在 AS 暴露。接口以项目及插件绑定源码和实际编译为准。

## Subsystems in Use

| 类 / 数据 | 位置 | 职责 |
| --- | --- | --- |
| `ABackstageGameMode` | AS | 指定 Controller 和 HUD |
| `ABackstageController` | AS | 输入、回合推进、本地生成舞台 |
| `ABackstageHUD` | AS | Canvas 界面、照片与预览绘制 |
| `FBackstageRound` | AS | 阶段、拍摄资源、选图裁切、发布结算 |
| `ABackstageStage` | C++ | 几何人物、场景捕获、投影证据和帧缓存 |

- 目前没有项目自定义 Pawn/Character、GameState 或 UE Subsystem 类；不套用角色移动模板替换现有拍摄玩法。
- 输入由 Controller 调用原生 `Pressed/Held` 与鼠标桥接；虽依赖 EnhancedInput 模块，当前玩法没有以 InputAction/MappingContext 资产组织输入。
- UI 使用 `1280×720` 设计坐标和等比居中，点击命中必须与缩放反变换一致。
- 演出按指定时间直接求值，几何网格禁用碰撞；不是 Chaos 物理、骨骼动画或 Sequencer 实现。
- 照片只存拍摄参数，多个不同时间的画面由同一舞台重绘。引入新动画或物理方案必须保持可复现。
- 目前没有项目实现的联网同步、存档、AI 导航、物品背包或独立对话系统。技能包包含相关技能仅供后续任务按需使用。

## Build Configuration

- 目标：`MegaJamProject`（Game）、`MegaJamProjectEditor`（Editor）；尚无专用 Server/Client target。
- 两个目标均设置 `BuildSettingsVersion.V7`、`EngineIncludeOrderVersion.Unreal5_8`。
- 原生桥接变更需重建 Editor；AS 普通变更可热重载并检查编译结果。DLL 占用时协调保存关闭编辑器。
- 构建命令与 `AngelscriptTest` 验证命令见根目录 `AGENTS.md`。当日已知逻辑测试基线为 5 项，不等于未来每次修改自动通过。
- `NullRHI` 不验证真实输入和渲染；舞台、材质、镜头、HUD 修改还需带渲染验收。
- AngelScript 的第三方运行时随插件源码提供；游戏模块未额外声明第三方库。

## Team Context

- 多位开发者使用 Agent 同步开发，人数和角色分配未固定；使用 Git。
- 优先独立 checkout/worktree；同目录按文件约定负责人，尤其共享绑定、核心脚本和构建配置。
- `.uasset/.umap` 采用单人编辑与协调集成；不要假定 Git LFS 或资源锁已经配置。
- 构建、缓存写入和编辑器重载需避免同目录争用。不要清理或覆盖其他开发者的工作。
- 任务分支默认可用 `codex/<task>`；提交范围、验证事实和交接格式遵循根目录 `AGENTS.md`。
