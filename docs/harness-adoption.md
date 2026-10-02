# 初回導入と引継ぎ

対象: [Issue #1](https://github.com/shinma06/obs-process-monitor/issues/1)。導入base: `813083007c12112cd08ce8b5b3eacdb2bc29ff57`。branch: `codex/1-agent-harness`。移植revisionは [inventory](inventory.md)。

共通規約・client入口・start/finish・Windows対応hooksとstate管理・CI・Issue/PRテンプレート・OBSの目標と実装現状を導入します。製品のC++/CMake/buildspecは変更しません。GUIはnot-requiredです。

検証結果と最終HEAD、CI、レビューの正本はIssue/PRです。[Case](verification/changes/issue-1.json)に手順と受入を記録します。別セッションのレビューはpendingで、自己レビューやunittestを代替にしません。

## 統合担当の次の操作

1. 固定HEAD/baseで別セッションレビューを実施し、Windows/Linux CIをreadbackする。
2. [GitHub設定](setup/github.md)を実際のjob名に照合して適用・readbackし、通常のPR経路でmainへ統合する。
3. main checkoutでbootstrapする。専用worktreeの設定はmainのhooksを有効にした証拠ではない。
4. main→developの専用同期PRでハーネスを揃える。直接pushしない。
5. ビルド基盤の整備を次の製品開発Issueとし、依存固定・DLL・Windows CIを確立する。その後、標準OBS UIへの統合と指標拡張を進める。

サーバー保護、他clientの自動読込、OBSのbuild/GUI、Project board、schedulerはファイル配置だけでは有効になりません。未確認項目には担当・再開条件を残します。レビュー待ちのbranch/worktreeを保持し、merge後に停止・cleanと所有を確認して整理します。
