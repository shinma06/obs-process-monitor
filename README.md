# OBS Process Monitor

Windowsのハードウェアリソースをリアルタイムに監視し、OBS標準のパネルと見分けがつかないほど自然なUIで表示するOBS Studioプラグインです。

現在はプロトタイプです。実装済みの測定は **OBSプロセス自身のCPU使用率とRAM working set**、更新間隔は1秒です。システム全体・GPU等の監視や、OBSテーマへの自然な追従は今後の開発対象です。実機での検証済みリリースはまだありません。

## 開発を始める

[AGENTS.md](AGENTS.md) → [プロジェクト情報](docs/project.md) → [開発フロー](docs/workflow.md) を読みます。変更は **Issue → 担当claim → 専用branch/worktree → Draft PR → 検証・独立レビュー → 統合** で管理します。

```powershell
# Python 3.11以上。環境により python3 または py -3 を使用
python scripts/doctor.py
python scripts/bootstrap.py
python scripts/check.py
```

bootstrapは実行に使ったPythonをローカルGit設定に保存します。Git for WindowsのBashからも同じPythonでhooksが動作します。既存hooksは保全して統合します。

- 通常の製品開発はdevelop向け。main昇格は同一ビルドで必要Caseを確認してから行います。
- GUI不要の文書・scripts・CI変更はmain向けtooling PRにできます。
- [試験記録](docs/verification/README.md) / [GUI予約・引継ぎ](docs/operations.md)
- [環境セットアップ](docs/setup/README.md) / [導入内容と差分](docs/inventory.md) / [初回導入記録](docs/harness-adoption.md)

## ビルドの現在地

このcheckout単体ではまだビルドできません。CMakeが参照する `cmake/common/bootstrap.cmake` とWindows用presetが欠けており、buildspecには未確定のhashがあります。旧READMEの公式templateへ上書きする手順は、検証済みの再現手順ではありません。

先に専用Issueで公式OBS plugin templateとの整合、依存版・hash固定、Windows CI、DLL生成を整備します。その後にOBSへのロード・テーマ・DPI・dockの受入を行います。ハーネスのCI成功は製品buildやGUIの合格を意味しません。

## ソース

`plugin-main.cpp` はOBS moduleとdock、`ProcessMonitorWidget.cpp` / `.hpp` はWin32計測とQt Widgets表示、`plugin-macros.h.in` は生成ヘッダーを担当します。ソースはリポジトリ直下にあります。
