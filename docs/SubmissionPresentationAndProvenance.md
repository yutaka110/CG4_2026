# 提出用画面と素材の出典確認

確認日：2026-09-27。対象は `Build/RailSubmissionResources.json` の提出用リソース。出典不明を権利侵害と判定するものではないが、全素材の提出許諾を確認できた状態でもない。

## 画面の整理

- 通常起動にタイトル画面を追加。作品名「レールであばレール」、操作説明、Enterで開始／Escで終了を表示。既存フォントと図形だけを使用し、外部素材は追加していない。作品名に必要な5文字を既存のHUDフォントアトラスへ追加。
- タイトル表示中はゲームの更新を行わず、コース・敵・制限時間を進めない。Enterの短い入力も取りこぼさないよう、タイトル操作にはWindowsのキーイベントを使用する。専用の戦闘検証モードは従来どおり直接開始する。
- Release / x64 のゲームを使用。起動時に開発用パネルを隠す既存設定を適用。
- Releaseでは、地形プレビュー経由のデバッグ描画を送信しない。コリジョン枠・選択表示・レールの補助線を除き、通常の攻撃予告とゲームHUDは残す。
- ゲームのウィンドウタイトルを「レールであばレール」に統一。
- 幅960px以上の画面下に、通常射撃・ロックオン・ポーズ・リトライの操作案内を追加。
- Pで一時停止／再開。R／Enterはリトライ可能時のみ有効。終了はAlt+F4。

## 素材ごとの根拠と残る確認

| 対象 | 今回確認できた根拠 | 状態・必要な対応 |
|---|---|---|
| neutralSurface、RailHazardBlock、RailHazardSolid、CombatTurret | リポジトリ内の生成コードは外部画像・モデルを読み込まない。別ディレクトリで再生成し、現在のファイルと照合 | 当該コードからの生成を確認。生成コード自体の制作担当・AI支援の申告は選考会の規定に合わせる |
| CurvedCanyonWall、OrganicArchLarge、RibTunnelWall、RootSpireColumn、SpireBrokenBridgeArc、VistaHoleWallと4種のBMP材質 | `generate_course_mesh_assets.py` から再生成。上記と合わせて20ファイルのSHA-256一致、6個のMTLは改行を正規化したテキストが一致 | 生成経路を確認。バイト一致とテキスト一致を記録上で区別 |
| terrain/materials/texturesのORM・height 6枚 | `generate_terrain_pbr_maps.py` に、上記BMPから各画像を生成する処理と出力名がある | 生成経路のコードを確認。今回のPython環境にはOpenCVがなく再生成一致は未確認 |
| Rocks016画像6枚 | 同名の[ambientCG公式素材](https://ambientcg.com/view?id=Rocks016)と[CC0の利用条件](https://docs.ambientcg.com/license/)を確認 | ローカル素材の取得元確認が必要。公式アーカイブとの照合はHTTP 403で完了せず。Rocks016.pngの変換・加工履歴も未確認 |
| rostock_laage_airport_4k.dds | 同名の[Poly Haven公式HDRI](https://polyhaven.com/a/rostock_laage_airport)と[CC0の利用条件](https://polyhaven.com/license)を確認 | 入手元とDDSへの変換経路が未確認。同名だけで公式配布物とは確定しない |
| M PLUS Rounded 1c Medium / Regular | `Resources/Editor/Fonts/README.md` と `OFL-MPLUS.txt` に出典・著作権表示・SIL OFL 1.1を記録 | 両フォントとライセンスを実行用・ソース用の両方に保持 |
| circle、circle2、gradationLine、streakNoise、beamRamp_lightning、iceShard、warpTunnelFlipbook、terrain_detail_normal | Git初出と画像メタデータを確認したが、作者・配布元・条件を確定できる情報は得られず | 8枚の入手元・利用条件の確認が必要。簡単な図柄という理由では自作と判定しない |
| ball.obj、CombatLaneRib、CombatShardGate、CombatAssaultの3モデル、CombatInterceptor、CombatSniper | 現在の提出用一覧に含まれる。今回確認した生成スクリプト群では生成元を特定できず | 8個のOBJとball.mtlについて制作履歴・作者を確認する |

全58件の画像・モデル・材質・フォントについて、個別ファイル名とSHA-256を `Source/Build/SubmissionAssetProvenance.json` に記録。再生成の照合結果は `Source/Build/SubmissionGeneratedAssetVerification.json` に記録。コードの存在だけを根拠に、他の素材まで一括して自作とは扱わない。

## ライブラリ・学校提供物

Assimp、DirectXTex、Dear ImGuiは `THIRD_PARTY_NOTICES.md` と `Licenses/` の許諾文を同梱する。DXC／DXILもSDKのライセンス・第三者通知を同梱するが、今回の確認は文書の保持までであり、バイナリの再配布条件の完全な照合は未完了。

学校提供コード、`lib/Receiver`、上記で出典未確認の素材については、学校または元の配布元に「就職選考会に実行ファイル・プレイ動画・ソースと必要な素材ファイルを提出できるか、必要な表記は何か」を確認する。学校配布と確認済みのmonsterBall・Alarm01・humanは、前回の作業で提出用リソースから除外済み。

このパッケージは実行ファイルとソースの確認用。提出方式で指定されているプログラム説明PDF・動画・自己PRシートは別途用意し、本人氏名・学校名を使った最終ZIPへまとめる。

## 検証記録

- Release / x64 ビルド成功。
- タイトル追加後、HUD未初期化でもタイトルが表示されることと、タイトルを閉じた後にメニュー表示が残らないことのテストを追加し成功。実画面でもタイトル表示とEnterでゲームの先頭から始まることを確認。
- EditorCore回帰テスト：155件中150件成功、既知の5件が失敗。カメラ・HUDに関する対象ケースは成功。全テスト成功とは扱わない。
- 既知の失敗：hand particle attachment / multi material showcase presentation defaults / course wave runtime compiler and preview actor bridge / course map bounds semantic lod and label layout / course map cartography persistence unit。
- 提出用フォルダーを作業ディレクトリにしてRelease版を起動。タイトル・HUD・画面下の操作案内を確認。開発パネルとデバッグ補助枠は表示されず、コースが進行した。確認用プロセスはAlt+F4で終了済み。
- シェーダーコンパイルとパイプラインのエラーログはともに0バイト。
- Pキーはコード上の割当を確認したが、自動入力では停止状態の確認に至らなかったため、実機での操作確認は未完了。
- 空が平坦な灰色に見える区間がある。現在のSkybox.PS.hlslはHDRIをサンプリングせず、白系の空を数式で生成している。灰色に見える原因は未確定。録画前に背景と地形の境界の見え方を再確認する。
- 実行用・ソース用の両方に対してファイルハッシュと教材素材の除外を検査。ZIPには記録済みファイルだけを含め、試遊で生成されたログやキャッシュを含めない。
