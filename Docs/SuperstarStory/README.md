# 超级巨星：独立剧情预演与 3D 布景初版

2026-11-15。按《剧本全.docx》将现有交付图接入项目；原有第一关、第二关及两张地图不作替换。现已支持剧情预演、相机构图、拍照确认、三图剪辑和本地发布；尚未为完整剧本编写独立的信任/关注度评分规则，不能套用原两关评分宣称通关。

## 进入与操作

运行项目，在原主菜单按 **3** 或点击 **超级巨星 · 完整剧情预演**。

| 操作 | 效果 |
| --- | --- |
| 空格 / 播放按钮 | 播放、暂停 |
| 左右方向键 / 上下段按钮 | 切换剧情段落 |
| F1、F2、F3、F4 | 跳到对应幕开头 |
| 1、2、3 | 正面、左侧、右侧机位，平滑切换 |
| WASD / 在画面内按住左键拖动 | 调整水平与垂直构图 |
| Q/E / 滚轮 | 拉远、拉近；相机FOV变焦 |
| Enter | 拍照；确认界面再次Enter或Y保留，N/Backspace重拍 |
| Tab | 打开相册/返回摄影，最多保留6张、选3张发布 |
| R | 重置当前段落到开头，保留播放/暂停状态 |
| 点击进度条 | 暂停并定位当前段落；动作帧按时间确定性求值 |
| B / 参考图按钮 | 在独立 3D 布景和原始参考图片之间切换 |
| 第四幕证据卡 | 点击或拖动选择，再点发送；也可 V 选择、再次 V 发送 |
| ESC / 返回按钮 | 现场返回原主菜单；照片界面先返回上一级 |

未发送证据时自动播放停留在对应段落。预演导航仍可跨段查看，不作为正式关卡胜负判定。

相册：1–6或点击原片选入/移除，点击槽位或C选择裁切对象，WASD调整裁切中心，Q/E裁切缩放，Z/X交换顺序，T切标题，P预览，Enter发布。裁切不会显示原图外的内容。发布为本地生成图片和记录，不向网络或他人发送。照片与每次发布的独立版本在 `Saved/SuperstarPhotos/`。6张用完可返回主菜单重开一轮；重拍拒绝不占名额。参考图对照与报纸信息页不开放快门。

表情使用 `head_anchors.json` 的267个逐帧头顶锚点；3D模式按精灵实际变换及当前相机投影，参考图模式按同一帧与镜像换算。跌倒/起身、移动、变焦不再使用固定屏幕高度。越出取景框的符号隐藏，避免钉在画面边缘。替换动画后运行 `python Tools/Build-SuperstarHeadAnchors.py` 并检查锚点；当前提取器适用于这批没有举物越过头顶的素材，未来举伞等动作需人工复核或覆写锚点。

摄影与表情验证：原生Editor构建成功；`-SuperstarPhotoSmoke` 在实际渲染中覆盖拒绝、三个机位取景、三张保存、选片、排序、裁切、预览和发布。`Tools/Validate-SuperstarPhotos.py <publication.json>` 验证发布图逐像素等于原图对应裁切。日志在 `Saved/Logs/Agent-Photography-*.log`，玩法回归9/9通过。桌面键鼠验证工具启动失败，未将自动状态流程等同于手动键鼠验收；未重新打包发布版。

## 本次接入

- 四幕、两个间幕，共 39 段。完整剧情顺序在 `Content/SuperstarStory/story.json`。
- 18 组、267 张透明 PNG，按交付的 **5 FPS** 播放。使用已截取的有效帧，不重新加入源视频开头参考图过渡。
- 循环片段按帧数取模；非循环片段播完停在末帧，不为了填满段落反复重播。不增加插帧或逐帧裁边。画布 640×640，脚底基点 `(320,570)`。
- 小北使用短棕发、白上衣、蓝裙；苏瑶使用双辫、珊瑚粉连衣裙。保留用户纠正后的身份。
- 男女路人的待机和走路全部有可达段落。
- 场景参考 4 张、报纸参考 1 张原样复制。来源和对应关系见下表。
- 3D 布景为运行时构建的独立 Actor：办公室片场、户外路灯、照片墙走廊、舞台、杂物间通道、化妆间、新片场。使用真实网格、灯光、透视相机与场景中的透明人物平面；不是骨骼角色，也没有物理碰撞。报纸段落使用图片合成。
- 第四幕三个证据卡与发送入口已接通；证据原图尚未提供，当前文字卡占位，不伪造证据照片。

