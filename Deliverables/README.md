# 生成任务交接包

- `H3AnimationHandoff-20261115.zip`：最初18项动画任务的生成交接。
- `H3AnimationAcceptance-20261115.zip`：原交付的验收报告与返修证据。
- `Superstar-MissingAssets-20261115.zip`：按完整剧本补充的85项动画（含21项表情循环、7项返修）、19项静态、12项音频任务，含原资料、工作流和验收工具；让生成Agent从包内 `交给生成Agent.txt` 开始。

ZIP是交付快照，解压即可使用；同名展开目录是本地工作副本，不重复入Git。后续项目已增加BGM、表情头部跟随和摄影流程，最新运行说明以 `Docs/SuperstarStory/README.md` 为准。历史ZIP不回写，避免原包哈希和已发给生成机器的任务发生变化。

项目内 `Tools/Build-SuperstarSupplement.py` 是早期任务资料著录脚本，依赖原机资料与展开的旧包；它不是完整交付重建器。生成机器执行的完整脚本、任务配置与提示词均在交付ZIP内。
