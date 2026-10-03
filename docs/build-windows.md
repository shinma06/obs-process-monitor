# Windows DLL のビルド

対象は OBS Studio 32.2.2、Qt 6.11.1、Windows x64 です。配布物は開発・QA 用で、実機受入前の公開リリースではありません。GUI の状態は [Issue #4 の Case](verification/changes/issue-4.json) とQA [Issue #8](https://github.com/shinma06/obs-process-monitor/issues/8)（親 #5） で追跡します。

## 必要な環境

- Git、CMake 4.4.3、PowerShell。
- Visual Studio 18 2026 の C++ desktop build tools。preset は MSVC 19.51.36260.0 / toolset 14.51（x64 host）と Windows SDK 10.0.26100.0 を選択します。CMake は異なる compiler 版を拒否し、実版を生成 manifest にも記録します。
- GitHub Actions は OBS 32.2.2 本体と同じ `windows-2025-vs2026` runner を使用します。CMake は公式 ZIP を SHA-256 検証して利用します。
- ハーネスチェックには Python 3.11 以上。製品の実行に Python / Node は不要です。

## fresh checkout からの手順

Issue branch を checkout し、追跡ファイルを commit 済みにして実行します。

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64 --parallel
ctest --test-dir build_x64 -C RelWithDebInfo --output-on-failure --no-tests=error
powershell -File scripts/package-windows.ps1
```

configure は `.deps/` に固定の依存 archive を取得し、OBS の libobs / frontend API を Debug と Release で構築します。plugin DLL は `build_x64/RelWithDebInfo/obs-process-monitor.dll`、配布物は `out/package-*/` に生成します。configure には実際の HEAD を記録するため Git checkout が必要です。HEAD を変更したときは configure から再実行してください。

package スクリプトは追跡ファイルの未 commit 差分と configure 時の HEAD 不一致を拒否します。未追跡ファイルもrepository / global / localのignore設定にかかわらず検査します。許可する生成先はroot直下の `build_x64/`、`.deps/`、`out/`、`.harness-local/`、`.vs/`、`cmake/.CMakeBuildNumber` と、`scripts/__pycache__/`・`tests/__pycache__/` 直下の `.pyc` のみです。source / data内の生成物や隠しファイル、`CMakeUserPresets.json`、`.env`・ローカルagent設定等も例外外なら拒否します。秘密ファイルは出力・自動削除しません。製品入力はcommitし、個人設定を持つcheckoutとは別のclean worktreeでpackageしてください。

入力検査後、固定presetを `--fresh` で再configureして古いcacheを破棄し、manifest用build-infoを再読取します。install前に `--clean-first` で再buildするため、以前の未追跡入力を消しただけのcheckoutでも古いobjectを再利用しません。生成先内の任意の手動改変や、実行中の別writerによる変更まで安全性を保証するものではありません。毎回別の出力 directory を使い、以前の配布物を上書きしません。ZIP には `obs-process-monitor/bin/64bit/obs-process-monitor.dll`、PDB、存在する locale data、および `build-manifest.json` が入ります。対応ソース ZIP と配布 ZIP の `SHA256SUMS.txt` も生成します。OBS 本体や Qt の DLL は同梱しません。

`build-manifest.json` の `source_sha`、`dll_sha256`、toolchain、依存 hash、CI run ID を QA 記録へ転記します。ZIP のハッシュと中の DLL ハッシュは別です。OBS ログにもロードした plugin の source SHA が出ます。

## 依存の固定と取得失敗

正本は [buildspec.json](../buildspec.json) と [CMakePresets.json](../CMakePresets.json) です。

- OBS source: [32.2.2.zip](https://github.com/obsproject/obs-studio/archive/refs/tags/32.2.2.zip)、SHA-256 `f15f001f1fa526405318835f44f9910046502f496ebc3a30d5296a5018b831aa`。
- obs-deps: [windows-deps-2026-07-15-x64.zip](https://github.com/obsproject/obs-deps/releases/download/2026-07-15/windows-deps-2026-07-15-x64.zip)、SHA-256 `6f90e9598fa10cff5ad23cdcfae49b87868c07bf896b02cd464582b4ce2f2ba9`。
- Qt: [windows-deps-qt6-2026-07-15-x64.zip](https://github.com/obsproject/obs-deps/releases/download/2026-07-15/windows-deps-qt6-2026-07-15-x64.zip)、SHA-256 `7c7f985711d80467bdc1795b6592275a27d5b0e5a2c7a61db1f2c1d08d6a5579`。上流 [Qt recipe](https://github.com/obsproject/obs-deps/blob/2026-07-15/deps.qt/qt6.ps1) は 6.11.1 を指定しています。
- CMake: [4.4.3 Windows x64](https://github.com/Kitware/CMake/releases/download/v4.4.3/cmake-4.4.3-windows-x86_64.zip)、SHA-256 `4d52ebab7193a698651639ed80d8d04fd903358843572cf44c7fd234cb7c26ab`。

obs-deps と Qt の hash は [OBS 32.2.2 の公式 preset](https://github.com/obsproject/obs-studio/blob/32.2.2/CMakePresets.json) と一致します。helper の固定 commit、保持ライセンス、ローカル変更は [cmake/UPSTREAM.md](../cmake/UPSTREAM.md) に記録します。

取得不能・SHA-256 不一致では configure を失敗させます。hash を回避したり `unknown` に変更せず、エラーで指定された archive のみを削除し、ネットワーク・取得元を確認して再取得します。保存済み archive は再利用時も hash を検証します。obs-deps / Qt は `.deps/` の指定 directory 内に書いた抽出 marker と VERSION を確認し、同版の外部 prefix や旧 checkout 共通 marker では展開を省略しません。指定 prefix を毎回検索順の先頭へ戻し、Qt / OBS の package cache も指定先へ揃えます。OBS の sub-build は毎回 `--fresh` で再構成し、以前の外部 library / package cache を引き継ぎません。通常の object build は差分 build のままです。`.deps/` 内の検証済み抽出物は手動で編集しないでください。

archive hash の負の試験は `cmake -P scripts/test-dependency-hash.cmake`、prefix と marker / CMake cache の回帰試験は `cmake -P scripts/test-dependency-prefix.cmake` です。後者はローカル ZIP fixture を使う offline 試験で、Ninja / Make または Visual Studio の generator が必要ですが compiler と OBS は不要です。runner image 自体はホスト側で更新されるため、toolset が利用不能なら明示的に失敗します。新しい版への切り替えは依存変更としてレビュー・Windows build・実機受入を行います。

package入力の回帰試験は `powershell -File scripts/test-package-inputs.ps1` です。使い捨てGit repositoryで実package entry pointをbuild境界まで動かし、未追跡header・data・ignore設定・追跡差分の拒否と通常生成物の許可、fresh configure / clean buildの順序を検査します。`cmake -P scripts/test-package-cache.cmake` は実CMake module/cacheを使い、削除済み入力のcache値が通常configureでは残り、`--fresh` で消えることを確認します。

## CI と受入

[Windows plugin build](../.github/workflows/windows-build.yml) は PR の実 HEAD を checkout し、fresh な runner で configure / DLL build / CTest / package / 入力・cache回帰 / hash 不一致・prefix 再利用試験を行います。artifact は DLL ZIP、対応 source ZIP、manifest、配布物 hash、build/test logs を保持します。実パッチ版・runner image・source SHA を記録した再現手順であり、別日時・別 build path でも全バイトが一致する保証ではありません。

プラグインを実機に配置する前に [GUI 操作予約](operations.md) に従います。ビルド CI はロード・テーマ・DPI・録画時負荷・終了の pass を意味しません。
