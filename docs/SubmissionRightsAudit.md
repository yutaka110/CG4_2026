# 提出前の素材・ライセンス確認

確認日：2026-09-27。対象：現在のソース、Resources、Development出力、同梱ライセンス、公式配布元の公開条件。

2026-09-29追記：空港の環境マップは実行時参照・提出用一覧から除外し、外部画像を使わない生成コード由来の `Resources/environment/canyon_soft_256.dds` に置換。生成方法は同フォルダーのREADME、現在のハッシュは `Build/SubmissionAssetProvenance.json` に記録。下記の空港素材の項目は差し替え前の監査記録。

追記：この監査後、ボール柄の参照差し替えと不要教材の提出用構成からの除外を実施した。変更と検証は[SubmissionAssetCleanup.md](SubmissionAssetCleanup.md)を参照。その後のRelease版の画面整理と58件の素材確認は[SubmissionPresentationAndProvenance.md](SubmissionPresentationAndProvenance.md)を参照。以下の一覧は監査時点の記録であり、全権利の確認完了を示すものではない。

**結論：現状のフォルダーをそのまま提出してよいとは、まだ確認できない。** 外部ライブラリとフォントの権利表記は整っているが、学校配布素材の外部提出条件と、一部素材の由来が未確認。侵害の認定ではなく、提出判断に必要な証拠の不足を示す。最終ZIPは未作成・未検査であり、法的な無侵害を保証するものではない。

## 優先して対応するもの

| 対象 | 確認した事実 | 提出前の対応 |
|---|---|---|
| `Resources/monsterBall.png` | ユーザー回答では学校配布。実画像はモンスターボールを連想させる赤白の図柄。`AppSceneResources.cpp:2124`で読み込み、予備テクスチャやVFX登録にも参照あり。Development出力にも存在 | 独自図柄へ差し替えるのが推奨。許諾の証拠がなければ、学校配布という理由だけで外部提出可としない。単に画面上で非表示にするだけでは同梱は止まらない |
| `Resources/Alarm01.wav` | 学校配布。`AppMain.cpp:294`で読み込み、296行で起動時再生。ファイル内・隣接ファイルに利用条件を発見できず | 配布元・録音の権利・作品および動画への使用・素材同梱条件を学校に確認。確認できない場合は自作音へ差し替え、起動時の不要な再生も整理 |
| `Resources/human/*` | 学校配布。`walk_gltf.gltf`と`sneakWalk.gltf`に`mixamorig:`、後者に`mixamo.com`の名前あり。Mixamoを経由した可能性が高いが、モデル本体の作者までは特定できない。起動時のモデル登録あり | 元モデルとアニメーションそれぞれの出典・条件を確認。ゲーム利用とFBX/glTF/Blenderファイルの提供を分けて判断。レール作品に不要なら読み込み依存を外して提出用コピーから除外 |
| 未使用教材の同梱 | `Build/GE3.Common.props:35`がResources全体をコピー。上記3種類とAnimatedCubeが実際のDevelopment出力に存在 | 提出用には必要な資産を明示してコピー。起動時の固定読み込みとフォールバックを先に直し、元作業フォルダーは保持。コードのソース提出にも不要な元素材を混ぜない |

