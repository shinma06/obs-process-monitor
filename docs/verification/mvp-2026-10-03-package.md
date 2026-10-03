# 初期版の最終再受入（2026-10-03、package修正後）

QA [#8](https://github.com/shinma06/obs-process-monitor/issues/8)、親 [#5](https://github.com/shinma06/obs-process-monitor/issues/5)、package修正 [#18 / PR #19](https://github.com/shinma06/obs-process-monitor/pull/19)。PM /rootが新DLLを実観察しました。[旧b02記録](mvp-2026-10-03.md) のpassは転用していません。

## 識別と統合の対応

- 実artifact source: `99f865252c2072441f91ddcb2309f9627015b850`
- PR19 squash: `8b7da0d2eca6a2c8ed14866f3bd5f4f2be3c3ae1`
- 両tree: `5fc23910dab6f465c4098a73b0a2336751215f21`。diff0をPMと別session qa_final_reviewが照合し、統合後にGUIを実施。
- [Windows37088286869](https://github.com/shinma06/obs-process-monitor/actions/runs/37088286869)、job `111102967005`、[Harness37088286870](https://github.com/shinma06/obs-process-monitor/actions/runs/37088286870) 全成功。
- artifact `11261243674`、外側ZIP SHA256 `dca2d4397536a62b5eeaa962cc10ecbeb3d0bff30267b2994a5689eb53c99854`
- `obs-process-monitor-99f865252c20-windows-x64.zip`: `c6a1664812bbb21833f95b7919737dbadb8f6150b3cf786c6e17aceb06f82925`
- `obs-process-monitor-99f865252c20-source.zip`: `1bc2816b81ba6a1e8972df1ee4d141500a59b80a74737446021a99179330a210`
- 実ロードDLL SHA256: `ad4cb7cb4335021887a5a8fa49c6399f101e4105793279fda37fbb3794aa88f5`
- locale en-US: `ff2e366e55c9c84719c6339f3b4844ac2baf5f6557493ed24ba7d38e169c40d2`、ja-JP: `41c5cd8eb788834be16d002bf0112de2410b2bad7e522e299ffcb22d9f4dd380`

内外manifest、SHA256SUMS、source ZIP commit、DLL、localeを照合。localeのGit blob照合ではcheckoutのCRLF変換を考慮し、上記hashは配布物の実bytesです。3起動とも実ロードmodule hashとログsourceが一致。後続文書/昇格HEADはPRで別記し、製品・build・依存・locale差分ゼロを独立レビューします。再生成された別DLLへ受入を移しません。

Windows 11 x64、8 logical CPUs、AMD内蔵GPU/NVIDIA GeForce RTX 4070 Ti SUPER。専用portable OBS32.2.2 / Qt6.11.1、CMake4.4.3、MSVC19.51.36260.0、toolset14.51、SDK10.0.26100.0、固定obs-deps2026-07-15です。通常利用のOBS設定は使っていません。

Windows CIではPS5.1/7各20入力・失敗境界ケース、実CMakeのcache/任意header残留と新tree非継承、28依存prefix回帰、依存hash、解決されたpackage config5件を検証。通常buildの4 CTestに加え、**実際にinstall/packageした新build treeでも4 CTest成功**をjobログで確認しました。独立コードレビューは [5398488736](https://github.com/shinma06/obs-process-monitor/pull/19#pullrequestreview-5398488736)、HEAD99f/basef19、未解決指摘0。最新自動レビューも99fで完了、追加指摘0です。

## 表示と操作

System CPU/RAM、OBS CPU/working set、GPU使用率/専用メモリと更新時刻が継続更新しました。NVIDIAの非zero使用率と15.7GiB総量、AMDの正常な0.0%と6.7/485.8MiBを確認。矢印キーとEnterでadapterを切り替えました。Tabによるパネル外への移動も確認しましたが、Shift+Tabの戻り先は明確に確認できず、成功の証拠には含めません。

標準Statsとの並置、Yami Default（内部名Original）darkからLightへのアプリ内変更、日本語/英語を確認。背景・文字・標準bar・選択色がOBSへ追従。約157pxの狭幅では単位が折り返し、縦scrollでGPU/更新時刻まで到達し、横scrollbarも表示されます。約350pxへ戻した表示も確認しました。

録画中の非表示/再表示・float/re-dockに応答し現在値へ復帰。float直後は「計測中…」「GPUを確認中…」となり旧fillが消えました。初回待ちと実測0はGUI証拠、unavailable/unsupported/stale/異常値/例外回復/in-flight終了は同候補の合成CTest証拠で、実driver障害ではありません。

## 実Windows DPIと復元

ユーザー承認のもと、Windows設定で主モニター2を100→150→200→100%と変更。各起動は `QT_SCALE_FACTOR=1` です。

- 100%: 日本語dark/Light、Stats並置、狭幅/拡大、録画中の操作。
- 150%: 英語Lightで起動。ログ144 DPI、値/単位/更新とGPUまでのscroll。
- 200%: 日本語darkで起動。ログ192 DPI、値/単位/更新、float内scroll、re-dockとタブ再表示。
- 復元: OBS起動中に100%へ戻し、Windows設定で2560x1440/横向きも読戻し。復元後のOBS更新を確認して終了。

副モニター1は96 DPI/100%、1080x1920/縦向きのまま。mixed-DPIモニター間のウィンドウ移動は未実施。DPI変更後に操作ツールの対象cache/座標が不一致となる場面は、ウィンドウ再取得と新しい画面観察で回復しました。設定画面画像は個人情報を含むため非公開です。

## 録画と負荷

単一Color Source、desktop/micともmute、base1920x1080、output1280x720/30fps。NVENC H.264、CBR6000kbps、p5/hq/qres、profile high、AAC160kbps。2026-10-03 **11:15:35.553〜11:19:17.907 JST、222.354秒**を録画し正常停止。MP4は166,577,196 bytes、ログ6,654 output / 6,671 drawn frames。停止後Statsはrender lag0/8,311、encode lag0/6,669。Statsの分母は撮影時点の累積値で録画ログとは集計時点が異なります。

入力を止めて各30秒、全8 logical CPUsを分母にOBSプロセス全体のCPU時間差分を測りました。

- 表示: 11:15:36.829開始、30.0108秒、CPU0.6118%、working set315.86MiB。
- 非表示: 11:16:29.899開始、30.0111秒、CPU0.9111%、working set314.62MiB。
- 差は-0.2994 percentage points。通常の他アプリが動作している単一ホスト・単純scene・単回の参考値です。この逆転をpluginの負荷改善効果とは解釈せず、plugin単独負荷・一般性能・配信性能の推定には使いません。

## 終了と判定

3 GUIセッションを通常終了し、実ロードhash/source一致、plugin unloaded、OBS memory leaks0、process消失を確認。時刻は2026-10-03 JSTです。

- 100%: ログ11-15-04、loaded11:15:05.422、unloaded11:22:34.915、leaks0（11:22:35.173）。
- 150%: ログ11-24-14、loaded11:24:14.833、unloaded11:25:28.889、leaks0（11:25:29.126）。
- 200%→100%: ログ11-26-19、loaded11:26:20.158、unloaded11:31:32.253、leaks0（11:31:32.486）。

OBSとWindows設定の対象ウィンドウがないことを読戻し、GUI leaseを解放しました。画像・性能JSONL・生ログ・録画・manifestはprivateに保持し、repositoryへ個人情報や生ログを載せません。公開Caseは [#8](changes/issue-8.json) と [#18](changes/issue-18.json)。この識別済み候補について初期版の必要GUI Caseはpassです。文書の独立レビューとmain昇格の固定HEAD/base・必須CI・merge親は各PRで別途確認します。

[追加QA #13](https://github.com/shinma06/obs-process-monitor/issues/13) はPM担当pending。mixed-DPI移動、長時間/複雑scene/配信、複数processor group実機、GPU driver再初期化は未観察です。専用環境と試験枠を確保し、その時点の候補で別途検証します。温度・電力・共有GPUメモリ、公開Release/installerは初期版対象外です。
