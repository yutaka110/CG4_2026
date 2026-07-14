# Editor Evolution Design

## 1. 目的

本資料は、現在のCG4 Editorを「Debug/Domain Panelを共通Workspaceへ配置したEditor」から、「Scene内のObjectを一貫した操作で選択、編集、保存、実行できる統合World Editor」へ昇華させるための設計と推奨実装順序を定義する。

本資料は次を対象とする。

- Editorの次段階に必要なarchitecture修正
- World Outliner、Viewport、Details、Content Browserの統合
- Transform Gizmoのproduction実装
- Transaction、File Safety、Play/Simulate分離
- Generic Document、Autosave、Crash Recovery
- Course/VFX固有Panelの共通Editor基盤への移行
- UI再編と商用品質の完了条件

エンジン全体の長期計画は`docs/CommercialEngineImplementationRoadmap.md`を参照する。

## 2. 現在地

現在のEditorには次の共通基盤が存在する。

- Panel Registry/Host
- Persistent Workspace Layout
- Command Registry、Menu、Toolbar、Command Palette
- Shared Selection
- Generic Details
- Asset Registry/Browser
- Diagnostics、Notifications、Status Bar
- Play/Simulate control
- Undo/Redo foundation
- Runtime Watch
- Course Timeline
- VFX/RenderGraph diagnostics

一方、次の制約が残っている。

- 左SidebarがSelection表示中心で、World hierarchyを持たない。
- Transform Gizmoは接続・準備状態が中心で、完全なdrag操作ではない。
- Course、VFX、Render debug機能が同一Panelへ集中している。
- Play/Simulate isolationの対象が全Authoring domainではない。
- Asset identityのdurable metadata移行が完了していない。
- Course専用Document Lifecycleが残っている。
- Asset/Layout保存が完全なatomic transactionになっていない。
- Bottom DockのPanel数が多く、一般AuthoringとDeveloper機能が混在している。

## 3. 次の完成像

次の一連の操作が、Course、Terrain、VFX、Camera、Gameplay Objectで共通に動作する状態を最初の完成点とする。

```text
Projectを開く
  -> World OutlinerからObjectを選択
  -> ViewportにTransform Gizmoを表示
  -> DetailsまたはGizmoで編集
  -> Validation
  -> Undo/Redo
  -> Document保存/再読込
  -> Play/Simulate
  -> StopでAuthoring状態を復元
  -> 必要なRuntime変更だけApply
```

目標Workspace：

```text
+--------------------------------------------------------------+
| File Edit Window Tools Build Play Help                       |
+--------------------------------------------------------------+
| Save Undo Redo | T R S | World Local Snap | Play Sim Stop    |
+----------------+-----------------------------+---------------+
| World Outliner |          Viewport           | Details       |
|                |                             | Components    |
|                |                             | Validation    |
+----------------+-----------------------------+---------------+
| Content Browser              | Output / Sequencer / Profiler |
+--------------------------------------------------------------+
| Status: dirty, autosave, import, shader, source control       |
+--------------------------------------------------------------+
```

## 4. Architecture方針

### 4.1 Authoring変更の正式経路

すべてのAuthoring変更を次へ統一する。

```text
UI / Command / Gizmo / Drag & Drop
  -> EditorAuthoringMutationGuard
  -> EditorValidationService
  -> EditorTransactionBuilder
  -> Domain Adapter
  -> EditorDirtyStateService
  -> EditorNotificationCenter
```

禁止事項：

- Panelから`CourseAsset`やRuntime stateを直接変更する。
- Gizmo drag中にTransactionを作らず保存対象を変更する。
- Play中にMutation Guardを通らずAuthoringを変更する。
- Asset rename/move/deleteをFile Transaction外で実行する。

### 4.2 SelectionのSingle Source of Truth

`EditorSelection`を唯一の選択状態とする。

```text
World Outliner ----+
Viewport Picking --+--> EditorSelection --> Details
Content Browser ---+                      --> Gizmo
Diagnostics -------+                      --> Status Bar
```

Domain側のselected indexは移行期間中のみmirrorし、最終的には`EditorObjectHandle`からresolveする。

### 4.3 DocumentとRuntimeの境界

```text
EditorProject
  -> EditorDocumentManager
      -> Authoring Documents
      -> Authoring World

EditorPlaySession
  -> Runtime World Clone
  -> Runtime Watch
  -> Runtime Change Set
```

RuntimeはAuthoring Documentへのwritable pointerを保持しない。

### 4.4 Build依存方向

```text
EditorUI
  -> EditorCore
  -> EditorAdapters
  -> Engine Runtime Interfaces

Engine Runtime
  -X-> EditorUI
  -X-> EditorCore
```

## 5. 推奨実装順序

実装はPhase A～Dの順で行う。Phase Aは見えない安全基盤、Phase Bは統合World Editor、Phase CはAsset/UI品質、Phase Dは高度Authoringである。

### 5.1 現在のActive Milestone

EV-A1 Transaction CoreのDomain非依存化は、既存のAsset MutationとRuntime Authoring Applyまで一度に置き換えると、後続のFile TransactionおよびEditor/Runtime分離より先に責務を固定してしまう。そのためA-1を次の2段階ゲートに分割する。

| ゲート | 今回の対象 | 完了後に進める対象 |
|---|---|---|
| A1-Foundation | 汎用Command API、失敗安全なUndo/Redo、履歴メモリ上限、Course/Detailsの移行 | A-2 File Transaction |
| A1-Final | Asset MutationとRuntime Applyの移行、旧Domain payload/APIの削除 | A-2およびA-3完了後 |

**A1-FoundationからD-3 Gameplay Visual Scripting、独立追加のEditor Font Foundation、Phase D Integration／Commercial Completion Gateまでは2026-07-14までに実装・検証済み**である。Transaction、File Safety、Play Isolation、Generic Document Lifecycle、統合World Authoring、versioned Scene/Prefab/Material/VFX/Animation/Gameplay authoring、永続Asset GUID、安全なContent Browser、責務分割されたInspector/Details/Bottom Dock、context-aware command chrome、Domain非依存Sequencer/Graph Coreとbounded Runtime VMの商用Editor基盤が揃った。Development構成の完了判定は11/11 gate、155/155 check、blocked 0、attention 0、performance warning 0で`ready`となった。次のActive Milestoneは、エンジン全体の商用化ロードマップに戻り、Stage 7 Runtime／Editor Build分離とSource-free Shipping Package検証とする。

#### A1-Foundation実装結果

| PR単位 | 状態 | 実装結果 |
|---|---|---|
| EV-A1-01 | 完了 | Core Error、Execution Service/Context、不変Undo Command契約を追加 |
| EV-A1-02 | 完了 | Command用Undo/Redo、失敗時history不変、例外境界、再入防止を追加 |
| EV-A1-03 | 完了 | 64 MiB既定予算、byte計上、最古履歴eviction、oversize拒否を追加 |
| EV-A1-04 | 完了 | CoursePropertyUndoCommandとCourseEditorExecutionServiceを追加 |
| EV-A1-05 | 完了 | Details/Courseのstaged propertyをCommand登録へ移行し、旧Snapshot履歴と同期 |
| EV-A1-06 | 完了 | Regression 19ケース、Smoke 10 step、Commercial Gate 9/9を通過 |

新Coreは`application/editor/core/`内でCourse、Terrain、Asset、Runtimeの具象型をincludeしない。依存方向はRegressionでソースを検査し、違反を自動検出する。

#### A1-Final実装結果

| ID | 状態 | 実装結果 |
|---|---|---|
| EV-A1-F01 | 完了 | `EditorAssetMutationChange`をAsset Domainへ移し、disk-backed `EditorAssetMutationUndoCommand`とExecution Serviceへ移行 |
| EV-A1-F02 | 完了 | Runtimeの選択Provider差分を`EditorRuntimeApplyUndoCommand`へ移し、Grouped Undo/RedoをExecution Service経由へ移行 |
| EV-A1-F03 | 完了 | `AssetMutation`／`RuntimeAuthoringApply` payload kind、`PushAssetMutation`／`PushRuntimeAuthoringApply`、Domain固有Apply分岐を削除 |
| EV-A1-F04 | 完了 | Transaction StackからCourse、Terrain、VFX、Post-process、Asset Registry、File Transaction具象依存を除去 |
| EV-A1-F05 | 完了 | Command事前予算検査、失敗時history不変、trashのCommand寿命管理、依存方向Regressionを追加 |

検証結果はDebug build成功、Regression 21/21、Smoke 10/10、Commercial Gate 9/9（121 checks、failed 0）である。

### 5.2 着手条件

実装開始前に、次を基準状態として記録する。

1. `Debug|x64`でエンジン本体とEditorテストがビルドできる。
2. 現行のRegression、Smoke、Commercial Gateの結果を保存する。
3. 作業開始時点の未コミット変更を確認し、本実装と無関係な変更を混在させない。
4. 現在のUndo/Redo挙動を、Course単一Property、複数選択Property、Asset Mutation、Runtime Applyごとにテスト名と対応付ける。

基準状態に既知の失敗がある場合は、件数、テスト名、原因を記録し、本変更による新規失敗と区別する。

### 5.3 A1-Foundationで追加するCore契約

以下を新設する。最初のPRでは既存の`EditorTransactionStack`を物理移動しない。既存includeとVisual Studio project設定への影響を抑え、API移行後に配置を整理する。

