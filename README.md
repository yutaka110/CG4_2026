# GE3 / CG4 Project

DirectX 12を使用したWindows向けゲームエンジン／エディタです。スキニング、アニメーション、GPU Particle、RenderGraph、VFXオーサリング、地形、商用エディタ基盤を単一プロジェクトで検証しています。

## 加点要素の実装状況

| 項目 | 状態 | 実装内容 |
|---|---|---|
| Skinningモデルの表示 (20) | 実装済み | Assimpから階層、骨、最大4ウェイトを読み込み、SkinClusterとMatrix Paletteを使用して描画します。`simpleSkin`、`human walk`、`human sneakWalk`を切り替えられます。 |
| ComputeShaderによるスキニング (10) | 実装済み | `Skinning.CS.hlsl`で頂点と法線を変形し、UAV出力をVertex Bufferへ遷移して描画します。Compute経路が利用できない場合はVertex Shader Skinningへフォールバックします。 |
| MultiMesh & MultiMaterial対応 (5) | 実装済み | 複数Meshを共有Vertex／Index Bufferへ結合し、SubMeshのIndex範囲ごとにMaterial Constant Buffer、Albedo、Normal Textureを切り替えて描画します。通常モデル、VS／Compute Skinningに対応しています。 |
| Animation補間 (5) | 実装済み | 平行移動・拡縮は線形補間、回転はQuaternion Slerpを使用します。ループ、速度、時間指定に対応しています。 |
| 骨のデバッグ表示 (10) | 実装済み | 親子Jointのライン、各JointのXYZマーカーを専用Debug Pipelineで描画します。 |
| 手からパーティクルを出す (10) | 実装済み | Bone Socketを永続Effect Instanceへ接続し、手JointのWorld座標からGPU管理Particleを連続生成します。UI、環境変数、状態Telemetryに対応しています。 |
| 武器を手に持たせる (10) | 実装済み | 既存Bone Socketを共有する`WeaponAttachment`が、手JointのWorld行列を共有訓練剣Meshの永続Transform Bufferへ反映します。UI、環境変数、状態Telemetry、安全な非表示フォールバックに対応しています。 |
| GPU Particle (20) | 実装済み | GPU管理Particle Pool、Compute更新、Emitter、Spawn、Dead/Alive List、Indirect Dispatch、Indirect Drawに対応しています。 |
| その他 (10) | 実装済み | 商用エディタ基盤として、RenderGraph、Animation State MachineとCross-fade、VFX Graph、GPU Driven Visibility、Terrain、Commercial Editor Automationを追加しています。提出時は評価対象とする機能をデモで明示してください。 |

安全側の自己評価は **100 / 100点（100%）** です。全加点要素を実装済みです。

## 必要環境

- Windows 10/11 x64
- Visual Studio 2026 (18.x)
- Desktop development with C++
- MSVC v145 x64/x86 Build Tools
- Windows SDK 10.0.26100.0
- PowerShell 5.1またはPowerShell 7
- DirectX 12対応GPU

既定のToolsetと依存プロジェクトはVisual Studio 2026用の`v145`に統一しています。Windows SDK、コンパイラ、DXC/DXILの承認済みバージョンとハッシュは`Build/GE3.Dependencies.lock.json`で管理します。Visual Studio 2022の`v143`ビルドは互換確認に限定し、商用検証済み成果物として扱いません。

## ビルド

Developer PowerShell for Visual Studio 2026でリポジトリのルートから実行します。

```powershell
./tools/check_prerequisites.ps1 -Configuration Debug
./tools/build.ps1 -Configuration Debug
```

Visual Studioからは`CG4.sln`を開き、`Debug | x64`で`GE3`をビルドします。成果物はソースツリー外の次の場所へ出力されます。

```text
../generated/outputs/Debug/GE3.exe
```

## スキニングデモの操作

エディタの`Material Settings`パネルにある`Scene Objects`を使用します。

- `Show Skinned`: スキニングモデルを表示
- `Show Skeleton`: 骨階層とJointマーカーを表示
- `Skinned Model`: 3種類のモデルを切り替え
- `Play Animation`: アニメーション再生／停止
- `Loop Animation`: ループ切り替え
- `Animation Speed`: 再生速度。負数で逆再生
- `Animation Time`: 任意時間のPoseを確認
- `Skinned Scale / Rotate / Translate`: モデルTransformの調整
- `Skinned Surface VFX`: Compute Skinning結果を使用するSurface VFX経路を有効化

起動時設定には次の環境変数も利用できます。

