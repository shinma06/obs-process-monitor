# GitHub・CI・保護設定

## 新規repositoryと初回導入

GitHubのTemplate機能はファイルを複製する入口です。GitHub設定、認証、ローカルhooks、Project、実行中状態は別途設定します。[公式Template](https://docs.github.com/en/repositories/creating-and-managing-repositories/creating-a-template-repository)

1. 公開範囲を決めてrepositoryを作成し、正規の `gh auth login` で必要なアカウントへログイン。
2. cloneし `gh repo view --json nameWithOwner,visibility,defaultBranchRef` で接続先を確認。
3. 小さいIssueを作成してclaimし、専用branch/worktreeからDraft PRを作成。
4. CIの `harness-checks` を実行して成功を確認。内容はテンプレート自身のチェックです。
5. required checksとして登録する前に、同名jobの実行実績と導入先アプリの必要な検証jobを確認。
6. 設定後はrulesetとbranchへの実適用を読み戻す。API失敗やプラン制限は未適用と記録。

## 保護方針

統合branchへのPR必須、force push/削除の禁止、必要checks、会話解決を基本にします。レビュー承認数は実際のレビュアー構成に合わせます。同一アカウントの別session reviewはGitHub上の別ユーザーApproveと同じではありません。独立sessionの固定SHAレビューを証拠として記録しても、サーバー強制がない場合は運用上の確認に留まります。

Rulesetの利用範囲はrepositoryの公開範囲と契約に依存します。対応を確認し、非対応時に「保護済み」と書かないでください。[公式Rulesets](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-rulesets/about-rulesets)

```bash
gh api repos/OWNER/REPOSITORY/rulesets
gh api repos/OWNER/REPOSITORY/rules/branches/main
gh pr checks PR_NUMBER
```

OWNER/REPOSITORY/PR_NUMBERは導入先の値です。元のruleset ID、Project番号、label体系、CODEOWNERS、必須4check名は移植しません。存在しないcheckをrequiredにするとmergeできなくなります。

## CIの信頼境界

同梱workflowはcontents:readで、checkoutのcredential保持を無効にしています。PRコードのテスト環境へwrite tokenや本番secretを渡しません。Actionsの版は導入時の方針に従って固定・更新し、起動を確認します。

成果物配布を追加する場合は、テスト/buildとpublishを分け、実際にbuildしたSHA/hashを保存します。branchが進んだだけで旧artifactを新SHAとして扱いません。PRの任意コードをtrustedなmerge/publish判定として実行せず、判断側を既定branchの確認済みrevisionに置きます。

元環境のchange-impact classifierは製品配置とビルドに結合しているためコピーしません。この小さいテンプレートは全harnessチェックを毎回実行します。導入先で重い検証を選択実行する場合は、rename/delete/mode変更、混在、unknown、不完全履歴を含めて検証し、hook/CI/coordinatorに判定を重複実装しません。

## Issue / Project / Milestone

Issueは具体作業、Projectは全体表示、Milestoneは到達目標にします。親子は実際の分解、blocked byは真の依存だけを登録します。単なる分類のための親を作りません。Projectが必要な導入先は登録・Status/Priority・関係を作成/更新/終了時に読み戻します。小規模の新templateでProjectを必須にする必要はありません。

この導入には元のPR policy / Agent review / Acceptance gateを実装したCIは含まれていません。独立reviewと受入は[workflow](../workflow.md)で記録する必須の手動gateです。自動化する場合は実装・検証してからrequiredに追加します。

## obs-process-monitorの設定

対象repositoryは `shinma06/obs-process-monitor`、統合branchはdevelopとmainです。初回tooling PRはmainへ統合し、専用同期PRでdevelopへ反映します。mainのdefault branch設定は維持します。

実装したjob名は `harness-checks (ubuntu-latest)` と `harness-checks (windows-latest)` です。branch rulesetはPR必須・会話解決・削除禁止・非fast-forward禁止・この2つのstrict required checksを設定し、checkの発行元をGitHub Actionsへ固定します。製品のdevelop統合とmain toolingはsquash、main promotionとmain→developの同期は履歴を保持するmerge commitを使います。rulesetはsquash/mergeを許可し、この使い分けはレビューで確認します。未実装のcheck名は登録しません。

2026-10-02に両OSのCI成功を確認し、[main ruleset](https://github.com/shinma06/obs-process-monitor/rules/24364287) と [develop ruleset](https://github.com/shinma06/obs-process-monitor/rules/24364288) をactiveで適用しました。bypass actorは空です。両branchへの実適用もAPIでreadbackしました。同一アカウントの独立セッションレビューのため、別GitHubユーザーの必須Approve数は0、独立レビュー自体は運用上必須です。

このファイルだけでは将来の設定状態を保証しません。統合担当は次のAPIで現行設定をreadbackします。

```bash
gh api repos/shinma06/obs-process-monitor/rulesets
gh api repos/shinma06/obs-process-monitor/rules/branches/main
gh api repos/shinma06/obs-process-monitor/rules/branches/develop
```

GitHub CLIがない環境は同じAPIを提供する正規のGitHub接続を使用できます。ローカルhooks成功とサーバー保護適用を混同しません。