```text
application/editor/core/
  EditorError.h
  EditorExecutionService.h
  EditorExecutionContext.h
  EditorExecutionContext.cpp
  EditorUndoCommand.h
  EditorTransactionMemoryBudget.h
  EditorTransactionMemoryBudget.cpp
```

Coreの中心契約は、Domain固有型を保持しない不変Commandとする。

```cpp
struct EditorUndoResult final {
    bool succeeded = false;
    std::string message;
};

class IEditorUndoCommand {
public:
    virtual ~IEditorUndoCommand() = default;
    virtual EditorUndoResult Apply(
        EditorTransactionApplyMode mode,
        EditorExecutionContext& context) const = 0;
    virtual std::size_t EstimatedBytes() const noexcept = 0;
    virtual std::string_view DomainId() const noexcept = 0;
    virtual std::string_view TypeId() const noexcept = 0;
};

using EditorUndoCommandPtr = std::shared_ptr<const IEditorUndoCommand>;
```

`shared_ptr<const IEditorUndoCommand>`を採用する理由は、Undo/Redoスタック間でRecordを移動してもCommandの寿命を安定させ、履歴登録後のpayload改変を禁止するためである。CommandにEditor UI、Scene、AppRuntimeStateへの生ポインタや参照をcaptureしてはならない。

`EditorExecutionContext`は、実行時に必要なDomain serviceを解決するための非所有レジストリとする。Coreはserviceの識別と取得だけを知り、Course型、Asset型、Runtime型をincludeしない。未登録service、無効Handle、revision不一致は、例外やsilent failureではなく`EditorUndoResult`の明示的失敗として返す。

### 5.4 EditorTransactionStackの変更

既存スタックに次のAPIと状態を追加する。

```cpp
bool PushCommand(
    std::string label,
    EditorTransactionTarget target,
    EditorUndoCommandPtr command,
    EditorError* error = nullptr);

bool Undo(EditorExecutionContext& context, EditorError* error = nullptr);
bool Redo(EditorExecutionContext& context, EditorError* error = nullptr);

void SetMemoryBudgetBytes(std::size_t bytes);
std::size_t HistoryBytes() const noexcept;
```

Recordは最低限、`label`、`target`、`command`、`estimatedBytes`、`sequenceId`を持つ。次の不変条件を満たすこと。

1. Commandの適用成功後にのみ、RecordをUndo側からRedo側、またはRedo側からUndo側へ移す。
2. 適用失敗時は両スタック、Dirty状態、選択状態を変更しない。
3. 新しいCommandの登録に成功した時点でRedo履歴を破棄する。
4. 履歴は件数上限とbyte上限の両方で制限し、超過時は最古のUndo Recordから破棄する。
5. 単一Commandの`EstimatedBytes()`が総予算を超える場合は登録を拒否し、理由を返す。将来のdisk-backed Commandは小さい復旧Handleだけを計上する。
6. null Command、0 byte予算、不明なApply modeを受理しない。
7. Undo/Redo中の再入登録を禁止し、明示的な`busy`エラーを返す。

既存の`PushPropertyDelta`、`PushMultiPropertyDelta`、`PushAssetMutation`、`PushRuntimeAuthoringApply`、`ApplyCallback`はこの段階では削除せず、`Legacy`移行経路として維持する。ただし、新規機能からは使用禁止とする。

### 5.5 最初に移行するDomain

最初の実利用対象はCourseのProperty編集とDetails panelである。Asset MutationはA-2のFile Transaction完了後、Runtime ApplyはA-3のEditor/Runtime分離完了後に移行する。

```text
application/editor/course/
  ICourseEditorExecutionService.h
  CourseEditorExecutionService.h
  CourseEditorExecutionService.cpp
  CoursePropertyUndoCommand.h
  CoursePropertyUndoCommand.cpp
```

`CoursePropertyUndoCommand`が保持する情報は、安定Handle、Property path、before値、after値、編集元revisionである。`CourseAsset`全体、`TerrainAuthoringState`全体、panelインスタンスは保持しない。

`CourseEditorExecutionService`は既存のProperty adapter/edit serviceへ処理を委譲する。ただしUndo/Redoから値を適用するときは、新しいTransactionを再登録しない専用経路を使う。通常編集と履歴再生で、Validation、Dirty通知、Document更新の結果が一致すること。

複数選択編集は1操作を1 Commandとして登録し、対象ごとのbefore/after値を保持する。途中の対象で適用に失敗した場合は、それ以前に適用した対象をbefore値へ戻し、全体を失敗として扱う。部分適用状態を残してはならない。

### 5.6 PR単位の実装順序

既存コード上の主な接続点は次の通りである。

- Stack本体：`application/editor/EditorTransactionStack.h/.cpp`
- Property登録経路：`application/editor/EditorPropertyEditService.h/.cpp`
- Course値適用：`application/editor/CourseObjectPropertyAdapter.h/.cpp`
- Details操作：`application/editor/EditorDetailsEditController.h/.cpp`
- Course履歴のcomposition/Undo/Redo呼出し：`application/AppRunLoop.h/.cpp`
- テスト：`application/editor/EditorCoreRegressionTests.cpp`、`EditorSmokeRun.cpp`、`EditorAutomationGate.cpp`
- Build登録：`GE3.vcxproj`および`GE3.vcxproj.filters`

| PR | 実装内容 | Exit条件 |
|---|---|---|
| EV-A1-01 | Core契約、Fake service、Fake Command | Domain includeなしで単体テストが通る |
| EV-A1-02 | `PushCommand`、失敗安全なUndo/Redo、再入防止 | 成功・失敗時のstack遷移テストが通る |
| EV-A1-03 | 件数/byte予算、最古履歴eviction、oversize拒否 | 境界値とメモリ計上テストが通る |
| EV-A1-04 | Course execution serviceと単一Property Command | Edit→Undo→Redoで値・Dirty・Validationが一致する |
| EV-A1-05 | Details panelと複数選択編集の移行 | 1 UI操作=1履歴、失敗時に全対象rollbackする |
| EV-A1-06 | Regression/Smoke/Gate統合、依存方向検査 | A1-Foundation完了条件を全て満たす |

各PRはビルド可能かつ既存Editorを起動可能な状態で終える。大規模rename、ディレクトリ移動、UI外観変更を同じPRに混ぜない。

### 5.7 必須テスト

最低限、次の自動テストを追加する。

1. Command登録後のUndo成功とRedo成功。
2. Undo失敗時、Redo失敗時に履歴位置が変わらない。
3. 新規Command登録によってRedoが消える。
4. 件数上限とbyte上限で最古履歴だけが破棄される。
5. 予算を超える単一Commandが明示的に拒否される。
6. 不足service、無効Handle、revision不一致がクラッシュせず失敗になる。
7. Undo/Redo中の再入登録が拒否される。
8. Course単一PropertyのEdit→Undo→Redoで値、Dirty、Validation結果が一致する。
9. 複数選択編集が1履歴になり、途中失敗時に全対象がrollbackされる。
10. Command登録後に編集元の一時変数を破棄してもUndo/Redoできる。
11. Core headerから`CourseAsset`、`TerrainAuthoringState`、`AppRuntimeState`、Asset固有型への依存がない。
12. 既存Asset MutationとRuntime ApplyのLegacy経路が退行していない。

### 5.8 A1-Foundation完了条件

次の条件をすべて満たした時だけA-2へ進む。

- 汎用Command APIがEditorTransactionStackの正規APIとして利用可能である。
- Course単一PropertyとDetails複数選択編集が新APIへ移行済みである。
- Core層からCourse、Asset、Runtimeの具象型が除去されている。
- Undo/Redo失敗で履歴、Document、Dirty状態に部分更新が残らない。
- 履歴の件数上限とbyte上限が機能し、現在値を診断表示できる。
- Regression、Smoke、Commercial Gateに新規失敗がない。
- Visual Studio project/filterへ新規ファイルが登録され、クリーンビルドできる。
- Legacy APIの利用箇所が一覧化され、新規利用を検出できる。

### 5.9 今回実装しないもの

A1-Foundationでは、次を実装しない。

- Assetファイルのatomic save、trash、rename、復旧。これはA-2で行う。
- Editor WorldとRuntime Worldの複製、PIE差分反映。これはA-3で行う。
- World Outliner、Transform Gizmo、Viewport picking。これはPhase Bで行う。
- `EditorContext`の全面分割、Docking/UI theme刷新。
- Asset MutationとRuntime Applyの旧payload削除。これはA1-Finalで行う。

実装開始時の最初の作業はEV-A1-01のCore契約とFake Commandテスト追加であり、A1-Foundationでは既存の具象payloadを削除せず、新経路を横に構築してCourse/Detailsから段階移行した。A1-FinalでAsset/Runtime移行が完了するまでLegacy APIは互換経路として残す。

## 6. Phase A：安全基盤

### A-1. Transaction CoreをDomain非依存化する

詳細な直近仕様は5.1～5.9を正とする。本節はPhase A全体におけるA-1の最終到達点を示す。

#### 変更対象

- `application/editor/EditorTransactionStack.*`
- `application/editor/core/*`
- Course property/DetailsのTransaction適用処理
- A-2完了後のAsset Mutation適用処理
- A-3完了後のRuntime Apply適用処理
- service登録を行うEditor composition root

#### 追加interface

```cpp
class IEditorUndoCommand {
public:
    virtual ~IEditorUndoCommand() = default;
    virtual EditorUndoResult Apply(
        EditorTransactionApplyMode,
        EditorExecutionContext&) const = 0;
    virtual std::size_t EstimatedBytes() const noexcept = 0;
    virtual std::string_view DomainId() const noexcept = 0;
    virtual std::string_view TypeId() const noexcept = 0;
};
```

