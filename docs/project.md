# プロジェクト情報

## 目標と製品条件

OBSにネイティブで備わっているパネルと思えるUIで、Windowsのハードウェアリソースをリアルタイム監視します。標準dockとの見た目・余白・文字・操作・テーマ・DPIの一貫性を受入の対象にします。独自の固定色や固定fontを標準UIの完成形として扱いません。色だけに依存せず、単位・測定対象・取得失敗を読み取れる表示にします。

今のOBSプロセスCPU/RAMだけを最終仕様と解釈しません。今後の対象指標、システム全体とプロセス単位の区別、更新間隔、負荷許容値はそれぞれのIssueで決定します。未実装のGPU等を機能として宣伝しません。

## 現在の実装

- C++ / Win32 API / Qt Widgets / OBS frontend API。runtimeにPythonやNodeを要求しません。
- plugin-main.cpp: moduleとdockの登録・解除。
- ProcessMonitorWidget.cpp / .hpp: GetProcessTimesによるCPU時間差分、GetProcessMemoryInfoによるworking set。1秒間隔のQTimer。
- CPUは論理processor数で正規化。RAMバーは総物理メモリに対するOBS working setの割合。
- 固定style sheetとピクセル指定があり、テーマ・DPIへの自然な統合は未達。
- buildspecのOBS 30.2.2 / Qt 6.6.1は既存入力であり、互換性試験済み版ではありません。

## 開発と検証

ハーネスはPython 3.11以上の標準ライブラリ、Git for Windows、Windows標準PowerShellを使用します。Linux CIでも共通部を確認します。入口は `python scripts/check.py`、診断は `python scripts/doctor.py`、hooksは `python scripts/bootstrap.py` です。

製品の再現可能なbuild/testコマンドは未確立です。cmake/common/bootstrap.cmakeとCMakePresets.jsonがなく、依存hashにunknownが残っています。cmake --preset windows-x64を成功済み手順として案内しません。製品ソース・CMake・依存変更時はWindowsビルドと関連試験が必須で、基盤未整備中はblockedとして記録します。

通常の統合先はdevelop、検証済み昇格先はmainです。初回ハーネスはmain向けtooling PRで導入し、統合後にmainからdevelopへの専用同期PRを作ります。branchの存在・同期・ruleset適用はIssue/PRでreadbackします。

作業と受入の正本はGitHub Issues / PR、Caseは [verification](verification/README.md)。Projectは一覧、Milestoneは実際の到達目標です。参照元のProject番号や既存claimは移植しません。独立レビューはwriterと別セッションで固定HEAD/baseを確認します。

## 製品変更の完了条件

対象Windows/OBS/Qt版を固定してbuild・関連試験が成功し、別セッションのレビュー指摘が解消済みであること。GUI変更は標準dockとの並置比較、テーマ、DPI、resize、dock/float、再起動・終了、取得失敗、録画/配信中の負荷を該当範囲で確認します。実ロードDLLのSHA-256とsource SHAを記録し、未観察をpassにしません。

実装統合とGUI/releaseは [workflow](workflow.md) に従って分離します。配布物の公開は実機受入とユーザーの公開範囲の確認後です。
