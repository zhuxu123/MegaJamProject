# MegaJamProject — Agent 开发与协作指南

本文件适用于整个仓库，供人类开发者和编程 Agent 共同使用。开始任务前先阅读；有更深层的 `AGENTS.md` 时，同时遵守其局部约定。用户当前明确指令优先。默认用中文沟通，代码符号沿用英文。

## 1. 项目定位与当前基线

- 游戏名：**营销达人**。玩家通过机位、拍摄时刻、选图顺序、裁切与标题，改变观众对事件的理解。
- 当前可玩关卡：**慈善活动后台**。流程为简报 → 50 秒现场 → 选择并编辑 3 张照片 → 预览/发布 → 关注度、公司信任、公众信任与人物反馈。
- 已实现：3 个机位、6 次快门、1 次回看、鼠标选图与选槽、键盘构图/裁切、顺序调整、3 类标题、结算与重新开始。
- 3D 基线：艺人、志愿者、工作人员三个几何人物，以及后台布景、物品掉落、争执、弯腰捡起和归还演出。
- **当前碰撞是程序化演出，不是物理碰撞系统。** 舞台网格使用 `NoCollision`；没有骨骼角色、AnimBP 或 Sequencer 驱动。不要把这些列为已完成能力。
- 本地验证引擎为 **UE 5.8.4**；`.uproject` 的 `EngineAssociation` 为 `5.8`。目录名 `Ue4_Project` 不代表使用 UE4。
- 本文基线核对日期：2026-11-14。当前源码和本次验证结果比历史说明更权威；修改下述契约时同步更新本文。
- 原始设计草案曾由用户提供为《营销达人游戏设计草案.docx》，目前不是仓库内的共享文件。不可假定其他开发者能访问原机器的附件路径；缺失设计细节时明确区分建议与已确认需求。

## 2. 首次接手：先读什么

先执行 `git status --short` 和 `git diff --stat`，辨认已有改动。随后按顺序阅读：

### 仓库内的 UE Skills

本项目随仓库携带 27 个 UE 技能及完整参考文件，入口是 [.agents/skills/README.md](.agents/skills/README.md)，共同上下文是 [.agents/ue-project-context.md](.agents/ue-project-context.md)。不依赖原开发者的用户目录，也不需要重新下载技能。

- 根据当前任务从索引选择相关技能，先读对应 `SKILL.md`，再按需读其 `references/`；不要每次加载全部技能。
- 支持项目 `.agents/skills/` 发现的客户端可按自身机制加载；未自动发现时直接按仓库路径读取。不能假定所有 Agent 客户端都自动安装了这些技能。
- 技能里的 UE C++ 示例必须结合本项目 AngelScript 绑定和当前引擎验证。以用户要求、本文项目约定和实际源码为准；不因技能描述就迁移到 GAS、UMG、Sequencer 或增强输入资产体系。
- 技能包是原文快照，项目差异记录在上下文中。修改技能时说明来源与原因，并更新 `.agents/skills-manifest.json` 中对应文件的 SHA256、字节数及实际快照信息。

### 代码阅读顺序

| 文件 | 职责 |
| --- | --- |
| `MegaJamProject.uproject` | 游戏模块、引擎关联、启用的插件 |
| `Config/DefaultEngine.ini` | 默认地图与脚本 GameMode |
| `Script/Marketing/BackstageRound.as` | 回合状态、照片数据、时间段、裁切约束、证据解读与评分 |
| `Script/Marketing/BackstageGame.as` | GameMode、PlayerController 输入、Canvas HUD 与流程呈现 |
| `Script/Marketing/BackstageRoundTests.as` | 玩法回归测试及可参考的 AS 测试写法 |
| `Source/MegaJamProject/BackstageStage.h/.cpp` | 3D 舞台、时间驱动姿态、机位投影、帧缓存与画面证据 |
| `Source/MegaJamProject/BackstageScriptBindings.cpp` | 原生功能注册到 AS：输入、鼠标坐标、HUD、舞台与渲染 |
| `Source/MegaJamProject/MegaJamProject.Build.cs` | 模块依赖，包含 `EnhancedInput` 与 `AngelscriptRuntime` |
| `Content/levels/Backstage.umap` | 当前入口地图 |
| `Content/Backstage/M_BackstageStage.uasset` | 舞台材质，代码使用向量参数 `Color` |
| `Plugins/Angelscript/README.md` | 本仓库插件快照的能力、绑定、缓存和测试说明 |

默认地图为 `/Game/levels/Backstage.Backstage`，默认 GameMode 为 `/Script/Angelscript.ABackstageGameMode`。舞台由脚本 Controller 在本地 `BeginPlay` 中生成，不是预先摆满人物的地图。

## 3. AngelScript 插件使用约定

### 插件来源与边界

