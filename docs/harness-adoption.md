# 初回導入と引継ぎ

対象: [Issue #1](https://github.com/shinma06/obs-process-monitor/issues/1)。導入base: `813083007c12112cd08ce8b5b3eacdb2bc29ff57`。branch: `codex/1-agent-harness`。移植revisionは [inventory](inventory.md)。

共通規約・client入口・start/finish・Windows対応hooksとstate管理・CI・Issue/PRテンプレート・OBSの目標と実装現状を導入します。製品のC++/CMake/buildspecは変更しません。GUIはnot-requiredです。

検証結果と最終HEAD、CI、レビューの正本は [PR #2](https://github.com/shinma06/obs-process-monitor/pull/2) とIssueです。[Case](verification/changes/issue-1.json)に手順と受入を記録します。

2026-10-02、ソースrevision `fc89976b6f920accc0f8b575e45adeefd85627a0` でWindowsローカル試験と [Windows/Linux CI](https://github.com/shinma06/obs-process-monitor/actions/runs/37008938059) が成功しました。全10件のうちWindowsは9件成功・POSIX専用1件skip、Linuxは7件成功・Windows専用3件skipです。独立セッション `harness_review` が全差分を確認し、未解決指摘0で承認しました。文書の最終差分の再確認・CI・統合結果はPRに記録します。main/developの [サーバー保護](setup/github.md) もactiveの実適用を確認しました。

## 統合担当の次の操作

1. 固定HEAD/baseの最終レビューとWindows/Linux CIをreadbackする。
2. 現行の保護設定を確認し、通常のPR経路でmainへ統合する。
3. main checkoutでbootstrapする。専用worktreeの設定はmainのhooksを有効にした証拠ではない。
4. main→developの専用同期PRでハーネスを揃える。直接pushしない。
5. ビルド基盤の整備を次の製品開発Issueとし、依存固定・DLL・Windows CIを確立する。その後、標準OBS UIへの統合と指標拡張を進める。

他clientの自動読込、OBSのbuild/GUI、Project board、schedulerは未実施です。製品build基盤が次の開発課題で、GUIや配布の合格を主張しません。初回の統合とmain checkoutのbootstrap結果はIssueで追跡し、merge後に停止・cleanと所有を確認して作業branch/worktreeを整理します。
