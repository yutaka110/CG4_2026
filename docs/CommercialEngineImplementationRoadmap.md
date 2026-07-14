# 商用エンジン実装ロードマップ

## 1. 目的

本資料は、CG4をUnreal Engine型の制作品質を持つ商用レベルのエンジンへ発展させるための、実装順序、変更対象、依存関係、完了条件を定義する。

ここで目標とするのは、Unreal Engineとの全機能比較ではない。CG4で必要な制作を、次の品質で安全かつ反復可能に行える状態を目標とする。

- Authoringデータが、失敗した操作やクラッシュによって部分破損しない。
- 対応するすべてのAuthoring変更が、検証、Undo/Redo、Dirty管理を経由する。
- Play/Simulate中のRuntime状態がAuthoring状態から分離される。
- Asset、Object、Scene、Documentが安定IDで参照される。
- 保存形式にschema versionとmigration経路がある。
- 中央UIやRuntimeを書き換えずにEditor機能を追加できる。
- ShippingビルドからEditorコードとsource asset依存を完全に除外できる。
- Clean Build、Test、Cook、Package、Recoveryを別環境でも再現できる。

関連資料：

- `docs/EditorCoreDesign.md`
- `docs/ProjectLayout.md`

## 2. 実装原則

本ロードマップは完了ゲート方式で進める。依存する前段階のゲートが完了するまで、次のStageを正式経路として採用しない。

各移行は、次の順序で行う。

1. 旧経路を残したまま新しいinterfaceを追加する。
2. 1つのproduction domainだけを新経路へ接続する。
3. Regression TestとFeature Guardで旧経路との動作差を検出する。
4. Command/UIの正式入口を新経路へ切り替える。
5. parity確認後にのみ重複した旧処理を削除する。

追加ルール：

- 1つのPull Requestには、原則として1つの可逆なarchitecture変更だけを含める。
- 保存形式の変更には、version、migration、backup、失敗時の復旧方針を必須とする。
- filesystem変更には、failure injectionとrecovery testを必須とする。
- 新しいEditor機能は、共通Transaction、Dirty State、Validation、Notification、Document経路を使用する。
- Material Graph、Visual Scripting、Sequencer相当などの大型機能は、Stage 0～7の完了後に開始する。

## 3. Stage一覧

| Stage | 目的 | 完了ゲート |
| --- | --- | --- |
| 0 | 現状を安全な基準点として固定する | G0：変更と基準値が記録済み |
| 1 | Clean Buildを再現可能にする | G1：複数構成のClean CI成功 |
| 2 | Editor Coreからdomain依存を除去する | G2：Core Transactionがdomain非依存 |
| 3 | ファイル操作を原子的・復旧可能にする | G3：途中失敗で部分状態を残さない |
| 4 | Play/Simulate分離を完成させる | G4：全Authoring domainが復元可能 |
| 5 | Document Lifecycleを汎用化する | G5：複数Documentの保存・復旧成功 |
| 6 | 商用規模の自動化を構築する | G6：規模・soak・GPU・復旧試験成功 |
| 7 | RuntimeとEditorのBuild Targetを分離する | G7：ShippingにEditor依存なし |
| 8 | Object・Reflection・Serialization基盤を作る | G8：安定Objectがversionをまたいで復元可能 |
| 9 | Scene・Prefab・World Authoringを作る | G9：Scene制作フローが安全かつUndo可能 |
| 10 | DDC・Cook・Packageを作る | G10：source不要のincremental Shipping成立 |
| 11 | Plugin・Scripting APIを安定化する | G11：中央改修なしで機能追加可能 |
| 12 | Unreal型の高度な制作機能を追加する | G12：高度機能が共通基盤を再利用 |
| 13 | 商用Release Gateを適用する | G13：全production gate成功 |

## 4. Stage 0：現状固定とBaseline作成

### 4.1 Working Treeを整理する

現在の変更を次の単位へ分離する。

- Viewport座標系
- Editor Automation Gate
- Editor UI統合
- Visual Studio project設定
- 資料更新

本ロードマップの基盤refactorと、未完了のViewport/UI作業を同じ変更へ混在させない。

### 4.2 Baselineを記録する

`docs/baselines/`またはCI artifactへ次を保存する。

