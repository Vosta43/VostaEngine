#pragma once

#include "Camera.h"
#include "FrameBuffer.h"
#include "StaticMesh.h"
#include "Core/AssetHandle.h"
#include "Core/Types.h"
#include "Light.h"
#include "Atmosphere.h"
#include "Clouds.h"
#include "VolumetricLight.h"
#include "Texture.h"
#include <glm.hpp>
#include <gtc/quaternion.hpp>
#include <cmath>
#include <unordered_map>
#include <string>

namespace ve {

	class RuntimeVirtualTexture;
class Material;

	// The authoring world is a flat chart: x/z horizontal, y the altitude above sea
	// level. The renderer draws a planet, so a chart point has to be placed onto
	// the sphere before it reaches the GPU. Without that, the ground stays the plane
	// tangent at the origin while the cloud shell and the atmosphere already curve
	// around it, and the two disagree by d^2 / (2R) at horizontal distance d -- the
	// cloud base sinks 785 m into the terrain by 100 km. XZ survives the mapping
	// untouched, so everything that addresses the world by column -- terrain
	// streaming, quad-tree LOD, the RVT page table -- keeps working in chart space.
	struct PlanetFrame {
		// A radius this large flattens the sphere onto the chart plane: the drop
		// over any sane world is below double's resolution. isFlat() short-circuits
		// the math, so a scene with no atmosphere renders bit-for-bit as before.
		static constexpr double kFlatRadius = 1.0e12;

		// The planet gate: independent of the radius, and of hasAtmosphere (which
		// only says the LUTs exist). On by default; switching it off forces a flat
		// chart even when a radius is set.
		bool       enabled{ true };
		glm::dvec3 center{ 0.0 };
		double     radius{ kFlatRadius };

		bool isFlat() const { return !enabled || radius >= kFlatRadius; }

		// Sea-level height at a chart column: the sphere's surface above the chart
		// plane there. Written as the stable "drop" form radius - sqrt(radius^2 - d^2)
		// = d^2 / (radius + sqrt(radius^2 - d^2)), because the direct radius^2 - d^2
		// cancels catastrophically as d approaches the radius.
		double seaLevel(double x, double z) const {
			if (isFlat()) return 0.0;
			const double dx = x - center.x;
			const double dz = z - center.z;
			const double d2 = dx * dx + dz * dz;
			const double r2 = radius * radius;
			if (d2 >= r2) return center.y;              // past the horizon: clamp
			const double drop = d2 / (radius + std::sqrt(r2 - d2));
			return center.y + radius - drop;
		}

		// Chart point -> render point. XZ passes through untouched (what keeps the
		// column-addressed systems valid); y becomes the altitude above the local
		// sea level.
		glm::dvec3 place(const ve::Vec3& chart) const {
			if (isFlat()) return glm::dvec3(chart);
			const glm::dvec3 c(chart);
			return glm::dvec3(c.x, seaLevel(c.x, c.z) + c.y, c.z);
		}

		// Local up: the sphere's radial at a chart column, +Y when flat.
		glm::dvec3 up(const ve::Vec3& chart) const {
			if (isFlat()) return glm::dvec3(0.0, 1.0, 0.0);
			return glm::normalize(place(chart) - center);
		}

