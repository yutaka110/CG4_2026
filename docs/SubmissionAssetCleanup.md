# 提出用の教材素材除外と共通テクスチャ差し替え

2026-09-27 実施。元の教材ファイルは開発用として残し、提出用の実行フォルダーとソースのResourcesから除外した。

## 変更

- `monsterBall.png`を読む箇所・フォールバック・VFX登録の実ファイルを、`Resources/common/neutralSurface.bmp`へ変更。`multiMaterial.mtl`も修正。
- 新しい画像は8×8の白い共通パレット。材質が指定する色をそのまま表示する。外部画像を読み込まない`tools/generate_neutral_surface.py`で再生成できる。
- 起動時の教材音`Alarm01.wav`の読み込み・再生を削除。
- 人物、SimpleSkin、AnimatedCube、確認用モデルは、ファイルがない場合に読み込みを行わない。提出用フォルダーにはこれらを含めない。
- `Build/RailSubmissionResources.json`に391個のリソースを明示。新規ファイルを自動で全部同梱する方式は提出用では使わない。
- `tools/package_rail_submission.py`が新規フォルダーへ実行ファイルとSourceをコピーし、SHA-256を記録。既存フォルダーへの上書きは拒否する。
- ZIPは記録したファイルのみを収録し、動作確認で発生したログ、キャッシュ、個人のVisual Studio設定やGit履歴は含めない。

## 作成したファイル

- `outputs/rail-submission-20260927/GE3.exe`：Development/x64、教材除外構成の起動確認用。
- `outputs/rail-submission-20260927/Source/`：ソースコード、ビルド設定、同じ除外を適用したResources、必要な外部ライブラリ。
- `outputs/rail-submission-20260927.zip`：上記の整理版。説明PDF・動画・自己PRシートを含む最終提出ZIPではない。
- `PACKAGE_MANIFEST.json`：2131ファイルのハッシュと、除外した66リソースの一覧。

通常のDevelopment出力は従来どおり開発用素材を持つ。提出対象には今回作成した整理版を使い、開発用フォルダーをそのままZIPにしない。

## 検証

- Developmentビルド成功。
- 開発用Resourcesで回帰テスト155件中150件PASS。失敗5件は作業前と同じ。提出障害物、車体フレーミング、カメラ連続性はPASS。
- 教材を含まないフォルダーを作業場所にして実機起動。トロッコ・敵・地形・HUDの描画を確認。
- runtime heartbeatはframe=2760、distance=939.285、Tunnel Diveまで進行し、障害物4個を登録。最終ステージまでの通しプレイ確認ではない。
- 実行時の`shader_compile_errors.log`と`app_pipelines_error.log`は空。動作確認したプロセスを終了済み。
- 起動後も記録ファイルのハッシュ一致を確認。実行用・Source用の両方に除外対象がないことを検証。ZIP CRC検査も成功。

## 残る確認

この作業は不要教材の同梱防止であり、全素材・学校提供コードの権利確認完了を意味しない。ゲームで使用する画像・基本モデル等の出典確認は`SubmissionRightsAudit.md`に残る。Development用UIも表示されるため、最終提出向けの画面整理・最終構成のビルド・説明資料を含む一式の検査は別途行う。

再作成例（出力先には存在しないフォルダーを指定）：

```powershell
python tools/package_rail_submission.py --output outputs/rail-submission-next
python tools/package_rail_submission.py --output outputs/rail-submission-next --verify
python tools/package_rail_submission.py --output outputs/rail-submission-next --zip
```