- Debug x64 build結果
- Development/Release buildの可否
- Commercial Gate結果
- Regression/Smoke結果
- Feature Guardのblocked/attention件数
- Asset indexing時間
- Editor frame timing
- 起動時間
- CPU/GPU memory使用量
- Test projectのAsset/Object件数

### Gate G0

- 現在の変更が意味のある単位でcommitされている。
- Commercial Gateを同じ手順で再実行できる。
- Baselineと検証環境が記録されている。
- 基盤refactorに無関係な未完了作業が混在していない。

## 5. Stage 1：再現可能なBuild基盤

### 5.1 Build設定を一元化する

追加対象：

```text
Directory.Build.props
Directory.Build.targets
docs/BuildEnvironment.md
tools/build_editor.ps1
tools/run_editor_gates.ps1
```

一元管理する項目：

- `PlatformToolset`
- Windows SDK version
- C++ language standard
- Warning levelとwarnings-as-errors方針
- Runtime Library
- Output/Intermediate directory
- Editor/Runtime feature definition
- Debug informationとoptimization policy

本体projectとthird-party projectに重複しているtoolset指定は、共通property参照へ置き換える。

### 5.2 Build構成を定義する

- **Debug**：assert、diagnostics、Editor Test有効、最小最適化。
- **Development**：日常的なEditor開発用。主要diagnosticsを有効化。
- **Release**：production検証用の最適化Editor。
- **Shipping**：Editor機能を除外したGame/Runtimeのみ。

### 5.3 Clean CIを追加する

CIは次を実行する。

1. Clean checkoutから開始する。
2. 必要なVisual Studio componentとSDKを検査する。
3. 宣言されたsource/packageからthird-party依存を構築する。
4. Debug、Development、Releaseをbuildする。
5. Regression Testを実行する。
6. Editor Smoke Testを実行する。
7. Commercial Automation Gateを実行する。
8. LogとReportをartifactとして保存する。

### Gate G1

- prebuilt object/libraryなしのclean machineでSolution全体をbuildできる。
- `BuildProjectReferences=false`を回避策として使用しない。
- Debug、Development、ReleaseがCIで成功する。
- 不足prerequisiteに対して具体的な修正方法を表示する。
- 初期Shipping構成がlinkまで成功する。

## 6. Stage 2：Domain非依存Editor Core

### 6.1 Domain非依存Undo Commandを追加する

追加対象：

```text
application/editor/core/EditorUndoCommand.h
application/editor/core/EditorTransactionStack.*
application/editor/core/EditorTransactionBuilder.*
application/editor/core/EditorTransactionMemoryBudget.*
application/editor/core/EditorExecutionContext.*
application/editor/core/EditorError.h
```

基本interface：

```cpp
class IEditorUndoCommand {
public:
    virtual ~IEditorUndoCommand() = default;

    virtual bool Undo(
        EditorExecutionContext& context,
        EditorError& error) = 0;

    virtual bool Redo(
        EditorExecutionContext& context,
        EditorError& error) = 0;

    virtual std::size_t EstimatedBytes() const = 0;
    virtual std::string_view DomainId() const = 0;
};
```

`EditorTransactionStack`の責務は次に限定する。

- Transaction順序
- Undo/Redo移動
- Group/Nested Transaction
- History revision
- 件数とbyte数の予算
- Apply失敗時の状態管理
- Redo破棄
- Transaction labelとID

### 6.2 具体PayloadをDomain Adapterへ移動する

追加例：

```text
application/editor/course/CoursePropertyUndoCommand.*
application/editor/course/CourseSnapshotUndoCommand.*
application/editor/assets/AssetMutationUndoCommand.*
application/editor/play/RuntimeApplyUndoCommand.*
application/editor/vfx/VfxPropertyUndoCommand.*
```

Core Transaction headerから次を除去する。

- `CourseAsset`
- `TerrainAuthoringState`
- `AppRuntimeState`
- `EditorAssetRecord`
- Asset Mutation固有payload
- Runtime Apply固有payload

### 6.3 段階移行する

