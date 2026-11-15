# 2026-11-16 头版发布动画

四关的 `SavePublication` 成功写入发布清单后才重置 `NewspaperRevealTime` 并进入头版。1.4秒内，报纸从右上角28%大小、9度倾斜，平滑移动、放大并回正至原布局，带相纸投影。`SuperstarLevels.cpp::DrawNewspaper` 使用Canvas临时变换绘制同一份真实头版，三张照片、顺序、裁切、中英文标题与正文一起运动；绘制结束恢复变换，不影响其他HUD。不创建预制头版图片，不改变发布文件或重复播放结算音效。

动画时间由HUD Tick独立累计，不使用暂停/倒放的剧情时钟。动画中Enter或点击立即展开并消费该次输入，避免激活尚在移动的按钮；下一次操作使用原有按钮坐标。ESC/M仍可返回选关，Backspace仍可回编辑。每次重新发布都重新播放。

本次验证：

- Editor Development构建成功：`Saved/Logs/Agent-Newspaper-Build.log`。
- `-SuperstarLevelsSmoke` 1280×720中文及1024×768英文（ForceRes）均退出0、报告PASS；日志 `Agent-Newspaper-Levels.log` / `Agent-Newspaper-Levels43.log`。
- Smoke在每关采集 `L*-Newspaper-Entry.png`、`L*-Newspaper-Mid.png`、`L*-Newspaper.png`。已查看第一关实际渲染的起始、中间和最终画面，整张报纸正常缩放/旋转，4:3英文头版完整可见。截图在 `Saved/Screenshots/SuperstarLevels/`，中文第一关副本在 `Saved/Screenshots/NewspaperReveal-1280/`。
- 对两份本次日志运行 `python Tools/Validate-SuperstarLevels.py --log <日志>` 均通过：四份独立发布、12张逐像素裁切、关卡隔离与证据条件不变。
- `-run=AngelscriptTest`：selected=9 executed=9 passed=9 failed=0，日志 `Agent-Newspaper-AS.log`。
- `git diff --check`通过。未重新打包，未人工逐按钮测试动画跳过；动画采样及四关流程由引擎smoke验证。

带渲染日志仍有既有旧关卡 `M_BackstageStage` 材质缺失报错，本次未修改该资源。