- 上游宿主项目来源：`TDGameStudio/AngelscriptProject`。当前实际使用的是仓库内的插件源码快照，以本地 `.uplugin`、README 和源码为准。
- 已启用 `Plugins/Angelscript`、`Plugins/AngelscriptGameplayTags`、`Plugins/AngelscriptGAS`。GAS 插件还声明依赖 UE `GameplayAbilities`。启用插件不等于当前玩法已使用 GAS。
- 主插件 README 的 Requirements 提到 UE 5.7；本项目已在 UE 5.8.4 构建运行。不要据此擅自降级引擎，也不要直接用上游最新代码覆盖本地插件。
- 日常玩法写在项目 `Script/`；游戏专属原生桥接写在 `Source/MegaJamProject/`。不要为了一个关卡功能修改插件内部编译器、绑定生成器或引擎源码。
- 确需修复插件时，单独说明原因、改动范围、兼容性与验证方式；避免夹带在玩法修改中。

### AS 编写与 API 查证

- 以当前可编译脚本为语法范例：Actor 类继承 UE 类型，生命周期覆写使用 `UFUNCTION(BlueprintOverride)`，默认属性使用 `default`，需要反射/GC 跟踪的对象成员使用适当的 `UPROPERTY()`。
- 不把标准 AngelScript、UE C++ 或另一套 AngelScript 集成的示例直接当作本插件可用 API。先搜索项目绑定和 `Plugins/Angelscript/Source/AngelscriptRuntime/Binds/`，再用当前项目编译验证。
- API 不存在时，优先复用现有项目绑定；确有需要再添加最小原生桥接。`BackstageScriptBindings.cpp` 已提供 `FAngelscriptBind`、`ExplicitBindings`、`ExistingClassForTarget` 和命名空间绑定范例。
- 手工 C++ 绑定签名中用明确的 `float32` / `float64`，与 C++ 实参和返回值匹配；不要使用含糊的 `float`。脚本中的 `float` 写法不能直接照搬到绑定声明字符串。
- 新增 UObject 引用要考虑 UE 垃圾回收与所有者生命周期；遵循现有 `UPROPERTY` / `TObjectPtr` 模式。
- 普通 `.as` 修改可利用编辑器热重载，但必须查看编译结果并重新进入玩法验证。C++、反射结构、模块依赖或插件配置变更需重建 Editor；不能把脚本热重载当作原生 DLL 更新。
- 脚本生成类位于 `/Script/Angelscript`。重命名已有 AS 类时核对地图/配置/蓝图引用；类重定向保留 AS 类实际的 `A` / `U` 前缀，参考插件 README。
- `Script/Binds.Cache`、`Script/Binds.Cache.Headers` 与 `Saved/Angelscript/CacheV2` 是生成数据，不手改、不提交。缓存异常先检查日志，不用清空整个项目目录来试错。
- Standalone 是独立工具链。UE profile 的离线验证需要匹配本项目导出的完整 bundle，不能替代真实 UE 中的运行、输入与渲染验证。插件 README 中的上游宿主 `Tools/` 命令，执行前确认脚本确实存在于当前 checkout。

## 4. 必须保持的玩法与渲染契约

### 状态与照片

- 状态枚举：`Briefing → Live → Edit → Preview → Results`，普通预览结束回到 `Edit`，发布预览结束进入 `Results`。
- `Photos` 保存原始拍摄参数，`Edits` 保存各照片的裁切参数，`Slots` 保存 3 张照片的顺序索引，`Cut` 是进入预览/发布时生成的序列。
- 照片目前保存 `Time / Station / Pan / Zoom`，由同一个 3D 舞台重绘，**不是持久化截图文件**。同一帧 HUD 可能请求多个不同时间的照片。
- 因此姿态必须能根据指定时间直接求值，不依赖上一帧、播放方向或某个只触发一次的事件。加入物理、随机行为或动画系统时，必须同时设计可复现的拍照/回看方案。
- 回看消耗唯一一次机会，时间归零，保留照片与剩余快门。未选满 3 张不得预览或发布。裁切不得显示原始照片之外的内容。

### 时间段与画面证据

| 时间（秒） | 逻辑事件 |
| --- | --- |
| 0 ≤ t < 7 | 入场 |
| 7 ≤ t < 14 | 碰撞与掉落 |
| 14 ≤ t < 23 | 争执 |
| 23 ≤ t < 33 | 捡起物品 |
| 33 ≤ t < 42 | 归还物品 |
| 42 ≤ t ≤ 50 | 同框收尾 |

