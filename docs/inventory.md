# 移植元と採用範囲

導入日: 2026-10-02。固定revisionを参照しました。

- [agent-harness-template](https://github.com/shinma06/agent-harness-template/tree/e822318a6c0fa7175a89687b929197dfae879184): 共通契約、start/finish、Git guard、bootstrap、check、GUI lease、private registry、運用文書。
- [cursor-in-android-studio](https://github.com/shinma06/cursor-in-android-studio/tree/a1e841ccaeb113687d676536c2d13ae98fe77ef4): Issue所有・worktree・PR、develop/main、独立レビュー、Case別受入、GUI直列化。

AGENTSを正本とし、CLAUDEはimport、Cursor ruleと各clientのSkillは同じ正本への入口です。Windowsでsymlink権限を必要としない薄いClaude用Skillを採用しました。

Unix専用部分をWindowsへ適合しました。Git hooksはbootstrap時のPythonを利用し、LFを固定します。linked worktreeのbootstrapは他checkoutのhooksを切り替えません。GUI leaseはWindowsではmsvcrtのファイルロック、private registryはWindows ACLとreparse-point検査を使います。POSIX側の権限検査は維持しました。

UTF-8、日本語文書、Windowsの個人パス検査、Windows/Linux CI、使い捨てGit remoteでのhooks検証を追加します。製品仕様とビルド欠落は [project](project.md) に記録します。

## 引き継がない状態

製品コード、Kotlin/Gradle試験、既存Issue/Project/Milestone番号、実registry、個人設定、認証情報、PAUSED automation、専用coordinator/ZIP配布engineはコピーしません。専用engineはテンプレートにも含まれません。[自動進行手順](setup/automation.md)を使い、定期起動は別途明示された場合だけ行います。

参照元のAgent review / Acceptance gate自動判定はこの導入では未実装です。独立レビューと受入はPR上の手動gateとして必要です。未実装jobをrequiredに指定しません。移植元の過去の結果・接続状態を、この環境のpassに置き換えません。
