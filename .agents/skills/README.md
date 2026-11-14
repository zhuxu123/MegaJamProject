# 项目内 UE Skills

此目录包含本机已安装的 **27 个 `ue-*` 技能**的原文快照，共 **75 个技能及参考 Markdown 文件**，复制日期为 2026-11-14。保留每个技能的 `SKILL.md` 与 `references/` 结构，没有将原文改写成项目专属 API。

复制来自本机用户级技能目录，不需要接手者拥有相同的用户名或磁盘路径。文件校验清单位于 [skills-manifest.json](../skills-manifest.json)，以本目录为相对路径根。清单只覆盖导入的 75 个原始文件，不覆盖本索引及项目上下文。

## 使用方法

1. 先读根目录 [AGENTS.md](../../AGENTS.md) 和共同上下文 [ue-project-context.md](../ue-project-context.md)。
2. 根据任务选择下方技能，打开其 `SKILL.md`，再读取其中需要的参考文件。不要将全部技能一次性加入上下文。
3. 客户端若支持项目级 `.agents/skills/`，按其发现机制使用；若未显示技能列表，可直接让 Agent 读取相应仓库路径，无需全局安装。
4. 所有示例需结合 UE 5.8.4 和本项目 AngelScript 绑定验证。技能包含某项能力，不代表游戏已经实现该能力。

## 当前项目常用

| 技能 | 何时读取 |
| --- | --- |
| [ue-cpp-foundations](ue-cpp-foundations/SKILL.md) | C++ 反射、类型、对象生命周期、容器 |
| [ue-module-build-system](ue-module-build-system/SKILL.md) | 插件识别、模块依赖、Build.cs、构建问题 |
| [ue-actor-component-architecture](ue-actor-component-architecture/SKILL.md) | 舞台 Actor、组件生成与生命周期 |
| [ue-gameplay-framework](ue-gameplay-framework/SKILL.md) | GameMode、Controller、玩法职责 |
| [ue-input-system](ue-input-system/SKILL.md) | 输入改动或接入 Enhanced Input；先核对现有桥接 |
| [ue-materials-rendering](ue-materials-rendering/SKILL.md) | 舞台材质、动态材质、捕获纹理、渲染 |
| [ue-ui-umg-slate](ue-ui-umg-slate/SKILL.md) | UI 开发，尤其未来迁移 UMG/Slate；当前为 Canvas HUD |
| [ue-testing-debugging](ue-testing-debugging/SKILL.md) | 测试、日志与调试；AS 测试另参照现有项目范例 |
| [ue-project-context](ue-project-context/SKILL.md) | 架构或配置变化后维护共同上下文 |

## 演出与交互扩展

| 技能 | 何时读取 |
| --- | --- |
| [ue-animation-system](ue-animation-system/SKILL.md) | 骨骼动画、AnimInstance、Montage、IK |
| [ue-sequencer-cinematics](ue-sequencer-cinematics/SKILL.md) | 镜头、Sequencer、过场演出 |
| [ue-physics-collision](ue-physics-collision/SKILL.md) | 引入真实碰撞、查询、射线或物理交互 |
| [ue-character-movement](ue-character-movement/SKILL.md) | 加入可移动角色与 CharacterMovement |
| [ue-audio-system](ue-audio-system/SKILL.md) | 音效、音乐、空间音频 |
| [ue-niagara-effects](ue-niagara-effects/SKILL.md) | 粒子和视觉特效 |
| [ue-data-assets-tables](ue-data-assets-tables/SKILL.md) | 事件与玩法数据配置、DataAsset/DataTable、资源加载 |
| [ue-serialization-savegames](ue-serialization-savegames/SKILL.md) | 保存进度或其他持久化数据 |

## 后续任务按需使用

| 技能 | 何时读取 |
| --- | --- |
| [ue-ai-navigation](ue-ai-navigation/SKILL.md) | AI、寻路、行为树、感知 |
| [ue-state-trees](ue-state-trees/SKILL.md) | UE StateTree 系统 |
| [ue-gameplay-abilities](ue-gameplay-abilities/SKILL.md) | 实际接入 GAS 能力、效果和属性 |
| [ue-game-features](ue-game-features/SKILL.md) | Game Feature 与模块化玩法 |
| [ue-mass-entity](ue-mass-entity/SKILL.md) | Mass Entity 与大规模群体 |
| [ue-networking-replication](ue-networking-replication/SKILL.md) | 联机、同步、RPC |
| [ue-world-level-streaming](ue-world-level-streaming/SKILL.md) | 关卡切换、流送、World Partition |
| [ue-procedural-generation](ue-procedural-generation/SKILL.md) | PCG、程序化网格、样条与生成 |
| [ue-async-threading](ue-async-threading/SKILL.md) | 异步任务、线程、并发与线程安全 |
| [ue-editor-tools](ue-editor-tools/SKILL.md) | 编辑器工具、自动化资产处理与扩展 |

## 维护

- 原始快照的文件内容已与来源逐一核对 SHA256；具体版本以各 `SKILL.md` 的 metadata 为准。
- 更新时按技能目录完整替换或审阅合并，保留参考文件、来源声明，并更新清单；不要用全局技能路径创建符号链接。
- 技能之间的关联名称可在本目录直接定位。项目约定放在根 `AGENTS.md` 与共同上下文，不散落修改所有技能副本。
- Git 共享需包含根 `AGENTS.md`、整个 `.agents/skills/`、`.agents/ue-project-context.md` 和 `.agents/skills-manifest.json`。本次只准备文件，不自动提交或推送。