1. 現行実装の隣に新Stackを追加する。
2. Course property編集を最初に移行する。
3. Label、Target、Before/After結果をmirrorして比較する。
4. 両経路をRegression Testで検証する。
5. Course commandを新経路へ切り替える。
6. Details、Asset、VFX、Runtime Applyを順番に移行する。
7. 最後の移行完了後に具体型variantを削除する。

### 6.4 `EditorContext`を分割する

単一の巨大Contextを次のcapability groupへ分割する。

```text
EditorFrameContext
EditorMutationContext
EditorUiContext
EditorAssetContext
EditorViewportContext
EditorPlayContext
```

Panel/Serviceには必要なcapabilityだけを渡す。必須serviceは、未検査のnullable raw pointerではなく、referenceまたは検証済みhandleで受け取る。

### Gate G2

- Core Transaction headerがCourse、Terrain、VFX、Runtime、Asset Pipeline具体型をincludeしない。
- 新しいAuthoring domain追加時にTransaction Stack本体を変更しない。
- Historyを件数と推定byte数の両方で制限できる。
- Undo/Redo失敗後もdataとhistoryが定義済み状態を保つ。
- Course、Details、Asset、VFX、Runtime Applyが新しい正式経路を使用する。
- Feature GuardのProperty Accessor attentionが0になる。

## 7. Stage 3：原子的かつ復旧可能なFile Transaction

**2026-07-13実装済み。** File Transaction Core、Path Policy、Atomic Writer、Journal/Recovery、disk-backed Trashを追加し、Asset rename/move/delete、Course Save、Editor Layout Saveへ接続した。残るDocument種別はStage 5のProvider追加時に同じAPIへ接続する。Debug build、Regression 19/19、Smoke 10/10、Commercial Gate 9/9を通過済みである。

### 7.1 File Transaction Layerを追加する

```text
application/editor/io/EditorAtomicFileWriter.*
application/editor/io/EditorFileTransaction.*
application/editor/io/EditorFileTransactionJournal.*
application/editor/io/EditorFileRecoveryService.*
application/editor/io/EditorProjectPathPolicy.*
application/editor/io/EditorTrashService.*
```

状態遷移：

```text
Created
  -> Prepared
  -> Validated
  -> Committing
  -> Committed
  -> Cleaned
```

復旧規則：

- `Created`/`Prepared`は安全に破棄できる。
- `Validated`はcommitまたはrollbackできる。
- `Committing`は次回起動時にjournalから復旧する。
- `Committed`はregistry/cache更新完了後にcleanupする。

### 7.2 Atomic Writeを実装する

すべてのDocument/Preference保存を次の順序へ統一する。

1. 同じvolume上のtemporary fileへ書き込む。
2. flushしてcloseする。
3. 再度openして内容を検証する。
4. journalへPrepared状態を記録する。
5. 必要なbackupを保存する。
6. destinationをatomic replaceする。
7. journalをCommittedにする。
8. memory上の状態を更新する。
9. 成功後にtemporary dataを削除する。

適用対象：

- Editor layout
- Editor preferences
- Asset `.meta`
- Course
- Effect/VFX
- Preset
- Scene
- Project Settings

### 7.3 Asset Rename/Move/Deleteを再構築する

Rename/Move：

1. 全pathをresolve/canonicalizeする。
2. 対象が許可されたproject root内にあることを確認する。
3. 直接・間接dependentを列挙する。
4. 未対応のpath-only referenceを拒否またはGUIDへmigrationする。
5. Domain serializerで実際のsource document参照を書き換える。
6. Staging内に変更後の全fileを生成する。
7. Staging fileを再読込して検証する。
8. File set全体をcommitする。
9. Registry、Selection、Thumbnail Cache、Diagnosticsを更新する。
10. Disk-backed recovery dataを参照するUndo Commandを記録する。

Delete：

- 直接削除せず、`.editor/trash/<transaction-id>/`へ移動する。
- Undoはtrashから復元する。
- Redoは再度trashへ移動する。
- History evictionまたは明示cleanup後に完全削除する。
- 大容量Assetのbinaryをmemory上のTransactionへ保持しない。

### 7.4 Failure Injectionを追加する

次の直後に意図的な失敗を発生させる。

- Source staging
- Metadata staging
- 各dependent rewrite
- Journal preparation
- 最初のfile replacement
- Registry update
- Cache refresh
- Commit marker書き込み
- Commit中のprocess終了

