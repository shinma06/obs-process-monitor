# OBS CMake helper の出典

`common/` と `windows/` は [obsproject/obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate/tree/3e7d7ac3b5342cd7d9b88890b9c70b472d1520fc/cmake) の commit `3e7d7ac3b5342cd7d9b88890b9c70b472d1520fc` から取得しました。上流の GPL-2.0 ライセンス全文は [LICENSE.obs-plugintemplate](LICENSE.obs-plugintemplate) に保持します。

2026-10-02 のローカル変更は `common/buildspec_common.cmake` のみです。保存済み archive の SHA-256 検証を追加し、取得失敗時は中途半端な download を除去して停止します。OBS の sub-build へ選択済み MSVC toolset を渡し、build 出力を CI ログへ残します。2026-10-03 は同ファイルで、obs-deps / Qt の抽出 marker を指定 directory 内へ移し、保存済み archive の照合後に完全一致の VERSION / Qt package を確認して再利用します。指定 prefix を優先し、Qt / OBS package cache と OBS sub-build の再構成も固定先へ揃えます。`common/buildspec_common.cmake` と `windows/buildspec.cmake` では、識別済みpackage用のarchive cacheと依存展開/build/install先も分離しています。通常開発はsource-root `.deps/` の信頼済み展開を再利用し、packageは新build tree内へ検証済みarchiveから抽出・buildします。それ以外のhelperは上記revisionの内容を保持します。

`build-info.json.in` は本プロジェクトの build identity 用テンプレートです。OBS / obs-deps の版と hash は [buildspec.json](../buildspec.json) に固定しています。
