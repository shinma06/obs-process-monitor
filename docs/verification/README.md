# 受入とOBS実機検証

Caseを `docs/verification/changes/issue-N.json` に記録します。Issueは作業と担当、PRは差分と固定SHAのレビュー、JSONは操作と観測結果の正本です。過去のpassを別ビルドへ引き継ぎません。

各Caseにid、steps、expected、status、evidenceを記録します。状態は pending / pass / fail / blocked / not-required。未完了Caseには次のownerと操作を記載します。GUI不要の作業にも理由とCLI試験を残します。現段階では構文をcheckし、Caseの意味・充足は独立レビュアーが確認します。

製品のGUI Caseでは対象変更に合わせて以下を具体化します。

- OBS標準dockとのフォント・色・余白・focus・操作の並置比較。
- 対応テーマ、Windowsの100/150/200% DPI、最小幅・拡大・float・再dock。
- 測定対象・単位・分母、取得失敗、低負荷/高負荷、一定間隔の更新。
- 録画/配信中の応答性と追加負荷、長時間・dock切替・OBS終了/再起動。

Windows/OBS版、source SHA、DLL SHA-256、テーマ/DPI、手順、期待、実際、観察者を残します。スクリーンショットから個人の配信情報を除きます。GUI操作には [lease](../operations.md) が必要です。

developは必要CLI試験・独立レビュー・Case追跡後に統合し、GUI未完了はQA Issueへ双方向linkで引き継ぎます。main promotionは固定develop候補の全commit・必要Caseを同一DLLでpassにしてから行います。候補後の製品差分は再build・再確認が必要です。

製品候補のsource SHAと、後から観察を記録する文書HEADは別に残します。文書だけの追記であってもCIが再生成した別DLLは実機受入済みになりません。昇格時は候補以降の全変更を独立レビューし、製品・build・依存・localeの差分ゼロを確認したうえで、受入に使った元package/source ZIP/DLLのhashとCI runを指定します。表示倍率は実OS DPIとプロセス限定Qtスケールを区別し、物理的なmixed-DPI環境の未観察を隠しません。

初期版の実際の結果は [package修正後の2026-10-03受入記録](mvp-2026-10-03-package.md) を参照してください。実OS100/150/200%はWindows表示設定で確認し、Qt限定スケールの補助試験と区別しています。短時間ローカル録画を確認済みで、mixed-DPI移動・長時間配信等の未観察条件は [#13](https://github.com/shinma06/obs-process-monitor/issues/13) でpendingとして追跡します。

[旧744候補の記録](mvp-2026-10-02.md) は履歴として保持します。依存修正後のb02候補は別DLLとして実機試験をやり直しています。

[旧b02候補の記録](mvp-2026-10-03.md) も履歴です。最終99f候補はPR19のCI artifactを用い、squash8b7とのtree一致を確認後、別DLLとして実機受入を実施しました。
