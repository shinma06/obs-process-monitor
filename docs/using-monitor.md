# リソースモニターの使い方

対象は Windows x64 と OBS Studio 32.2.2 です。別の OBS / Qt 版の互換性を推定しません。検証結果は [初期版の受入](verification/changes/issue-8.json) と [QA #8](https://github.com/shinma06/obs-process-monitor/issues/8) に記録します。

## 検証用 ZIP を用意する

[Windows plugin build](https://github.com/shinma06/obs-process-monitor/actions/workflows/windows-build.yml) の成功した run から、受入記録の source SHA と一致する artifact を取得します。`obs-process-monitor-<SHA>-windows-x64.zip` が本体、`-source.zip` が対応ソースです。`SHA256SUMS.txt` と `build-manifest.json` で ZIP と DLL の SHA-256 を別々に照合できます。公開 Release と installer はまだ提供していません。

初期版の実機確認用候補は [run 37093185917](https://github.com/shinma06/obs-process-monitor/actions/runs/37093185917) の artifact `11262922424`、source `689a46291bc80e2f5a1a995b75227ba0acb89495` です。DLL SHA-256 は `8923421a5e7668ee0367e3f53875ba5fce9416d8e88b207b21303cc4a553d812`。後続の文書更新・昇格CIで作られた別DLLは、この候補の実機受入結果を引き継ぎません。受入状態は冒頭の記録を確認してください。

## OBS に配置する

OBS を終了してから、本体 ZIP を展開します。通常インストールとportableでは配置先が異なります。

- 通常の Windows インストール: `obs-process-monitor` フォルダ全体を `C:\ProgramData\obs-studio\plugins\` へ配置します。DLL は `obs-process-monitor\bin\64bit\obs-process-monitor.dll`、言語ファイルは `obs-process-monitor\data\locale\*.ini` です。
- portable OBS 32.2.2: ZIP 内の DLL を `<OBSのルート>\obs-plugins\64bit\obs-process-monitor.dll` へ、`data\locale` を `<OBSのルート>\data\obs-plugins\obs-process-monitor\locale` へ配置します。

同じプラグインを複数の検索先へ重複配置しません。通常インストールは [OBS公式Plugins Guide](https://obsproject.com/kb/plugins-guide) の推奨配置です。同ページのportable `data/plugins` の記述は32.2.2では読み込みを確認できませんでした。上記portable手順は [32.2.2の検索パス登録](https://github.com/obsproject/obs-studio/blob/32.2.2/frontend/widgets/OBSBasic.cpp#L115-L184) と [Windows標準パス](https://github.com/obsproject/obs-studio/blob/32.2.2/libobs/obs-windows.c#L37-L46) に基づき、検証環境でDLLのロードと翻訳を確認しています。

OBS を起動し、**ドック → リソースモニター**（英語では **Docks → Resource Monitor**）を開きます。ほかの標準 dock と同じようにタイトル部分をドラッグして配置できます。閉じた場合もドックメニューから再表示できます。

## 表示する指標

- **システム**: 全 CPU の使用時間の割合、使用中 / 総物理 RAM。
- **OBS Studio**: 現在の OBS プロセスの CPU 使用率、常駐する RAM（working set）。CPU と RAM のバーの分母はシステム全体です。
- **GPU**: 選択した adapter の最も忙しい engine の使用率、使用中 / 総専用メモリ。複数の GPU がある場合は選択欄を切り替えます。OBS だけの GPU 使用量ではありません。

値は表示中に約 1 秒間隔で更新します。dock を隠すと収集を休止し、再表示時は新しい計測区間から始めます。`計測中…` は初回の区間待ち、`取得不可` は現在値を取得できない状態、`非対応` は指標が提供されない状態です。取得できない値を実測ゼロとして表示しません。各項目の tooltip に対象と分母を記載しています。

CPU は Task Manager の周波数補正を含む Processor Utility と同じ指標ではありません。GPU の可用性は Windows/WDDM/driver に依存します。温度、電力、共有 GPU メモリは初期版の対象外です。計算と回復の詳細は [計測仕様](metrics.md) を参照してください。

## 表示されない場合

OBS のログで `obs-process-monitor` のロード記録と source SHA を確認します。DLL の対象 OBS / x64、配置階層、locale の有無を順に確認してください。収集不可の状態が続く場合は、Windows/OBS 版・症状・該当指標を Issue に記録します。ログを添付する前に配信先・個人情報・認証情報を除きます。
