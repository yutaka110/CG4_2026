import fs from "node:fs/promises";
import path from "node:path";
import { Presentation, PresentationFile } from "@oai/artifact-tool";

const ROOT = "C:/Users/youta/OneDrive/Desktop/2026/CG4/CG4_project";
const BUILD = path.join(ROOT, "presentation_build");
const OUT = path.join(ROOT, "CG4_Editor_機能紹介_提出用.pptx");
const RENDER = path.join(BUILD, "rendered");

const C = {
  paper: "#F5F7FA",
  white: "#FFFFFF",
  ink: "#111827",
  muted: "#5B6472",
  line: "#D8DEE8",
  blue: "#2267D8",
  cyan: "#35B8C8",
  gold: "#C89243",
  navy: "#0B1422",
  paleBlue: "#EAF2FF",
  paleCyan: "#E7F8FA",
  paleGold: "#FBF2E5",
};

const FONT = "Yu Gothic UI";
const MONO = "Cascadia Mono";

async function bytes(file) {
  const data = await fs.readFile(file);
  return data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength);
}

function addBox(slide, name, p, fill = C.white, line = C.line, radius = "rounded-xl") {
  return slide.shapes.add({
    geometry: "roundRect",
    name,
    position: p,
    fill,
    line: { style: "solid", fill: line, width: 1 },
    borderRadius: radius,
  });
}

function addText(slide, name, text, p, opts = {}) {
  const s = slide.shapes.add({
    geometry: "textbox",
    name,
    position: p,
    fill: opts.fill ?? "none",
    line: { style: "solid", fill: "none", width: 0 },
  });
  s.text = text;
  s.text.style = {
    fontSize: opts.fontSize ?? 22,
    typeface: opts.typeface ?? FONT,
    bold: opts.bold ?? false,
    color: opts.color ?? C.ink,
    alignment: opts.alignment ?? "left",
    verticalAlignment: opts.verticalAlignment ?? "top",
    autoFit: opts.autoFit ?? "none",
    insets: opts.insets ?? { top: 0, right: 0, bottom: 0, left: 0 },
  };
  return s;
}

function addTitle(slide, title, page, section = "CG4 EDITOR") {
  addText(slide, `section-${page}`, section, { left: 41.33, top: 28, width: 360, height: 28 }, {
    fontSize: 16, bold: true, color: C.blue,
  });
  addText(slide, `title-${page}`, title, { left: 41.33, top: 58, width: 1197.33, height: 76 }, {
    fontSize: 48, bold: true, color: C.ink,
  });
  addText(slide, `page-${page}`, String(page).padStart(2, "0"), { left: 1184.18, top: 666, width: 54.48, height: 18 }, {
    fontSize: 14, color: C.muted, alignment: "right",
  });
}

function addAccentRule(slide, color = C.blue) {
  slide.shapes.add({
    geometry: "rect",
    name: "accent-rule",
    position: { left: 41.33, top: 146, width: 96, height: 5 },
    fill: color,
    line: { style: "solid", fill: color, width: 0 },
  });
}

function addImageFrame(slide, name, imageBytes, p, alt) {
  addBox(slide, `${name}-backing`, p, C.white, C.line, "rounded-xl");
  slide.images.add({
    blob: imageBytes,
    contentType: "image/png",
    alt,
    fit: "cover",
    geometry: "roundRect",
    borderRadius: "rounded-xl",
    position: { left: p.left + 7, top: p.top + 7, width: p.width - 14, height: p.height - 14 },
  });
}

function addEvidenceTag(slide, text, p, color = C.paleBlue) {
  addBox(slide, `tag-${text}`, p, color, color, "rounded-lg");
  addText(slide, `tag-text-${text}`, text, { left: p.left + 12, top: p.top + 8, width: p.width - 24, height: p.height - 12 }, {
    fontSize: 17, bold: true, color: C.ink,
  });
}

function setNotes(slide, lines) {
  slide.speakerNotes.textFrame.setText(lines.join("\n"));
  slide.speakerNotes.setVisible(true);
}

