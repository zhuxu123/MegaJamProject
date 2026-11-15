# 视频参考表现接入（2026-11-16）

用户确认将视频参考的九项全部接入；保留此前取景框、最近三张照片、头版展开动画，以及保留/Retake后自动播放。

| 功能 | 实现及操作 |
| --- | --- |
| 摄像机外框、REC、时间码 | 仅现场HUD显示，30格时间码由本关SequenceTime求值，倒放同步回退；REC红点用UI时间闪烁。不是录像编码或修改5 FPS素材。 |
| 操作标签 | 实际镜头目标发生变化时显示构图/拉近/拉远；可移动的快进/倒放显示3倍标签，松手淡出。 |
| 对焦反馈 | 默认青色、操作时黄色、照片保留成功后短暂绿色。 |
| 照片入场 | 最近一张用0.45秒缓动从右侧飞入；序号及待确认/已保存文案随相纸旋转；只保留最近三张显示。 |
| 裁切可视化 | 选中剪辑槽绘制未裁原图、裁外暗遮罩、红框与四角标记；框内拖动平移，滚轮缩放；原WASD/QE仍可用。所有路径复用CropBounds，不能超出原图。最终裁切看P预览。 |
| 调序动画 | Z/X交换相邻槽，0.32秒滑动，选中状态跟随原照片，提示交换编号。 |
| 报纸收藏 | 选关或头版按H/点击进入，每关保留最新有效发布；1–4或顶部按钮切换，Backspace返回来源页面。只读已发布裁切图，不覆盖/改动当前照片。 |
| 直播进度 | 当前关卡有证据时显示LIVE及已发送数，第一次成功发送时短暂绿色反馈；位于左上，不遮证据卡和发送按钮。 |
| 终章封面 | 四期齐全且结束第四关时显示标题和四期真实报纸；Enter从第一关重新开始、H回看、ESC选关。历史文件不删除。 |

首次现场、确认照片、剪辑、调序、时间控制和证据操作提供4秒短提示，位于顶部预留条，不使用宣传片的大字幕挡住底部控件。每次HUD会话各提示只展示一次。

## 文件与状态

- `SuperstarPresentation.cpp`：公共反馈、裁切遮罩、报纸读取/收藏/终章。
- `SuperstarPhotography.cpp`：摄影入场、保留反馈、裁切指针处理、调序绘制与发布收藏。
- `SuperstarStory.h/.cpp`：GC跟踪的Edition/照片引用、UI时间、现场覆盖层和首次提示。
- `SuperstarLevels.cpp`：Archive/Finale状态及输入路由，正常四关流程不变。
- `SuperstarTransport.cpp`：真实时间控制操作提示。
- `Content/Localization/en.json`：新增显示文案；中文原始剧情和ID不变。

收藏扫描`Saved/SuperstarPhotos/**/publication.json`，按文件时间由新到旧选择每关最新有效三图发布，读取已导出的裁切PNG。关卡/段落范围、文件名和图片可读性检查失败就跳过。每关最多驻留3张收藏纹理，USTRUCT/UPROPERTY链负责GC。旧版有效记录仍兼容；新增`test_run`字段标记测试产物，正常游戏不会加载带此标记的记录。旧测试产物没有该字段，无法自动与旧玩家记录区分。

所有新覆盖层在Capturing返回之后绘制，不写入照片。1280×720设计坐标和Pointer逆变换保持一致。只有显示层及收藏流程新增，不影响旧两关脚本/评分、地图、演出时间或插件。

## 验证

- UE5.8.4 Editor Development构建成功：`Saved/Logs/Agent-Presentation-Build.log`。
- 中文1280×720四关smoke、英文1024×768四关smoke均退出0，含12次拍照、四份独立发布、证据门槛、终章和清空内存后从磁盘重新加载收藏；日志 `Agent-Presentation-Levels.log` / `Agent-Presentation-Levels43.log`。英文版另通过460项运行时翻译审计。
- 摄影smoke中文1280×720/英文1024×768通过：保留/重拍自动播放、调序、裁切、发布。增加指针处理检查：图外按下不捕获、图内按下+滚轮、拖出边界钳制、松开释放。`Agent-Presentation-PhotoFinal.log` / `Agent-Presentation-Photo.log`。
- `Tools/Validate-SuperstarLevels.py --log Saved/Logs/Agent-Presentation-Levels43.log`通过四份发布、12张逐像素裁切和关卡隔离。摄影发布也通过`Validate-SuperstarPhotos.py`逐像素校验。
- `SuperstarTransportSmoke`通过引擎Left/Right/Z/C/Space/R/F2/D/E/Q注入及跨段/握点回归：`Agent-Presentation-Transport.log`。
- AS测试：selected=9 executed=9 passed=9 failed=0，`Agent-Presentation-AS.log`。
- 翻译静态校验458条覆盖/460案例通过；`git diff --check`通过。
- 已查看实机渲染的取景/照片状态、裁切遮罩、LIVE/证据控件、英文4:3终章和收藏页。截图位于`Saved/Screenshots/SuperstarPhoto/`与`Saved/Screenshots/SuperstarLevels/`。

没有重新打包或人工全程鼠标/听音验收。自动指针处理测试不等同于人工系统鼠标拖动。日志仍有既有旧后台`M_BackstageStage`缺失报错，本次未改该资源。
