#include "vepch.h"
#include "RuntimeVirtualTexture.h"

#include "Asset/TextureFormat.h"
#include "Core/Log.h"

#include <algorithm>
#include <cmath>

namespace ve {

	namespace {
		// Nearest-first page candidate built while collecting the request set.
		struct Candidate {
			int   key;       // flat pyramid index (see keyOf)
			int   mip;
			int   x;
			int   y;
			float priority;  // lower is better: world distance to the location it serves
		};

		// Hysteresis: a resident page competes as if it were this much closer, so a
		// page on a band boundary does not thrash frame to frame.
		constexpr float kResidentBias = 0.6f;

		// The reserved coarse tail's priority: nothing can beat it, so the pool cap
		// never evicts it.
		constexpr float kReservedPriority = -1.0f;

		// Sanity cap on the mip-0 grid, so a mistyped texels/m cannot allocate a
		// multi-billion-texel page table.
		constexpr int64_t kMaxPagesMip0 = 4096;

		int nextPow2(int v) {
			int p = 1;
			while (p < v) p <<= 1;
			return p;
		}
	}

	void RuntimeVirtualTexture::init(const Settings& settings) {
		m_settings = settings;
		m_settings.pageSizeTexels = std::max(1, m_settings.pageSizeTexels);
		m_settings.borderTexels   = std::max(0, m_settings.borderTexels);
		m_settings.physicalSlots  = std::max(1, m_settings.physicalSlots);
		m_settings.texelsPerMeter = std::max(1e-3f, m_settings.texelsPerMeter);
		m_pageWorldSize = (float)m_settings.pageSizeTexels / m_settings.texelsPerMeter;

		const int ps = m_settings.pageSizeTexels;
		const int phys = ps + 2 * m_settings.borderTexels;
		const int slots = m_settings.physicalSlots;

		// Four arrays, one per GBuffer color target, so the bake's MRT writes through
		// unchanged. Formats match the gbuffer attachments on purpose.
		m_physical[0] = Texture2DArray::create((uint32_t)phys, (uint32_t)phys, (uint32_t)slots, TextureFormat::RGBA,    nullptr);
		m_physical[1] = Texture2DArray::create((uint32_t)phys, (uint32_t)phys, (uint32_t)slots, TextureFormat::RGBA16F, nullptr);
		m_physical[2] = Texture2DArray::create((uint32_t)phys, (uint32_t)phys, (uint32_t)slots, TextureFormat::RGBA,    nullptr);
		m_physical[3] = Texture2DArray::create((uint32_t)phys, (uint32_t)phys, (uint32_t)slots, TextureFormat::RGBA16F, nullptr);

		// Bake target: four color attachments, no depth (the bake is a pure XZ
		// projection). Attachments are re-pointed at array layers per request.
		FramebufferSpec spec;
		spec.width = (uint32_t)phys;
		spec.height = (uint32_t)phys;
		spec.hasDepth = false;
		spec.colorAttachments.resize(4);
		m_bakeFbo = Framebuffer::create(spec);

		m_slotPage.assign((size_t)slots, -1);
		m_pageSlot.clear();
		m_bakeRequests.clear();
	}