```powershell
$env:GE3_SHOW_SKINNED='1'
$env:GE3_SHOW_SKELETON='1'
$env:GE3_SKINNED_SURFACE_VFX='1'
$env:GE3_SKINNED_MODEL_INDEX='1'
../generated/outputs/Debug/GE3.exe
```

`GE3_SKINNED_MODEL_INDEX`は`0=simpleSkin`、`1=human walk`、`2=human sneakWalk`です。Compute Skinningを無効化してVertex Shader経路を比較する場合は`GE3_DISABLE_COMPUTE_SKINNING=1`を設定します。

Skin PaletteはAssimpのBone OffsetとBind Pose Jointから実際のMesh Root基準を復元し、行ベクトル規約の`InverseBind * JointGlobal * MeshRootInverse`で生成します。提出用ShowcaseはCompute Skinningを優先し、Compute ResourceまたはPipelineを利用できない場合だけVertex Shader Skinningへフォールバックします。実Humanoid全頂点について、Bind Pose変形後の位置が入力位置と一致する回帰テストを実行します。

Animation ClipもMesh／Skeletonと同じ`aiProcess_ConvertToLeftHanded`で読み込み、TranslationとQuaternionの座標系を統一します。Humanoid Animationは開始・中間・終端でJoint行列と全頂点の有限性を検証し、変形後Boundsが人体サイズを維持することを回帰テストで保証します。

## MultiMesh / MultiMaterial

`ModelData`は共有Vertex／Index Bufferに加え、`SubMeshData(indexStart, indexCount, materialIndex)`とMaterial Slot配列を保持します。Assimpは`aiMesh::mMaterialIndex`、Base Color、Albedo Texture、Normal Textureを保持し、Textureパスをモデルディレクトリ基準で正規化します。旧単一Materialモデルは自動的にMaterial Slot 0と全Index範囲のSubMeshへ移行します。

GPU側ではMaterial Slotごとに永続Constant BufferとTexture Descriptorを一度だけ確保します。Textureは正規化パスで重複排除され、Albedo欠落時はモデル既定Texture、Normal欠落時は生成Flat Normalを使用します。描画時はBufferを再生成せず、SubMeshごとに`StartIndexLocation`とMaterial Bindingだけを切り替えます。

提出用Editorは引数なしで`MultiMaterialShowcaseSceneState`を起動します。人型のCompute Skinned Modelと、正面・側面・上面に3つの独立Material Slotを持つ立体`multi_material_demo`を固定カメラと2灯ライティングで同時表示します。Presentation Defaultsはカメラ、暗色Clear Color、Material Tint、Environment反射、Key／Fill Light、人型Transformを明示的に初期化するため、直前のEditor設定やAutosave状態に左右されません。人型は画面高の45～75%に収まる近接構図です。さらに`mixamorig:RightHand`のBone Socketへ共有訓練剣と青い`hand_socket_particle`、`mixamorig:LeftHand`へ短寿命・狭範囲の赤い`left_hand_socket_particle`を独立接続します。武器と両手Particleは引数、環境変数、Editor操作なしで自動的に有効化されます。提出シーンのVFX／Animation時計はEditorのPlay／Stop状態から分離した固定16ms刻みで進むため、exeを起動するだけでEmitterが開始します。Particleは提出画角向けにサイズ、発光、放出範囲、Soft Depth Fade、Socket Offsetを調整し、赤い粒子にはEmitter継続時間と独立した`particle.lifetime`を設定して左手が発生源だと判別できる範囲に制限しています。

訓練剣は右手Jointへ追従する共有GPUモデルです。Procedural Box生成時の三角形WindingをNormal基準で修復し、`Blade`／`Guard`／`Grip`の3 SubMesh・3 Materialへ分割しています。各Materialは暗いVFX用`gradationLine`ではなく白Albedoへ不透明な青銀／橙金／濃茶のBase Colorを乗せるため、固定照明下でも輪郭と部位差を確認できます。提出用Socket OffsetはScale 1.10、Identity Quaternion、手前方向Z Offsetを固定しています。Mixamo Humanoidの`Armature`が持つ単位変換用`0.01`スケールは、Bone SocketでJoint平行移動を維持しながら基底だけGram–Schmidt正規化して除去します。これにより武器が約100分の1へ縮小することを防ぎつつ、明示的な`InheritJointScale`モードではSquash／Stretchを継承できます。Showcaseパネルには入力Joint Scaleと正規化後World Scaleも表示します。`Weapon Draw`はAttachment状態とは別に実GPU Draw数とCPU投影Boundsを表示し、正常時は`visible | SubMeshes 3 / Materials 3`となります。`not submitted`ならGPU Material／Descriptor、`offscreen`ならSocket Transform／画角、`too small`ならJoint／Owner Scaleを調査してください。12×12 px未満は画面内でも可読表示とは判定しません。

