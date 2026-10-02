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

## Windows ビルド

OBS 32.2.2 / Qt 6.11.1 / Windows x64 を対象に、公式 OBS helper と SHA-256 固定の依存を使います。Visual Studio 2026（MSVC 19.51.36260.0 / toolset 14.51）、Windows SDK 10.0.26100.0、CMake 4.4.3 が必要です。

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64 --parallel
powershell -File scripts/package-windows.ps1
```

配布物は `out/package-*/` に生成され、`build-manifest.json` に source SHA・DLL SHA-256・実コンパイラ版を含みます。GitHub Actions の Windows plugin build も同じコマンドを実行します。[詳しい手順と取得失敗時の扱い](docs/build-windows.md)、[Issue #4 の検証記録](docs/verification/changes/issue-4.json) を参照してください。ハーネス成功、製品 build 成功、OBS 実機受入はそれぞれ区別して記録します。

## ソース

`plugin-main.cpp` はOBS moduleとdock、`ProcessMonitorWidget.cpp` / `.hpp` はWin32計測とQt Widgets表示、`plugin-macros.h.in` は生成ヘッダーを担当します。ソースはリポジトリ直下にあります。