#### Domain Command

- `CoursePropertyUndoCommand`
- `CourseSnapshotUndoCommand`
- `TransformUndoCommand`
- `AssetMutationUndoCommand`
- `VfxPropertyUndoCommand`
- `RuntimeApplyUndoCommand`

#### 移行順

1. A1-Foundationとして新interfaceを旧Stackの隣に追加する。
2. Course property deltaを移行する。
3. Details multi-editを移行し、A1-Foundation Gateを通す。
4. A-2でFile Transactionと復旧機構を完成させる。
5. Asset mutationをdisk-backed Commandへ移行する。
6. A-3でEditor WorldとRuntime Worldを分離する。
7. Runtime Applyを明示的な差分Commandへ移行する。
8. A1-Finalとして旧concrete payloadとLegacy APIを削除する。

#### 完了条件 A1

- A1-Foundationでは、Core契約がCourse/Terrain/VFX/Asset具体型をincludeしない。
- A1-Finalでは、`EditorTransactionStack`自身もDomain具体型をincludeしない。
- 1回の操作をGrouped Transactionとして記録できる。
- Historyを件数とbyte数で制限できる。
- Apply失敗時にhistory positionを移動しない。
- Asset Mutationは復旧可能なFile Transactionを参照し、巨大なasset bytesを履歴へ直接保持しない。
- Runtime ApplyはEditor Worldのsnapshot全体ではなく、検証可能な差分を保持する。

### A-2. Atomic File Transactionを実装する

**実装状態: 2026-07-13 完了。** Core、Asset Mutation、Course Save、Layout Save、起動時Recoveryまで本番経路へ接続済みである。Scene/VFX/Preset/Project SettingsはA-4の各Document Provider実装時に、同じFile Transaction APIへserializer/validatorを接続する。

#### 追加対象

```text
application/editor/io/EditorAtomicFileWriter.*
application/editor/io/EditorFileTransaction.*
application/editor/io/EditorFileTransactionJournal.*
application/editor/io/EditorFileRecoveryService.*
application/editor/io/EditorProjectPathPolicy.*
application/editor/io/EditorTrashService.*
```

#### 保存手順

1. 同一volumeのtemporary fileへ書き込む。
2. Flush/Closeする。
3. 再読込してschema/内容を検証する。
4. JournalへPreparedを記録する。
5. Destinationをatomic replaceする。
6. Registry/Cacheなど、同一transactionに参加する外部状態を更新する。
7. JournalをCommittedにする。
8. Temporary/Backupをcleanupする。

Registry更新に失敗した場合はJournalをPreparedのままFile操作を逆順rollbackする。強制終了でPreparedが残った場合も、次回起動時に`EditorFileRecoveryService`が同じ規則で復旧する。

#### 適用対象

- Layout
- Preferences
- Asset `.meta`
- Course
- Scene
- VFX/Effect
- Preset
- Project Settings

#### Asset Delete

Deleteはbinaryをmemoryへ保持せず、`.editor/trash/<transaction-id>/`へ移動する。Undoはtrashから復元し、history eviction後にcleanupする。

#### 実装済み項目

| ID | 状態 | 内容 |
| --- | --- | --- |
| EV-A2-01 | 完了 | Project Root境界、`..`、外部absolute path、symlink解決後の逸脱を拒否するPath Policy |
| EV-A2-02 | 完了 | 同一directory staging、Flush/Close、再読込照合、任意schema validator、atomic replace、backup rollback |
| EV-A2-03 | 完了 | Prepared/Committed/RolledBack Journalと起動時Recovery |
| EV-A2-04 | 完了 | Asset source/meta/dependency/registryを二相commitするimport/reimport/rename/move |
| EV-A2-05 | 完了 | `.editor/trash/<transaction-id>/`を使うdisk-backed delete/undo/redo/history eviction cleanup |
| EV-A2-06 | 完了 | Course SaveとEditor Layout SaveをAtomic File Transactionへ移行 |
| EV-A2-07 | 完了 | 全failure point、crash recovery、path traversal、2 MiB Asset memory上限のRegression追加 |

検証結果はDebug build 0 warning/0 error、Regression 19/19、Smoke 10/10、Commercial Gate 9/9（119 checks）である。

#### 完了条件 A2

- 各commit段階のfailure injectionで部分状態を残さない。
- 強制終了後にJournalから復旧できる。
- Source/Meta/Registry/Dependencyが常に整合する。
- Large Asset Undoがmemory budgetを超えない。

### A-3. Play/Simulate IsolationをProvider化する

**実装状態: 2026-07-13 完了。** 型消去Snapshotと決定的順序のProvider Registryを追加し、現在のProduction Property Adapterが所有するCourse、Terrain/Gameplay Tuning、VFX Authoring Asset、Post-processを本番Play/Sim経路へ登録した。Scene/Entity/Componentなど今後導入されるAuthoring modelは、model導入時に同じProvider契約へ登録する。

#### 追加対象

```text
application/editor/play/IEditorPlayIsolationProvider.h
application/editor/play/EditorPlayIsolationRegistry.*
application/editor/play/EditorPlaySnapshot.*
application/editor/play/EditorRuntimeChangeSet.*
application/editor/play/EditorPlayMutationGuard.*
```

#### Provider

- Course
- Terrain
- Scene/Entity/Component
- VFX
- Post-process
- Camera
- Render Preset
- Gameplay Tuning
- Mutable Project Settings

#### Keep Changes

Runtime差分を一覧表示し、変更単位でApply/Ignoreを選択する。採用分は1つのGrouped Transactionとして記録する。

#### A3実装結果

| PR単位 | 状態 | 実装結果 |
|---|---|---|
| EV-A3-01 | 完了 | `IEditorPlayIsolationProvider`、型消去`EditorPlaySnapshot`、順序付きRegistryを追加 |
| EV-A3-02 | 完了 | Captureを全Provider成功時だけpublishし、Restore途中失敗時はrollback snapshotから全Domainを復旧 |
| EV-A3-03 | 完了 | Course、Terrain/Gameplay Tuning、VFX Authoring Asset、Post-process Providerと決定的fingerprintを追加 |
| EV-A3-04 | 完了 | `EditorPlayMutationGuard`でPlay/Sim中の直接Authoring変更を拒否し、Runtime/Keep Changes/Restoreだけを許可 |
| EV-A3-05 | 完了 | Runtime Changes panelでProvider差分、fingerprint、Apply/Ignore選択を表示 |
| EV-A3-06 | 完了 | 選択Providerだけをrestore baselineへ採用し、1件のGrouped TransactionとしてUndo/Redo |
| EV-A3-07 | 完了 | Provider順序、重複、Capture失敗、Restore rollback、選択Apply/Ignore、VFX/Post-processをRegressionへ追加 |

検証結果はDebug build警告0/エラー0、Regression 21/21、Smoke 10/10、Commercial Gate 9/9（119 checks、failed 0）である。

#### 完了条件 A3

- Play前とStop後の全Authoring fingerprintが一致する。
- 未採用Runtime変更が残らない。
- Applyした変更だけが残り、Undoできる。
- Capture/Restore途中失敗でAuthoringを破壊しない。

### A-4. Generic Document Managerを追加する

#### 追加対象

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

#### 共通操作

- Open
- Save
- Save As
- Save All
- Reload
- Close/Reopen
- Duplicate
- Validate
- Autosave
- Recover
- Migrate

#### 初期Provider

1. Course
2. Scene
3. VFX/Effect
4. Material/Render Preset
5. Project Settings

#### 完了条件 A4

- 複数Document typeを同時に開ける。
- Save AllがAtomic Writerを使用する。
- Crash後にAutosaveから復元できる。
- 外部変更を無断で上書きしない。

**実装状態: 2026-07-13 完了。**

| ID | 状態 | 実装結果 |
|---|---|---|
| EV-A4-01 | 完了 | Stable `EditorDocumentId`、Provider Registry、複数Document Manager、Active Documentを追加 |
| EV-A4-02 | 完了 | Course、Scene、Effect/VFX、Material/Render Preset、Project Settingsの初期Providerを登録 |
| EV-A4-03 | 完了 | Open、Save、Save As、Atomic Save All、Reload、Close/Reopen、Duplicate、Validateを共通化 |
| EV-A4-04 | 完了 | `.editor/autosave/<guid>/<revision>/`への非破壊Autosaveとhash付きmanifestを追加 |
| EV-A4-05 | 完了 | Crash Recovery scan、内容hash検証、live model復元、復元後dirty保持を追加 |
| EV-A4-06 | 完了 | 外部変更hash監視、Save前競合ブロック、Compare data、UI競合表示を追加 |
| EV-A4-07 | 完了 | Schema migration前の原本backup、migration report、移行後validationを追加 |
| EV-A4-08 | 完了 | 複数Dirty Documentを単一確認でAtomic Save All後に閉じるflowを追加 |
| EV-A4-09 | 完了 | Project Path PolicyをOpen、Duplicate、Recovery、Save Asへ強制しpath traversalを拒否 |

検証結果はDebug build成功、Regression 22/22、Smoke 10/10、Commercial Gate 9/9（121 checks、failed 0）である。

## 7. Phase B：統合World Editor

### B-1. Editor World Modelを定義する

