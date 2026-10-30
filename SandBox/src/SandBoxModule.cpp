// SandBox gameplay module; the host loads this DLL from <projectRoot>/Binaries/game.dll.

#include <VostaEngine.h>

#include <string>

// A HUD authored inline, so the demo exercises the whole document -> build ->
// bind path without a file on disk. A .vehud carries exactly this JSON body.
static const char* kHudJson = R"JSON(
{
  "referenceResolution": [1920, 1080],
  "root": {
    "type": "stack",
    "name": "Root",
    "children": [
      { "type": "colored_box", "name": "Panel",
        "anchor": [0.5, 0.0], "offset": [0, 60],
        "props": { "size": [420, 200], "color": [0.10, 0.11, 0.14, 0.85] } },

      { "type": "text", "name": "Title",
        "anchor": [0.5, 0.0], "offset": [0, 90],
        "props": { "text": "Vosta HUD", "color": [1, 1, 1, 1], "fontSize": 44 } },

      { "type": "button", "name": "StartButton",
        "anchor": [0.5, 0.0], "offset": [0, 190],
        "props": { "size": [220, 56], "color": [0.16, 0.42, 0.72, 1] },
        "children": [
          { "type": "text", "name": "StartLabel",
            "props": { "text": "Start", "color": [1, 1, 1, 1], "fontSize": 28 } }
        ] },

      { "type": "input_box", "name": "NameInput",
        "anchor": [0.5, 0.0], "offset": [0, 280],
        "props": { "size": [360, 52], "text": "", "fontSize": 28 } },

      { "type": "text", "name": "Echo",
        "anchor": [0.5, 0.0], "offset": [0, 348],
        "props": { "text": "Echo: ", "color": [0.75, 0.83, 0.95, 1], "fontSize": 28 } },

      { "type": "nine_grid_image", "name": "NineNative",
        "anchor": [1, 0], "offset": [-100, 40],
        "props": { "source": "content/textures/ninegrid_test.png", "cellSize": 72, "size": [216, 216] } },

      { "type": "text", "name": "NineNativeLabel",
        "anchor": [1, 0], "offset": [-100, 262],
        "props": { "text": "cell 72 - 3x3 blocks - 216x216", "color": [0.85, 0.88, 0.95, 1], "fontSize": 16 } },

      { "type": "nine_grid_image", "name": "NineTiled",
        "anchor": [1, 0], "offset": [-100, 300],
        "props": { "source": "content/textures/ninegrid_test.png", "cellSize": 72, "size": [300, 140] } },

      { "type": "text", "name": "NineTiledLabel",
        "anchor": [1, 0], "offset": [-40, 524],
        "props": { "text": "size 300x140 -> 360x216", "color": [0.85, 0.88, 0.95, 1], "fontSize": 16 } },

      { "type": "nine_grid_image", "name": "NineCell48",
        "anchor": [1, 0], "offset": [-100, 560],
        "props": { "source": "content/textures/ninegrid_test.png", "cellSize": 48, "size": [300, 140] } },

      { "type": "text", "name": "NineCell48Label",
        "anchor": [0, 0], "offset": [530, 570],
        "props": { "text": "cell 48 - size 300x140 -> 336x144", "color": [0.85, 0.88, 0.95, 1], "fontSize": 16 } },

      { "type": "image", "name": "ImageStretch",
        "anchor": [0.5, 1], "offset": [30, -30],
        "props": { "source": "content/textures/ninegrid_test.png", "size": [300, 90] } }
    ]
  }
}
)JSON";