		// Minimal rotation taking +Y to up(chart): the orientation a rigid chunk of
		// geometry needs so its own +Y points away from the planet. Placing a tile
		// by translation alone leaves it horizontal, and neighbours at different
		// distances then step by tileSize * d / radius -- 31 m between 2 km tiles at
		// 100 km -- instead of meeting within a millimetre. The direction is reduced
		// in double (it is a difference of planet-scale points); the quaternion is
		// float, which is all a unit rotation needs.
		glm::quat surfaceFrame(const ve::Vec3& chart) const {
			if (isFlat()) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
			const glm::vec3 u = glm::normalize(glm::vec3(up(chart)));
			const float c = u.y;                        // dot((0,1,0), up)
			if (c > 1.0f - 1e-7f) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
			if (c < -1.0f + 1e-7f) return glm::quat(0.0f, 1.0f, 0.0f, 0.0f);   // 180 deg
			const glm::vec3 axis = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), u));
			return glm::angleAxis(std::acos(c), axis);
		}

		// The camera's chart position relative to the sphere centre: the vector the
		// atmosphere irradiance bake wants. place() lifts the camera onto the sphere
		// first, so this stays a planet-scale difference in double -- the chart
		// position alone drifts off the sphere as xz grows.
		glm::dvec3 cameraRelativeToCenter(const ve::Vec3& cameraChart) const {
			return place(cameraChart) - center;
		}
	};

	struct DrawMeshCommand {
		AssetHandle meshHandle;
		AssetHandle materialHandle;
		// Rotation * scale, in render space. The translation lives in worldPosition
		// instead, so the matrix a pass uploads carries no world-magnitude term.
		glm::mat4 transform;
		// Authoritative chart translation (double under LWC). A pass places it on
		// the planet sphere and writes
		// transform[3] = Vec4f(Vec3f(place(worldPosition) - ctx.renderOrigin), 1).
		ve::Vec3 worldPosition{ 0.0 };

		// Set for terrain tiles; the RVT bake pass finds its bake sources by this flag.
		bool isTerrain = false;

		// World XZ footprint of a terrain tile, so the bake can skip tiles outside the
		// page it renders.
		glm::vec2 terrainMinXZ{ 0.0f };
		glm::vec2 terrainSizeXZ{ 0.0f };

		// An in-memory mesh with no ResourceManager handle (e.g. the geometry
		// graph's live preview). When set, the passes draw this instead of
		// resolving meshHandle, so previews never register a temporary asset.
		Ref<StaticMesh> transientMesh;

		uint32_t startIndex = 0;
		uint32_t indexCount = 0;
	};

	struct DrawLightCommand {
		Light light;
		glm::vec3 position;
	};

	// One LOD-selected cube-sphere tile: its mesh (tile-local vertices) and the tile
	// centre in planet-local space (|centre| == planet radius). A pass places it by
	// translating (planetCenter + centerLocal) into render space.
	struct GlobeTileDraw {
		Ref<StaticMesh> mesh;
		ve::Vec3        centerLocal{ 0.0 };
	};


	struct DrawSpriteCommand {
		// Rotation * scale; the translation is worldPosition (see DrawMeshCommand).
		glm::mat4 transform;
		ve::Vec3 worldPosition{ 0.0 };
		glm::vec2 size;
		AssetHandle textureHandle;
	};

	struct RenderContext {

		// Chart -> sphere mapping every placed position goes through this frame
		// (see PlanetFrame). Flat when the frame is disabled or carries the flat
		// radius, in which case renderOrigin, the model matrices and the
		// planet-relative uniforms all reduce to their pre-planet form.
		PlanetFrame planetFrame;

		// CHART position of the camera. Scene queries read it as authored --
		// gameplay scripts, terrain streaming, RVT page requests, the quad-tree LOD
		// -- and x/z mean the same thing there as in every entity transform. Its y
		// is the altitude above sea level, i.e. the value cameraAltitude reports.
		ve::Vec3 cameraPosition{ 0.0 };
		// Origin of this frame's render space: the camera, placed on the planet
		// sphere. Chart positions stay authoritative (DrawMeshCommand.worldPosition,
		// cameraPosition, planetCenter) and each pass places + subtracts renderOrigin
		// as it uploads them, so no matrix handed to a shader carries a
		// world-magnitude translation. At ~7 km a float32 view/model translation
		// already quantizes the parallax and the depth reconstruction to a fraction
		// of a pixel, which TAA turns into shimmer; near the origin those terms are
		// exact. Because the anchor IS the camera, the view carries no translation
		// at all -- u_CameraPos is the render-space origin when the shader wants it.
		// Set by SceneViewRenderer; left at 0 by renderers that work near the origin.
		ve::Vec3 renderOrigin{ 0.0 };
		// View matrix mapping RENDER space (authoritative world minus renderOrigin).
		glm::mat4 viewMatrix = glm::mat4(1.0f);
		glm::mat4 projMatrix = glm::mat4(1.0f);
		// The previous frame's jittered view-projection (from Camera::endFrame),
		// used by the TAA pass to reproject history. Zero until the first
		// endFrame() runs.
		glm::mat4 prevViewProjMatrix = glm::mat4(1.0f);
		// Where the pipeline presents its final image (the "@default" source).
		// Owned by the caller: the editor viewport / sandbox target, or a
		// thumbnail's output FBO. Null for callers that don't present.
		Ref<Framebuffer> outputFrameBuffer;
		// The framebuffer written by the last pass that actually ran this frame;
		// what "@previous" resolves to.
		Ref<Framebuffer> previousTarget;
		// Per-frame tables the pass binder reads: pass-name -> framebuffer written
		// this frame, and FBO-name -> framebuffer. Owned by RenderPipeline.
		const std::unordered_map<std::string, Ref<Framebuffer>>* passOutputs = nullptr;
		const std::unordered_map<std::string, Ref<Framebuffer>>* fboRegistry = nullptr;
		// Present pass behaviour: when true, clear the output to transparent and
		// discard sky pixels (thumbnail previews on a transparent background).
		bool presentDiscardBackground = false;

		Ref<TextureCubeMap> skyboxTexture;
		Ref<TextureCubeMap> irradianceMap;
		Ref<TextureCubeMap> prefilteredEnvMap;
		Ref<Texture2D> brdfLUT;

		std::vector<DrawMeshCommand> drawMeshCommands;
		std::vector<DrawLightCommand> drawLightCommands;
		std::vector<DrawSpriteCommand> drawSpriteCommands;

		float deltaTime;
		float totalTime;
		// Camera's frame counter (the one behind the halton projection jitter).
		// Drives the cloud blue-noise phase so it advances a fixed amount every
		// frame regardless of frame rate.
		uint32_t frameIndex = 0;

		int viewPortX = 0;
		int viewPortY = 0;
		int viewPortWidth = 0;
		int viewPortHeight = 0;

		float exposure = 1.0f;
		glm::vec3 ambientLight = glm::vec3(0.05f);

		AtmosphereParams atmosphere;
		bool hasAtmosphere = false;
		ve::Vec3 planetCenter{ 0.0 };
		// Precomputed transmittance LUT for the current atmosphere params;
		// null when no atmosphere is present (sunTransmittance then stays at
		// the shader's default unit-0 sampling, which is never read anyway).
		Ref<Texture2D> transmittanceTexture;

		// Precomputed single-scattering 3D LUTs (see AtmosphereBaker::bakeScatteringLUT),
		// split into Rayleigh and Mie RGB16F textures; sampled by the HDR pass. Mie
		// gets its own texture so its per-channel color survives — the transmittance
		// it multiplies reddens strongly at dusk, which a single gray channel would
		// wash out. Null when no atmosphere is present.
		Ref<Texture3D> scatteringTexture;
		Ref<Texture3D> mieScatteringTexture;

		// Precomputed multiple-scattering 3D LUT (see
		// AtmosphereBaker::bakeMultipleScatteringLUT); added to the single-scattering
		// radiance without a phase function. Null when no atmosphere is present.
		Ref<Texture3D> multipleScatteringTexture;

	CloudParams clouds;
	// Perlin-Worley shape field (RGBA: R = Perlin base billow, G/B/A = Worley F1
	// octaves, see WorleyNoiseBaker::bakeMultiOctave). Sampled once per step and
	// dotted with FBM weights in the shader. Null when not baked.
	Ref<Texture3D> cloudNoiseTexture;
	float cloudWorleyCells = 1.0f;
	// Second, structurally independent Worley field for the detail band
	// (different cells + seed, see WorleyNoiseBaker). Null when not baked.
	Ref<Texture3D> cloudDetailTexture;
	float cloudDetailCells = 1.0f;
	// Pre-summed warp FBM (RG: field A / field B, see
	// WorleyNoiseBaker::bakeWarp) that bends the shape-sample position. Null
	// when not baked.
	Ref<Texture3D> cloudWarpTexture;
	float cloudWarpCells = 1.0f;
	// 2D weather map (see WeatherMapBaker): R = per-position coverage sampled by
	// the height gradient to position the cloud band; G/B unused. Null when not
	// baked.
	Ref<Texture2D> cloudWeatherMap;

	// Sun god-ray parameters, from the scene's VolumetricLightComponent. The
	// pass needs an atmosphere's shadow cascades to be meaningful, so
	// hasVolumetricLight stays false without one (see SceneViewRenderer).
	VolumetricLightParams volumetric;
	bool hasVolumetricLight = false;

	// Cascaded shadow maps, fitted once per frame from the camera + sun (see
	// ShadowCascadeCalculator). shadowCascadeCount is 0 when there is no sun, in
	// which case the HDR pass skips the lookup entirely.
	glm::mat4 shadowLightVP[3];
	float     shadowSplitFar[3]   = { 0.0f, 0.0f, 0.0f };
	float     shadowTexelWorld[3] = { 0.0f, 0.0f, 0.0f };
	int       shadowCascadeCount  = 0;

	// Debug: rasterize meshes as wireframe lines instead of filled triangles.
	// Only the GBuffer pass reads it; all full-screen passes are unaffected.
	bool wireframe = false;

	// Kill switch for temporal anti-aliasing: gates the TAA pass (via the "taa"
	// condition provider) and the camera's subpixel jitter, which exists only to
	// feed it. Off means present reads the HDR buffer directly.
	bool taaEnabled = true;

	// World XZ footprint of this frame's terrain tiles, accumulated by SceneRenderer.
	// Defines the RVT's virtual grid. False when the scene has no drawable terrain.
	bool      hasTerrainBounds = false;
	glm::vec2 terrainMinXZ = glm::vec2(0.0f);
	glm::vec2 terrainSizeXZ = glm::vec2(0.0f);

	// Built and refreshed each frame by SceneViewRenderer. Null means no RVT this
	// frame; consumers no-op.
	Ref<RuntimeVirtualTexture> rvt;
	// RVT debug view selector (0 = off; see RuntimeVirtualTexture::kDebug*).
	int rvtDebugView = 0;

	// Editor view helper: composite a y=0 ground grid in the present pass (see
	// screen.glsl). Off by default; only the editor viewport turns it on.
	bool groundGrid = false;
	// Non-jittered view-projection. The present pass reconstructs the grid ray
	// from this, so the grid does not shimmer with the TAA subpixel jitter
	// (ctx.projMatrix is the jittered one).
	glm::mat4 projMatrixNoJitter = glm::mat4(1.0f);

	// Prototype planet body: the LOD-selected cube-sphere tiles for this frame, empty
	// unless the editor's globe toggle is on. Each tile is placed absolutely at
	// (planetCenter + centerLocal) -- the sphere's frame, not a chart column, so it
	// bypasses place() (up() degenerates at the sphere centre).
	std::vector<GlobeTileDraw> globeTiles;
	// The terrain's layered material and its sphere-domain weight atlas, used to shade
	// the globe tiles (see SceneViewRenderer). Both null when the scene has no rule
	// graph, in which case the GBuffer pass falls back to its own default material.
	Ref<Material>       globeMaterial;
	Ref<Texture2DArray> globeWeights;

		float getAspectRatio() const {
			return (float)viewPortWidth / (float)viewPortHeight;
		}
	};

	// Absolute world position -> render space, with no planet mapping. For values
	// that already live in placed space -- planetCenter is the sphere's centre, not
	// a chart column. The subtraction runs in the authoritative precision (double
	// under LWC) and narrows only at the end, so a planet-scale value never rounds
	// at float32 first.
	inline ve::Vec3f toRenderSpace(const ve::Vec3& worldPos, const ve::Vec3& origin) {
		return ve::Vec3f(worldPos - origin);
	}

	// Chart position -> render space. The planet mapping runs first -- XZ unchanged,
	// y lifted onto the local sphere -- then the frame origin is subtracted. Pass
	// the camera's own chart position and this is the render origin; the shaders
	// that want a camera-relative vector read it that way.
	inline ve::Vec3f toRenderSpaceChart(const ve::Vec3& chartPos, const RenderContext& ctx) {
		return ve::Vec3f(ctx.planetFrame.place(chartPos) - ctx.renderOrigin);
	}

	// Origin the ground-grid shader adds back to anchor its line pattern in world.
	// It is the camera's chart position: x/z place the pattern (place() leaves x/z
	// untouched, so the frame origin's column is the camera's) and y is the chart
	// altitude -- the height of the y = 0 plane below the camera, which the placed
	// height is not.
	inline ve::Vec3f groundGridOrigin(const RenderContext& ctx) {
		return ve::Vec3f(ctx.cameraPosition);
	}

	// A draw command's model matrix, for a mesh or a sprite. Rotation/scale as-is,
	// pre-rotated into the local surface frame, and the translation rebuilt from the
	// placed chart position minus the frame origin. The planet mapping is rigid per
	// draw -- within one mesh the sphere is treated as its tangent plane, which for
	// a 2 km terrain tile is 8 cm out, far under the tile's own height resolution.
	inline glm::mat4 renderModelMatrix(const glm::mat4& rotScale, const ve::Vec3& worldPos,
	                                   const RenderContext& ctx) {
		glm::mat4 m = rotScale;
		if (!ctx.planetFrame.isFlat())
			m = glm::mat4_cast(ctx.planetFrame.surfaceFrame(worldPos)) * m;
		m[3] = glm::vec4(toRenderSpaceChart(worldPos, ctx), 1.0f);
		return m;
	}

	inline glm::mat4 renderModelMatrix(const DrawMeshCommand& cmd, const RenderContext& ctx) {
		return renderModelMatrix(cmd.transform, cmd.worldPosition, ctx);
	}

	// Altitude of the camera above sea level: the chart y, which is exactly what the
	// planet frame defines it to be. (Measuring |planetCenter - cameraPosition| - R
	// against the sphere was wrong the moment the ground became flat -- the chart
	// camera drifts off the sphere as xz grows, so it read 785 m at 100 km where the
	// answer is 0. The atmosphere and cloud shaders need altitudes rather than
	// radii, and this is the authoritative one.) Gated on the planet frame, not on
	// hasAtmosphere, so there is one "is this a planet" test.
	inline float cameraAltitude(const RenderContext& ctx) {
		if (ctx.planetFrame.isFlat())
			return 0.0f;
		return (float)ctx.cameraPosition.y;
	}


}