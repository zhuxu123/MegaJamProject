# 操作与结算音效 · 2026-11-15

8个原创音效由 `Tools/Generate-InterfaceSfx.py` 使用振荡器、包络和固定随机种子的噪声合成，没有第三方采样或模型。原始PCM16/48kHz/立体声WAV、哈希清单和试听串在 `ArtSource/InterfaceSfx/`；运行时使用 `Content/Audio/Interface/` 的8个有限长度SoundWave资产。

| ID | 用途/声音 | 时长 | 运行增益 |
|---|---|---|---|
| chapter | 章节/关卡选择，双音短提示 | 0.23s | 0.55 |
| forward | 向前、快进，上行磁带掠过声 | 0.42s | 0.42 |
| rewind | 返回、倒带，下行磁带声 | 0.42s | 0.42 |
| shutter | 成功发起拍照，机械快门双响 | 0.24s | 0.65 |
| pan | 镜头移动/机位切换，低音量马达 | 0.20s | 0.18 |
| zoom_in | 拉近焦距，上行伺服声 | 0.26s | 0.24 |
| zoom_out | 拉远焦距，下行伺服声 | 0.26s | 0.24 |
| result | 发布/结算，四音钟琴短旋律 | 1.85s | 0.55 |

源文件峰值约-6dBFS，再通过上述运行增益留出BGM混音余量。试听顺序同表：`ArtSource/InterfaceSfx/preview_all.wav`。

## 接入与边界

- `BackstageSfx.*`：按本地PlayerController持有音效组件；预载8个SoundWave，四个固定声道分别供时间控制、镜头、快门、结算使用。EndPlay停止并销毁这些组件。章节短提示使用可跨OpenLevel的有限SoundWave，自然播完自动释放，避免选关时被地图切换截断。
- `PlayCue`按音效ID节流；镜头移动最短间隔0.28s，变焦0.30s，两者共用一个声道，变焦优先。音效都是短单次播放，没有无限循环的马达/磁带音。快进/倒带在方向变化时提示，按住不逐帧重复，松开淡出35ms；两个相反键一起按不应产生移动反馈。
- 原两关：`BackstageGame.as` 的选关/开始、实际镜头目标变化、拍照请求、进入Results挂接。新增AS原生绑定 `BackstagePlayCue(const FName&)`、`BackstageAudioMotion(int,bool,int)` 在组件实现文件注册，不修改插件。Round结构、评分、照片和按键功能保持原样。
- 超级巨星：F1–F4章节、PgUp/PgDn及对应按钮、Home/End、进度拖动和快进/倒带、WASD/拖动/机位/变焦、TakePhoto、成功SavePublication、正常播放至全剧结尾挂接。普通自动换段、相册缩略图重绘、照片数据求值不发操作声；未达拍照条件/发布写入失败不误播成功结算。
- 保留既有BGM和M键的“音乐静音”含义，M不会关闭操作反馈。没有新增模块依赖或覆盖场景资源。
- `Config/DefaultGame.ini` 增加 `/Game/Audio/Interface` 的 `DirectoriesToAlwaysCook`，因为音效按稳定ID动态加载；不能只依靠字符串路径期待Cook自动发现。

## 生成、导入与验证

```powershell
python Tools/Generate-InterfaceSfx.py
# 必须先保存并关闭同checkout编辑器；导入仅替换这8个SFX资产。
& '<UE>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' '<Project>/MegaJamProject.uproject' -run=pythonscript '-script=<Project>/Tools/Import-InterfaceSfx.py' -unattended -NullRHI -nosound
python Tools/Validate-InterfaceSfx.py
```

重建原生Editor后运行真实音频输出验证，不能加-NoSound：

```powershell
& '<UE>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' '<Project>/MegaJamProject.uproject' '/Game/levels/MainMenu?game=/Script/MegaJamProject.SuperstarStoryGameMode' -game -unattended -windowed -RenderOffscreen -InterfaceSfxSmoke -as-skip-threaded-initialize '-ini:Engine:[Audio]:UnfocusedVolumeMultiplier=1.0' '-ExecCmds=t.MaxFPS 60,au.NeverDisableSubmixes 1'
python Tools/Validate-InterfaceSfx.py --recording Saved/AudioValidation/InterfaceSfx-Smoke.wav --log Saved/Logs/Agent-SFX-AudioSmoke.log
```

上例日志参数应补 `-ABSLOG=<Project>/Saved/Logs/Agent-SFX-AudioSmoke.log` 或将验证器--log指向实际本次日志。音频smoke临时静音超级巨星BGM，以便逐音核对；运行期间依次播放8个音效，检查全部自然结束，再测试100次瞬时移动请求只发声一次，导出master submix录音后退出。不要把这项调试参数带入普通游玩。

本次结果：

- UE5.8.4 Editor重建、8个SoundWave导入成功；9项AS测试全部通过。
- 实际引擎录音的8项源波形相关性0.8504–0.9998；没有削波，短声均正常结束，100次突发请求只有一个pan声。记录在 `Saved/AudioValidation/InterfaceSfx-validation.json`，音频日志 `Saved/Logs/Agent-SFX-AudioSmoke.log`。
- 扩展 `-SuperstarTransportSmoke` 实际注入Left/Right/Z/C/Space/R/F2/D/E/Q，章节、移动、变焦、正倒放输入与对应音效均出现。`-SuperstarPhotoSmoke`为拒绝重拍加3次拍摄，共4个快门，只在最终成功发布产生1个result；照片导出逐像素裁切校验通过。
- 对原两关执行带渲染BackstageSnap，并从主菜单进入超级巨星回归39段。该原两关自动演出检查不等于人工逐按键听感验收。
- 配置已包含Cook目录；本轮未重打完整发布包。既有旧Backstage材质缺失日志仍在，本次未改其资产。

需要主观微调时优先改 `BackstageSfx.cpp` 的增益/间隔；需要改音色时修改生成脚本、重新导入并重跑录音验证，保留源文件与manifest一致。
