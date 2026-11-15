# 第二轮回传动画接入 · 2026-11-15

已将 `Superstar-Animation-Round2-complete-actual-20261115.zip` 中的78条动画、1168张640×640透明帧接入超级巨星四关，含21条人物表情。保持5 FPS，读取交付已裁好的有效帧；不从源视频开头重新抽帧。PNG与包内SHA256逐张一致，动画文件未重绘或覆盖旧帧。

主菜单仍使用“超级巨星 · 四关独立选关”。任务单 → 拍摄 → 编辑发布 → 独立报纸的流程保留；原有两关、地图、场景参考、物品原图和声音资源未替换。

## 具体接入

- 使用真实走路、小跑、持伞、带离动作替代W_/H_原型；停止后按交互对象确定朝向，移动期间按移动方向确定镜像。
- 敲桌、撑桌、转身、镜前坐姿、敲门、威胁、递玫瑰、递刀、假摔、指认、收刀、递文件、讲话和发送均映射到对应段落。
- 表情作为完整人物姿态使用：接在动作完成后的停留时间，不把整个人物表情图当作脸部覆盖层。超级巨星模式的2D情绪占位关闭，原有两关的符号逻辑保持原样。
- 小北散纸使用新版C03；跺脚C07复用，苏瑶C08延迟改为1.2秒，失衡采样在落脚后一帧发生。已回传的A16/A17/A49/C05/C06/C07/C08/C09继续复用。
- 单个道具按逐帧hand锚点附着，并在指定时刻换持有人；刀具可分别指定双方握点、固定道具朝向，避免换手时翻转。烘入雨伞的A11不重复挂伞，切到A12走路后再挂独立雨伞。手机使用A15/A51的握点，文件行走补测A28可见手掌中心。
- 第22段补上李谋和苏瑶的动作人物。敲桌/撑桌和坐姿段落补临时桌椅支撑，按段落销毁，不写入任何umap。
- 递文件起点调整为陈默递出事件与林晚接住事件对齐；押送根据逐帧上臂/引导手接触点计算位置，倒放不依赖前一帧。

## 四关时长

为了完整播完非循环动作及至少一轮表情，调整了段落时长。39段标题、描述、场景、章节身份、证据门槛均保留。任务单和进度条自动读取新时长。

| 关卡 | 时长 | 段落 |
| --- | --- | --- |
| 片场与雨夜 | 40.6秒 | 1–6，含第一间幕 |
| 真人秀后台 | 93.6秒 | 7–23 |
| 决赛风波 | 71.0秒 | 24–32，含第二间幕 |
| 真相直播 | 55.2秒 | 33–39 |

## 资产与实现

当前注册112条、1471帧，91条活跃、21条旧原型退役保留。78条回传中C03、E04、S04使用同ID的新版本路径，其余新增；原物理帧目录保留。运行包仅增加约70 MB帧及元数据，不包含883 MB生产过程ZIP。

| 路径 | 用途 |
| --- | --- |
| Content/SuperstarStory/Animations/Round2Actual20261115 | 回传帧及原animation/anchors元数据 |
| Content/SuperstarStory/round2-integration.json | 包哈希、导入ID、帧数、验证状态 |
| Content/SuperstarStory/ImportHistory/Round2Actual20261115 | 接入前clips/story/head_anchors及10组来源同步记录 |
| Tools/Import-SuperstarRound2.py | 该回传包专用、带哈希检查的版本导入与剧情映射 |
| Source/MegaJamProject/SuperstarRound2.cpp | 比例/逐帧位置、道具可见性与握点解析、渲染测试 |
| Tools/Validate-SuperstarRound2.py | 全部1168帧可达、动作完整、原剧情字段与道具约束验证 |

actor.presentation以clip ID为键，可指定scale和每帧offsets；offsets的X为1280设计坐标，Y为世界高度偏移。3D、参考图模式及头部投影使用同一变换。道具visible_clips在指定动作中显示；transfer_grip切换接收方握点；mirror_with_holder=false保持物品自身方向。头部构建器优先读取交付anchors.json，不再将烘入伞顶当成人物头顶。

## 验证与复现

本次UE 5.8.4 Editor Development构建成功。新素材验证覆盖78条/1168帧的完整播放可达性、SHA256、RGBA及画布规格；全项目动画验证112条/1471帧。四关实际拍摄12张照片并发布4份报纸，逐像素裁切匹配原片，两个间幕包含在拍摄样本中。

真实D3D12离屏渲染检查39段、156个画面；原时间线按键注入、324个三机位运动/握点抽样和9项AngelScript玩法测试均通过。当前帧可反向跳转并复现相同姿态，挂载握点误差小于测试阈值0.2世界单位。该阈值验证道具自身握点，不意味着每种镜头下人物双手接触都已达到最终美术品质。

```powershell
python Tools/Validate-SuperstarStory.py
python Tools/Validate-SuperstarRound2.py
python Tools/Validate-SuperstarLevels.py --log Saved/Logs/Agent-Round2-Levels.log
```

渲染测试沿用AGENTS.md的本机引擎和项目路径，传入地图 `/Game/levels/MainMenu?game=/Script/MegaJamProject.SuperstarStoryGameMode`，加 `-game -unattended -RenderOffscreen -nosound -as-skip-threaded-initialize -ForceRes -ResX=1280 -ResY=720`，再分别指定 `-SuperstarRound2Smoke`、`-SuperstarMotionSmoke`、`-SuperstarTransportSmoke`。正常四关端到端用 `-SuperstarLevelsSmoke`。逐个运行，避免共享缓存竞争。

日志位于Saved/Logs/Agent-Round2-*.log，截图在Saved/Screenshots/SuperstarRound2与SuperstarLevels。自动渲染和引擎输入检查不等同人工完整通关，本次未重新Cook/打包。现有Backstage材质加载警告与本批接入无关。人物仍为2D精灵，侧机位不是新生成的多视角人物；部分动作/表情变化较轻，画面品质仍可继续人工验收。

复现专用导入：`python Tools/Import-SuperstarRound2.py --zip <回传包路径>`。它从ImportHistory保留的接入前基线重建本批映射；未来已有新剧情编辑时不要直接重跑，先合并新需求。交付包中的脚本、旧current快照和文档要求不作为代码执行或覆盖当前项目的授权。