**実装状態: 2026-07-13 完了。** `application/editor/world/`にDomain非依存のRecord、Provider Registry、World Modelを追加した。Object identityは`EditorDocumentId + ProviderId + Persistent Object GUID`を正規キーとし、配列indexは既存UI互換の一時locatorとしてのみ保持する。

World Outlinerを先にUIだけで作らず、Authoring Objectの共通modelを定義する。

```cpp
struct EditorWorldObjectRecord {
    EditorObjectHandle handle;
    EditorObjectHandle parent;
    std::string displayName;
    std::string typeName;
    bool visible = true;
    bool locked = false;
    bool runtimeOnly = false;
};
```

`IEditorWorldObjectProvider`がDomain objectを列挙、resolveする。Rename、Reparent、Duplicate、Deleteなどの可否はRecordのcapabilityで公開し、実変更はB-2でTransaction/Validation/Dirty State経路へ接続する。ProviderからDomainを直接変更してはならない。

初期Provider：

- Course Terrain Placement
- Course Rock Cluster
- Course Camera Key
- Course Event
- Scene Entity
- VFX Object

現行の構造化Authoring Modelに対して、Course Terrain Placement、Course Rock Cluster、Course Camera Key、Course Eventの4種と、VFX Effect Asset/Runtime Instance Providerを登録した。VFX Runtime Instanceは`runtimeOnly + locked + capabilityなし`として公開する。Scene Entityは構造化Scene/Entity Model導入時に同じProvider契約へ登録し、text documentを疑似Entityへ変換しない。

#### B1実装結果

| ID | 状態 | 実装結果 |
|---|---|---|
| EV-B1-01 | 完了 | Domain非依存`EditorWorldObjectRecord`、Capability、永続Object IDを追加 |
| EV-B1-02 | 完了 | 決定的優先順の`EditorWorldObjectRegistry`と集約`EditorWorldModel`を追加 |
| EV-B1-03 | 完了 | Stable resolve、Document filter、children query、revision/fingerprintを追加 |
| EV-B1-04 | 完了 | Duplicate handle、missing parent、hierarchy cycleの診断を追加 |
| EV-B1-05 | 完了 | Course 4種へ永続`editorGuid`を追加し、Course schema v1→v2 migrationを追加 |
| EV-B1-06 | 完了 | Course ProviderとVFX read-only/runtime-only Providerを本番`AppImGuiLayer`へ登録 |
| EV-B1-07 | 完了 | reorder/rename/save-reload/migration/duplicate/10,000 object性能Regressionを追加 |

#### 完了条件 B1

- Course Objectの配列順や表示名が変わっても、保存済みGUIDから同一Objectをresolveできる。
- 旧Courseはschema migrationでGUIDを付与し、backup/reportを伴うA-4 lifecycleを通る。
- 異なるProviderのObjectを単一World Modelで決定的に列挙できる。
- Runtime-only ObjectをAuthoring mutation対象と誤認しない。
- 10,000 Course ObjectのrefreshがDebug buildで2秒未満で完了する。

検証結果はDebug build 0 warning/0 error、Regression 23/23、Smoke 10/10、Commercial Gate 9/9（121 checks、failed 0）である。

### B-2. World Outlinerを実装する

**実装状態: 2026-07-13 完了。** `EditorWorldModel`を直接の表示sourceとし、`EditorSelection`を唯一の選択状態として、左Sidebarへ`editor.worldOutliner` Panelを登録した。Outliner固有のDomain分岐は持たず、Providerが公開するcapabilityに基づいて表示とmutation可否を決定する。

#### 機能

- Hierarchy
- Search/Type filter
- Visibility
- Lock
- Rename
- Duplicate/Delete
- Drag reparent
- Multi-selection
- Runtime-only区別
- Missing object表示

実装結果：

| ID | 状態 | 実装結果 |
|---|---|---|
| EV-B2-01 | 完了 | cached children indexを使うHierarchy、検索、型filter、Runtime/Missing filterを実装 |
| EV-B2-02 | 完了 | `EditorSelection`へCtrl multi-select、Outliner click、Viewport pick、Diagnostics clickの双方向同期を実装 |
| EV-B2-03 | 完了 | Visibility/Lock、inline Rename、Duplicate、確認付きDelete、drag reparent UIを実装 |
| EV-B2-04 | 完了 | Domain非依存`IEditorWorldMutationProvider`、mutation plan、execution service、不変Undo Commandを実装 |
| EV-B2-05 | 完了 | Authoring lock、runtime-only/missing/locked/capability、provider/document境界、transaction memory予算を変更前に検証 |
| EV-B2-06 | 完了 | Course schema v3でpersistent GUIDに加えてvisibility/lockを保存し、schema 1/2からmigration |
| EV-B2-07 | 完了 | Rename名称衝突拒否、Duplicate新規GUID、Delete/Reparent Undo/Redo、変更失敗rollbackをRegressionで保護 |

Courseの4カテゴリは型の意味が固定されるため`Reparent` capabilityを公開しない。drag reparent、Provider request、snapshot apply、Undo/Redoの共通経路は実装済みであり、可変Hierarchy ProviderのRegressionで検証する。B-5のScene Entity Providerは同じ経路へ接続する。

#### Selection同期

- Outliner click → `EditorSelection`
- Viewport pick → Outliner scroll/select
- Diagnostics click → Outliner/Details select
- Detailsは`EditorSelection`だけを参照

#### 完了条件 B2

- Outliner/Viewport/Detailsの選択が一致する。
- Rename/Reparent/Duplicate/DeleteをUndoできる。
- Play中Runtime objectをread-only表示できる。

検証結果はDebug v143 build成功（0 error）、Regression 24/24、Smoke 10/10、Commercial Gate 9/9（121 checks、failed 0、warnings 0）である。

### B-3. Production Transform Gizmoを実装する

**実装状態: 2026-07-13 完了。** 既存のCourse viewport操作を`EditorViewportCoordinateService`、`EditorPropertyEditSession`、汎用Gizmo Mathへ接続し、表示だけの`EditorTransformGizmoService`をselection、space、pivot、plane/uniform handle、multi-selection状態を公開するproduction contractへ拡張した。

#### 機能

- Translate/Rotate/Scale
- X/Y/Z/Plane handle
- World/Local
- Translation/Rotation/Scale snap
- Pivot mode
- Multi-selection
- Numeric transform
- Begin/Preview/Commit/Cancel
- Escape cancel
- Play mutation lock

実装結果：

| ID | 状態 | 実装結果 |
|---|---|---|
| EV-B3-01 | 完了 | Translate/Rotate/ScaleとX/Y/Z、XY/YZ/ZX、uniform scale handleを実装 |
| EV-B3-02 | 完了 | display→render→world ray契約を使うray-axis／ray-plane constraintとsigned rotationを実装 |
| EV-B3-03 | 完了 | World/Local basis変換、Active/Median/Individual pivot、W/E/R shortcutを実装 |
| EV-B3-04 | 完了 | translation/scale/rotation snapと既存Details数値propertyを同一Property Adapterへ接続 |
| EV-B3-05 | 完了 | 同一Domain multi-selectionを1件の`EditorPropertyEditSession`としてBegin/Preview/Commit |
| EV-B3-06 | 完了 | Escape、Viewport離脱、Play/Sim lockで全対象をbegin stateへCancel復元 |
| EV-B3-07 | 完了 | 1 dragを1 grouped Undo Commandとして登録し、全対象のUndoをRegressionで保護 |
| EV-B3-08 | 完了 | gizmo space/pivot/multi-selectionをPlay Isolation fingerprintとCourse history snapshotへ追加 |

#### 操作flow

```text
Mouse Down
  -> Hit Test
  -> Before Transform保存
  -> Begin Transaction

Mouse Drag
  -> Coordinate Serviceでray生成
  -> Constraint plane/axisへproject
  -> Preview Transform適用

Mouse Up
  -> After Transform保存
  -> Commit Transaction

Escape
  -> Before Transform復元
  -> Cancel Transaction
```

#### 完了条件 B3

- 1 dragが1件のUndoになる。
- Escapeで完全に元へ戻る。
- Window resize/DPI変更後もhit testがずれない。
- GizmoとDetailsのTransformが同期する。

検証結果はDebug v143 build成功（0 error）、Regression 25/25、Smoke 10/10、Commercial Gate 9/9（121 checks、failed 0、warnings 0）である。

### B-4. Viewport OverlayをLayer化する

**実装状態: 2026-07-13 完了。** Domainから生の`ImDrawList`を受け取る経路を撤去し、安定Layer ID、Provider/Command Sink、Viewport coordinate/clip限定Rendererから成る`EditorViewportOverlayService`へ統合した。Rail Actor、Course/Navigation、Camera Safe Frame、Object Label、Viewport診断をLayer Commandへ移行し、Gameplay HUD RenderGraph passも同じGameplay Layer visibilityへ接続した。

#### 実装結果

| ID | 状態 | 実装結果 |
|---|---|---|
| EV-B4-01 | 完了 | 8 Layerの安定ID、Layer Settings、Provider Registry、Command Sinkを追加 |
| EV-B4-02 | 完了 | Line/Rect/Circle/Label/Iconをrender座標で収集し、Viewport clip内だけでImGuiへ変換するRendererを追加 |
| EV-B4-03 | 完了 | Rail Visibility、Object Label、Threat Helper、Safe Frame、Viewport診断をDomain固有raw描画から移行 |
| EV-B4-04 | 完了 | Layer visibility、Gameplay/Editor独立制御、選択限定、距離fade、Zoom LOD、Label overlap回避、Icon fallbackを追加 |
| EV-B4-05 | 完了 | Overlay popup、Layer設定のatomic layout persistence、Screenshot suppression scopeを追加 |
| EV-B4-06 | 完了 | 10,000 Label bounded layout、Regression、Smoke、Commercial Performance Gateを追加 |

