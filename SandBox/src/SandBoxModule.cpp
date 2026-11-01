// SandBox gameplay module; the host loads this DLL from <projectRoot>/Binaries/game.dll.

#include <VostaEngine.h>

#include <Renderer/Projection.h>

#include <gtc/matrix_transform.hpp>

#include <string>

// One HUD document, kept self-contained: its root sizes itself from the
// background box, so the instance drops into the code-built page as a section.
// A .vehud carries exactly this JSON body.
static const char* kHudJson = R"JSON(
{
  "referenceResolution": [1920, 1080],
  "root": {
    "type": "stack",
    "name": "Root",
    "children": [
      { "type": "colored_box", "name": "HudPanel", "offset": [0, 0],
        "props": { "size": [440, 260], "color": [0.10, 0.11, 0.14, 1] } },

      { "type": "text", "name": "Title", "offset": [16, 12],
        "props": { "text": "HUD Document", "color": [1, 1, 1, 1], "fontSize": 30 } },

      { "type": "text", "name": "HudCaption", "offset": [16, 50],
        "props": { "text": "built from JSON, bound by name · 你好，Vellum 中文", "color": [0.70, 0.78, 0.92, 1], "fontSize": 14 } },

      { "type": "button", "name": "StartButton", "offset": [16, 78],
        "props": { "size": [200, 44], "color": [0.16, 0.42, 0.72, 1] },
        "children": [
          { "type": "text", "name": "StartLabel",
            "props": { "text": "Start", "color": [1, 1, 1, 1], "fontSize": 22 } }
        ] },

      { "type": "input_box", "name": "NameInput", "offset": [16, 132],
        "props": { "size": [300, 40], "text": "", "fontSize": 20 } },

      { "type": "text", "name": "Echo", "offset": [16, 186],
        "props": { "text": "Echo: ", "color": [0.75, 0.83, 0.95, 1], "fontSize": 18 } }
    ]
  }
}
)JSON";

namespace vl = ve::vellum;

class SandBoxModule : public ve::GameModule {
public:
	void onStart(ve::Scene& scene) override {
		VE_CORE_INFO_PRINT("[SandBox] onStart, entities=%zu", scene.getRegistry().each().size());

		vl::VellumLayer* layer = ve::Application::get().getVellumLayer();
		if (!layer)
			return;

		ve::Ref<ve::HudAsset> asset = ve::HudAsset::fromJson(kHudJson);
		if (!asset) {
			VE_CORE_WARN_PRINT("%s", "[SandBox] HUD document failed to parse");
			return;
		}

		ve::HudBuildContext ctx;
		ctx.atlasFor = [layer](const std::string& name, float size) {
			return name.empty() ? layer->font(size) : layer->font(name, size);
		};
		ctx.textureFor = [layer](const std::string& path, glm::vec2& sizeOut) {
			return layer->texture(path, sizeOut);
		};

		m_hud = ve::HudInstance::create(asset, ctx);
		if (!m_hud)
			return;

		startPreview(layer);
		bindHud();
		m_page = buildPage(layer);
		layer->setRoot(m_page);
	}

	void onUpdate(ve::Scene&, float deltaTime) override {
		m_elapsed += deltaTime;
		if (m_hud) {
			if (auto* title = m_hud->text("Title"))
				title->setText("HUD Document  " + std::to_string(static_cast<int>(m_elapsed)));
		}
		renderPreview(deltaTime);
	}

	void onEnd(ve::Scene& scene) override {
		m_preview = nullptr;
		// The host's Vellum layer is shared; drop our tree so it does not linger.
		if (auto* layer = ve::Application::get().getVellumLayer())
			layer->setRoot(nullptr);
		m_page = nullptr;
		m_hud = nullptr;
		VE_CORE_INFO_PRINT("[SandBox] onEnd, entities=%zu", scene.getRegistry().each().size());
	}

private:
	void startPreview(vl::VellumLayer* layer) {
		m_preview = ve::CreateRef<ve::UIRenderTarget>(*layer, glm::vec2(416.0f, 216.0f));

		m_previewMeshHandle = ve::ResourceManager::store<ve::StaticMesh>("content/sofa.veasset");
		m_previewMesh = ve::ResourceManager::get<ve::StaticMesh>(m_previewMeshHandle);
		if (!m_previewMesh)
			VE_CORE_WARN_PRINT("%s", "[SandBox] preview mesh failed to load");
		m_previewMaterial = ve::BuiltinResources::getDefaultMaterial();
	}