### Gate G3

- すべてのfailure injection後に、完全な旧状態または完全な新状態へ復旧する。
- Source、metadata、registry、dependencyの部分不整合が残らない。
- Layout/Preference保存中の中断後もEditorを起動できる。
- 大容量Delete/Undo履歴が設定済みmemory/disk予算内に収まる。
- 未対応path-only dependentがあるmutationを明確なrepair action付きで拒否する。

## 8. Stage 4：完全なPlay/Simulate Isolation

**2026-07-13実装済み。** Provider Registry、型消去Snapshot、failure-safe Restore、Mutation Guard、Runtime ChangeSet UI、選択的Keep Changesを追加した。現在の実Authoring経路であるCourse、Terrain/Gameplay Tuning、VFX Authoring Asset、Post-processを登録し、採用ProviderだけをGrouped TransactionでUndo/Redoできる。Debug build、Regression 21/21、Smoke 10/10、Commercial Gate 9/9を通過済みである。

### 8.1 Authoring WorldとRuntime Worldを分離する

目標ownership：

```text
EditorProject
  -> AuthoringWorld

EditorPlaySession
  -> RuntimeWorld clone
```

Runtime systemはAuthoring Worldへのwritable pointerを保持しない。

### 8.2 Isolation Providerを追加する

```text
application/editor/play/IEditorPlayIsolationProvider.h
application/editor/play/EditorPlayIsolationRegistry.*
application/editor/play/EditorPlaySnapshot.*
application/editor/play/EditorPlayMutationGuard.*
application/editor/play/EditorRuntimeChangeSet.*
```

基本interface：

```cpp
class IEditorPlayIsolationProvider {
public:
    virtual ~IEditorPlayIsolationProvider() = default;
    virtual bool Capture(EditorPlaySnapshot&, EditorError&) = 0;
    virtual bool Restore(const EditorPlaySnapshot&, EditorError&) = 0;
    virtual bool BuildRuntimeChangeSet(EditorRuntimeChangeSet&, EditorError&) = 0;
    virtual std::uint64_t AuthoringFingerprint() const = 0;
};
```

必須Provider：

- Course
- Terrain
- Scene/Entity/Component
- VFX Authoring Asset
- Post-process
- Camera Rig/Key
- Render Preset
- Gameplay Tuning
- Mutable Project Settings

### 8.3 Mutation Gatewayを強制する

すべてのAuthoring変更を次の経路へ統一する。

```text
UI/Command
  -> Mutation Guard
  -> Validation
  -> Transaction Builder
  -> Domain Adapter
  -> Dirty State/Notification
```

Play/Simulate中の直接Authoring書き込みは、Debugではassert、AutomationではFeature Guard errorにする。

### 8.4 Selective Keep Changesを実装する

Runtime変更を差分一覧として表示し、変更単位でApply/Ignoreを選択できるようにする。採用した変更は1つのGrouped Transactionとして記録し、Authoringへ戻った後もUndo可能にする。

### Gate G4

- 登録済み全Authoring fingerprintがPlay前とStop後で一致する。
- 未採用Runtime変更が完全に破棄される。
- 採用変更だけが適用され、Undo/Redoできる。
- Capture/Restoreの途中失敗がAuthoring状態を破壊しない。
- Play/Stop反復試験と中断session復旧試験が成功する。

## 9. Stage 5：汎用Document Lifecycle

### 9.1 Document Modelを追加する

```text
application/editor/documents/EditorDocumentId.h
application/editor/documents/IEditorDocumentProvider.h
application/editor/documents/EditorDocumentRegistry.*
application/editor/documents/EditorDocumentManager.*
application/editor/documents/EditorDocumentSaveService.*
application/editor/documents/EditorAutosaveService.*
application/editor/documents/EditorExternalChangeMonitor.*
application/editor/documents/EditorDocumentRecoveryService.*
```

例：

```cpp
struct EditorDocumentId {
    EditorAssetGuid assetGuid;
    EditorDocumentTypeId type;
};
```

### 9.2 Document操作を統一する

すべてのeditable document providerが次に対応する。

- Open
- Save
- Save As
- Save All参加
- Reload
- Close
- Reopen
- Duplicate
- Validate
- Autosave serialization
- 旧schemaからのmigration

