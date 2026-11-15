# 中英文版本接入（2026-11-15）

首页右上角语言按钮（也可按 `L`）在中文和英文之间即时切换。进入原有两关或超级巨星四关后继续沿用选择，重新启动也会记住语言。首次没有设置时默认中文。

## 实现与维护

- `Source/MegaJamProject/GameLocalization.h/.cpp`：显示层翻译、动态格式匹配、复合文本翻译及有界缓存。语言写入用户自己的 `GameUserSettings.ini` 的 `[GameLocalization] Language=zh/en`，不修改项目默认配置。
- `Content/Localization/en.json`：427 条可审阅的中英对照，包括任务、剧情动作、角色/表情名、证据、拍照/剪辑、报纸、结果、音乐名和错误提示。33 条格式模板保留数字与变量，捕获的角色名等继续翻译。新增显示文案后必须补齐目录；内部剧情 ID、评分输入和原始中文数据不变。
- AS 的绘制与测量桥接，以及原生 Superstar 的 `Label/FitLabel` 共用翻译入口。测量在翻译后执行；按钮、长标题、人物标签有边界，段落支持英文换行。1280×720 设计坐标与鼠标反变换不变。
- `Content/Localization/Images/EV03_roster_en.png` 是独立英文证据名单：英文模式中，3D 物品、参考图模式、证据缩略图和拍摄截图统一使用它。原名单文件未覆盖；三个红叉仍在第 1、2、4 槽，苏瑶第 3 槽不划掉。
- 参考图的舞台招牌、后台监视器与报纸报头使用运行时英文文字面板；原始 PNG 保留。已存照片保留拍摄时的语言，不在切换后改写历史照片。
- `publication.json` 增加 `language`、`display_headline`；原有 `headline/level_title` 原始值保留，兼容原验证与数据工具。
- `Config/DefaultGame.ini` 将 `Content/Localization` 纳入 UFS，必须随游戏打包；不新增插件或外部依赖。

## 验证

本次 UE 5.8.4 Development Editor 构建通过；AS 测试 `selected=9 executed=9 passed=9 failed=0`。

```powershell
python Tools/Validate-GameLocalization.py --probes
# 427条覆盖；输出429条真实运行时测试样例到 Saved/Localization/probes.json
```

在原有运行命令上组合以下参数：

- `-GameLanguage=en` / `-GameLanguage=zh`：仅本次运行覆盖语言；不自动改写用户偏好。点击切换才保存。
- `-GameLocalizationAudit`：读取上述 probes，实际执行 C++ 翻译校验，不允许零案例通过。
- `-LanguageMenuSmoke -GameLanguage=zh`，在 `/Game/levels/MainMenu`：模拟真实 `L` 输入来回切换，以及按设计坐标定位后的真实鼠标点击；截图至 `Saved/Screenshots/Localization`。
- `-LanguageReloadSmoke`（不传 GameLanguage）：新进程验证此前保存的英文偏好。
- `-BackstageSnap -GameLanguage=en`：原有两关的任务单、现场、确认、剪辑与结果截图；补充 `Verify_Briefing.png`、`Verify_Confirm.png`。
- `-SuperstarLevelsSmoke -GameLanguage=en`：4 个任务单、时间线、12 张真实照片、4 份独立报纸、证据门槛、继续及重置均通过；`python Tools/Validate-SuperstarLevels.py --log Saved/Logs/Agent-Language-Levels.log` 逐像素验证照片裁切。
- `-SuperstarSmoke -SuperstarReferences -GameLanguage=en`：39 段参考图渲染及 3 份证据通过。

实渲染使用 `-game -unattended -windowed -ForceRes -ResX=960 -ResY=540 -RenderOffscreen -as-skip-threaded-initialize`。另检查 1024×768 居中适配与语言记忆，以及 1280×720 的全部参考图剧情。日志在 `Saved/Logs/Agent-Language-*.log`，不提交本机验证产物。

本轮没有重新 Cook/打包或验证 Shipping。日志仍含既存的旧舞台材质缺失 `M_BackstageStage` 提示；本轮没有修改此旧资源路径，实际原有两关可渲染。

## 英文图片来源

使用内置 `imagegen` 编辑，原始参考为 `Content/SuperstarStory/Static/Partial20261115/EV03_roster.png`。已人工核对四个人物、顺序、英文姓名和红叉含义。最终图保存在上文项目路径，不依赖生成机器的用户目录。

生成提示词：

> Edit this existing game evidence illustration for English text localization ONLY. Preserve the exact 16:9 framing, all four portrait positions, the characters' identities, faces, clothing, background, board colors and red X evidence marks. Keep the first, second and fourth slots crossed out; the THIRD slot (pink dress) must remain unmarked. Replace only the four Chinese name labels beneath the portraits, from LEFT to RIGHT, with these exact English labels in a clean legible dark sans-serif font: 'Lin Wan', 'Xiao Bei', 'Su Yao', 'Other Contestants'. Do not add any fifth portrait or change who is crossed out. Any small show-logo Chinese text in the background monitors should instead read 'RISE'. This is an alternate localized asset; preserve all the visual evidence and composition.
