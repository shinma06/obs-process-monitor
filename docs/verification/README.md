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