XInput互換パッドを接続すると、左スティックで人型モデルを移動、右スティックで向きを変更できます。`A`は骨デバッグ表示、`X`はwalk／sneakWalk切替、`B`は位置・向き・Animation時間のリセットです。未接続時もAnimationは自動再生され、画面左上の`Submission Showcase`パネルで接続状態、入力値、操作方法を確認できます。

Assimp取込後は全SubMeshの三角形について幾何法線と頂点法線を照合し、逆向きのWindingを自動修復します。欠損・非有限・ゼロ長Normalは修復後の面法線から再生成されます。提出用OBJ自体の12三角形もDirectXの時計回りFront Faceと外向きNormalが一致するよう修正済みです。

```powershell
../generated/outputs/Debug/GE3.exe
```

専用Modeは`--multi-material-showcase`でも明示指定できます。従来のVFX Previewは`--vfx-preview`、既存のレールシューティングは次を使用します。

```powershell
../generated/outputs/Debug/GE3.exe --multi-material-showcase
../generated/outputs/Debug/GE3.exe --vfx-preview
```

```powershell
../generated/outputs/Debug/GE3.exe --rail-shooter
```

PreviewはEditorカメラ入力とVFX Model Objectsを有効にし、RailShooterのCourse／自動カメラ初期化をスキップするため、課題の描画確認を再現可能にします。`GE3_VFX_MODEL_0_INDEX`を指定した場合は初期デモモデルを上書きできます。

Document Recoveryは起動直後にAutosave ManifestとContent Hashを検証します。破損世代は削除せず`.editor/recovery/quarantine`へ移動し、復元候補から除外するため、同じHash mismatch通知は次回起動で繰り返されません。正常な候補は対応Documentが利用可能になるまで保持され、Rail側のCourseが開いた時点で復元されます。`.editor/autosave`と`.editor/recovery`は端末固有の状態としてGit管理から除外します。

## Bone Socket基盤

`BoneSocketBinding`は対象Joint名、Quaternion形式のローカルオフセット、検証済みJoint indexを保持します。行ベクトル規約に従い、Socket World行列は次の順序で評価します。

```text
SocketLocalOffset * JointSkeletonSpace * OwnerWorld
```

初回評価時だけ`Skeleton::jointMap`からJointを検索し、以降はindexとJoint名の一致を確認してキャッシュを再利用します。SkeletonのHot Reloadなどで不一致を検出した場合はキャッシュを破棄します。評価失敗時は古いPoseを返さず、恒等行列と`BoneSocketStatus`の診断値を返します。利用側は`BoneSocketPose::IsValid()`を確認してから武器またはEmitterへ行列を渡してください。

### 手元GPU Particle

`Material Settings > Scene Objects > Hand GPU Particle`で有効化します。`human walk`または`human sneakWalk`を選択し、Right/Left HandとSocket Offsetを調整できます。更新順序は`Animation -> Bone Socket -> Effect Instance -> GPU Spawn`です。JointまたはEffectが見つからない場合はEmitterを停止し、パネルへ診断状態を表示します。

起動時から有効化する場合は次を使用します。`GE3_SKINNED_MODEL_INDEX`を省略すると`human walk`が選択されます。

```powershell
$env:GE3_HAND_PARTICLE='1'
$env:GE3_HAND_PARTICLE_JOINT='mixamorig:RightHand'
$env:GE3_HAND_PARTICLE_OFFSET_X='0.0'
$env:GE3_HAND_PARTICLE_OFFSET_Y='0.0'
$env:GE3_HAND_PARTICLE_OFFSET_Z='0.0'
../generated/outputs/Debug/GE3.exe
```

### Weapon Attachment

`Material Settings > Scene Objects > Weapon Attachment`で有効化します。`WeaponAttachment`は手元GPU Particleと同じ`BoneSocketBinding`／行列規約を使用し、Animation更新後の手Jointへ訓練剣を接続します。訓練剣のMeshとTextureは管理モデルライブラリが一度だけ生成・所有し、AttachmentはモデルIDと専用の永続Transform Bufferのみを使用します。Joint、非有限行列、モデル資源のいずれかが無効な場合は、古いPoseを描画せず安全に非表示へ遷移します。

訓練剣は外部アセットへ依存しないコード生成Meshです。起動時から有効化する場合は次を使用します。Quaternionは`X/Y/Z/W`順です。