| 用户文件 | 项目副本 | 用途 |
| --- | --- | --- |
| 场景参考/演播室.png | References/studio.png | 第一、四幕片场及新片场参考 |
| 场景参考/户外.png | References/outdoor.png | 两次路灯间幕 |
| 场景参考/直播间.png | References/dressing.png | 化妆间，走廊/杂物间的临时参考背景 |
| 场景参考/观众席.png | References/stage.png | 舞台 |
| 物品参考/报纸.png | References/newspaper.png | 第四幕新闻结果，界面覆盖三个绿色图框 |

所有项目副本位于 `Content/SuperstarStory/`。参考图本身含有人物，因此图片对照模式可能出现背景人物；3D 布景独立重建，没有把这些背景人物当作新角色。没有修改用户原图。

## 素材缺口和临时替代

本次使用的动画包尚有此前验收记录中的绿边、脚底基线与跺脚停顿问题；接入不等于将其改判为最终美术验收通过。

1. 第二幕撕照片：完整剧本写“照片边缘贴纸或旧标签”，现有 `C01` 明确撕裂整张照片。需要重生成歧义动作。
2. 小北先扶膝站起再蹲下，缺少对应无资料起身动作；此段用站姿临时占位，后续收集、抱起、逃离已有动画。不要误用持纸起身覆盖这个语义差异。
3. 林晚走路、敲桌、抢伞、撑伞、拍照、表情、偷听、握拳、镜前坐姿、握刀、对镜头讲话和发送手势等仍待补。没有对应动作的段落使用待机与位移，并有画面提示。
4. 小周、女主演员、导演/李谋、陈默、小夏与男人没有本次专属人物交付，用带姓名标记的路人替身或文字提示。原始剧本角色不因替身而改名。
5. 第三幕苏瑶捂肩指认暂用现有跌坐；拖走林晚、玫瑰、道具刀、文件、雨伞交互仍未完成专用演出。
6. 三张证据原图、情绪图标、对白、配音、掌声和尖叫待补。剧本未给五年前真相的具体对白，本次不自行编造。
7. 3D 为布景初版，用基础网格表达空间与道具；材质和模型还需精修。人物没有骨骼、口型或真实受力；原有照片玩法没有迁移到这套预演。

逐段缺口也保存在 `story.json` 的 `note` 字段；新素材生产可据此对照，不要只按旧动作需求表重做 18 项就假定全剧齐全。

## 更新生成机的新素材

Python 需 Pillow。生成机保留原有 clip ID、640×640 RGBA、5 FPS 和脚底基点；角色身份不能再交换。

```powershell
python Tools/Import-SuperstarAnimations.py --source 'D:/NewDelivery/final' --replace
python Tools/Validate-SuperstarStory.py
```

导入器先验证全部 18 项，再只更新 `Content/SuperstarStory/Animations` 与 `clips.json`。原目录、原地图、原有场景素材均不动。不自动清理历史多余帧；运行时按新 manifest 的帧数读取。重启预演加载新资源。新增动作需要给 `story.json` 关联新 ID，并相应扩展导入/验证的预期清单。

段落时长为预演节奏安排，不声称是剧本指定秒数。若新非循环动作变长，需要调整对应段落 `duration`，避免动作尚未结束就跳段。

## 代码与隔离边界

| 文件 | 职责 |
| --- | --- |
| Source/MegaJamProject/SuperstarStory.h/.cpp | 独立 GameMode、Controller、HUD、加载、时序和证据交互 |
| Source/MegaJamProject/SuperstarStage.h/.cpp | 独立布景、镜头、材质实例、世界空间精灵 |
| Content/SuperstarStory/story.json | 四幕 39 段的镜头数据及占位标记 |
| Content/SuperstarStory/clips.json | 帧数、循环、基点、路径、逐帧 SHA256 |
| Tools/Import-SuperstarAnimations.py | 新动画替换入口 |
| Tools/Validate-SuperstarStory.py | 素材完整性和剧情引用验证 |

原文件增加主菜单入口、`Json` 模块依赖和新目录 UFS 打包配置。通过 `MainMenu?game=/Script/MegaJamProject.SuperstarStoryGameMode` 使用已有入口地图承载独立运行模式，**不保存或覆盖 MainMenu.umap / Backstage.umap**。原有 `BackstageRound`、舞台脚本和旧资源没有改动。后续表情占位接入只在原生绑定文件中增加了读取现有四种情绪贴图的函数，不更改旧关卡行为。