	void RuntimeVirtualTexture::setWorldBounds(const glm::vec2& minXZ, const glm::vec2& maxXZ) {
		if (m_gridValid && minXZ == m_minXZ && maxXZ == m_maxXZ) return;

		m_gridValid = true;
		m_minXZ = minXZ;
		m_maxXZ = maxXZ;

		const float extX = std::max(maxXZ.x - minXZ.x, 1e-3f);
		const float extY = std::max(maxXZ.y - minXZ.y, 1e-3f);
		const int   pageSize = std::max(1, m_settings.pageSizeTexels);

		// Backed off only when the mip-0 grid it implies is absurd. A sanity cap, not
		// a residency policy -- the pool is a cache, so the grid may be far larger.
		float density = std::max(m_settings.texelsPerMeter, 1e-3f);
		int contentX = 1, contentY = 1;
		for (int guard = 0; guard < 64; ++guard) {
			m_pageWorldSize = (float)pageSize / density;
			contentX = std::max(1, (int)std::ceil(extX / m_pageWorldSize));
			contentY = std::max(1, (int)std::ceil(extY / m_pageWorldSize));
			if ((int64_t)contentX * (int64_t)contentY <= kMaxPagesMip0) break;
			density *= 0.8f;
		}
		m_contentPagesX = contentX;
		m_contentPagesY = contentY;

		// Padded to powers of two: a pyramid covering the grid needs ceil(pages / 2^k)
		// pages per level, while glTextureStorage2D halves by flooring, and the two
		// agree only on power-of-two edges. The padding is addressable but empty --
		// no slot is ever spent on it.
		m_pagesX = nextPow2(contentX);
		m_pagesY = nextPow2(contentY);

		// Pyramid down to a single page. The coarse levels are tiny, so they always
		// fit the pool and serve as the fallback for finer non-resident pages.
		m_mipCount = 1;
		for (int p = std::max(m_pagesX, m_pagesY); p > 1; p >>= 1)
			++m_mipCount;

		m_mipOffset.assign((size_t)m_mipCount + 1, 0);
		for (int mip = 0; mip < m_mipCount; ++mip)
			m_mipOffset[mip + 1] = m_mipOffset[mip] + levelPagesX(mip) * levelPagesY(mip);

		m_pageTable = Texture2D::create((uint32_t)m_pagesX, (uint32_t)m_pagesY,
		                                TextureFormat::R16UI, (uint32_t)m_mipCount);
		m_pageSlot.assign((size_t)m_mipOffset.back(), kUnallocated);
		m_slotPage.assign((size_t)std::max(1, m_settings.physicalSlots), -1);
		m_bakeRequests.clear();
		uploadPageTable();

		VE_CORE_SUCCESS_PRINT("RVT: %dx%d mip-0 pages at %.2f texels/m over %.1fx%.1f m "
		                      "(%d mips, table %dx%d, %d pages, %d slots)",
			m_contentPagesX, m_contentPagesY, density, extX, extY, m_mipCount,
			m_pagesX, m_pagesY, m_mipOffset.back(), m_settings.physicalSlots);
	}

	glm::vec2 RuntimeVirtualTexture::pageWorldOrigin(int x, int y, int mip) const {
		// Aligned to the mip-0 block the page covers, matching the consumer's mapping.
		return m_minXZ + glm::vec2((float)x, (float)y) * pageWorldSizeAt(mip);
	}

	glm::vec2 RuntimeVirtualTexture::bakedWorldOrigin(int x, int y, int mip) const {
		const float pad = (float)m_settings.borderTexels * pageWorldSizeAt(mip)
		                / (float)m_settings.pageSizeTexels;
		return pageWorldOrigin(x, y, mip) - glm::vec2(pad);
	}

	bool RuntimeVirtualTexture::pageTouchesContent(int mip, int x, int y) const {
		const glm::vec2 lo = pageWorldOrigin(x, y, mip);
		const glm::vec2 hi = lo + glm::vec2(pageWorldSizeAt(mip));
		return hi.x > m_minXZ.x && lo.x < m_maxXZ.x && hi.y > m_minXZ.y && lo.y < m_maxXZ.y;
	}

