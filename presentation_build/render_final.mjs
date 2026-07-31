import fs from "node:fs/promises";
import path from "node:path";
import { FileBlob, PresentationFile } from "@oai/artifact-tool";

const root = "C:/Users/youta/OneDrive/Desktop/2026/CG4/CG4_project";
const input = path.join(root, "CG4_Editor_機能紹介_提出用.pptx");
const output = path.join(root, "presentation_build", "final_render");

await fs.mkdir(output, { recursive: true });
const deck = await PresentationFile.importPptx(await FileBlob.load(input));
for (const [index, slide] of deck.slides.items.entries()) {
  const blob = await deck.export({ slide, format: "png", scale: 1 });
  await fs.writeFile(
    path.join(output, `slide-${String(index + 1).padStart(2, "0")}.png`),
    new Uint8Array(await blob.arrayBuffer()),
  );
}
console.log(`rendered ${deck.slides.items.length} slides`);
