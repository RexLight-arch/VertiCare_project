# GitHub 提交指南

提交前先完成 [`ACCEPTANCE_CHECKLIST.md`](ACCEPTANCE_CHECKLIST.md)。

## 推荐提交范围

应提交：

- `README.md`
- `.gitignore`
- `VertiCareDemo/`
- `VertiCareBridge/`
- `VertiCareQt/`
- `docs/`

不要提交：

- `VertiCareDemo/config.h`
- `VertiCareQt/config.ini`
- `VertiCareBridge/bridge.properties`
- `VertiCareBridge/target/`
- `build-VertiCareQt-*`
- `QtMqtt-*`
- `01_数据手册/`
- 任何包含 Access Key、Token、设备密钥、消费组 Key 的临时文件

## 提交前检查命令

```powershell
git status --short
git status --ignored --short
git diff --stat
```

确认密钥文件显示为 `!!` ignored，而不是 `??` untracked。

## 推荐提交信息

```text
Polish VertiCare v1.5 demo documentation and dashboard
```

## GitHub 上传建议

1. 先在本地确认 Qt 能编译运行。
2. 用 Arduino IDE 编译并烧录 ESP32，确认启动日志出现 `v1.5-polish`。
3. 手动检查 `git status --short`，不要把真实配置文件加入暂存区。
4. 提交并推送。
5. 在 GitHub README 页面确认中文显示正常、文档链接可点击。

## 发现误提交密钥怎么办

如果密钥已经进入 Git 历史，不要只删除文件后再次提交。应立即：

1. 在 OneNet 重新生成或更换相关密钥/Token。
2. 从 Git 历史中清理敏感文件。
3. 强制推送清理后的历史。

课程设计场景下，最稳妥的方式是提交前严格检查，避免密钥第一次进入仓库。
