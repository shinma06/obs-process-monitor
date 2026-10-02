# Windows リソース計測

計測は `src/metrics/ResourceSampler.hpp` の Qt 非依存 C++17 API を使います。`ResourceSampler` の生成・`sample()`・`reset()`・破棄は同じ worker thread で行います。推奨間隔は 1 秒です。Windows API は同期呼出しなので GUI thread では実行しません。dock 非表示中は呼出しを止め、再表示時に `reset()` して新しい区間を開始します。

`Metric<T>` は値と状態を返します。`Ok` のみ値を持ち、実測のゼロも含みます。`WarmingUp` は初回、CPU カウンターのリセット、時刻逆行、CPU 数変更による基準更新です。`Unavailable` は取得失敗、無効な区間、対応する GPU instance がない場合です。`Unsupported` はカウンター未提供、hardware adapter なし、専用 GPU メモリなしを表します。取得不可の値に前回値やゼロを代入しません。`gpuStatus` は adapter 列挙の状態で、個々の GPU カウンター状態とは別です。

## CPU と物理メモリ

- システム CPU は `GetSystemTimes` の差分から `(kernel + user - idle) / (kernel + user)` を求めます。kernel は idle を含みます。複数 processor group の場合、この API は全 CPU を表さないため、PDH の `Processor Information(_Total) / % Processor Time` を使います。
- OBS CPU は現在の process の kernel/user 時間差分を、単調増加する QPC の経過秒数と全 group の active logical processor 数で割ります。100% はシステム全体の計算時間を占有した状態です。CPU frequency 補正値や Task Manager の Processor Utility と同じ指標ではありません。
- システム RAM 使用量は `GlobalMemoryStatusEx` の総物理メモリから利用可能物理メモリを引いた値です。OBS RAM は `GetProcessMemoryInfo` の working set です。commit/private bytes ではなく、共有ページを含む resident memory です。
- 0 分母、逆行、取得失敗を検出し、失敗後の CPU は次の正常なサンプルで基準を作り直します。QPC の整数 tick と CPU 数を乗算してから割る計算は避けます。

参照: [GetSystemTimes](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getsystemtimes)、[GetProcessTimes](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocesstimes)、[QueryPerformanceCounter](https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter)、[GetActiveProcessorCount](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getactiveprocessorcount)、[GlobalMemoryStatusEx](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-globalmemorystatusex)、[GetProcessMemoryInfo](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getprocessmemoryinfo)。

## GPU と専用メモリ

DXGI hardware adapter の LUID と GPU performance counter instance の LUID を照合します。software adapter は除外します。`id` は Windows session 内の識別子、`name` は UTF-8 です。adapter 番号は Task Manager の番号と同一とは限りません。

使用率は `GPU Engine(*) / Utilization Percentage` を取得し、同じ LUID・physical index・engine index のプロセス別値を合計して、最も忙しい engine を採用します。3D と copy、video decode 等の独立 engine をすべて足した値にはしません。同じ process/engine の重複 alias は一度だけ数えます。matching instance の一部が無効なときは adapter の使用率全体を取得不可にし、部分的な合計を実測値として表示しません。新規 GPU context の出現や終了で一時的に取得不可になり、次の正常 interval で復帰する場合があります。

専用メモリ使用量は `GPU Adapter Memory(*) / Dedicated Usage` です。process 別 GPU memory は共有 allocation を二重計上するため使いません。分母は DXGI の `DedicatedVideoMemory` で、OBS process の動的 budget ではありません。`IDXGIAdapter3::QueryVideoMemoryInfo` の CurrentUsage/Budget は process 対象なので、システム GPU 全体の使用量・容量として代用しません。shared GPU memory、温度、電力は本 API の対象外です。専用容量ゼロの adapter は専用メモリを非対応として返します。

GPU performance counter の可用性は Windows/WDDM/driver に依存します。instance の構造を認識できない場合も取得不可にします。LUID 相関と busiest-engine 集計は本実装の処理であり、Microsoft がこのプラグインの測定値一致を保証しているという意味ではありません。

参照: Microsoft GPU scheduler 担当者の [GPUs in the Task Manager](https://devblogs.microsoft.com/directx/gpus-in-the-task-manager/)、[DXGI_ADAPTER_DESC](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/ns-dxgi-dxgi_adapter_desc)、[QueryVideoMemoryInfo](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/nf-dxgi1_4-idxgiadapter3-queryvideomemoryinfo)。

## 回復と負荷の境界

PDH は言語非依存の `PdhAddEnglishCounterW` を使い、wildcard を `PdhGetFormattedCounterArrayW` で取得します。CPU の PDH query と各 GPU query は分離し、GPU provider の不具合が CPU/RAM を止めないようにします。query は RAII で close し、DXGI interface も scope 終了時に release します。現在 process の pseudo handle は close しません。

PDH query の失敗後は 5 秒間再作成を待ち、次の呼出しで再試行します。sleep は行いません。GPU adapter 情報は 30 秒ごと、または DXGI factory が古くなった時に再列挙します。buffer は query ごとに最大 4 MiB、instance は 16,384 件、buffer 再読込は 3 回、adapter 列挙は最大 32 件に制限します。超過時は取得不可にし、部分結果を成功扱いしません。Windows provider の同期呼出し時間そのものに強制 timeout は設けていません。worker 終了は進行中の API が戻ってからになります。

参照: [PdhAddEnglishCounterW](https://learn.microsoft.com/en-us/windows/win32/api/pdh/nf-pdh-pdhaddenglishcounterw)、[PdhCollectQueryData](https://learn.microsoft.com/en-us/windows/win32/api/pdh/nf-pdh-pdhcollectquerydata)、[PdhGetFormattedCounterArrayW](https://learn.microsoft.com/en-us/windows/win32/api/pdh/nf-pdh-pdhgetformattedcounterarrayw)。

## 検証

`MetricMathTests.cpp` は CPU 差分・分母・reset・失敗からの再開、RAM 範囲、instance parser、複数 adapter/engine/process の集計を試験します。assert に依存せず Release build でも検証します。

`SamplerSmoke.cpp` は Windows 実 API を呼び、初回、通常値、CPU 実負荷、reset、複数 sampler の破棄後の handle 数を確認します。headless CI では GPU の非対応を許容し、その状態を出力します。GPU 搭載機では `--require-gpu --samples 15` を指定すると、全列挙 adapter の GPU 使用率と専用メモリが少なくとも 1 回そろうことを要求します。特定機種名や LUID をログに出しません。

Windows MinGW が既存環境にある場合の計測層単独コマンドは次のとおりです。OBS plugin の MSVC build や GUI 検証を代替するものではありません。

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -Werror -pedantic -I . src/metrics/MetricMath.cpp tests/metrics/MetricMathTests.cpp -o $env:TEMP/obs-metric-math.exe
& "$env:TEMP/obs-metric-math.exe"
g++ -std=c++17 -O2 -Wall -Wextra -Werror -pedantic -I . src/metrics/MetricMath.cpp src/metrics/ResourceSampler.cpp tests/metrics/SamplerSmoke.cpp -o $env:TEMP/obs-sampler-smoke.exe -lpsapi -lpdh -ldxgi
& "$env:TEMP/obs-sampler-smoke.exe" --require-gpu --samples 15
```

受入証拠と未実施条件は [Issue #6 Case](verification/changes/issue-6.json) を参照してください。OBS での実表示・録画負荷・終了、複数 processor group の実機値、GPU driver の交換/再起動中の挙動は CLI 単独の通過だけで成功扱いしません。