Layer：

- Gameplay HUD
- Authoring Helpers
- Selection Outline
- Object Labels
- Course/Navigation
- VFX Debug
- Performance
- Camera Safe Frame

機能：

- Layer別visibility
- 選択Objectのみ表示
- 距離fade
- Label overlap回避
- Icon化
- Zoom依存detail
- Screenshot時一括非表示

#### 完了条件 B4

- 通常AuthoringでSceneがDebug文字に隠れない。
- Gameplay HUDとEditor Overlayを個別に制御できる。
- OverlayがViewport clip/coordinate contractだけを使用する。

検証結果はDebug v143 build成功（0 warning、0 error）、Regression 27/27、Smoke 10/10、Commercial Gate 9/9（124 checks、failed 0、warnings 0）である。10,000 Label候補のbounded layoutは最新Debug計測で21.86 ms、予算50 ms以内、表示結果128件以下を達成した。

### B-5. Scene/Entity/Component基盤へ接続する

最低限必要な概念：

- Stable Entity GUID
- Component Type ID
- Transform hierarchy
- Component add/remove
- Object reference
- Serialization version
- Scene Document

#### 完了条件 B5

- Scene EntityをOutlinerで作成/削除できる。
- DetailsからComponentを追加/削除できる。
- ViewportへAssetをdropしてEntityを生成できる。
- Scene Save/ReloadでGUIDとhierarchyを維持する。

#### B-5実装結果

- `EditorScene`をScene Documentのauthoring正本とし、Stable Entity GUID、親GUIDによるTransform hierarchy、必須Transform、安定Component Type ID、Entity/Asset object reference、schema versionを実装した。
- `EditorSceneDocumentProvider`をGeneric Document lifecycleへ登録し、新規Scene作成、validation、deterministic serialization、Atomic Save、Reloadを同じ経路へ接続した。
- `SceneWorldObjectProvider`をWorld Modelへ登録し、Create/Rename/Reparent/Duplicate/Delete/Visibility/Lock/Add Component/Remove Componentを不変snapshotのUndo/Redo Commandとして実装した。
- World Outlinerの`+ Entity`、DetailsのComponent追加/削除、Content BrowserからViewportへのAsset Dropを同じWorld Mutation Serviceへ接続した。Mesh、Effect、Audio dropは型付きComponentとAsset GUID参照を生成する。
- Regressionの`scene entity component foundation`でEntity作成、Asset Drop相当生成、hierarchy、Component Undo/Redo、File Transaction Save、Reload後のGUID・親子・Component Type・Asset reference保持を検証する。

## 8. Phase C：AssetとUIの商用品質化

### C-1. Durable Asset Identityへ移行する

#### 実装

- 新規Assetへ必ず`.meta`を生成する。
- 既存AssetへBatch Meta migrationを提供する。
- GUID重複/欠損を検出する。
- Path-only referenceを診断する。
- Rename/Move前にdurable GUIDを要求する。
- Redirect/Repair情報を管理する。

#### 完了条件 C1

- 対応Assetのmetadata coverageが100%になる。
- Rename/Move後もGUID referenceが維持される。
- Missing referenceをDiagnosticsから修復できる。

#### C-1実装結果

- 新規ImportとBatch Meta Migrationで、path hash由来の暫定IDとは独立した32桁のdurable GUIDを発行し、`.meta`を`EditorFileTransaction`でatomicに保存する。既存の暫定GUIDはmigration時にdurable GUIDへ更新する。
- `--editor-asset-meta-migrate`をheadless project migration/CI entry pointとして追加し、既存Assetの一括移行後にcoverage、GUID重複、同一Kind/IDへ正規化されるsource path衝突を検査する。`Resources/**/*.meta`はversion control対象とし、GUIDを開発環境間で共有する。
- `EditorAssetRegistry`へmetadata coverage、GUID重複監査、GUID/path/redirectを統合したreference resolver、永続`.assetredirects`を追加した。Rename/Move時はdurable metadataとGUID一意性を必須とし、成功したmutationの旧ID・旧logical path・旧source pathをredirectとして保存する。
- Dependency scanは`asset-guid:<guid>`を正規参照として保持し、path-only referenceを分離してDiagnosticsへ通知する。Missing/ambiguous GUID、duplicate GUID、stale redirectも同じAsset Diagnostics経路で検出する。
- Content Browserの`Repair Path Refs`とCommand Paletteの`asset.repairPathOnlyReferences`を追加した。解決可能なpath-only referenceをGUID参照へ置換し、`.meta`とRegistryを同一File Transactionで更新してUndo/Redoできる。
- DetailsのAsset pickerとmissing-reference repairは、現行IDだけでなくGUID、logical/source path、redirectを解決し、旧pathを現在のAssetへ誘導する。
- Regressionの`durable asset identity`でmetadata coverage 100%、重複GUID拒否、path reference repairのUndo/Redo、Rename後のGUID維持、redirect永続化・再読込、legacy migrationを検証する。

検証結果はDebug v143 build成功、Regression 28/28、Smoke 10/10、Commercial Gate 9/9（124 checks、failed 0、warnings 0）である。

### C-2. Content Browserを完成させる

追加機能：

- Folder Tree
- Grid/List切替
- Search/Filter/Tag
- Collection/Favorite
- Context Menu
- Duplicate/Rename/Move/Delete
- Import/Reimport
- Reference Viewer
- Dependency Viewer
- Source Control状態
- Dirty/Cook状態
- Drag & Drop配置

#### 完了条件 C2

- Folder/Filter/Selectionをsession間で復元する。
- AssetをViewport/Detailsへdragできる。
- Asset操作がAtomic File Transactionを使用する。

#### C-2実装結果

- `EditorContentBrowserState`をUI非依存の永続状態モデルとして追加した。Folder、Search、Kind、Tag、Grid/List、選択Asset GUID、Favorite、Collectionをversioned schemaで保持し、750 ms debounce後に`EditorFileTransaction`でatomic保存する。
- Registryのsource pathからFolder Treeを構築し、Folder配下、case-insensitive search、Kind、Tag、Favorite、Collectionの積集合で表示Assetを決定する。Folder/Filter/View/Selectionは次sessionで復元される。
- Grid/Listの両表示から同一durable GUID payloadをdragできる。ViewportはScene Entity生成へ、Detailsは互換`AssetRef` propertyへのdropへ接続し、ImportされたMeshを含む全対応Asset種別をreferenceableにした。
- Favorite/Collection、Duplicate、Reimport、Delete、Rename/Move誘導を持つContext Menuを追加した。Registryを変更する操作は次frameの安全なqueueで処理し、描画中のrecord pointer invalidationを防ぐ。
- `Duplicate`をAsset Mutation Coreへ追加した。source copyと新しいdurable GUID `.meta`を同一File Transactionで作成し、Transaction StackのUndo/Redoでもsource、metadata、Registryをatomicに同期する。Rename/Move/Delete/Repair/Import/Reimportは既存のFile Transaction経路を継続使用する。
- 既存Reference InspectorをDependency/Referenced By/Missing viewerとして維持し、ListへSource Control、Dirty、Cook列を追加した。`IEditorAssetWorkspaceStatusProvider`とGUID keyed registryにより、Source Control/Cook backendが推測値ではなく正式状態をpublishできる。
- Regressionの`production content browser`でFolder/Search/Tag/Kind/Collection filter、Favorite/CollectionとSelectionのsession復元、SCM/Dirty/Cook provider、DuplicateのGUID分離とAtomic Undo/Redoを検証する。

検証結果はDebug v143 build成功、Regression 29/29、Smoke 10/10、Commercial Gate 9/9（124 checks、failed 0、warnings 0）である。

### C-3. Right Inspectorを再編する

既定tabを`Details`にし、VFX Inspectorを次へ分割する。

- VFX Details
- VFX Runtime Inspector
- Scene Lighting
- Post Process
- Render Debug Views
- Performance

Details追加機能：

- Property検索
- Category state保存
- Array/Map/Struct
- Asset/Object picker
- Favorite
- Edit Condition
- Changed value highlight
- Per-property validation
- Prefab override

#### C-3実装結果

- 旧`vfx.inspector`を`vfx.details`、`vfx.runtimeInspector`、`scene.lighting`、`postprocess.inspector`、`render.debugViews`、`editor.performance`へ分割し、Effect authoring、runtime control、Lighting、Post Process、debug view、timingの責務を独立Panelへ移した。
- Right Inspectorの既定active panelを`editor.details`に固定した。既存layoutが旧`vfx.inspector`を指す場合はDetailsへ一度だけmigrationし、Atomic Layout Persistenceへ保存する。
- `EditorDetailsViewState`を追加し、Property Search、Favorite、Changed-only、Category表示状態をversioned schemaで保持する。変更は750 ms debounce後に`EditorFileTransaction`でatomic保存され、session間で復元される。
- Details tableへFavorite操作、default/transaction差分によるChanged row highlight、property path単位のValidation severity/message、Edit Conditionによる編集可否と理由表示を追加した。
- `EditorPropertyKind`へArray、Map、Structを追加し、descriptorのelement/key/value type metadataとtransaction-backed serialized container editorを実装した。AssetRefはdurable Asset picker/Drag Drop、ObjectRefはWorld Model pickerを使用する。
- Prefab機能へ直接依存しない`IEditorPrefabOverrideProvider`を追加した。Detailsはobject/property単位でInherited/Overridden/Added/Removed状態とsource Prefabを表示し、Providerが許可するRevertを実行する。Prefab backend未接続時は推測せず`N/A`とする。
- Regressionの`right inspector evolution`でDetails状態復元、Array/Map/Struct value round-trip、Edit Condition/Prefab descriptor metadata、Prefab Override query/revert、6分割Panel ID、Details active tab永続化を検証する。

