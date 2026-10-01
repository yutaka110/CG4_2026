# レールであばレール — タイトルロゴ

- ファイル: `rail_title_logo.png`、1536×1024、RGBA、背景透過。
- 作成日: 2026-09-29。
- 同日改訂: 角を柔らかくし、厚い茶色の輪郭・立体影を除いて白一色に整理。ゲーム側で右1.5px・下2.5px、不透明度22%の薄い影を描く（1600×900基準）。表示幅を620pxから560pxへ縮小。
- 現行版の編集プロンプト: `rail_title_logo.refinement.md`。以下の初版プロンプトに続いて2段階で編集。
- 制作方法: Codexの組み込み `image_gen` による生成画像。人による手描き素材ではない。
- 表記: 上段「レールで」、下段「あばレール」。感嘆符なし。
- ユーザーの参考画像から、白い極太文字・勢いのある字形という方向性を採用。参考画像そのものをゲーム素材として転載・切り抜きしていない。生成ツールへ参照画像ファイルは送信せず、以下の文章で方向性を指定。
- 読み込み: `AppRunLoop::EnsureRailLockOnHudAtlas` でRGBAをHUDアトラスへ格納。表示・フェード・暗転は既存HUDと共通。画像の読み込みに失敗した場合は文字によるタイトルに戻る。

## 初版の生成プロンプト（built-in tool、transparent_background=true）

Use case: logo-brand. Create a production-ready transparent PNG Japanese video game title logo for a playful minecart rail shooter. Exact text in TWO lines: top line 「レールで」 bottom line 「あばレール」. Combined title is exactly レールであばレール, no punctuation, no exclamation point. Only these nine Japanese characters (レ ー ル で / あ ば レ ー ル), no English text. Original custom hand-lettered, very heavy white brush-cut display lettering, broad solid strokes, organic slightly rounded angular edges, a few controlled asymmetric cuts, slightly varied character tilts and energetic forward lean, compact tight balanced kerning, professionally art-directed Japanese action adventure game wordmark. Top line about 80% width of bottom line, both lines optically centered. White/off-white faces with a subtle warm charcoal-brown offset dimensional shadow down-right, short solid extrusion and a narrow dark edge for readability on a tan canyon scene. The reference aesthetic is bold white chunky hand-drawn Japanese fantasy adventure lettering, not thin typography or a UI heading. Two readable horizontal lines, bottom line larger, each character remains completely legible, especially で and ば dakuten. Do not add wheels, rails, scenery, characters, icons, dust, sparkles, decorative background shapes, slogans, badges or border. No cyan, no gradients, no shiny bevel, no spiky comic explosion, no repeated scratch distress. Entire background truly transparent alpha. Landscape canvas 1536 by 1024; lettering occupies most of width and middle 80% height with clean margins and no clipping. The white lettering must be opaque. Make it feel like a memorable joyful console game title drawn by a lettering artist.