- 时间段位于 AS `BeatAt`，具体动作曲线位于 C++ `Block/Pose`；评分还有更细条件，例如碰撞证据需要 `Time < 9`。修改时要同时核对，不能只改一处时间数字。
- `BackstageRendering::Evidence` 返回位掩码：`1` 艺人、`2` 志愿者、`4` 物品。AS 结合可见证据、照片顺序和标题计算结果。
- 当前 Evidence 使用角色/物品代表点的投影和画幅边界，不包含遮挡射线或完整轮廓识别。新增遮挡布景时要评估“看不见却仍算证据”的情况。
- 构图、裁切与 Evidence 必须采用一致的相机空间。AS 的 `ProjectionScale` 与 C++ 的 `WideWidth/PanUnits` 有对应关系，改镜头尺度时一起核对。
- 当前是正交相机；界面称 A/D 为“转向”，实际实现为相机横移。要改成真实旋转/透视时，必须连同裁切约束和证据投影一起改。

### 帧缓存、材质与输入

- 画面比例为 `8:3`；显示时保持比例，不任意拉伸。现有缓存槽：`0` 现场/单张预览，`1–6` 原图缩略图，`7–9` 剪辑槽/结果图；C++ 当前只接受 `0–12`。
- 一个槽的缓存键包含时间、机位、Pan、Zoom。不要让同一显示周期内需要保留的不同照片互相覆盖同一 RenderTarget。
- 当前只有材质 `IsComplete()` 后才记入缓存键，避免把着色器未就绪时的灰色画面永久缓存。修改缓存逻辑时保留这个保障或提供等效方案。
- HUD 使用 `1280×720` 设计坐标，按最小缩放比例居中。鼠标桥接执行相反的缩放/偏移；改布局、分辨率适配或点击区域时必须一起改。
- A/D、Q/E 同时支持短按步进和长按连续移动。曾修复“按键看似无效”和“鼠标不能选图”，这是必须保留的回归项。

## 5. 构建、运行与验证

以下命令在**仓库根目录 PowerShell**执行。引擎路径是本机示例，其他机器按实际安装位置修改；不要把本机绝对路径写入游戏代码。

```powershell
$ProjectRoot = (Get-Location).Path
$ProjectFile = Join-Path $ProjectRoot 'MegaJamProject.uproject'
$EngineRoot = 'G:\UE_Version\UE_5.8'
$BuildTool = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$EditorCmd = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$LogDir = Join-Path $ProjectRoot 'Saved\Logs'
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$RunId = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$BuildLog = Join-Path $LogDir "Agent-Build-$RunId.log"
$TestLog = Join-Path $LogDir "Agent-Tests-$RunId.log"
```

首次构建需要匹配引擎的 Windows C++ 工具链和 Windows SDK。先确认同一 checkout 的编辑器已保存并关闭，且没有其他开发者正在构建；不要强制终止别人的编辑器。

```powershell
& $BuildTool MegaJamProjectEditor Win64 Development "-Project=$ProjectFile" -WaitMutex -NoHotReload -DisableAdaptiveUnity "-Log=$BuildLog"
if ($LASTEXITCODE -ne 0) { throw "Editor build failed: $BuildLog" }
```

游戏脚本测试：

```powershell
& $EditorCmd $ProjectFile -run=AngelscriptTest -Unattended -NullRHI -NoSound -ddc=NoShared "-ABSLOG=$TestLog"
if ($LASTEXITCODE -ne 0) { throw "AngelScript tests failed: $TestLog" }
Select-String -LiteralPath $TestLog -Pattern 'Reflected script-test commandlet summary'
```

- 核对退出码和汇总，不能把“进程启动了”或“零项被选中”当通过。文档基线为 `selected=5 executed=5 passed=5 failed=0`；后续新增测试应按新数量核对。
- 上述命令运行项目反射脚本测试。插件 README 中 `bCompileAngelscriptUnitTests` 是另一项原生插件自动化测试注册开关，不要为跑这 5 项项目测试随意修改构建配置。
- 新增 AS 测试参考 `UBackstageRoundTests : UAngelscriptTestSuite`，类标注 `AngelscriptTestFlags="CommandletContext;EngineFilter"`，测试方法为无参 `void()` 且标注 `UFUNCTION(meta=(AngelscriptTest))`。
- 纯文档修改检查路径、命令和差异即可；玩法逻辑修改跑相关脚本测试；原生代码修改先重建再测试；输入、HUD、材质或演出修改还需实际带渲染运行。`NullRHI` 测试不验证画面和真实鼠标输入。
- 历史测试日志和截图在 `Saved/`，已被 Git 忽略，其他 checkout 不一定拥有。不要把旧日志称为本次测试结果。

### 手动验收

在编辑器打开 `Backstage` 地图，Play 后点击游戏视口取得焦点，Enter 开始。

