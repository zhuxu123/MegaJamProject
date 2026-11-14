# 超级巨星配乐

2026-11-15：七首原创器乐配乐已加入独立剧情预演。采用确定性编曲和合成音色，不使用第三方录音采样，也不是音乐大模型生成的录音。钢琴式拨击、柔和铺底、低音与轻节奏围绕同一动机变化，为剧情和未来对白留出空间。

| 曲目 | 使用段落 | 氛围 | 时长 |
| --- | --- | --- | --- |
| 镜头背后 / set_pressure | 1–4，片场 | 克制、压迫 | 48秒 |
| 路灯下的雨 / rain_secrets | 5–6、32，户外间幕 | 稀疏、疑问、冷静 | 53.33秒 |
| 甜笑与暗流 / backstage_intrigue | 7–19、21–23，走廊及杂物间 | 小步推进、表面平静的暗流 | 43.64秒 |
| 灯光下的裂缝 / spotlight | 20、29–31，舞台 | 更密的脉冲与冲突 | 38.4秒 |
| 镜中疑云 / mirror_doubt | 24–28，化妆间 | 犹疑、亲密距离下的警惕 | 50.53秒 |
| 让证据说话 / truth_live | 33–36，真相直播 | 稳定推进、揭露 | 40秒 |
| 落幕之后 / afterglow | 37–39，报纸与新片场 | 较明亮的余韵 | 48秒 |

WAV 在 `Content/SuperstarStory/Audio/`，为48kHz双声道16位PCM。`music.json` 保存逐段映射、音量、淡化时间、文件哈希和测量值。每首16小节，音符尾音与混响环绕到循环开头；没有整曲首尾淡至静音造成的循环空隙。预览拼接文件 `bgm-preview.mp3` 仅供试听，游戏播放原WAV。

## 游戏行为

- 主菜单按3进入“超级巨星”。同一曲目的相邻段落保持播放位置；换曲交叉淡化1.4秒。B切换参考图/3D布景不改变音乐。
- 空格或暂停按钮同步暂停音乐；定位进度条会暂停音乐。音乐是氛围轨，恢复或重播同一音乐区域不会按图片帧精确寻址。
- 等待玩家提交证据时继续循环。到全剧终点随预演暂停。M键以0.2秒音量渐变切换静音；界面显示当前曲名。
- 返回菜单时停止并销毁音频组件；不向旧关卡播放音乐，不改动原场景、原素材或历史交接ZIP。
- `SuperstarMusic` 拥有两路音频做交叉淡化。自定义 `USuperstarMusicWave` 的PCM在开始播放前固定，音频线程只读并独占循环游标，不通过游戏Tick续播来拼接循环。
- `SuperstarStory` 已有的 UFS 目录配置包含这些WAV。此次未重新制作完整发布包，不能将Editor验证视为打包版验收。

## 重新生成与验证

```powershell
python Tools/Generate-SuperstarBGM.py
python Tools/Validate-SuperstarMusic.py
```

生成脚本依赖NumPy，会按固定种子重新生成这七个文件；修改音乐后同步重新生成manifest。不需要ComfyUI或额外模型。

原生接口依赖新增 `AudioExtensions`；混音输出录制依赖 `AudioMixer`。重建Editor后，以本机引擎路径为例运行带音频的验证（不要加 `-NoSound`）：

```powershell
& 'G:/UE_Version/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' './MegaJamProject.uproject' '/Game/levels/MainMenu?game=/Script/MegaJamProject.SuperstarStoryGameMode' -game -unattended -windowed -ResX=1280 -ResY=720 -RenderOffscreen -SuperstarMusicSmoke -as-skip-threaded-initialize '-ini:Engine:[Audio]:UnfocusedVolumeMultiplier=1.0' '-ExecCmds=t.MaxFPS 60,au.NeverDisableSubmixes 1' '-ABSLOG=G:/Ue4_Project/MegaJamProject/Saved/Logs/Agent-BGM-AudioSmoke.log'
python Tools/Validate-SuperstarMusic.py --recording Saved/AudioValidation/SuperstarMusic-Smoke.wav
```

约90秒的验证实际录制引擎主混音输出：首曲跨越48秒循环，52–54秒暂停，56–58秒静音，60秒起逐一换曲，84秒销毁音乐组件，86秒结束录音。校验输出非静音、暂停/静音/销毁后的静音区、循环相关性与峰值。日志和录音保存在Saved，不随源码提交。

后台录音命令仅为该测试进程启用失焦音量，避免UE默认失焦静音得到全零录音；不修改项目或用户的全局音量配置。`au.NeverDisableSubmixes 1` 让静音区仍保留采样：否则子混音自动休眠会省略录音中的部分静音时间，无法直接用壁钟时刻验收。音源使用 `PlayWhenSilent` 保持淡入初始零音量和静音期间的播放位置。

## 本次检查记录

- Editor原生构建成功：`Saved/Logs/Agent-BGM-Build.log`。
- 七首源文件检查通过，全部39段映射完整；峰值保留超过5dB余量，文件无削波或意外静音小节。
- 实际引擎主混音录音85.74秒通过：七首均有有效输出；暂停、静音和组件销毁后均为静音；48秒循环前后相关系数1.000000，无削波。录音路径 `Saved/AudioValidation/SuperstarMusic-Smoke.wav`，日志 `Saved/Logs/Agent-BGM-AudioSmoke.log`。
- 本次原玩法回归：`selected=9 executed=9 passed=9 failed=0`，日志 `Saved/Logs/Agent-BGM-Tests.log`。
- 本次带渲染遍历39段及全部证据门槛通过，日志 `Saved/Logs/Agent-BGM-Render.log`；已查看第24段截图，曲名“镜中疑云”和M静音提示显示正常。
- 未重新打包发布版；音乐风格与音色可通过试听串烧和游戏内播放继续调整。