function addTimeline(slide, page, title, stages, section) {
  slide.background.fill = C.paper;
  addTitle(slide, title, page, section);
  addAccentRule(slide, C.cyan);
  const xs = [42, 452, 862];
  slide.shapes.add({
    geometry: "straightConnector1",
    name: `timeline-line-${page}`,
    position: { left: 42, top: 336, width: 1128, height: 0 },
    fill: "none",
    line: { style: "solid", fill: C.line, width: 3 },
  });
  stages.forEach((stage, i) => {
    const x = xs[i];
    addText(slide, `stage-label-${page}-${i}`, stage.label, { left: x, top: 278, width: 330, height: 34 }, {
      fontSize: 18, bold: true, color: C.blue,
    });
    slide.shapes.add({
      geometry: "ellipse",
      name: `stage-dot-${page}-${i}`,
      position: { left: x, top: 326, width: 20, height: 20 },
      fill: i === 1 ? C.cyan : C.blue,
      line: { style: "solid", fill: C.white, width: 3 },
    });
    addText(slide, `stage-title-${page}-${i}`, stage.title, { left: x, top: 374, width: 330, height: 82 }, {
      fontSize: 32, bold: true, color: C.ink,
    });
    addText(slide, `stage-body-${page}-${i}`, stage.body, { left: x, top: 458, width: 330, height: 164 }, {
      fontSize: 22, color: C.muted,
    });
  });
}

function addFourGrid(slide, page, title, items, section) {
  slide.background.fill = C.paper;
  addTitle(slide, title, page, section);
  addAccentRule(slide, C.blue);
  const frames = [
    { left: 41.33, top: 204, width: 581.33, height: 176 },
    { left: 656.86, top: 204, width: 581.33, height: 176 },
    { left: 41.33, top: 418, width: 581.33, height: 176 },
    { left: 656.86, top: 418, width: 581.33, height: 176 },
  ];
  items.forEach((item, i) => {
    const f = frames[i];
    const tint = [C.paleBlue, C.paleCyan, C.paleGold, C.white][i];
    addBox(slide, `grid-box-${page}-${i}`, f, tint, C.line);
    addText(slide, `grid-no-${page}-${i}`, `0${i + 1}`, { left: f.left + 22, top: f.top + 18, width: 52, height: 28 }, {
      fontSize: 18, bold: true, color: C.blue,
    });
    addText(slide, `grid-title-${page}-${i}`, item.title, { left: f.left + 86, top: f.top + 15, width: f.width - 108, height: 42 }, {
      fontSize: 28, bold: true, color: C.ink,
    });
    addText(slide, `grid-body-${page}-${i}`, item.body, { left: f.left + 22, top: f.top + 69, width: f.width - 44, height: 86 }, {
      fontSize: 22, color: C.muted,
    });
  });
}