初期Provider：

- Course
- Scene
- Effect/VFX
- Material/Render Preset
- Project Settings

### 9.3 AutosaveとCrash Recoveryを追加する

Autosaveはsourceを直接上書きせず、次へ保存する。

```text
.editor/autosave/<document-guid>/<revision>/
```

復旧時に比較する情報：

- 正式fileのtimestamp/content hash
- Autosaveのtimestamp/content hash
- Document schema version
- 最後に成功したsave revision

### 9.4 外部変更を検出する

Editor変更と外部変更が競合した場合は、次を選択できるようにする。

- 外部versionをReload
- Editor versionを維持
- Compare
- Save As
- Domainが安全なmergerを提供する場合のみMerge

外部変更を無断で上書きしない。

### Gate G5

- 複数Document typeをまたぐSave Allが成功する。
- 複数Dirty Documentを1つの安全な確認flowで閉じられる。
- 強制終了後にAutosaveから作業を復元できる。
- 外部変更を上書き前に検出する。
- Schema migrationがbackupとvalidation reportを生成する。

**2026-07-13実装済み。** Generic Document ID／Provider Registry／Manager、5種の初期Provider、Atomic Save All、Autosave、Crash Recovery、外部変更競合検出、Schema Migration backup/report、単一確認Close flowを実装した。Courseはlive authoring modelを直接serialize/deserializeするProviderへ移行し、Documentタブと保存Commandも共通経路へ接続した。Regression 22/22、Smoke 10/10、Commercial Gate 9/9を通過済みである。

## 10. Stage 6：商用規模Automation

### 10.1 Test Layerを分離する

- Unit Test
- Integration Test
- Recovery/Failure Injection Test
- Performance Test
- GPU Test
- End-to-End Editor Workflow Test
- Soak Test
- Shipping Verification

### 10.2 規模試験を追加する

- 10,000/100,000 Asset indexing
- 数千Scene Entity
- 数百Material/VFX Asset
- 深いdependencyとcycle
- 複数GBのimport
- 大量Thumbnail queueとcache eviction

### 10.3 Soak/Repetition Testを追加する

- 8時間Editor session
- 数千回のPlay/Stop
- Import/Reimport連続実行
- Layout/DPI/Window変更の反復
- Document Save/Reload反復
- 対応可能なGPU device-lost simulation
- Memory、descriptor、handle、thread leak監視

### 10.4 Environment Matrixを拡張する

- NVIDIA/AMD/Intel GPU
- 対応Windows SDK
- 日本語path
- OneDrive配下project
- Long path
- Read-only file/folder
- Source Control管理下のworkspace

### Gate G6

- Feature Guardがblocked 0、attention 0である。
- Recovery scenarioでdata不整合が発生しない。
- Soak Testがmemory/performance予算内で完了する。
- 代表datasetの性能劣化をCIが検出する。
- 対応GPU/環境ごとの結果が記録される。

## 11. Stage 7：RuntimeとEditorのBuild分離

### 11.1 Moduleを分割する

```text
EngineCore
EngineObject
EngineSerialization
EngineRuntime
EngineRenderer
EngineAssetRuntime

EditorCore
EditorUI
EditorAssetPipeline
EditorPlaySession
EditorAdapters

EditorTests
GameRuntime
```

許可する依存方向：

```text
GameRuntime
  -> EngineRuntime
  -> EngineCore

EditorAdapters
  -> EditorCore
  -> EngineRuntime/EngineObject

EditorUI
  -> EditorCore/EditorAdapters
```

禁止する依存：

```text
EngineRuntime -> EditorCore
GameRuntime   -> EditorUI
EngineCore    -> application/editor
```

### 11.2 ShippingからEditorを除外する

Shipping除外対象：

- ImGui
- Asset Importer
- Source Asset Parser
- Thumbnail Renderer
- Editor Command/Panel
- Editor-only reflection metadata
- Editor Diagnostics UI
- Editor Document/Transaction Service

### Gate G7

- ShippingがEditor library/DLLへlinkしない。
- RuntimeからEditor headerをincludeできないことをCIで検査する。
- ShippingがCook済みdataだけで起動する。
- Development/Editorは承認済みRuntime APIを共有できる。