### 2026-11-15 补充：角色身份与表情

用户确认片场导演就是李谋，当前预演显示名已统一。依据最新要求，剧情先复用原有 `Emotion_Anger / Emotion_Shock / Emotion_Care / Emotion_Warm` 四种2D符号并附中文情绪标签。`story.json` 中每个人物用 `emotion_placeholder` 和 `emotions` 控制；这不是新生成的面部动画。符号由 `BackstageArt.h` 的只读桥接取得，新生成表情接入后可按角色状态关闭。

完整补充生产包在 `Deliverables/Superstar-MissingAssets-20261115.zip`：包含新增动作、21种表情循环、返修、角色母版、道具、证据图及逐段映射，继续复用原有场景。

### 2026-11-15 补充：场景配乐

七首原创合成器乐BGM已接入39段剧情，自动按氛围换曲并交叉淡化；空格同步暂停，M静音，退出预演停止音乐。文件、逐段用途、生成源码及真实混音验证方法见 [BGM说明](BGM.md)。原有两个玩法入口不受影响。

贴图由 HUD 的 UPROPERTY 缓存，按段落释放不再使用的引用；没有永久 AddToRoot。动态网格和精灵属于新布景 Actor，换景只清理自己创建的组件。3D 动画帧来自与参考图模式相同的绝对时间索引。

## 验证方式

按根目录 AGENTS.md 构建 Editor 并运行原有 AngelScript 测试，再运行资源验证器。

```powershell
& $EditorCmd $ProjectFile '/Game/levels/MainMenu?game=/Script/MegaJamProject.SuperstarStoryGameMode' -game -unattended -nosound -windowed -ResX=1280 -ResY=720 -RenderOffscreen -SuperstarSmoke
```

预演 smoke 逐段渲染全部 39 段，并验证证据未选择时不能发送、选择后可以发送。末尾日志必须出现 `SuperstarStory smoke PASS: 39 beats`，退出码必须为 0。图片参考模式增加 `-SuperstarReferences`。

从原主菜单验证入口时，URL 使用 `/Game/levels/MainMenu`，参数改为 `-SuperstarMenuSmoke`；它会通过新增的 AS 绑定进入独立模式，再完成同样的逐段检查。

截图位置为 `Saved/Screenshots/Superstar3D/`、`Saved/Screenshots/SuperstarReferences/`。这些文件不随 Git 保存。自动渲染和内部交互状态检查不等于真人鼠标、拖动、完整时长观看和打包版验收。

### 本次验证记录

- Editor 构建成功：`Saved/Logs/Agent-Superstar-FinalBuild.log`。
- 原有脚本测试：`selected=9 executed=9 passed=9 failed=0`，退出码 0，见 `Agent-Superstar-FinalTests.log`。
- 资源验证：39 段 / 4 幕 / 3 次证据交互 / 18 动画 / 267 原样 RGBA 帧 / 5 张参考图通过。
- 原有两关带渲染运行退出码 0；现场、剪辑与两关结算截图生成，见 `Agent-Superstar-OldLevels.log` 和 `Saved/Screenshots/Verify_*.png`。已查看第一关现场和第二关剪辑画面。
- 新剧情直接启动和从主菜单进入均已逐段渲染，所有证据门槛内部状态检查通过；截图用于检查画面，不把单张截图当作循环动画最终美术验收。
- 最终主菜单入口回归：`Agent-Superstar-FinalMenu.log`，两次 GameMode 切换记录正确、39 段 smoke 完成、退出码 0。图片参考模式在 960×540 下完成相同 39 段检查，见 `Agent-Superstar-FinalReferences.log`，退出码 0；已查看证据发送画面。
- 未执行本次打包 Cook、Shipping、手动完整回合和真实鼠标拖动验收；不能宣称这些已通过。

项目原有告警仍保留：旧原生 `ABackstageStage` 构造器引用的 `/Game/Backstage/M_BackstageStage` 在当前 checkout 缺失，新旧运行都报告该资源问题；当前旧关卡 AS 演出和新预演均能渲染。本次没有创建同名替代材质或改写原场景。运行 `-game` 时也曾遇到本地 AngelScript 插件线程初始化 ensure，最终带渲染回归使用插件已有参数 `-as-skip-threaded-initialize`；未修改插件源码或全局配置。这些记录不等于零告警验收。