| 阶段 | 操作 |
| --- | --- |
| 现场 | `1/2/3` 切机位；`A/D` 左右构图；`Q` 拉远、`E` 拉近；空格拍照；`R` 回看；拍够 3 张后 Enter 剪辑 |
| 剪辑 | 鼠标点缩略图或 `1–6` 选入/移除；鼠标点槽位或 `C` 选槽；`Z/X` 交换相邻顺序；`A/D/Q/E` 裁切；`T` 换标题 |
| 预览/发布 | 选满 3 张后 `P` 预览、Enter 发布；普通预览可 Backspace 返回；剪辑中 Backspace 返回简报 |
| 结果 | Enter 重新开始 |

验证碰撞/争执/捡起/归还，可观察约 `8/17/27/37` 秒，并检查侧机位、缩略图、裁切与预览颜色一致。需要验证评分时，参考测试中的 `8→17→27`、`27→37→46` 序列与裁切反例；不仅检查结算页能打开。

## 6. 多开发者 / 多 Agent 协作

1. **先对齐范围。** 开始时说明任务目标、预计修改的文件和共享接口；交接用下方模板。只执行已授权的任务，不因看到待办就扩展成大重构。
2. **优先独立 checkout/worktree。** 每个开发者使用独立工作目录和任务分支；默认可用 `codex/<task>`。同目录中仅切换分支不能隔离正在运行的编辑器、Agent 和构建产物。
3. **共享目录时按文件分工。** 同一文件尽量由一人修改，尤其 `BackstageGame.as`、`BackstageRound.as`、绑定文件、`.uproject`、`Build.cs` 和公共配置。发现未预期改动先读差异，保留对方工作；不要用整文件覆盖解决冲突。
4. **共享接口先约定。** 修改 AS/C++ 函数签名、Evidence 位含义、相机参数、时间段或缓存槽之前，把调用方与实现方列齐；在同一可验证变更中更新。跨任务接口变更主动通知协作者。
5. **二进制资源单人编辑。** `.uasset/.umap` 不适合文本合并。编辑前在协作渠道约定所有者与范围；不要默认仓库已经配置 Git LFS 或文件锁。确需并行时拆分独立资产，再由负责人集成。
6. **构建与编辑器有占用关系。** 同一 checkout 同时只安排一位开发者更新 DLL、执行会写共享缓存的验证或重载插件。`-WaitMutex` 不能代替协作约定。等待占用释放期间可继续独立文档或代码审查。
7. **保持任务差异可审阅。** 不顺手格式化插件/全仓库、不升级依赖、不重命名共享资产、不提交机器相关配置。禁止擅自 `reset --hard`、`clean -fd`、覆盖他人文件或强推来“清理环境”。
8. **提交前逐项核对。** 使用明确文件路径暂存，不默认 `git add .`；检查 `git diff --check`、状态和实际 diff。`Binaries/Intermediate/Saved/DerivedDataCache`、IDE 文件与生成绑定缓存不提交。
9. **交付记录事实。** 说明已实现内容、改动文件、接口变化、验证命令和结果、未验证部分。未编译、未看到画面、仅跑过逻辑测试要分别写清，不把计划写成完成。
10. **同步维护指南。** 更换入口、添加依赖、改变按键/时间线、拆分模块或新增验证方式时更新对应章节。不要把个人机器临时问题写成所有机器必须遵守的项目规则。

建议按职责分任务：玩法与结算、输入与 HUD、3D 舞台与美术、底层绑定与构建。职责是协作边界，不是永久所有权；涉及同一文件时先约定串行集成。

### 任务/交接模板

```text
任务：
工作分支 / checkout：
目标与验收条件：
负责文件 / 二进制资产：
共享接口变化及需协调的人：
已完成内容：
验证命令、结果、日志或截图位置：
未验证内容 / 已知限制：
下一步与交接事项：
```

## 7. 常见问题排查顺序

- **插件识别不到：** 检查 `Plugins/<Name>/<Name>.uplugin` 是否多套了一层目录、`.uproject` 是否启用、依赖是否齐全、Editor target 是否成功构建。源码插件不能只靠复制目录就假定 DLL 已可用。
- **链接失败或 DLL 被占用：** 检查本 checkout 的编辑器/构建进程，协调保存关闭，再重建；不重复启动多个构建争抢 DLL。
- **AS API 找不到或绑定失败：** 查看第一条编译/绑定错误，核对本地绑定、模块依赖、签名类型和注册阶段，不先猜函数名。
- **按键/点击无效：** 检查视口焦点、阶段分支、Pressed/Held、输入模式、设计坐标与鼠标坐标变换，再检查玩法状态；不要仅看 UI 提示就判定修复完成。
- **照片灰色、错帧或裁切评分异常：** 检查材质路径及 `Color` 参数、着色器就绪、缓存槽/键、确定性姿态、相机投影与 Evidence 是否一致。
- **场景或 GameMode 不对：** 核对默认地图、地图上的 GameMode override 和脚本编译日志；不要直接替换整个关卡来绕过配置问题。
