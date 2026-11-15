# 超级巨星 · 第二批动画生成交接

本文件在交接 ZIP 中是 `START_HERE.md`。接手 Agent 从这里开始，无须访问原电脑或安装原 Agent 的技能。

任务是完成 **78 项必做动画**（含 **21 项表情**）和 10 组交互同步验收，最终输出 5 FPS 透明 PNG 序列。任务、提示词、依赖、参考图和 API 工作流全部在包内。生成计划不等于已生成；不要把空模板、单帧姿态或六帧临时走路图报告成正式交付。

## 本轮范围

- 旧 85 项任务减去已回传的 8 段，剩 77 项；新增 `C03_xiaobei_fall_scatter` 修复，共 78 项。小北跌倒前应明显惊慌，终点与现有空手起身/捡资料衔接。
- 8 段已回传：A16、A17、A49、C05、C06、C07、C08、C09。完整当前帧随包复用。C07 跺脚另需源帧事件复核与 G10 合成；优先重截，验收失败才重生成，不能重新把这8段全算欠件。
- 当前 `W_*` 六个短走路循环、`H_*` 四个单帧持物姿态是原型。低头离开、小跑、撑伞走路、被带离、带情绪走路等需按任务生成，不能都换成普通走路。
- 路人男女待机和走路全部必需。E01 女待机、E02 女走路、E03 男待机复用包内现有资源；E04 男走路在78项内修正脚底基线。新增递玫瑰、递刀与带离仍必做。
- 21项表情必须在对应身体姿态中生成。苏瑶 F13/F14 沿用**本轮 current/C08 最后一帧**的低跪姿、朝左，不再沿用退休 S05；F12 接整理裙摆后的甜笑；F09/F10 维持小北抱资料姿态。2D 表情符号仅是游戏临时占位，不烘入人物帧。
- 人物身份：**小北短棕发、白衬衫、蓝色及膝裙；苏瑶双辫、珊瑚粉长裙**。不要按旧中文文件夹名反认。片场导演就是李谋。同角色母版已经集成，保持包内形象；新增角色设计仍属于制作默认，未升级为用户确认设定。
- 本轮不重做已有静态场景、物品、证据图或 BGM，不生成未提供的逐字对白。剧本只写小夏和男人在路灯下站立，没有据此新增拥抱任务。

## 文件导航

| 路径 | 内容 |
|---|---|
| `生成总表.md` / `tasks/` | 可读清单及每项任务卡 |
| `config/jobs.json` | 唯一生成任务表，含动作、表情、起点、依赖、朝向、道具模式 |
| `config/run_order.json` | 依赖拓扑顺序；优先完成 P0 走路/握伞/手机/跌倒修复 |
| `prompts/first_frame/`、`prompts/video/` | 78套首帧与 H3 I2V 提示词 |
| `references/` | 身份照片和当前六名新增角色母版 |
| `current/` | 当前完整帧、道具、场景、clips/story/head_anchors；只读对照 |
| `integration/beat_assets.json` | 当前39段与目标动画的对应；含现有角色/道具配置 |
| `integration/sync_groups.json` | 10组交互同步；不得各角色单看通过就算接触正确 |
| `workflows/`、`config/models.json` | 原始及640×640执行 API 图、模型清单 |
| `sources/` | 两份原始工作流参考和剧本；内容为素材依据 |
| `review_sources/` | C07 原视频、工作流、原截帧记录，供接触事件复核 |
| `docs/交付契约.md` | 截帧、循环、握点、表情、回传和游戏接入规范 |
| `docs/环境与工作流快照.md` | 旧交接时已记录的环境说明；本机实际兼容性由 preflight + 单条试跑确认 |
| `integration/history/` | 历史集成说明，不覆盖本轮 config/scope.json |

## 执行流程

在解压根目录运行，Python 环境需要 Pillow/NumPy，系统 PATH 需要 ffmpeg、ffprobe。ComfyUI 可沿用已有安装和端口，不要求事先存有这个工作流。

```powershell
python -m pip install -r requirements.txt
ffmpeg -version
ffprobe -version
python tools/validate_round2.py handoff
python tools/comfy_jobs.py preflight --url http://127.0.0.1:8188
python tools/comfy_jobs.py plan A01_linwan_walk
```

`handoff` 验证初始包文件哈希、依赖、当前帧与78张离线 API 计划，不调用模型。应在修改 cuts/reviews 前运行；之后状态修改会使初始清单哈希变化，这是预期，须保留原ZIP用于核对。