## 12. Stage 8：Object・Reflection・Serialization基盤

### 12.1 Object Modelを実装する

- Stable Object GUID
- Stable Type ID
- Object ownership/lifetime
- Weak Object Handle
- Handle generation/invalidation
- Class Descriptor
- Property Descriptor
- Default Object/Value
- Property Flag
- Reference Resolution
- 明確なGarbage CollectionまたはOwnership model

### 12.2 Property情報を共通化する

1つのProperty metadataを次で共有する。

- Details UI
- Serialization
- Validation
- Undo/Redo
- Default/Reset
- Multi-selection intersection
- Copy/Paste
- Runtime exposure policy
- Scripting exposure policy

### 12.3 保存DataをVersion化する

各serialized rootへ次を保存する。

- Format ID
- Schema Version
- Engine compatibility range
- 必要なObject/Type version
- Migration元version

### Gate G8

- ObjectがGUID参照を保ったままSave/Loadできる。
- Asset RenameでGUID-backed referenceが壊れない。
- 対応する旧schemaをbackup付きで自動migrationできる。
- Details、Serialization、Validation、Transactionが同じmetadataを使用する。

## 13. Stage 9：Scene・Prefab・World Authoring

### 13.1 Scene/Level Document

- Entity hierarchy
- Component
- Transform parenting
- Stable Entity/Component GUID
- Outliner integration
- Viewport picking
- Multi-selection
- Duplicate/Copy/Paste
- Transaction-backed hierarchy編集

### 13.2 Prefab

- Prefab Asset/Instance
- Nested Prefab policy
- Property/Structural Override
- Apply/Revert
- Missing Prefab recovery
- Prefab migration

### 13.3 World Composition

- Authoring World
- Runtime World clone
- Scene Instance
- Sublevel/Streaming単位
- Cross-scene reference policy
- Load/Unload validation

### Gate G9

- Sceneを作成、保存、再読込、複製、migrationできる。
- Hierarchy/Transform編集をUndoできる。
- Prefab変更が有効なoverrideを失わずinstanceへ伝播する。
- PIEがScene Worldを独立Runtime Worldへcloneする。

## 14. Stage 10：Asset Pipeline・DDC・Cook・Package

### 14.1 Asset形態を分離する

```text
Source Asset
  -> Import
  -> Intermediate Product
  -> Derived Data Cache
  -> Cook
  -> Runtime Package
```

記録する情報：

- Source content hash
- Import settings hash
- Importer ID/version
- Dependency hash
- Target platform
- Derived product version

### 14.2 Derived Data Cache

- Deterministic cache key
- Local cache
- Optional shared cache
- Disk budget
- LRU/Generation cleanup
- Corruption detection
- Sourceを変更しないcache rebuild

### 14.3 Incremental Cook/Package

- 変更Assetと影響dependentだけをCookする。
- Missing/cyclic dependencyを検出する。
- Deterministic manifestを生成する。
- Runtime packageをSource/Editor dataから分離する。
- Release前にpackage内referenceを検証する。

### Gate G10

- ShippingがSource Assetなしで起動する。
- 変更Assetと影響dependentだけが再Cookされる。
- Importer version変更が正しいderived productをinvalidateする。
- 同一inputのCookが同等outputを生成する。

## 15. Stage 11：Plugin・Scripting API

### 15.1 Plugin Model

- Plugin ID/version
- Engine compatibility range
- Dependency
- Load phase
- Editor-only/Runtime capability
- Feature flag
- Startup/Shutdown hook

### 15.2 Registration API

- Command
- Panel/Menu/Toolbar
- Asset Importer/Type Handler
- Object/Component Type
- Property Customization
- Validation Provider
- Serialization Migration
- Runtime Watch Provider
- Cook Extension

不安定なC++ ABIを直接公開しすぎず、versioned interfaceまたはC boundaryを検討する。

### 15.3 Missing Plugin対策

- 可能な限りunknown serialized dataを保持する。
- Missing type/assetを明示する。
- Data lossを伴うSaveは明示確認なしに許可しない。
- Project load完了前にcompatibility診断を出す。

### Gate G11