	void RuntimeVirtualTexture::requestPages(const glm::vec2& viewerXZ, float worldPerPixelAtUnitDepth) {
		m_bakeRequests.clear();
		if (!m_pageTable || m_pagesX <= 0 || m_pagesY <= 0) return;

		// CPU-driven on purpose -- no GPU feedback buffer to read back.
		const float radius = m_settings.requestRadius;
		const bool  unlimited = radius <= 0.0f;
		const int slots = std::max(1, m_settings.physicalSlots);

		// Coarse tail, kept whole: the levels whose pages fit in a quarter of the
		// pool. This is the no-holes guarantee -- a fine page that loses its slot
		// degrades upward and always finds an ancestor here.
		int reserveMip = m_mipCount - 1;
		{
			int used = levelPagesX(reserveMip) * levelPagesY(reserveMip);
			while (reserveMip > 0) {
				const int n = levelPagesX(reserveMip - 1) * levelPagesY(reserveMip - 1);
				if (used + n > std::max(1, slots / 4)) break;
				used += n;
				--reserveMip;
			}
		}

		// The distance beyond which a mip-0 page's texels are coarser than a screen
		// pixel, so each location asks for the single level it can actually use.
		const float texelWorld = m_pageWorldSize / (float)std::max(1, m_settings.pageSizeTexels);
		const float detailDist0 = texelWorld / std::max(1e-6f, worldPerPixelAtUnitDepth);
		const float invDetail0  = 1.0f / std::max(1e-3f, detailDist0);

		std::vector<char>  wanted(m_pageSlot.size(), 0);
		std::vector<float> bestPrio(m_pageSlot.size(), 0.0f);

		for (int mip = reserveMip; mip < m_mipCount; ++mip) {
			const int px = levelPagesX(mip);
			const int py = levelPagesY(mip);
			for (int y = 0; y < py; ++y) {
				for (int x = 0; x < px; ++x) {
					if (!pageTouchesContent(mip, x, y)) continue;
					const int key = keyOf(mip, x, y);
					wanted[key] = 1;
					bestPrio[key] = kReservedPriority;
				}
			}
		}

		// Streamed levels: one page per mip-0 location, the level its own distance
		// calls for. Several locations share a coarse page, so keep the nearest claim.
		const float half = m_pageWorldSize * 0.5f;
		for (int y0 = 0; y0 < m_contentPagesY; ++y0) {
			for (int x0 = 0; x0 < m_contentPagesX; ++x0) {
				// Distance to the location's rect, not its centre, so the page under
				// the camera is zero away and asks for level 0.
				const glm::vec2 o = pageWorldOrigin(x0, y0, 0) + glm::vec2(half);
				const float dx = std::max(0.0f, std::abs(o.x - viewerXZ.x) - half);
				const float dy = std::max(0.0f, std::abs(o.y - viewerXZ.y) - half);
				const float d  = std::sqrt(dx * dx + dy * dy);

				const int level = std::clamp(
					(int)std::floor(std::log2(std::max(d, 1e-3f) * invDetail0) + 0.5f),
					0, m_mipCount - 1);
				if (level >= reserveMip) continue;                       // reserved wholesale
				if (!unlimited && d > radius * m_pageWorldSize) continue;

				const int key = keyOf(level, x0 >> level, y0 >> level);
				const float prio = (m_pageSlot[key] != kUnallocated) ? d * kResidentBias : d;
				if (!wanted[key] || prio < bestPrio[key]) {
					wanted[key] = 1;
					bestPrio[key] = prio;
				}
			}
		}

		std::vector<Candidate> cands;
		for (int mip = 0; mip < m_mipCount; ++mip) {
			const int px = levelPagesX(mip);
			const int py = levelPagesY(mip);
			for (int y = 0; y < py; ++y)
				for (int x = 0; x < px; ++x) {
					const int key = keyOf(mip, x, y);
					if (wanted[key]) cands.push_back({ key, mip, x, y, bestPrio[key] });
				}
		}

		// Keep the nearest pages when more are wanted than the pool holds; the reserved
		// tail's priority survives the cap.
		if ((int)cands.size() > slots) {
			std::nth_element(cands.begin(), cands.begin() + slots, cands.end(),
				[](const Candidate& a, const Candidate& b) { return a.priority < b.priority; });
			cands.resize((size_t)slots);
		}

		// Two passes -- residents keep their slot, the free pool fills the rest -- so a
		// camera that barely moved re-bakes nothing.
		std::vector<uint16_t> newPageSlot(m_pageSlot.size(), kUnallocated);
		std::vector<int>      newSlotPage(m_slotPage.size(), -1);
		for (const auto& c : cands) {
			const uint16_t slot = m_pageSlot[c.key];
			if (slot == kUnallocated) continue;
			newPageSlot[c.key] = slot;
			newSlotPage[slot] = c.key;
		}

		int freeScan = 0;
		for (const auto& c : cands) {
			if (newPageSlot[c.key] != kUnallocated) continue;
			while (freeScan < slots && newSlotPage[freeScan] != -1) ++freeScan;
			if (freeScan >= slots) break;   // cannot happen: candidates <= slots
			newPageSlot[c.key] = (uint16_t)freeScan;
			newSlotPage[freeScan] = c.key;
			m_bakeRequests.push_back({ c.mip, c.x, c.y, freeScan });
			++freeScan;
		}

		if (newPageSlot != m_pageSlot) {
			m_pageSlot.swap(newPageSlot);
			m_slotPage.swap(newSlotPage);
			uploadPageTable();
		}
	}

	void RuntimeVirtualTexture::commitBakes() {
		m_bakeRequests.clear();
	}

	void RuntimeVirtualTexture::uploadPageTable() {
		if (!m_pageTable) return;
		// One upload per level; each level is a few hundred texels, so this beats
		// per-texel bookkeeping.
		for (int mip = 0; mip < m_mipCount; ++mip) {
			const int px = levelPagesX(mip);
			const int py = levelPagesY(mip);
			m_pageTable->setDataLevel(m_pageSlot.data() + m_mipOffset[mip],
			                          (uint32_t)((size_t)px * py * sizeof(uint16_t)), (uint32_t)mip);
		}
	}

}