```powershell
$env:GE3_WEAPON_ATTACHMENT='1'
$env:GE3_WEAPON_ATTACHMENT_JOINT='mixamorig:RightHand'
$env:GE3_WEAPON_ATTACHMENT_OFFSET_X='0.0'
$env:GE3_WEAPON_ATTACHMENT_OFFSET_Y='0.0'
$env:GE3_WEAPON_ATTACHMENT_OFFSET_Z='0.0'
$env:GE3_WEAPON_ATTACHMENT_ROTATION_X='0.0'
$env:GE3_WEAPON_ATTACHMENT_ROTATION_Y='0.0'
$env:GE3_WEAPON_ATTACHMENT_ROTATION_Z='0.0'
$env:GE3_WEAPON_ATTACHMENT_ROTATION_W='1.0'
../generated/outputs/Debug/GE3.exe
```

## GPU Particle

ParticleはRenderGraphへSimulation PassとDraw Passを登録します。GPU管理経路では次の順序で処理します。

```text
Frame Begin
  -> Emitter Update / Reset
  -> Particle Update
  -> Spawn Prepare
  -> ExecuteIndirect Spawn
  -> Indirect Draw Args生成
  -> ExecuteIndirect Draw
```

エディタでは`VFX Visibility > Particles`を有効にし、Effect AssetまたはVFX Showcaseから再生します。Particle数、Dead/Alive数、Indirect Dispatch数はVFX Telemetryで確認できます。

## 検証

```powershell
../generated/outputs/Debug/GE3.exe --editor-core-regression
../generated/outputs/Debug/GE3.exe --effect-authoring-smoke
../generated/outputs/Debug/GE3.exe --editor-smoke-run
```

商用検証一式は次のコマンドで実行します。

```powershell
./tools/run_editor_validation.ps1 -Configuration Development -RequireCleanTree
```

詳細は`docs/BuildEnvironment.md`、`docs/VfxEngineFlow.md`、`docs/EditorCoreDesign.md`を参照してください。

2026-07-21時点ではVisual Studio 2026/v145のDevelopment／Releaseビルド成功（警告0・エラー0）、Editor Core Regression `71 / 71`、引数なしのCompute Skinning＋MultiMaterial提出デモでDevelopment／Releaseの実GPUスモークを確認済みです。Humanoid Bind Pose全13,538頂点の一致、Animation 3時点のJoint／頂点／Bounds、提出構図、GamepadのDead Zone、正規化、最大入力に加え、提出用訓練剣の3 Material Layout・全Index範囲・白Albedo・Winding／Normal整合性も回帰テスト対象です。

## 主要実装

- `application/ModelLoaderAssimp.cpp`: Mesh、Node、Bone Weight、SubMesh、Material Slotの読み込み
- `application/AnimationClip.cpp`: Animation Clip読み込みと補間
- `application/Skeleton.cpp`: Skeleton生成、Pose適用、Cross-fade
- `application/BoneSocket.cpp`: Joint解決、キャッシュ検証、Socket World行列評価
- `application/HandParticleAttachment.cpp`: Bone SocketとGPU Particle Effect Instanceの接続
- `application/WeaponAttachment.cpp`: Bone Socketと共有武器モデル描画の接続、診断Telemetry
- `application/AppSceneResources.cpp`: SkinCluster、Palette、Material Constant Buffer、Texture Descriptor管理
- `application/AppSceneRenderPipeline.cpp`: 通常／VS／Compute SkinningのSubMesh Material描画
- `application/AppGpuParticleSystem.cpp`: GPU Particle ResourceとSimulation
- `application/src/vfx/ParticleRenderer.cpp`: Particle RenderGraph PassとIndirect Draw
- `Resources/Skinning.CS.hlsl`: Compute Skinning
- `Resources/SkinningObject3D.VS.hlsl`: Vertex Shader Skinning
- `Resources/SkeletonDebug.VS.hlsl`: 骨デバッグ描画

## 現在の制約

- Assimp埋め込みTextureは現在Fallback Textureを使用します。外部TextureファイルのMultiMaterialは対応済みです。
- Bone Socket、手Particle、武器AttachmentのRuntime接続は実装済みです。現在のデモ武器はコード生成の単一Material訓練剣です。
- 実行時にアセットが見つからない場合、該当モデルは安全に非表示になります。`Resources/`を実行ファイルと同じ出力ディレクトリへ配置してください。

## ライセンス

第三者ライブラリとライセンスは`THIRD_PARTY_NOTICES.md`およびビルド成果物内の`Licenses/`を参照してください。