検証結果はDebug v143 build成功、Regression 30/30、Smoke 10/10である。性能予算を含む最終Commercial Gateは最適化済みDevelopment v143で9/9（124 checks、failed 0、warnings 0）を通過した。Performance BudgetはAsset Index 29.26 ms、Details traversal 1.96 ms、10,000 Overlay Label 2.78 msで、各予算内である。

### C-4. Bottom Dockを再編する

```text
Output
  -> Diagnostics / Notifications / Log

Profiling
  -> Performance / Runtime Status / Runtime Queues / RenderGraph

Authoring
  -> Course Timeline / Sequencer / Transactions

Developer
  -> Feature Guard / Properties / Commands / Rail Lock-On
```

追加機能：

- Tab overflow menu
- Tab検索
- Pin/Close/Reopen
- Dragによるarea移動
- Error/Warning badge
- Developer Panel一括非表示

#### 完了条件 C4

- すべてのBottom Dock panelがOutput、Profiling、Authoring、Developerのいずれかへ分類される。
- Tab検索、overflow、Pin、Close、Reopenが狭いwindowでも操作できる。
- Tabの分類area移動とUI状態がatomic layout persistenceで再起動後に復元される。
- Diagnostics/NotificationsのError/Warning badgeが現在のservice状態と同期する。
- Developer分類を一括非表示にしても一般Authoring panelを使用できる。
- missing/closed panelを含む保存layoutから安全に復旧できる。

#### C-4実装結果

- `EditorPanelDescriptor`へBottom Dock分類、close/pin capability、badge providerを追加し、plugin/tool登録境界を維持したまま拡張可能にした。
- `EditorPanelHost`をBottom Dock専用production tab hostへ拡張した。4分類、compact area selector、Tab検索、overflow、Pin/Close/Reopen、context move、tab dragによるarea移動を提供する。
- DiagnosticsはValidation Report、NotificationsはNotification CenterからError/Warning badgeを構築し、表示文字列へ反映する。
- Developer分類は永続化された一括表示toggleで隔離し、非表示時はOutputへ安全にfallbackする。
- `EditorLayoutPersistenceService`をschema v2へ更新し、active group、search、pin、close、group override、Developer表示を`EditorFileTransaction`でatomic保存する。v1 layoutは引き続き読込可能である。
- PerformanceをRight InspectorからProfilingへ移し、旧active stateはDetailsへmigrationする。
- 検証結果はDebug v143 build成功、実フレーム8秒安定、Regression 31/31、Smoke 10/10である。最適化済みDevelopment v143 Commercial Gateは9/9（124 checks、failed 0、warnings 0、blocked 0）を通過した。Feature Guardは40 ok、既存Property accessor fixture由来のattention 1、blocked 0である。Performance BudgetはAsset Index 28.41 ms、Details traversal 1.93 ms、10,000 Overlay Label 2.49 msで各予算内である。

### C-5. Menu/Toolbar/Status Barを整理する

Menu：

```text
File / Edit / Window / Tools / Build / Play / Help
```

常設Toolbar：

- Save
- Undo/Redo
- Translate/Rotate/Scale
- World/Local
- Snap
- Play/Sim/Stop
- Pause/Step
- Command Palette

Course固有操作はactive documentに応じてcontextual表示する。

Status Bar追加情報：

- Background Task
- Autosave
- Asset Import
- Shader Compile
- Source Control
- Cook/Package
- Memory/GPU
- Error/Warning count

#### 完了条件 C5

- 一般Authoring WorkspaceにDeveloper専用操作が露出しすぎない。
- 狭いwindowでもToolbar/Tabを操作できる。
- Active Documentに応じてcontextual actionが切り替わる。

#### C-5実装結果

- MenuをCommandの旧category直結から、安定した`File / Edit / Window / Tools / Build / Play / Help`へ再編した。Command IDから責務menuへ決定的にroutingし、Course commandにはactive document typeのcontext条件を付与した。
- ToolbarをSave All、Undo/Redo、Translate/Rotate/Scale、World/Local、Snap、Play/Sim/Stop/Pause/Resume/Step、Command Paletteの主要workflow順へ再編した。Course固有Freeze/Apply/ReloadはCourse documentのactive時だけ表示する。
- Toolbar buttonを固定幅からlabel計測幅へ変更し、表示領域を超えるcommandをoverflow popupへ移す。Transform modeとSnapは現在状態をactive styleで示し、World/LocalとSnap labelはGizmo stateへ同期する。
- `EditorBuiltinCommandProvider`へSave All、Transform、Window area focus、Details focus、Help commandを追加し、Menu/ToolbarからもCommand Registryのenable/disabled reasonと実行statusを共通利用する。
- Status Barを`EditorStatusBarSnapshot`によるUI非依存のproject health modelへ分離した。Validation、Dirty Document、Autosave pending、Background preview/GPU task、active document、Play session、Command result、選択AssetのSCM/Cook statusを表示し、未接続のShader Compile/Memory providerは推測せず`Unbound`とする。
- 狭いwindowではStatus項目を優先順に省略し、`Status...` popupでAsset Import、Shader Compile、Source Control、Cook、GPU、Memory、Notificationを含む全状態を確認できる。
- C-5専用Regressionで固定7 menu順、command routing、Course context metadata、Toolbar主要command、Status snapshotのtruthful provider状態を検証する。
- 検証結果はDebug v143 build成功、実フレーム8秒安定、Regression 32/32、Smoke 10/10である。最適化済みDevelopment v143 Commercial Gateは9/9（124 checks、failed 0、warnings 0、blocked 0）を通過した。Feature Guardは40 ok、既存Property accessor fixture由来のattention 1、blocked 0である。Performance BudgetはAsset Index 27.09 ms、Details traversal 1.97 ms、10,000 Overlay Label 2.56 msで各予算内である。

## 9. Phase D：高度Authoringへの移行

### D-1. Course TimelineをTrack Provider化する

現在のCourse Timelineを汎用Sequencerへ直接書き換えず、まずTrack Adapterへ分解する。

```text
IEditorSequencerTrackProvider
  -> Course Event Track
  -> Course Placement Track
  -> Camera Track
  -> Lighting Track
  -> Material Track
  -> VFX Track
  -> Gameplay Trigger Track
```

#### 完了条件 D1

- Sequencer CoreがCourse具体型をincludeしない。
- Key操作が共通Transactionを使う。
- ScrubとRuntime Previewが同期する。
- Keyのmulti-select、copy/paste、snapが動作する。

#### D-1実装結果

- `application/editor/sequencer/EditorSequencer.*`にCourse型を参照しない`IEditorSequencerTrackProvider`と`EditorSequencerService`を追加した。Track/Keyの読取、選択、Scrub、interactive preview、確定、取消、clipboard、snapを共通契約に集約する。
- SequencerのKey編集は`sequencer` Domainの汎用`IEditorUndoCommand`として`EditorTransactionStack`へ登録する。複数Providerを跨ぐ適用失敗時は適用済みProviderを逆順rollbackし、Transaction登録失敗時もpreview前の状態へ復元する。
- `CourseSequencerTrackProvider`はCourse Event、Placement、Camera、Lighting、Material、VFX、Gameplay Triggerの7 Trackへ既存Courseデータを投影する。移動・削除・複製・挿入をpersistent editor GUIDで解決し、構造変更を含むUndo/Redoを提供する。
- Course Timeline UIはProvider列挙結果を描画し、Ctrl/Shift複数選択、選択群drag、Ctrl+C/Ctrl+V、snap間隔、playhead scrubを共通Serviceへ委譲する。ScrubはCourse runtime teleport callbackへ接続し、authoring timelineとruntime previewを同期する。
- RegressionはCoreのCourse include禁止、7 Track分類、複数選択、snap付きgroup move、1操作1Transaction、Undo/Redo、copy/paste、structural Undo/Redo、Scrub同期を検証する。
- 検証結果はDebug/Development v143 build成功、実フレーム8秒安定、Regression 33/33、Smoke 10/10である。Development Commercial Gateは9/9（124 checks、failed 0、warnings 0、blocked 0）を通過した。

### D-2. Prefabを追加する

- Prefab Asset/Instance
- Nested Prefab policy
- Property/Structural Override
- Apply/Revert
- Missing Prefab recovery
- Schema migration

#### D-2実装結果