	void renderPreview(float deltaTime) {
		if (!m_preview || !m_previewMesh)
			return;

		const glm::vec3 aabbMin = m_previewMesh->getAabbMin();
		const glm::vec3 aabbMax = m_previewMesh->getAabbMax();
		const glm::vec3 center = (aabbMin + aabbMax) * 0.5f;
		const float radius = glm::max(glm::length(aabbMax - aabbMin) * 0.5f, 1e-4f);

		// renderModelMatrix() rebuilds the translation from worldPosition, not transform.
		const glm::mat4 rotScale =
			glm::rotate(glm::mat4(1.0f), m_elapsed * 0.8f, glm::vec3(0.0f, 1.0f, 0.0f)) *
			glm::scale(glm::mat4(1.0f), glm::vec3(1.0f / radius));
		const glm::vec3 offset = glm::vec3(rotScale * glm::vec4(-center, 1.0f));

		const glm::vec3 eye(0.0f, 0.35f, 2.6f);
		ve::RenderContext ctx{};
		ctx.deltaTime = deltaTime;
		ctx.totalTime = m_elapsed;
		// Chart camera; the frame is flat, so this only feeds unused atmosphere inputs.
		ctx.cameraPosition = ve::Vec3(eye);
		ctx.viewMatrix = glm::lookAt(eye, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		ctx.projMatrix = ve::makePerspective(
			glm::radians(40.0f), m_preview->size().x / m_preview->size().y, 0.05f, 100.0f);

		const auto& submeshes = m_previewMesh->getSubmeshes();
		if (submeshes.empty()) {
			ve::DrawMeshCommand cmd;
			cmd.meshHandle = m_previewMeshHandle;
			cmd.materialHandle = m_previewMaterial;
			cmd.transform = rotScale;
			cmd.worldPosition = ve::Vec3(offset);
			ctx.drawMeshCommands.push_back(cmd);
		}
		else {
			for (const auto& sub : submeshes) {
				ve::DrawMeshCommand cmd;
				cmd.meshHandle = m_previewMeshHandle;
				cmd.materialHandle = sub.materialHandle.isValid() ? sub.materialHandle : m_previewMaterial;
				cmd.transform = rotScale;
				cmd.worldPosition = ve::Vec3(offset);
				cmd.startIndex = sub.firstIndex;
				cmd.indexCount = sub.indexCount;
				ctx.drawMeshCommands.push_back(cmd);
			}
		}

		ve::DrawLightCommand light;
		light.light.color = glm::vec3(1.0f);
		light.light.intensity = 7.0f;
		light.light.range = 30.0f;
		light.position = ve::Vec3(eye + glm::vec3(1.0f, 1.5f, 1.0f));
		ctx.drawLightCommands.push_back(light);

		m_preview->render(ctx);
	}

	void bindHud() {
		if (auto* panel = m_hud->box("HudPanel"))
			panel->setTooltip("ColoredBox background from the JSON document");
		if (auto* title = m_hud->text("Title"))
			title->setTooltip("TextBlock: setText() runs every frame from C++");
		if (auto* start = m_hud->button("StartButton")) {
			start->setOnClick([] { VE_CORE_INFO_PRINT("%s", "[SandBox] HUD StartButton clicked"); });
			start->setTooltip("Button authored in JSON, onClick bound by name");
		}
		if (auto* input = m_hud->inputBox("NameInput")) {
			input->setTooltip("InputBox: click to focus, type, Enter submits");
			input->setOnTextChanged([this](const std::string& value) {
				if (auto* echo = m_hud->text("Echo"))
					echo->setText("Echo: " + value);
			});
			input->setOnSubmit([](const std::string& value) {
				VE_CORE_INFO_PRINT("[SandBox] submitted '%s'", value.c_str());
			});
		}
	}

	ve::Ref<vl::ScrollPanel> buildPage(vl::VellumLayer* layer) {
		glm::vec2 nineSize{}, earthSize{};
		const vl::TextureId texNine = layer->texture("content/textures/ninegrid_test.png", nineSize);
		const vl::TextureId texEarth = layer->texture("content/earth.png", earthSize);
		if (texNine == 0 || texEarth == 0)
			VE_CORE_WARN_PRINT("%s", "[SandBox] UI textures failed to load");

		const glm::vec4 titleColor{ 1.0f, 1.0f, 1.0f, 1.0f };
		const glm::vec4 captionColor{ 0.62f, 0.70f, 0.84f, 1.0f };
		const glm::vec4 panelBg{ 0.10f, 0.11f, 0.14f, 1.0f };

		auto text = [layer](float size, std::string value, const glm::vec4& color = glm::vec4(1.0f)) {
			return ve::CreateRef<vl::TextBlock>(layer->font(size), value, color);
		};
		auto section = [&](float w, float h) {
			auto s = ve::CreateRef<vl::Stack>();
			s->add(ve::CreateRef<vl::ColoredBox>(panelBg, glm::vec2(w, h)));
			return s;
		};

		auto page = ve::CreateRef<vl::ScrollPanel>();
		page->setBackgroundColor(glm::vec4(0.055f, 0.065f, 0.085f, 0.96f));
		page->setTooltip("ScrollPanel page: wheel anywhere that is not an inner list");

		auto content = ve::CreateRef<vl::Stack>();

		content->add(text(34, "Vellum UI Showcase", titleColor), { 24.0f, 16.0f });
		content->add(text(15, "layout - input - text - textures - animation - HUD assets", captionColor),
		             { 24.0f, 58.0f });

		// Authored HUD document as one section of the page.
		content->add(m_hud->rootRef(), { 24.0f, 90.0f });

		// ---- Buttons -------------------------------------------------------
		auto buttons = section(440.0f, 230.0f);
		buttons->add(text(18, "Buttons", captionColor), { 12.0f, 8.0f });

		auto clicksText = text(15, "Clicks: 0", captionColor);

		auto solid = ve::CreateRef<vl::Button>(glm::vec2(200.0f, 48.0f),
		                                       glm::vec4(0.20f, 0.45f, 0.75f, 1.0f));
		solid->addChild(text(20, "Colour"));
		solid->setTransitionRate(22.0f);
		solid->setTooltip("Colour-only button: hover and press fade between tints");
		solid->setOnClick([this, clicksText] { clicksText->setText("Clicks: " + std::to_string(++m_clicks)); });

		auto textured = ve::CreateRef<vl::Button>(glm::vec2(200.0f, 48.0f));
		textured->setStyle(vl::ButtonStyle::fromTexture(texEarth));
		textured->addChild(ve::CreateRef<vl::ColoredBox>(glm::vec4(0.0f, 0.0f, 0.0f, 0.35f),
		                                                 glm::vec2(200.0f, 48.0f)));
		textured->addChild(text(20, "Texture"));
		textured->setTooltip("ButtonStyle::fromTexture: hover keeps the tint, press darkens 15%");
		textured->setOnClick([this, clicksText] { clicksText->setText("Clicks: " + std::to_string(++m_clicks)); });

		vl::ButtonStyle artStyle = vl::ButtonStyle::fromTexture(texNine);
		artStyle.hoveredTexture = texEarth;
		artStyle.pressedTexture = texNine;
		artStyle.pressedTint = glm::vec4(0.55f, 0.62f, 0.75f, 1.0f);
		auto artButton = ve::CreateRef<vl::Button>(glm::vec2(200.0f, 48.0f));
		artButton->setStyle(artStyle);
		artButton->addChild(ve::CreateRef<vl::ColoredBox>(glm::vec4(0.0f, 0.0f, 0.0f, 0.4f),
		                                                  glm::vec2(200.0f, 48.0f)));
		artButton->addChild(text(20, "Hover art"));
		artButton->setTooltip("Per-state art: hover swaps to the earth texture, press returns to the grid");
		artButton->setOnClick([this, clicksText] { clicksText->setText("Clicks: " + std::to_string(++m_clicks)); });

		auto iconRow = ve::CreateRef<vl::Stack>();
		auto icon = ve::CreateRef<vl::Image>();
		icon->setTexture(texEarth, earthSize);
		icon->setSize(glm::vec2(24.0f, 24.0f));
		iconRow->add(icon, { 8.0f, 4.0f });
		iconRow->add(text(18, "Icon + label"), { 38.0f, 6.0f });
		auto iconButton = ve::CreateRef<vl::Button>(glm::vec2(200.0f, 48.0f),
		                                            glm::vec4(0.18f, 0.20f, 0.24f, 1.0f));
		iconButton->addChild(iconRow);
		iconButton->setTooltip("Button content: Image + TextBlock inside a Stack child");
		iconButton->setOnClick([this, clicksText] { clicksText->setText("Clicks: " + std::to_string(++m_clicks)); });

		buttons->add(solid, { 12.0f, 36.0f });
		buttons->add(textured, { 228.0f, 36.0f });
		buttons->add(artButton, { 12.0f, 94.0f });
		buttons->add(iconButton, { 228.0f, 94.0f });
		buttons->add(clicksText, { 12.0f, 158.0f });
		buttons->add(text(14, "Hover fades the tint; textures snap per state", captionColor),
		             { 12.0f, 188.0f });
		content->add(buttons, { 24.0f, 366.0f });

		// ---- UI render target ----------------------------------------------
		auto rtSection = section(440.0f, 286.0f);
		rtSection->add(text(18, "UI Render Target", captionColor), { 12.0f, 8.0f });
		if (m_preview) {
			auto preview = m_preview->image();
			preview->setTooltip("Live render target: the pipeline renders the sofa, Vellum samples the texture");
			rtSection->add(preview, { 12.0f, 36.0f });
		}
		rtSection->add(text(14, "416x216 pipeline render, rotating every frame", captionColor),
		               { 12.0f, 258.0f });
		content->add(rtSection, { 24.0f, 866.0f });

		// ---- Anchors -------------------------------------------------------
		auto anchors = section(440.0f, 240.0f);
		anchors->add(text(18, "Stack anchors", captionColor), { 12.0f, 8.0f });

		auto board = ve::CreateRef<vl::Stack>();
		board->add(ve::CreateRef<vl::ColoredBox>(glm::vec4(0.07f, 0.08f, 0.10f, 1.0f),
		                                         glm::vec2(416.0f, 196.0f)));
		const glm::vec2 anchorPos[9] = {
			{ 0.0f, 0.0f }, { 0.5f, 0.0f }, { 1.0f, 0.0f },
			{ 0.0f, 0.5f }, { 0.5f, 0.5f }, { 1.0f, 0.5f },
			{ 0.0f, 1.0f }, { 0.5f, 1.0f }, { 1.0f, 1.0f },
		};
		const char* anchorName[9] = { "TL", "TC", "TR", "ML", "C", "MR", "BL", "BC", "BR" };
		for (int i = 0; i < 9; ++i) {
			auto marker = ve::CreateRef<vl::Stack>();
			marker->add(ve::CreateRef<vl::ColoredBox>(glm::vec4(0.35f, 0.60f, 0.95f, 1.0f),
			                                          glm::vec2(8.0f, 8.0f)));
			marker->add(text(14, anchorName[i], captionColor), { 12.0f, 0.0f });
			marker->setTooltip("anchor is normalised over the leftover space");
			board->add(marker, glm::vec2(0.0f), anchorPos[i]);
		}
		board->setTooltip("pos = slot.pos + (slot.size - childSize) * anchor + offset");
		anchors->add(board, { 12.0f, 32.0f });
		content->add(anchors, { 24.0f, 606.0f });

		// ---- ScrollPanel + Column -----------------------------------------
		auto list = ve::CreateRef<vl::ScrollPanel>(glm::vec2(360.0f, 300.0f));
		list->setBackgroundColor(glm::vec4(0.08f, 0.09f, 0.12f, 1.0f));
		list->setTooltip("ScrollPanel + Column: wheel here; outside it the page scrolls");
		auto column = ve::CreateRef<vl::Column>();
		column->setSpacing(4.0f);
		for (int i = 0; i < 40; ++i) {
			column->addChild(text(18, "Scrollable row " + std::to_string(i),
			                      i % 2 ? glm::vec4(0.85f, 0.88f, 0.95f, 1.0f)
			                            : glm::vec4(0.62f, 0.70f, 0.82f, 1.0f)));
		}
		list->addChild(column);
		content->add(list, { 500.0f, 90.0f });

		// ---- NineGridImage -------------------------------------------------
		auto grids = section(740.0f, 300.0f);
		grids->add(text(18, "NineGridImage: blocks tile, never stretch or cut", captionColor),
		           { 12.0f, 8.0f });

		auto addGrid = [&](float cell, const glm::vec2& slot, const char* label, float x) {
			auto img = ve::CreateRef<vl::NineGridImage>();
			img->setTexture(texNine, nineSize);
			img->setCellSize(cell);
			img->setSize(slot);
			img->setTooltip("NineGridImage: 3x3 blocks repeated at a fixed cell size");
			grids->add(img, { x, 34.0f });
			grids->add(text(14, label, captionColor), { x, 258.0f });
		};
		addGrid(72.0f, glm::vec2(216.0f, 216.0f), "cell 72 - exact 3x3", 8.0f);
		addGrid(72.0f, glm::vec2(200.0f, 120.0f), "cell 72 - rounds to 216x216", 240.0f);
		addGrid(48.0f, glm::vec2(200.0f, 120.0f), "cell 48 - rounds to 240x144", 472.0f);
		content->add(grids, { 500.0f, 410.0f });

		// ---- Image ---------------------------------------------------------
		auto imageSection = section(740.0f, 140.0f);
		imageSection->add(text(18, "Image: texture stretched into an explicit slot", captionColor),
		                  { 12.0f, 8.0f });
		auto image = ve::CreateRef<vl::Image>();
		image->setTexture(texEarth, earthSize);
		image->setSize(glm::vec2(280.0f, 80.0f));
		image->setTooltip("Image: stretched, tinted, optional UV sub-rect");
		imageSection->add(image, { 12.0f, 40.0f });
		imageSection->add(text(14, "256x256 earth -> 280x80 slot", captionColor), { 308.0f, 64.0f });
		content->add(imageSection, { 500.0f, 726.0f });

		// ---- Splitter ------------------------------------------------------
		auto splitSection = section(740.0f, 300.0f);
		splitSection->add(text(18, "Splitter: drag the bars; weights and pixel minimums", captionColor),
		                  { 12.0f, 8.0f });

		auto splitBox = [](const glm::vec4& color, const glm::vec2& size, const char* tip) {
			auto box = ve::CreateRef<vl::ColoredBox>(color, size);
			box->setTooltip(tip);
			return box;
		};
		auto vertical = ve::CreateRef<vl::Splitter>(vl::Splitter::Orientation::Vertical);
		vertical->addSlot(splitBox({ 0.20f, 0.32f, 0.55f, 1.0f }, { 240.0f, 100.0f }, "Top slot"),
		                  1.0f, 40.0f);
		vertical->addSlot(splitBox({ 0.15f, 0.23f, 0.40f, 1.0f }, { 240.0f, 100.0f }, "Bottom slot"),
		                  1.0f, 40.0f);

		auto split = ve::CreateRef<vl::Splitter>(vl::Splitter::Orientation::Horizontal);
		split->setHandleThickness(8.0f);
		split->addSlot(vertical, 1.0f, 80.0f);
		split->addSlot(splitBox({ 0.55f, 0.26f, 0.18f, 1.0f }, { 180.0f, 160.0f }, "Right slot"),
		               1.0f, 60.0f);
		split->setTooltip("Splitter: capture-based drag; minimums clamp each side");
		splitSection->add(split, { 12.0f, 36.0f });
		content->add(splitSection, { 500.0f, 882.0f });

		page->addChild(content);
		return page;
	}

	ve::Ref<ve::HudInstance> m_hud;
	ve::Ref<vl::ScrollPanel> m_page;
	ve::Ref<ve::UIRenderTarget> m_preview;
	ve::Ref<ve::StaticMesh>  m_previewMesh;
	ve::AssetHandle m_previewMeshHandle;
	ve::AssetHandle m_previewMaterial;
	float m_elapsed = 0.0f;
	int   m_clicks = 0;
};

VE_GAME_MODULE(SandBoxModule)