学校の授業のための例外を就職選考会への外部提出にそのまま適用できるとは判断しない。授業目的の制度の範囲は[文化庁の説明](https://www.bunka.go.jp/seisaku/chosakuken/hokaisei/h30_hokaisei/)を参照。

Mixamoの[Adobe公式FAQ](https://helpx.adobe.com/creative-cloud/faq/mixamo-faq.html)はゲームを含む商用・非商用作品への利用を認めている。一方、[Adobe一般利用条件3.6](https://www.adobe.com/legal/terms.html)はContent Filesの作品外での単独配布を認めていない。今回の取得時の条件、学校からの配布経路、提出形態への適用は未確認であり、ソース付き提出を一律に禁止・許可とは判定しない。

## ライセンス文書を確認できたもの

| 対象 | 条件・確認結果 |
|---|---|
| Assimp | ローカルLICENSEはBSD-3-Clause、Poly2Triの条項を含む。ソースの表示を保持し、バイナリ配布にも必要な文書を付ける。Developmentの`Licenses/Assimp_LICENSE.txt`は元ファイルとSHA-256一致。[公式](https://github.com/assimp/assimp/blob/master/LICENSE) |
| DirectXTex | MIT。著作権表示・許諾文を保持。Developmentのライセンスと元ファイルはSHA-256一致。[公式](https://github.com/microsoft/DirectXTex/blob/main/LICENSE) |
| Dear ImGui | MIT。著作権表示・許諾文を保持。Developmentのライセンスと元ファイルはSHA-256一致。[公式](https://github.com/ocornut/imgui/blob/master/LICENSE.txt) |
| M PLUS Rounded 1c Medium / Regular | SIL OFL 1.1。両フォントのSHA-256は同梱READMEの記録と一致。著作権表示入り`OFL-MPLUS.txt`が本体と出力の両方にあり一致。アプリ同梱ではこれを残す。[公式OFL FAQ](https://openfontlicense.org/ofl-faq/) |
| DXC / DXIL | Windows SDK由来のDLLとSDKライセンス・第三者通知が出力に存在することを確認。ただしバイナリ再配布条項の完全な照合、Assimpのプリビルド内部を含む全依存の網羅検証は未完了。「ライセンス文書がある」ことと「全条件確認済み」は別。[Microsoft DXC](https://github.com/microsoft/DirectXShaderCompiler) |

`THIRD_PARTY_NOTICES_SHIPPING.md`は最小の`GE3.Runtime`専用で、現在のゲームを動かしている`GE3.exe`の通知文として流用しない。提出する実行ファイルに対応する`THIRD_PARTY_NOTICES.md`と`Licenses/`を使う。

## 配布元候補・生成経路を確認できた素材

| 対象 | 根拠と残る確認 |
|---|---|
| `terrain/Rocks016_1K-JPG/*` | 同名の[ambientCG Rocks016](https://ambientcg.com/view?id=Rocks016)があり、本番地形設定が使用。[公式条件](https://docs.ambientcg.com/license/)はCC0で商用・改変・元ファイル同梱可、クレジット必須ではない。ローカルファイルの取得履歴・公式ファイルとの一致は未照合 |
| `rostock_laage_airport_4k.dds` | 同名の[Poly Haven HDRI](https://polyhaven.com/a/rostock_laage_airport)があり、スカイボックス処理から参照。[公式条件](https://polyhaven.com/license)はCC0。DDS変換前の入手元・変換履歴を記録する |
| `AnimatedCube/*` | generatorがVKTSで、Khronosサンプルと整合する。[現行公式サンプルの表記](https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/main/Models/AnimatedCube/README.md)は2017 UX3D / Norbert Nopper、CC0。旧READMEには寄贈の説明だけだったため、現行の出典も記録する。ローカル一式との同一性は未照合 |
| `simpleSkin/*` | [同名のKhronosサンプル](https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Models/master/2.0/SimpleSkin/README.md)はCC0。ただしローカルはBlender再出力で`uvChecker.png`も含む。名前だけでは全ファイルの由来を確定できない |
| 今回作成した岩・砲台 | `tools/generate_submission_hazards.py`から形状とパレットを生成。外部モデルや画像を読み込まずに生成する経路を確認。対象はRailHazardBlock、RailHazardSolid、CombatTurret。AI支援で作成したコード・素材であることと、選考会のAI利用規定は別途整理 |
| 一部背景モデル・岩材質 | `tools/generate_course_mesh_assets.py`が形状とBMPを生成。ORM/heightは`generate_terrain_pbr_maps.py`がそのBMPから生成。ただしこの事実を他の全画像・全モデルに拡張しない |

CC0素材も、同じファイル名という理由だけで由来を確定しない。クレジット一覧への登録は、取得元・必要ならハッシュを確認してから確定する。

## 未確認として残すもの

- `uvChecker.png`（複製を含む）、`fence/fence.png`、`circle.png`、`circle2.png`、`gradationLine.png`、`streakNoise.png`、`beamRamp_lightning.png`、`iceShard.png`、`warpTunnelFlipbook.bmp`、`terrain/terrain_detail_normal.png`：自作・教材・外部配布の根拠と条件を今回のファイル群から確定できなかった。
- 上記で生成経路を示したもの以外のモデル、教材由来のBlenderファイル、人物テクスチャ等：学校の配布一覧・制作履歴との対応が必要。
- 学校提供コード、参考実装、`lib/Receiver`等：著作権ヘッダーがないことは自由利用の証拠ではない。外部提出の許諾と、自作担当範囲を確認する。
- 提出する動画・説明資料に別のBGM・画像・ロゴを加える場合は別途確認。現在の監査は完成した提出物一式の確認ではない。
- 選考会の外部素材、AI支援、学校コード、共同制作の申告規定は、添付の提出方式の画像だけでは判断できない。

## 次に行う作業

1. 学校に「この素材・授業コードを、実行ファイル・動画・ソースコード付きで就職選考会へ提出できるか。FBX/glTF/画像/音声などの元データ同梱も含むか。必要な表記と元の配布条件は何か」を確認する。
2. `monsterBall.png`は提出用の独自画像へ差し替える。起動時参照、フォールバック、VFX登録も合わせて整理する。
3. 不要な教材モデル・起動音を提出用の読み込み経路から外す。必要な素材で許諾が確認できないものは自作または条件を確認した素材へ置換する。
4. 確定した素材一覧に作者・配布URL・ライセンス・変更内容を記載し、必要なライセンス全文を添付する。クレジットを記載するだけで未許諾素材が使えるわけではない。
5. 最終ZIPを展開し、許可済みファイルだけか、起動可能か、表示・音声に教材が残らないか確認する。古いビルド、キャッシュ、`.git`、`.vs`、不要な制作履歴を丸ごと提出しない。

今回の作業では監査資料のみ追加し、素材の削除・ゲーム処理の変更・外部への問い合わせは行っていない。