- `.prefab`をDurable Asset KindとDocument Typeへ追加し、`EditorPrefabAsset`、`EditorPrefabDocumentProvider`、64 MiB validation、atomic generic document save、schema v1からv2へのmigrationを実装した。
- Scene schemaを後方互換のv2へ更新し、Prefab Instance GUID、source Asset GUID、root Entity、source-to-instance Entity binding、Property/Structural Overrideを永続化する。既存Scene v1は空のPrefab instance tableを持つv2へ移行する。
- `EditorPrefabService`はAssetからのInstance生成、persistent binding、Property Override、Entity/ComponentのAdded/Removed Override、個別Revert、Instance全体Revert、AssetへのApply、Missing Asset placeholderと復旧を提供する。
- Prefab操作は`prefab` Domainの汎用Commandとして共通Transaction Stackを使用する。ApplyはSceneとPrefab Assetを単一snapshot commandで更新し、Undo/RedoおよびTransaction登録失敗時rollbackを保証する。
- Nested Prefabは明示的なmount Entity参照で構成し、循環参照を拒否し、最大深度を8に制限する。失敗時は途中生成したEntity/Instanceを公開しない。
- Content BrowserはPrefabのfilter、metadata、preview、drag payloadを扱う。ViewportへのPrefab dropはAsset GUIDを解決してInstance化し、World OutlinerはPrefab Instance、Prefab Entity、Missing Prefabを区別して表示する。
- Detailsにはsource Asset、接続状態、Override数、Apply Overrides、Revert Instance、Recover Missing Prefabを表示し、property rowの`IEditorPrefabOverrideProvider`はInherited/Overriddenと個別Revertを実体Serviceから返す。
- RegressionはAsset/Scene round-trip、両schema migration、instantiate、binding、Property/Structural Override、Apply/Revert、Undo/Redo、Missing recovery、nested cycle rollback、Content Browser分類を検証する。
- 検証結果はDebug/Development v143 build成功、実フレーム8秒安定、Regression 34/34、Smoke 10/10である。Development Commercial Gateは9/9（124 checks、failed 0、warnings 0、blocked 0）を通過し、全performance budget内である。

### D-3. 高度Toolを追加する

実装順：

1. Generic Sequencer
2. Prefab
3. Material Graph
4. Advanced VFX Graph
5. Animation State Machine
6. Gameplay Visual Scripting

高度Toolは独自のUndo、Save、Dirty、Asset ID systemを作らず、Phase A～Cの共通基盤を使用する。

#### D-3 Material Graph実装結果

- `application/editor/graph/EditorGraph.*`へMaterial型を参照しないGraph Coreを追加した。型付きPin、方向、単一入力cardinality、明示conversion、stable Node/Link ID、4096 Node/16384 Link上限、線形時間の循環検出を共通契約とする。
- `.material`/`.materialgraph`をDurable `MaterialGraph` Asset KindとDocument Typeへ追加した。Material Graph schema v2はDomain、Blend Mode、Shading Model、Node/Property/Linkを永続化し、64 MiB上限、v1 migration、Atomic Save/Autosave/Recoveryを共通Document基盤から利用する。
- Material SchemaはOutput、Scalar、Vector3、TexCoord、Texture Sample、Add、Multiply、Lerp、Normal Decodeを提供する。Compilerは型・必須入力・循環を検証し、Texture GUID一覧、決定的HLSL、source fingerprint、node単位diagnosticを生成する。
- `EditorMaterialGraphService`はAdd/Remove/Connect/Disconnect/Property/Moveを`material-graph` Domainの汎用snapshot Commandとして共通Transaction Stackへ登録する。登録失敗時rollback、Global Undo/Redo、Document Dirty、通知を既存基盤へ統合し、compile失敗時も最後の正常Artifactを保持する。
- Bottom DockへMaterial Graph canvas、型付き接続、node property、Compile、Diagnostics、Generated HLSLを追加した。Content BrowserはMaterialGraph filter/preview/thumbnailを扱い、共通DiagnosticsはTexture Asset GUIDの欠落・Kind不一致を検出する。
- RegressionはGraph CoreのMaterial include禁止、決定的compile、Document round-trip/v1 migration、型不一致・循環拒否、last-known-good Artifact、Transaction Undo/Redo、Durable Asset分類を検証する。Debug/Development v143 buildは警告0/エラー0、実フレーム8秒安定、Editor Core Regressionは35/35、Smokeは10/10である。Development Commercial Gateは9/9（124 checks、failed 0、warnings 0、blocked 0）を通過し、全performance budget内である。

#### D-3 Advanced VFX Graph実装結果

- Materialに依存しない`EditorGraph` Coreを再利用し、System Output、Emitter、Spawn Rate/Burst、Initialize Velocity、Gravity/Drag、Sprite/Ribbon/Beam Rendererの型付きVFX Schemaを追加した。System Outputは最大64 Emitterを受け取り、Graph全体は既存の4096 Node/16384 Link・cardinality・循環拒否制約に従う。
- `.vfxgraph`/`.vfxsystem`をDurable `VfxGraph` Asset KindとDocument Typeへ追加した。Schema v2はCPU/GPU Simulation Target、最大Particle数、Fixed Timestep、Node/Property/Linkを永続化し、64 MiB上限、v1 migration、Atomic Save/Autosave/Recoveryを共通Document基盤から利用する。
- CompilerはSpawn/Initialize/Update/Render段階をEmitter Programへ決定的に変換し、source fingerprint、simulation HLSL、Material/Texture Asset依存、node単位diagnosticを生成する。compile失敗時は最後の正常Artifactを保持し、不完全GraphをRuntimeへ公開しない。
- `EditorVfxGraphService`はNode/Link/Property/Move/Simulation Settingsを`vfx-graph` Domainの汎用snapshot Commandとして共通Transaction Stackへ登録する。Global Undo/Redo、Dirty、通知を統合し、Preview ApplyはAsset GUIDを解決した成功Artifactだけを既存`EffectRuntime`のParticle/Trail/Beam componentへ変換する。
- Bottom DockへAdvanced VFX Graph canvas、全Property選択、CPU/GPU・Capacity・Fixed Step編集、Compile/Diagnostics、Execution Program/Simulation HLSL、明示Apply Previewを追加した。Content BrowserはVfxGraph filter/preview/thumbnailを扱い、共通DiagnosticsはMaterial/Texture GUIDの欠落・Kind不一致を検出する。
- Regressionは決定的compile、Document round-trip/v1 migration、型不一致、last-known-good Artifact、Transaction Undo/Redo、Durable Asset分類を検証する。Debug/Development v143 buildは警告0/エラー0、Editor Core Regressionは36/36、Smokeは10/10である。

#### D-3 Animation State Machine実装結果

- `EditorGraphSchema`へDomain非依存のcycle policyを追加した。Material/VFX Graphは従来どおりcycleを拒否し、Animation SchemaだけがEntry、Animation State、Transitionによる循環遷移を許可する。
- Runtimeへ`AnimationStateMachineInstance`を追加し、Bool/Float/Int/Trigger Parameter、条件演算、Transition priority、normalized Exit Time、State speed/loop、Trigger消費、Cross-fadeを決定的に評価する。`ApplyAnimationBlend`は2 Clipのjoint translate/rotate/scaleを既存Skeletonへ補間適用する。
- `.animsm`/`.animstate`をDurable `AnimationStateMachine` Asset KindとDocument Typeへ追加した。Schema v2はtyped Parameter、stable Entry/State/Transition、Clip source Asset GUID、condition/blend設定を永続化し、64 MiB上限、v1 migration、Atomic Save/Autosave/Recoveryを共通Document基盤から利用する。
- Compilerは状態と遷移をstable ID順に正規化し、Entry index、typed Parameter table、source/target State index、condition、threshold、exit time、blend duration、priorityを持つfingerprinted Runtime Programを生成する。compile失敗時は最後の正常Programを保持する。
- `EditorAnimationStateMachineService`はNode/Link/Property/Move/Parameterを`animation-state-machine` Domainの汎用snapshot Commandとして共通Transaction Stackへ登録する。Global Undo/Redo、Dirty、通知、Preview interpreterを共通Editorへ統合する。
- Bottom Dockへ循環Graph canvas、全Property編集、Parameter追加/削除、Bool/Float/Int/Trigger Preview、step/reset、Diagnostics、Generated Programを追加した。Content BrowserはAnimationStateMachine filter/preview/thumbnailを扱い、共通DiagnosticsはAnimation source GUIDをskinned Mesh Assetとして検証する。
- Regressionは循環Schema、決定的compile、Runtime条件遷移/Cross-fade、Skeleton pose blend、Document round-trip/v1 migration、last-known-good Program、Transaction Undo/Redo、Durable Asset分類を検証する。Debug/Development v143 buildは警告0/エラー0、Editor Core Regressionは37/37、Smokeは10/10である。段階検証方針に従い、Development Commercial Gateと実フレームprobeはrelease/integration checkpointへ繰り延べる。

#### D-3 Gameplay Visual Scripting実装結果