class SandBoxModule : public ve::GameModule {
public:
	void onStart(ve::Scene& scene) override {
		VE_CORE_INFO_PRINT("[SandBox] onStart, entities=%zu", scene.getRegistry().each().size());

		ve::vellum::VellumLayer* layer = ve::Application::get().getVellumLayer();
		if (!layer)
			return;

		ve::Ref<ve::HudAsset> asset = ve::HudAsset::fromJson(kHudJson);
		if (!asset) {
			VE_CORE_WARN_PRINT("%s", "[SandBox] HUD document failed to parse");
			return;
		}

		ve::HudBuildContext ctx;
		ctx.atlasFor = [layer](float size) { return layer->font(size); };
		ctx.textureFor = [layer](const std::string& path, glm::vec2& sizeOut) {
			return layer->texture(path, sizeOut);
		};

		m_hud = ve::HudInstance::create(asset, ctx);
		if (!m_hud)
			return;

		// The event stays code-side; the document only carries the name it binds to.
		if (auto* start = m_hud->button("StartButton")) {
			start->setOnClick([] { VE_CORE_INFO_PRINT("%s", "[SandBox] HUD StartButton clicked"); });
			start->setTooltip("Fires the Start handler in the game module");
		}
		if (auto* title = m_hud->text("Title"))
			title->setTooltip("Pushed from SandBoxModule::onUpdate every frame");
		if (auto* img = m_hud->image("ImageStretch"))
			img->setTooltip("Image: a 344x216 source stretched into a 300x90 slot");

		if (auto* input = m_hud->inputBox("NameInput")) {
			input->setOnTextChanged([this](const std::string& text) {
				if (auto* echo = m_hud->text("Echo"))
					echo->setText("Echo: " + text);
			});
			input->setOnSubmit([](const std::string& text) {
				VE_CORE_INFO_PRINT("[SandBox] submitted '%s'", text.c_str());
			});
		}

		if (auto* root = dynamic_cast<ve::vellum::Stack*>(m_hud->rootRef().get())) {
			auto panel = ve::CreateRef<ve::vellum::ScrollPanel>(glm::vec2(380.0f, 320.0f));
			panel->setBackgroundColor(glm::vec4(0.08f, 0.09f, 0.12f, 0.85f));

			auto list = ve::CreateRef<ve::vellum::Column>();
			list->setSpacing(4.0f);

			ve::vellum::FontAtlas* rowFont = layer->font(22.0f);
			for (int i = 0; i < 40; ++i) {
				auto row = ve::CreateRef<ve::vellum::TextBlock>(
					rowFont, "Scrollable row " + std::to_string(i));
				row->setColor(i % 2 ? glm::vec4(0.85f, 0.88f, 0.95f, 1.0f)
				                    : glm::vec4(0.62f, 0.70f, 0.82f, 1.0f));
				list->addChild(row);
			}

			panel->addChild(list);
			panel->setTooltip("A ScrollPanel: wheel over it, then hover its rows");
			root->add(panel, { 24.0f, 80.0f }, { 0.0f, 0.0f });

			// Splitter demo: horizontal split, with a nested vertical split on the
			// left. Drag either bar; minimums are enforced on both sides.
			auto makeBox = [](const glm::vec4& color, const glm::vec2& size,
			                  const char* tooltip) {
				auto box = ve::CreateRef<ve::vellum::ColoredBox>(color, size);
				box->setTooltip(tooltip);
				return box;
			};

			auto left = ve::CreateRef<ve::vellum::Splitter>(
				ve::vellum::Splitter::Orientation::Vertical);
			left->addSlot(makeBox({ 0.20f, 0.32f, 0.55f, 1.0f }, { 280.0f, 110.0f },
			                      "Left top. Drag the bar below me."), 1.0f, 40.0f);
			left->addSlot(makeBox({ 0.15f, 0.23f, 0.40f, 1.0f }, { 280.0f, 110.0f },
			                      "Left bottom"), 1.0f, 40.0f);

			auto split = ve::CreateRef<ve::vellum::Splitter>(
				ve::vellum::Splitter::Orientation::Horizontal);
			split->setHandleThickness(8.0f);
			split->addSlot(left, 1.0f, 80.0f);
			split->addSlot(makeBox({ 0.55f, 0.26f, 0.18f, 1.0f }, { 200.0f, 220.0f },
			                      "Right slot. Drag the vertical bar to resize both sides."),
			               1.0f, 60.0f);
			root->add(split, { 24.0f, 424.0f }, { 0.0f, 0.0f });
		}

		layer->setRoot(m_hud->rootRef());
	}

	void onUpdate(ve::Scene&, float deltaTime) override {
		if (!m_hud)
			return;
		// Live data is pushed by the game each frame; the document holds only layout.
		m_elapsed += deltaTime;
		if (auto* title = m_hud->text("Title"))
			title->setText("Vosta HUD  " + std::to_string(static_cast<int>(m_elapsed)));

		// One shot, once the tree has arranged: proves ceiling of the block grid.
		if (!m_layoutLogged) {
			if (auto* native = m_hud->nineGridImage("NineNative");
				native && native->drawnSize().x > 0.0f) {
				m_layoutLogged = true;
				for (const char* name : { "NineNative", "NineTiled", "NineCell48" }) {
					auto* img = m_hud->nineGridImage(name);
					if (!img)
						continue;
					VE_CORE_INFO_PRINT("[SandBox] %s cell=%.0f drawn=%.0fx%.0f", name,
					                   img->cellSize(), img->drawnSize().x, img->drawnSize().y);
				}
				if (auto* img = m_hud->image("ImageStretch"))
					VE_CORE_INFO_PRINT("[SandBox] ImageStretch drawn=%.0fx%.0f",
					                   img->bounds().size.x, img->bounds().size.y);
			}
		}
	}

	void onEnd(ve::Scene& scene) override {
		// The host's Vellum layer is shared; drop our tree so it does not linger.
		if (auto* layer = ve::Application::get().getVellumLayer())
			layer->setRoot(nullptr);
		m_hud = nullptr;
		VE_CORE_INFO_PRINT("[SandBox] onEnd, entities=%zu", scene.getRegistry().each().size());
	}

private:
	ve::Ref<ve::HudInstance> m_hud;
	float m_elapsed = 0.0f;
	bool m_layoutLogged = false;
};

VE_GAME_MODULE(SandBoxModule)
