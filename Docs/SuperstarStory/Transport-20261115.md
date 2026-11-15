# 时间线播放控制修复

上一版保留了按时间取帧的动作序列，但遗漏了原玩法的时间控制：左右键变成切段，进度条只有单击处理。此修复恢复原玩法的快进/倒放按键，并将39段串成一条可拖动的全剧时间线。

| 操作 | 行为 |
| --- | --- |
| 按住 ← 或 Z | 3倍倒放，跨段连续回退 |
| 按住 → 或 C | 3倍快进，跨段连续前进 |
| 空格 / R / 鼠标右键 | 播放或暂停 |
| 拖动进度条 | 按住持续定位，允许拖出条带范围，松手保持暂停 |
| PageUp / PageDown | 上一段 / 下一段 |
| Home / End | 全剧起点 / 终点；终点保持暂停 |
| 本段重播按钮 | 回到当前段落开头 |
| F1–F4 | 原有跳幕功能 |

快进/倒放松键后恢复此前的播放或暂停状态；到全剧两端停止。相册内的C选槽、Z/X调序不变。进度条拖动优先于场景拖镜头，避免拖到画面上时误移动相机。已拍照片不变，已发送的证据状态在来回定位时保留。

正常播放仍停在尚未发送的证据段末尾。主动快进/拖动可以浏览后续段落，但不自动发送任何证据。BGM在快进、倒放和拖动时暂停，正常播放时继续；没有声称实现音频倒放。

实现位于 `SuperstarTransport.cpp`：整剧时间映射为段落索引与段内Clock，演员sequence、walk_clip、手部道具和表情继续按同一Clock确定性求值。这沿用项目现有脚本/精灵序列模式，没有替换成UE LevelSequence资产。

## 验证

- Editor构建通过：`Saved/Logs/Agent-Transport-Build.log`。
- `-SuperstarTransportSmoke` 验证跨段快进/倒放、松键状态、连续拖动与越界钳制、镜头拖动互斥、证据门槛与已发送状态、A16/A17动作边界和递伞时间回跳。
- 同一smoke通过 `FInputKeyEventArgs::CreateSimulated` 向PlayerController注入Left/Right/Z/C/Space/R，经过真实Tick按键分支验证。不是直接设置Clock来代替键盘检查。
- 以上检查及截图通过：`Agent-Transport-Smoke.log`；照片流程回归通过：`Agent-Transport-Photo.log`；AngelScript `selected=9 executed=9 passed=9 failed=0`：`Agent-Transport-Tests.log`。
- 进度条拖动验证调用游戏实际使用的指针处理函数，覆盖按下/持续移动/松开；没有将其宣称为人工桌面鼠标验收。没有重新Cook或验收打包版。

截图：`Transport-Timeline.png`。旧地图、旧关卡时间控制和所有素材未改。