- Editor非依存の`GameplayVisualScriptInstance`を追加し、Bool/Float/Int/String Value、typed Variable、BeginPlay/Tick Event、式評価、Branch、Set Variable、Print、Emit Event、Returnをcompiled Runtime Programとして実行する。Host側はPrint/Event callbackだけを受け、Authoring Graphへのwritable pointerを保持しない。
- RuntimeはAssetごとのinstruction budget（1～1,000,000）と64段のexpression depth guardを強制する。Exec cycleは意図的なLoopとして許可する一方、budget超過を`BudgetExceeded`として停止し、data-expression cycleはCompiler errorとして拒否する。
- Gameplay Schemaは既存`EditorGraph` Core上でExec/Bool/Float/Int/String Pin、Event、Flow、Variable、Literal、Math、Compare、Logic、Debug Nodeを定義する。Compilerはstable Node ID順にtyped expression table、instruction table、event entry pointを決定的に生成し、source fingerprintとnode単位diagnosticを出力する。
- `.gameplay`/`.visualscript`をDurable `GameplayVisualScript` Asset KindとDocument Typeへ追加した。Schema v2はtyped Variable、instruction budget、Node/Property/Linkを永続化し、64 MiB上限、v1 migration、Atomic Save/Autosave/Recoveryを共通Document基盤から利用する。
- `EditorGameplayVisualScriptService`はNode/Link/Property/Move/Variable/Budgetを`gameplay-visual-script` Domainの汎用snapshot Commandとして共通Transaction Stackへ登録する。Global Undo/Redo、Dirty、通知、Diagnostics、compile失敗時のlast-known-good Programを共通Editorへ統合する。
- Bottom DockへGameplay Graph canvas、typed connection、全Property編集、Variable/Budget操作、BeginPlay/Tick Preview、実行Trace/Output、Diagnostics、Generated Programを追加した。Content BrowserはGameplayVisualScript filter/preview/thumbnailを扱う。
- Regressionは決定的compile、typed Bool Branchとflow merge、VM Event/Print、instruction budgetによるcycle停止、Document round-trip/v1 migration、last-known-good Program、Transaction Undo/Redo、Durable Asset分類を検証する。Debug/Development v143 buildは警告0/エラー0、Editor Core Regressionは38/38、Smokeは10/10である。段階検証方針に従い、Commercial Gateと実フレームprobeはPhase D integration checkpointへ繰り延べる。

#### 独立追加 Editor Font Foundation実装結果

- `EditorFontService`へregular/monospace font、各pixel size、UI scale、日本語glyph rangeを持つversioned設定を追加した。Gameplay、Graph、Runtimeの各domainには依存せず、ImGui初期化境界でのみfont atlasを構築する。
- font sourceは`Resources/Editor/Fonts`配下の相対`.ttf`/`.otf`/`.ttc`に限定し、canonical path containment、8～48 px、UI scale 0.75～2.5、64 MiB上限を検証する。不正設定、欠落、load失敗時はImGui built-in fontへ安全にfallbackするため、設定ミスでEditor UIを失わない。
- 設定保存は`EditorFileTransaction`を使用する。Bottom Dockの`Editor Fonts` panelからfont discoveryの明示refresh、regular/monospace選択、size/scale/glyph設定、preview、default reset、unsaved revertを行い、変更は次回起動で適用する。runtime中のD3D12 font texture差替えは行わず、restart requiredを明示する。
- 日本語対応の既定UI fontとしてGoogle Fonts配布の`MPLUSRounded1c-Medium.ttf`（weight 500／中字）を同梱し、`MPLUSRounded1c-Regular.ttf`（weight 400）も選択肢として保持した。SIL OFL 1.1全文、公式source URL、各SHA-256を`Resources/Editor/Fonts`へ保持し、追加fontについてもlicense確認責任を`README.md`に定義した。
- Debug/Development v143 buildは警告0/エラー0、Editor Core Regressionは39/39、Smokeは10/10である。通常Debug起動はD3D12/ImGui font atlas初期化を含め8秒間安定した。この独立追加はGameplay Visual Scriptingの次工程やActive Milestoneを変更しない。

## 10. Pull Request単位の実装順

1. `IEditorUndoCommand`とbyte budget追加
2. Course Transaction移行
3. Details Transaction移行
4. Asset/Runtime Apply Transaction移行
5. Core Transactionからdomain payload削除
6. Atomic Layout/Preference Writer追加
7. File Transaction Journal/Recovery追加
8. Asset DeleteをTrash方式へ移行
9. Asset Rename/MoveをStaging方式へ移行
10. Filesystem Failure Injection追加
11. Play Isolation Provider Registry追加
12. Course/Terrain Provider移行
13. VFX/Post-process/Camera/Gameplay Provider追加
14. Generic Document Manager追加
15. Course Document移行
16. Autosave/External Change/Recovery追加
17. Editor World Object Provider追加
18. World Outliner read-only版追加
19. Outliner/Viewport/Details Selection同期
20. Outliner mutationとTransaction追加
21. Transform Gizmo hit test追加
22. Transform Gizmo preview/commit/cancel追加
23. Viewport Overlay Layer追加
24. Scene/Entity/Component最小基盤追加
25. Asset Drag & DropによるEntity生成
26. Batch Meta migration追加
27. Content Browser Folder Tree追加
28. Right Inspector再編
29. Bottom Dock/Menu/Toolbar再編
30. Course Timeline Track Provider化

## 11. Test Strategy

### 11.1 North-star Workflow Test

次を1本のEnd-to-End Testとして保護する。

1. Projectを開く。
2. Content BrowserからMesh Assetを選択する。
3. Viewportへdragする。
4. World OutlinerへEntityが追加される。
5. DetailsへTransform/Mesh propertyが表示される。
6. Gizmoで移動、回転、拡縮する。
7. Undo/Redoする。
8. Validationする。
9. Sceneを保存する。
10. Reloadして同じ状態へ戻る。
11. Play中にRuntime Entityを変更する。
12. StopしてAuthoring状態へ戻る。
13. 選択したRuntime変更だけApplyする。
14. Apply結果をUndoする。
15. 保存中断後にRecoveryする。

### 11.2 Unit/Integration Test

- Transaction success/failure/history budget
- Selection synchronization
- Outliner hierarchy mutation
- Gizmo coordinate/hit test
- Transform preview/commit/cancel
- Atomic writer
- File transaction recovery
- Play snapshot/provider failure
- Document autosave/recovery
- GUID migration/reference repair
- Drag & Drop entity creation

### 11.3 Performance Budget

- Outliner 10,000 object filter/scroll
- Content Browser 100,000 asset index/filter
- Details multi-selection
- Gizmo frame cost
- Overlay label layout
- Autosave latency
- Background import impact

## 12. Commercial Completion Gate

次を満たした時点で、現在のEditorから統合World Editorへ昇華したと判定する。

- World Outliner、Viewport、Detailsが同一Selectionを使用する。
- Production Transform GizmoがTransaction-backedで動作する。
- Scene/Entity/Componentが保存・再読込できる。
- Asset Drag & DropでScene Objectを生成できる。
- Asset metadata coverageが100%である。
- Save/Asset mutationがAtomic File Transactionを使用する。
- Play/Stopで全Authoring domainが復元される。
- Runtime Applyが選択式かつUndo可能である。
- Generic Document、Save All、Autosave、Recoveryが動作する。
- Feature Guardがblocked 0、attention 0である。
- North-star Workflow TestがCIで成功する。

### 12.1 Phase D Integration／Commercial Completion Gate実装結果

- `--editor-commercial-gates`を11ゲートへ拡張し、構造化レポートを`editor.commercialCompletion.v4`へ更新した。`commercialCompletionReady`は全ゲート成功、blocked 0、attention 0を同時に満たした場合だけ`true`になる。
- `editor.phaseDIntegration`はMaterial Graph、Advanced VFX Graph、Animation State Machine、Gameplay Visual Scriptを同一の`EditorDocumentManager`、`EditorTransactionStack`、`EditorExecutionContext`へ接続し、決定的compile、domain横断Undo/Redo、4 Documentのatomic Save All、Content Browser分類、Autosave Recoveryを検証する。
- `editor.northStarWorkflow`はdurable Asset選択、Viewport Asset Drop、World Outliner生成、共有Selection、Details Component追加、Scene EntityのMove/Rotate/Scale、Undo/Redo、Scene Save/Reload、Play isolation、selective Runtime Apply、Undo Apply、interrupted-save Recoveryを1本のworkflowで検証する。
- Scene EntityがWorld ModelでTransform capabilityを持ちながらGizmo側で拒否されていた接続漏れを修正した。`SetComponentProperty` World mutationによりTransformのtranslation/rotation/scaleをprovider snapshot付きの汎用Commandとして処理する。
- Feature Guardはblockedだけでなくattentionも失敗扱いとした。Viewport pick後も同一stable IDを使用するfixtureへ修正し、既定editor surfaceでblocked 0、attention 0を保証する。
- 2026-07-14のDevelopment x64実測は11/11 gate、155/155 check、performance sample 6/6、blocked 0、attention 0、warning 0、`ready`である。レポートは`logs/editor_automation_report.json`と`logs/editor_automation_report.md`、個別証跡は`logs/editor_phase_d_integration_report.log`と`logs/editor_north_star_workflow_report.log`へ出力する。

このGateはEditor Evolution Phase Dの完了条件であり、エンジン全体のCommercial Release Gate G13そのものではない。G13では引き続きClean Release/Shipping、Runtime／Editor分離、Cook/Package、Soak、GPU matrix、Crash Reporter、Third-party attributionを完了する必要がある。

## 13. 実装を後回しにする機能

次はPhase A～C完了前に開始しない。

- Material Graph
- Blueprint相当Visual Scripting
- Advanced VFX Graph
- Full Sequencer
- Animation Graph
- Plugin Marketplace

これらを先に実装すると、独自Transaction、独自Document、独自Asset referenceが増え、後の統合コストが大きくなる。

## 14. 最終方針

最も目立つ次の昇華点は、World Outliner、Production Transform Gizmo、Contextual Detailsである。ただし、それらを商用品質で成立させるため、実装順は次を守る。

```text
Transaction/File Safety/PIE/Document
  -> World Model/Outliner/Gizmo
  -> Asset Identity/Content Browser/UI再編
  -> Sequencer/Prefab/Graph Tool
```

新機能の数ではなく、「選択、編集、Undo、保存、Play、復元」が全domainで同じ安全な経路を使うことをEditor進化の基準とする。