async function main() {
  await fs.mkdir(RENDER, { recursive: true });

  const hero = await bytes(path.join(BUILD, "frame_00_0.png"));
  const terrainShot = await bytes(path.join(BUILD, "editor_terrain_mode.png"));
  const modelingShot = await bytes(path.join(BUILD, "editor_modeling_mode.png"));
  const placeShot = await bytes(path.join(BUILD, "editor_place_mode.png"));

  const deck = Presentation.create({ slideSize: { width: 1280, height: 720 } });

  // 01 — Codex Grid slide 01 hierarchy: eyebrow / large title / subtitle.
  {
    const s = deck.slides.add();
    s.background.fill = C.navy;
    s.images.add({ blob: hero, contentType: "image/png", alt: "Canyon runtime view", fit: "cover", position: { left: 0, top: 0, width: 1280, height: 720 } });
    s.shapes.add({ geometry: "rect", name: "title-panel", position: { left: 0, top: 0, width: 720, height: 720 }, fill: C.navy, line: { style: "solid", fill: C.navy, width: 0 } });
    addText(s, "title-eyebrow", "CG4 / CUSTOM D3D12 EDITOR", { left: 41.33, top: 41.18, width: 598.67, height: 50 }, { fontSize: 24, bold: true, color: C.cyan });
    addText(s, "title-main", "Production\nAuthoring Pipeline", { left: 41.33, top: 182.55, width: 650, height: 250 }, { fontSize: 72, bold: true, color: C.white, verticalAlignment: "bottom" });
    addText(s, "title-sub", "Terrain / Mesh Sync / Asset Placement\nプレビュー・履歴・実行反映・永続化を一貫設計", { left: 41.33, top: 497.87, width: 610, height: 115 }, { fontSize: 26, color: "#D7E0EB" });
    setNotes(s, ["[Sources]", "- Internal runtime capture: .codex/video_frames_capture10/frame_00_0.png", "- Technical scope supplied by the user and verified against internal project sources."]);
  }

  // 02 — Codex Grid slide 09 hierarchy: large message with three callouts.
  {
    const s = deck.slides.add();
    s.background.fill = C.paper;
    addTitle(s, "3つの編集系を、共通のライフサイクルで統合", 2, "SYSTEM OVERVIEW");
    addAccentRule(s, C.blue);
    addText(s, "overview-thesis", "Previewで結果を確認し、Accept後だけ履歴・保存・ゲーム反映へ進める。", { left: 41.33, top: 184, width: 1110, height: 70 }, { fontSize: 34, bold: true, color: C.ink });
    const cards = [
      ["Terrain", "非破壊ブラシ\n局所再構築\nterrainRevision"],
      ["Mesh Sync", "Editable Copy\nGUID維持Rebake\nRenderer / Physics更新"],
      ["Placement", "Content Drawer\nPreview配置\nScene永続化"],
    ];
    cards.forEach((c, i) => {
      const x = [40.77, 452.38, 864][i];
      addBox(s, `overview-card-${i}`, { left: x, top: 340, width: 374.67, height: 280 }, [C.paleBlue, C.paleCyan, C.paleGold][i], C.line);
      addText(s, `overview-no-${i}`, `0${i + 1}`, { left: x + 30, top: 370, width: 50, height: 28 }, { fontSize: 18, bold: true, color: C.blue });
      addText(s, `overview-title-${i}`, c[0], { left: x + 30, top: 410, width: 310, height: 45 }, { fontSize: 32, bold: true });
      addText(s, `overview-body-${i}`, c[1], { left: x + 30, top: 474, width: 315, height: 116 }, { fontSize: 22, color: C.muted });
    });
    setNotes(s, ["[Sources]", "- User-provided feature requirements.", "- docs/EditorCoreDesign.md: shared Tool Manager lifecycle, Placement, Terrain, Geometry, Mesh Bake, and Scene Instance pipeline sections."]);
  }

  // 03 — Codex Grid slide 08 hierarchy: half text / half image.
  {
    const s = deck.slides.add();
    s.background.fill = C.paper;
    addTitle(s, "Terrain：形状と材質を、止めず・壊さず編集", 3, "01 / TERRAIN");
    addAccentRule(s, C.cyan);
    addImageFrame(s, "terrain-shot", terrainShot, { left: 658.17, top: 170, width: 581.6, height: 470 }, "Terrain mode in the CG4 editor");
    addText(s, "terrain-target", "CanyonAssaultRoute01.course", { left: 41.33, top: 188, width: 581.33, height: 40 }, { fontSize: 24, bold: true, color: C.blue, typeface: MONO });
    addText(s, "terrain-body", "Course Freeze後、Terrain Modeで4ツールを使用。\n\nSculpt　隆起・掘削\nSmooth　局所平滑化\nFlatten　基準面へ整形\nPaint　4層の地表バリエーション\n\nドラッグ中はPreviewのみ。Acceptで\n1 stroke = 1 Transactionとして確定。", { left: 41.33, top: 248, width: 570, height: 326 }, { fontSize: 22, color: C.muted });
    addEvidenceTag(s, "Undo / Redo → Play / Sim → Reload → revision", { left: 41.33, top: 584, width: 570, height: 54 }, C.paleCyan);
    addText(s, "terrain-caption", "実機キャプチャ｜Terrain / Sculpt / Smooth / Flatten / Paint", { left: 675, top: 646, width: 550, height: 24 }, { fontSize: 14, color: C.muted, alignment: "right" });
    setNotes(s, ["[Sources]", "- Internal UI capture: .codex/slide_build/editor_terrain_mode.png", "- docs/EditorCoreDesign.md lines covering TerrainEditLayer and TerrainChunkManager.", "- application/editor/terrain/EditorTerrainBrushTools.cpp", "- application/terrain/TerrainEditLayer.cpp"]);
  }

  // 04 — Codex Grid slide 17 hierarchy: three-stage timeline.
  {
    const s = deck.slides.add();
    addTimeline(s, 4, "安全に試せる → 戻せる → 再起動後も残る", [
      { label: "PREVIEW", title: "Sculptを短くドラッグ", body: "Radius 4–8\nStrength 0.10–0.25\nHardness 0.30–0.50\nSpacing 0.50–1.00" },
      { label: "ACCEPT & HISTORY", title: "4操作を順に確定", body: "Sculpt → Smooth（≈0.10）\n→ Flatten → Paint\n各操作をAcceptし、\nUndo / Redoで単位を確認" },
      { label: "RUNTIME & SAVE", title: "反映と永続化を証明", body: "Play / Simで形状を確認\nSave → Reload / restart\nRuntime Watchで\nterrainRevisionの増加を確認" },
    ], "01 / TERRAIN DEMO");
    setNotes(s, ["[Sources]", "- User-provided Terrain recording sequence and safe parameter ranges.", "- application/AppImGuiLayer.cpp: Runtime Watch terrainRevision evidence."]);
  }

  // 05 — Codex Grid slide 13 hierarchy: four-point grid.
  {
    const s = deck.slides.add();
    addFourGrid(s, 5, "非破壊編集を支える4つの設計判断", [
      { title: "Previewを分離", body: "drag中はpreviewEditLayerのみ更新。Accept前のCourseと履歴は変更しない。" },
      { title: "変更領域だけ再構築", body: "各chunkは重なるstampだけをhash化。dirty regionのみ非同期で再生成する。" },
      { title: "Smoothを安定化", body: "Apply単位で読込元を固定するダブルバッファ。重複sampleは正規化して尖りを防ぐ。" },
      { title: "1 stroke = 1履歴", body: "EditorTerrainEditUndoCommandで開始前へ復元。Save / Loadも同じ永続Layerを扱う。" },
    ], "01 / TERRAIN DESIGN");
    setNotes(s, ["[Sources]", "- docs/EditorCoreDesign.md: TerrainEditLayer, one command per stroke, filtered chunk rebuild.", "- application/editor/terrain/EditorTerrainBrushTools.cpp: immutable Smooth apply pass.", "- application/terrain/TerrainEditLayer.cpp: normalized overlapping Smooth samples."]);
  }

  // 06 — Codex Grid slide 08 hierarchy: half text / half image.
  {
    const s = deck.slides.add();
    s.background.fill = C.paper;
    addTitle(s, "Mesh Sync：同じGUIDへRebakeし、配置済み形状を更新", 6, "02 / PRODUCTION MESH");
    addAccentRule(s, C.blue);
    addImageFrame(s, "modeling-shot", modelingShot, { left: 658.17, top: 170, width: 581.6, height: 470 }, "Modeling mode in the CG4 editor");
    addText(s, "mesh-body", "ball_productionを選択し、Modelingへ。\n\nCreate Editable Copy\n元形状をPreview複製 → Accept\n\nSelect Faces → Extrude（0.2–0.5）\nRecalculate Normals\nGenerate Box Collision\n\nBake Mesh Asset\nBake Type: Rebake / 同一Asset GUID", { left: 41.33, top: 188, width: 570, height: 342 }, { fontSize: 22, color: C.muted });
    addText(s, "mesh-guid", "GUID  a7853409f86661149beaecb724ea5104\nHASH  13349175553422702320", { left: 41.33, top: 544, width: 580, height: 60 }, { fontSize: 18, bold: true, color: C.ink, typeface: MONO });
    addEvidenceTag(s, "再起動なしでRenderer / Physicsへ反映", { left: 41.33, top: 608, width: 570, height: 42 }, C.paleBlue);
    addText(s, "modeling-caption", "実機キャプチャ｜Modeling Tool Palette", { left: 675, top: 646, width: 550, height: 24 }, { fontSize: 14, color: C.muted, alignment: "right" });
    setNotes(s, ["[Sources]", "- Internal UI capture: .codex/slide_build/editor_modeling_mode.png", "- Resources/Generated/Imported/ball_production.mesh.meta", "- application/editor/mesh/EditorCreateEditableCopyTool.cpp", "- application/editor/mesh/EditorMeshBakePipeline.cpp"]);
  }

  // 07 — Codex Grid slide 17 hierarchy: three-stage timeline.
  {
    const s = deck.slides.add();
    addTimeline(s, 7, "複製・加工・再発行の3段階を連続で見せる", [
      { label: "EDITABLE COPY", title: "元形状を非破壊複製", body: "Previewは元Assetを変更しない\nAccept後、Mesh Rendererに\nEditable Source Asset GUID /\nGeometry Hashを表示" },
      { label: "GEOMETRY", title: "局所面を加工", body: "1–2面をSelect Faces\nExtrude Distance 0.2–0.5\nNormalsとBox Collisionも\nPreview → Accept" },
      { label: "REBAKE & SYNC", title: "同一GUIDで即時更新", body: "Bake Type: Rebake\n配置済みballが再起動なしで更新\nUndo / RedoでBake前後を往復\nPlayでゲーム側描画を確認" },
    ], "02 / MESH DEMO");
    setNotes(s, ["[Sources]", "- User-provided Production Mesh recording sequence.", "- application/editor/scene/EditorSceneComponentRegistry.cpp: editable source GUID/hash fields.", "- application/editor/mesh/EditorMeshBakeTools.cpp: Rebake property."]);
  }

  // 08 — Codex Grid slide 13 hierarchy: four-point grid.
  {
    const s = deck.slides.add();
    addFourGrid(s, 8, "Asset Change TrackerがRendererとPhysicsを同時更新", [
      { title: "Stable Identity", body: "RebakeはAsset GUID・ID・pathを維持。Scene側のdurable referenceを切らない。" },
      { title: "Atomic Publication", body: "source / cooked / collision / Registry / Sceneを1 File Transactionで発行。失敗時は全域rollback。" },
      { title: "Change Detection", body: "artifact stampをPollし、変更AssetだけRuntime CacheへReconcile。検証失敗時はlast-known-goodを維持。" },
      { title: "Runtime Generation", body: "新generationへ差し替え、Renderer / Physicsが再Resolve。Draw Mode: Autoと各Readyを診断表示。" },
    ], "02 / MESH ARCHITECTURE");
    setNotes(s, ["[Sources]", "- docs/EditorCoreDesign.md: prepared File Transaction and GUID-preserving Rebake.", "- application/editor/mesh/EditorProductionMeshAsset.cpp: Asset Change Tracker and runtime generations.", "- application/editor/EditorBuiltinDetailsSectionProviders.cpp: Production Mesh Presentation Ready stages."]);
  }

  // 09 — Codex Grid slide 08 hierarchy: half text / half image.
  {
    const s = deck.slides.add();
    s.background.fill = C.paper;
    addTitle(s, "Placement：選択AssetをPreview配置し、Sceneへ確定", 9, "03 / LOADER & PLACEMENT");
    addAccentRule(s, C.gold);
    addImageFrame(s, "place-shot", placeShot, { left: 658.17, top: 170, width: 581.6, height: 470 }, "Place mode in the CG4 editor");
    addText(s, "place-body", "Ctrl + Space → Content Drawer\nAssets → Resources / Generated\n\nSelected Mesh: ball_production / GUID\nPlace Mode → Place Selected Asset\nUniform Scale → Viewport click → Accept\n\nWorld Outliner：Scene配下のEntity\nDetails：Transform / Mesh Renderer / Asset GUID\n\nF2で Demo_Mesh に変更", { left: 41.33, top: 188, width: 570, height: 390 }, { fontSize: 22, color: C.muted });
    addEvidenceTag(s, "Undoで消去 → Redoで同じEntityを復元", { left: 41.33, top: 594, width: 570, height: 50 }, C.paleGold);
    addText(s, "place-caption", "実機キャプチャ｜Place Selected Asset / Selected Mesh", { left: 675, top: 646, width: 550, height: 24 }, { fontSize: 14, color: C.muted, alignment: "right" });
    setNotes(s, ["[Sources]", "- Internal UI capture: .codex/slide_build/editor_place_mode.png", "- docs/EditorCoreDesign.md: Production Placement lifecycle.", "- application/editor/tools/EditorPlacementTools.cpp"]);
  }

  // 10 — Codex Grid slide 17 hierarchy: three-stage timeline.
  {
    const s = deck.slides.add();
    addTimeline(s, 10, "選択 → 生成 → 復元までを1本のデモで完結", [
      { label: "SELECT", title: "Content Drawerで選択", body: "ball_production行をクリック\nSelected MeshとGUIDを撮影\nDrawerを閉じてPlaceへ" },
      { label: "PLACE", title: "PreviewからEntity生成", body: "Place Selected Asset\n見やすいUniform Scale\nViewport click → Accept\nOutliner / Detailsを確認" },
      { label: "PERSIST", title: "Demo_Meshを永続化", body: "F2でrename\nUndoで消去 / Redoで復元\nSave → Scene reopen / restart\nPlayでゲーム側表示" },
    ], "03 / PLACEMENT DEMO");
    setNotes(s, ["[Sources]", "- User-provided Loader and Placement recording sequence.", "- application/editor/tools/EditorPlacementTools.cpp: Uniform Scale and Place Selected Asset."]);
  }

  // 11 — Codex Grid slide 10 hierarchy: left narrative / right checklist.
  {
    const s = deck.slides.add();
    s.background.fill = C.paper;
    addTitle(s, "提出動画で残すべき5つの実装証拠", 11, "VERIFICATION");
    addAccentRule(s, C.blue);
    addText(s, "verify-lead", "“編集できる”だけではなく、\nデータの一貫性が保たれることを示す。", { left: 41.33, top: 184, width: 581.33, height: 100 }, { fontSize: 32, bold: true, color: C.ink });
    addText(s, "verify-body", "評価者が確認できる連続性\n\n変更前 → Preview → Accept\n→ Undo / Redo → Runtime\n→ Save / Reload\n\nTerrain・Mesh・Placementで同じ観点を揃えることで、個別機能ではなくEditor基盤として説明できます。", { left: 41.33, top: 320, width: 581.33, height: 292 }, { fontSize: 22, color: C.muted });
    const checks = [
      "Preview中はAuthoring dataが未変更",
      "Accept後、Undo / Redoが同一単位",
      "同一GUID・revision / Ready診断",
      "Play / Simでゲーム画面へ反映",
      "Reload後もTerrain / Demo_Meshを維持",
    ];
    checks.forEach((t, i) => {
      const y = 258 + i * 70;
      s.shapes.add({ geometry: "ellipse", name: `check-${i}`, position: { left: 748, top: y + 4, width: 30, height: 30 }, fill: i === 2 ? C.cyan : C.blue, line: { style: "solid", fill: C.white, width: 2 } });
      addText(s, `check-mark-${i}`, "✓", { left: 754, top: y + 5, width: 20, height: 24 }, { fontSize: 18, bold: true, color: C.white, alignment: "center" });
      addText(s, `check-text-${i}`, t, { left: 800, top: y, width: 420, height: 42 }, { fontSize: 22, bold: true, color: C.ink });
    });
    setNotes(s, ["[Sources]", "- User-provided evidence requirements for Undo/Redo, Play/Sim, Reload, Runtime Watch, GUID and Ready states.", "- Internal project implementation sources listed in .codex/slide_build/source-notes.txt."]);
  }

  // 12 — Codex Grid slide 26 hierarchy: eyebrow / large close / three lines.
  {
    const s = deck.slides.add();
    s.background.fill = C.navy;
    addText(s, "close-eyebrow", "CG4 EDITOR", { left: 41.33, top: 41.18, width: 260, height: 54 }, { fontSize: 24, bold: true, color: C.cyan });
    addText(s, "close-title", "編集・反映・永続化を\n同じTransaction設計で接続", { left: 41.33, top: 182.55, width: 1120, height: 260 }, { fontSize: 64, bold: true, color: C.white, verticalAlignment: "bottom" });
    addText(s, "close-details", "Terrain：局所差分　｜　Mesh：GUID維持　｜　Placement：Scene永続化", { left: 41.33, top: 522.13, width: 1120, height: 75 }, { fontSize: 28, color: "#D7E0EB" });
    s.shapes.add({ geometry: "rect", name: "close-rule", position: { left: 41.33, top: 624, width: 1197, height: 4 }, fill: C.cyan, line: { style: "solid", fill: C.cyan, width: 0 } });
    setNotes(s, ["[Sources]", "- Synthesis of the internal implementation and user-provided demonstration scope."]);
  }

  for (const [i, slide] of deck.slides.items.entries()) {
    const png = await deck.export({ slide, format: "png", scale: 1 });
    await fs.writeFile(path.join(RENDER, `slide-${String(i + 1).padStart(2, "0")}.png`), new Uint8Array(await png.arrayBuffer()));
    const layout = await slide.export({ format: "layout" });
    await fs.writeFile(path.join(RENDER, `slide-${String(i + 1).padStart(2, "0")}.layout.json`), await layout.text());
  }
  const montage = await deck.export({ format: "webp", montage: true, scale: 1 });
  await fs.writeFile(path.join(BUILD, "deck-montage.webp"), new Uint8Array(await montage.arrayBuffer()));
  const pptx = await PresentationFile.exportPptx(deck);
  await pptx.save(OUT);
  console.log(OUT);
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