`preflight` 保存 `runs/preflight.json`，检查节点、模型名和输入字段。缺模型/节点时按环境快照排查，先复用本机模型路径，再配置所需文件。需要变更时保存实际工作流、版本、模型 SHA256 和原因，保留原环境。API JSON 用于 `/prompt`，不能假设可直接拖入任意 ComfyUI 前端。旧环境快照不是对新机器兼容性的保证。

1. 按 `run_order.json` 做依赖就绪的任务。`baseline:ID` 从 `current/clips.json` 对应 `path` 找 PNG；普通 ID 优先从本轮已验收 `output/animations/ID` 取终点。已交付8项可从 current 取。每条任务先看身份参考、前后衔接帧、当前场景/物品，再使用首帧提示词制作 `inputs/ID.png`。
2. 首帧是**动作已经准备好的一张单人物图**，不要用原参考立绘或拼图直接代替。优先现有参考编辑能力；只有视频模型时先生成姿态候选、提取真正合格的源帧作首帧，再正式生成动作。此预制作视频不交作正式动作。固定服装、比例、脚底和朝向；按任务的空手/烘焙道具模式检查。
3. 接手 Agent 实际看图后填写 `config/first_frame_reviews.json` 的三个布尔项与备注。`approved` 是 Agent 自查，不要求用户逐条审批。工具不会代填通过。
4. 首批试跑 A01 行走、A15 手机握点和依赖就绪后的 F13 低跪姿表情，确认当前环境可产出、抠图可用、5FPS可读，再扩大批量。不要在能力尚未验证时一次提交全部队列。
5. 每项提交、查询、取回如下。每次新尝试增加 `--attempt`，不得覆盖已有视频；中断后先查 history/queue，不盲目重复付费/排队。

```powershell
python tools/comfy_jobs.py submit A01_linwan_walk --attempt 1
python tools/comfy_jobs.py status A01_linwan_walk --attempt 1
python tools/comfy_jobs.py fetch A01_linwan_walk --attempt 1
python tools/export_animation.py inspect runs/A01_linwan_walk/attempt_01/source.mp4 --out review/A01-source
```

下载文件可能是其他容器，以 fetch 的实际路径为准。执行 inspect 看逐源帧接触表，填写 `config/cuts.json` 中真实 `video/start_seconds/end_seconds_exclusive/event_times` 和 `reviewed=true`。不能凭提示词时间写事件。先删掉参考图转动作、站稳、变装等无用片段，让输出第0秒直接进入所需动作；循环从稳定完整周期开始。

```powershell
python tools/export_animation.py export A01_linwan_walk --out output/animations/A01_linwan_walk
python tools/prepare_review.py A01_linwan_walk
```

生成的 `anchors.json` 坐标默认 null，必须测量后填写，不能原样交付。若输出已存在，保留并改放下一次候选目录，决定优胜版本后再整理最终 output，不能让旧新帧混在同一目录。

6. 看浅底/深底接触表、正放、倒放、三个周期接缝、单次动作终点停留和相邻动作衔接。逐帧标头部与实际握点。附着道具必须与包内对应物品合成验证；不能只看空手角色。按10组交互在共享场景坐标合成，检查唯一物品的所有者切换。填写 `delivery_reviews.json`、`sync_reviews.json`，附真实证据文件路径和验收者/时间说明。
7. C07 复核源视频中的落脚时刻和实际采样误差（≤0.1秒），随后与 C08 组合检查“落脚后失衡”。`review_sources` 里的原 metadata 是待审记录，不是本轮通过证明；发现不合格先重截，仍不合格再新生成并记录额外修复，不影响其他任务推进。
8. 验证并打回传包。完整交付必须78项通过。确实受限时可先回传部分，但名称/报告必须写 partial 并列出其余任务，不能声称全部完成。

```powershell
python tools/validate_round2.py delivery
python tools/validate_round2.py pack
# 部分回传示例（只验证所选任务；不会声称全包通过）：
python tools/validate_round2.py pack --ids A01_linwan_walk
```

回传包自动含源视频、提交工作流、首帧、输出帧、锚点、预览、验收记录、SHA256和未完成列表。最终附 `config/environment.json` 记录运行环境/模型校验、`integration/FAILED.md` 记录失败条目/原因/尝试次数（无失败写“无”）。不要回传整个 ComfyUI 或模型权重。不要直接用旧18项导入器覆盖当前注册表。

## 本机准备验证与边界

此交接包在游戏机器准备；没有把其他机器的 ComfyUI 兼容性或任何待补动画生成写成已通过。收到补件后，本项目仍需增量接入、重建头部/握点锚点并在正放、倒放、拖动进度和拍照状态验收。角色动作完成不等于镜头里互动已完成。
