#pragma once

#include "Core/Core.h"
#include "Renderer/FrameBuffer.h"
#include "Renderer/Texture.h"

#include <glm.hpp>
#include <algorithm>
#include <cstdint>
#include <vector>

namespace ve {

	// Paged runtime virtual texture: bakes a terrain's layered surface into a pool of
	// physical pages addressed through a page table, so consumer sampling cost is
	// constant regardless of how many material layers went into the bake.
	//
	// Every pyramid level is baked for real, not filtered from level 0. That is what
	// lets a non-resident page fall back to a coarser ancestor instead of showing a
	// hole. The physical pool is a cache over the pyramid.
	class VE_API RuntimeVirtualTexture {
	public:
		struct Settings {
			int   pageSizeTexels = 128;    // page edge, in texels
			int   borderTexels   = 2;
			int   physicalSlots  = 128;    // how many pages the pool holds
			// Mip-0 texels per world meter; every level halves it. A request, not a
			// guarantee -- an absurdly large grid backs the density off (setWorldBounds).
			float texelsPerMeter = 4.0f;
			// Range limit on streamed pages, in multiples of the mip-0 page size. <= 0
			// streams every location and lets the pool cap alone decide what stays.
			float requestRadius  = 0.0f;
		};

		// Selects what RVTDebugPass draws (RenderContext::rvtDebugView).
		enum DebugView {
			kDebugNone = 0, kDebugPhysicalAlbedo = 1, kDebugPageTable = 2, kDebugMipLevel = 3
		};

		// Unallocated-page sentinel; the debug shader spells it 0xFFFFu too.
		static constexpr uint16_t kUnallocated = 0xFFFF;

		// Consumer texture units, near the top of the 16-unit budget. Must be bound
		// after applyMaterialBindings: they overlap the layered material's own units.
		static constexpr uint32_t kPageTableUnit = 11;
		static constexpr uint32_t kPhysicalUnit  = 12;  // 12..15, one per physical array

		// One page queued for baking this frame.
		struct BakeRequest {
			int mip = 0;         // pyramid level; x, y index that level's page table
			int x = 0;
			int y = 0;
			int slot = 0;        // physical slot this page renders into
		};

		// Builds the physical arrays and the bake target. Needs a live GL context;
		// never call from BuiltinResources::init.
		void init(const Settings& settings);
		bool isInitialized() const { return m_pageTable != nullptr; }

		// Rebuilds the virtual grid and its pyramid. Identical bounds are a no-op,
		// so this is safe every frame.
		void setWorldBounds(const glm::vec2& minXZ, const glm::vec2& maxXZ);

		// Assigns physical slots to the pages the view needs, nearest first; pages
		// that lose out release their slots. Newly assigned pages land in
		// bakeRequests(). `viewerXZ` is the view's position on the ground plane, and
		// `worldPerPixelAtUnitDepth` is the world size one screen pixel covers at one
		// unit of view distance -- together they pick the mip level worth sampling at
		// each page's distance.
		void requestPages(const glm::vec2& viewerXZ, float worldPerPixelAtUnitDepth);

		const std::vector<BakeRequest>& bakeRequests() const { return m_bakeRequests; }
		// Called by the bake pass once every request has rendered.
		void commitBakes();

		Ref<Texture2D>      pageTable() const { return m_pageTable; }
		Ref<Texture2DArray> physical(int i) const { return (i >= 0 && i < 4) ? m_physical[i] : nullptr; }
		// The bake target. Its four color attachments are re-pointed at the target
		// page's slot in the physical arrays per request.
		Ref<Framebuffer> bakeFramebuffer() const { return m_bakeFbo; }

		// Page (x,y) at level k is exactly mip-0 pages [x<<k, (x+1)<<k) x [y<<k, (y+1)<<k);
		// the consumer shader derives the same alignment.
		glm::vec2 pageWorldOrigin(int x, int y, int mip) const;
		float     pageWorldSizeAt(int mip) const { return m_pageWorldSize * (float)(1 << mip); }
		float     pageWorldSize() const { return m_pageWorldSize; }

		glm::vec2 bakedWorldOrigin(int x, int y, int mip) const;
		float     bakedWorldSizeAt(int mip) const {
			return pageWorldSizeAt(mip) * (float)physicalPageSize() / (float)m_settings.pageSizeTexels;
		}

		glm::vec2 worldMin()  const { return m_minXZ; }
		glm::vec2 worldSize() const { return glm::vec2((float)m_pagesX, (float)m_pagesY) * m_pageWorldSize; }
		int pagesX() const { return m_pagesX; }
		int pagesY() const { return m_pagesY; }
		int mipCount() const { return m_mipCount; }
		int pageSizeTexels() const { return m_settings.pageSizeTexels; }
		int borderTexels() const { return m_settings.borderTexels; }
		int physicalPageSize() const { return m_settings.pageSizeTexels + 2 * m_settings.borderTexels; }
		int slotCount() const { return m_settings.physicalSlots; }

	private:
		void uploadPageTable();

		// Exact halving: the grid is padded to powers of two, so these match the level
		// sizes glTextureStorage2D allocates.
		int levelPagesX(int mip) const { return std::max(1, m_pagesX >> mip); }
		int levelPagesY(int mip) const { return std::max(1, m_pagesY >> mip); }
		// Flat page index across the whole pyramid.
		int keyOf(int mip, int x, int y) const { return m_mipOffset[mip] + y * levelPagesX(mip) + x; }
		bool pageTouchesContent(int mip, int x, int y) const;

		Settings m_settings;
		float    m_pageWorldSize = 0.0f;

		// Terrain bounds, in world units (XZ). The page-table grid below covers at
		// least these and can reach past them into the power-of-two padding.
		glm::vec2 m_minXZ{ 0.0f };
		glm::vec2 m_maxXZ{ 0.0f };
		int       m_pagesX = 0;
		int       m_pagesY = 0;
		int       m_contentPagesX = 0;
		int       m_contentPagesY = 0;
		bool      m_gridValid = false;

		int m_mipCount = 1;
		// level k's page (0,0) key; the extra last entry holds the total page count.
		std::vector<int> m_mipOffset;

		// R16UI, pagesX x pagesY, m_mipCount levels, uploaded on the CPU.
		Ref<Texture2D>      m_pageTable;
		Ref<Texture2DArray> m_physical[4];  // albedo / normal / packed metal-rough-ao / emissive
		Ref<Framebuffer>    m_bakeFbo;

		// CPU mirror of the page table, indexed by keyOf: page -> slot, slot -> page.
		std::vector<uint16_t> m_pageSlot;
		std::vector<int>      m_slotPage;

		std::vector<BakeRequest> m_bakeRequests;
	};

}