- 中央Engine/UI fileを編集せず、新Asset type、Panel、Component、Validation、Cook stepを追加できる。
- Plugin無効化でProjectを破壊しない。
- API incompatibilityを危険なstartup前に検出する。

## 16. Stage 12：高度なUnreal型Authoring Tool

推奨実装順：

1. Material Graph
2. Advanced VFX Graph
3. Sequencer/Timeline
4. Animation State Machine
5. Gameplay Visual Scripting
6. CPU/GPU Profiler
7. RenderGraph Visualizer
8. Navigation/AI Authoring Tool
9. Localization Tool
10. Build/Cook/Package UI

各Graph/Timeline systemは独自のUndo、Dirty、Save、Asset ID systemを作らず、次を再利用する。

- Stable Object/Asset ID
- Reflection/Property metadata
- Document Lifecycle
- Transaction Stack
- Atomic Persistence
- Validation/Diagnostics
- Autosave/Crash Recovery
- Plugin Registration

### Gate G12

- 高度機能がprivateなUndo/Dirty/Save/Asset ID systemを導入していない。
- Documentがcrash recoveryとschema migrationに対応する。
- Runtime出力をCookでき、Editor依存を含まない。

## 17. Stage 13：Commercial Release Gate

商用利用開始条件：

- G0～G12完了
- Clean Debug/Development/Release/Shipping成功
- Feature Guard blocked 0
- Feature Guard attention 0
- Recovery/Failure Injection成功
- Representative Scale Budget成功
- 8時間Soak成功
- 対応GPU Matrix成功
- Shipping Package Validation成功
- Asset/Document Migration Backup確認
- Crash Reporter/Diagnostic Artifact確認
- Third-party License/Attribution確認
- Release Checklist承認

P0 Gateが1つでも未完了の場合、Commercial Readyとは表現しない。

## 18. 直近のPull Request実装順

1. Toolset/SDK設定の一元化
2. Clean Build Script追加
3. Debug Buildと既存Gateを実行するCI追加
4. `IEditorUndoCommand`とbyte予算追加
5. Course Transaction移行
6. Details/Property Transaction移行
7. Asset Transaction移行
8. Runtime Apply Transaction移行
9. Core Transaction headerからdomain payload削除
10. `EditorContext`をcapability contextへ分割
11. Layout/PreferenceのAtomic Write化
12. File Transaction Journal/Recovery追加
13. Asset Deleteをdisk-backed Trashへ移行
14. Asset Rename/Moveをstaged transactionへ移行
15. Filesystem Failure Injection Gate追加
16. Play Isolation Provider Registry追加
17. Course/Terrain Isolation移行
18. VFX/Post-process/Camera/Render/Gameplay Provider追加
19. Generic Document Registry/Manager追加
20. CourseをGeneric Document Providerへ移行
21. Save All/Autosave/Crash Recovery追加
22. External Change Detection/Schema Migration追加
23. Editor/Runtime Build Target分割
24. Source-free Shipping Packageの初回検証

## 19. 優先度分類

### P0：商用Data Safety Blocker

- Clean Reproducible Build
- Domain-neutral Transaction
- Atomic File Transaction
- Complete Play/Sim Isolation
- Generic Document Lifecycle
- Autosave/Crash Recovery

### P1：Production Scale/Distribution

- Scale/Soak/Recovery/GPU Automation
- Editor/Runtime Module分離
- Shipping Build
- Object/Reflection/Serialization
- Scene/Prefab Authoring
- DDC/Cook/Package

### P2：Platform Extensibility/Advanced Tool

- Plugin/Scripting API
- Material Graph
- VFX Graph
- Sequencer
- Animation Tool
- Visual Scripting
- Advanced Profiling/Diagnostics

## 20. 最終原則

機能数より先に、次の保証を完成させる。

1. Projectを必ず再buildできる。
2. User dataは完全に変更されるか、まったく変更されない。
3. すべてのAuthoring変更が検証され、Undo可能である。
4. Runtime上の実験がSource dataを暗黙に変更しない。
5. 保存DataがCrashとEngine Upgradeを越えて復旧できる。
6. Runtime ProductがEditor実装詳細へ依存しない。

これらの保証を確立した後に高度なUnreal型Toolを追加することで、互換性のないSave、Undo、Asset、Runtime systemが増殖することを防ぐ。
