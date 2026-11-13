#include "vepch.h"
#include "Network/SnapshotView.h"

#include "Scene/Scene.h"

#include <cstring>

namespace ve {

	namespace {

		// Samples further apart than this mean replication stalled. Smeared over
		// such a gap a jump would read as slow motion, so it snaps instead.
		constexpr double kMaxInterpSpanMs = 150.0;

	}

	void SnapshotView::reset() {
		m_objects.clear();
	}

	SnapshotView::ViewObject& SnapshotView::ensure(const NetHandle& handle, const RepLayout& layout) {
		auto it = m_objects.find(handle);
		if (it != m_objects.end())
			return it->second;

		ViewObject object;
		object.handle = handle;
		object.layout = &layout;
		object.values.resize(layout.fields.size());
		for (Sample& sample : object.ring)
			sample.fields.resize(layout.fields.size());

		return m_objects.emplace(handle, std::move(object)).first->second;
	}

	void SnapshotView::resolve(ViewObject& object, Scene& scene) {
		if (!object.layout)
			return;

		for (auto& [name, ptr] : scene.getRegistry().getComponentsForEntity(object.handle.netId)) {
			if (ptr && name == object.layout->className) {
				object.component = ptr;
				return;
			}
		}
	}

	bool SnapshotView::applyBlock(const NetHandle& handle, const std::vector<RepChange>& changes) {
		const RepLayout* layout = RepLayoutRegistry::get().find(handle.classId);
		if (!layout)
			return false;

		ViewObject& object = ensure(handle, *layout);
		object.present = true;

		for (const RepChange& change : changes) {
			if (change.handle >= object.values.size())
				return false;
			if (change.value.size() != layout->fields[change.handle].size)
				return false;
			object.values[change.handle] = change.value;
		}

		return true;
	}

	void SnapshotView::writeDiscrete(ViewObject& object) {
		if (!object.component || !object.layout)
			return;

		const RepLayout& layout = *object.layout;
		for (size_t i = 0; i < layout.fields.size(); ++i) {
			const RepField& field = layout.fields[i];
			if (field.interp || object.values[i].empty())
				continue;
			std::memcpy(field.getter(object.component), object.values[i].data(), field.size);
		}
	}

	void SnapshotView::closeTick(Scene& scene, uint32_t tickId, uint32_t timeMs) {
		for (auto& entry : m_objects) {
			ViewObject& object = entry.second;
			if (!object.layout)
				continue;

			if (!object.component && object.present)
				resolve(object, scene);

			// One sample per object per tick, whether or not it changed. An
			// unchanged object repeats its previous value, which keeps the timeline
			// uniform and every interpolation interval one tick wide.
			object.head = (object.head + 1) % kHistory;
			Sample& sample = object.ring[object.head];
			sample.tick = tickId;
			sample.timeMs = timeMs;

			const RepLayout& layout = *object.layout;
			for (size_t i = 0; i < layout.fields.size(); ++i) {
				if (!layout.fields[i].interp) {
					sample.fields[i].clear();
					continue;
				}
				const Bytes& src = object.values[i];
				sample.fields[i].assign(src.begin(), src.end());
			}

			if (object.count < kHistory)
				++object.count;

			if (object.present && object.component)
				writeDiscrete(object);

			object.present = false;
		}
	}

	SnapshotView::InterpStats SnapshotView::interpolate(Scene& scene, double renderTimeMs) {
		InterpStats stats;

		for (auto& entry : m_objects) {
			ViewObject& object = entry.second;
			if (object.count == 0 || !object.layout)
				continue;
			if (!object.component)
				resolve(object, scene);
			if (!object.component)
				continue;

			const int newestIdx = object.head;
			const int oldestIdx = (object.head - (object.count - 1) + kHistory) % kHistory;
			const Sample& newest = object.ring[newestIdx];
			const Sample& oldest = object.ring[oldestIdx];

			const Sample* a = nullptr;
			const Sample* b = nullptr;

			if (renderTimeMs >= (double)newest.timeMs) {
				a = &newest;
				b = &newest;
				++stats.late;
			}
			else if (renderTimeMs <= (double)oldest.timeMs) {
				a = &oldest;
				b = &oldest;
				++stats.starved;
			}
			else {
				for (int i = 0; i + 1 < object.count; ++i) {
					const Sample& sa = object.ring[(oldestIdx + i) % kHistory];
					const Sample& sb = object.ring[(oldestIdx + i + 1) % kHistory];
					if (renderTimeMs <= (double)sb.timeMs) {
						a = &sa;
						b = &sb;
						break;
					}
				}
				if (!a) {
					a = &newest;
					b = &newest;
					++stats.late;
				}
			}

			float alpha = 1.0f;
			if (a != b) {
				const double span = (double)b->timeMs - (double)a->timeMs;
				if (span <= 0.0 || span > kMaxInterpSpanMs)
					alpha = 1.0f;
				else
					alpha = (float)((renderTimeMs - (double)a->timeMs) / span);
			}

			const RepLayout& layout = *object.layout;
			for (size_t i = 0; i < layout.fields.size(); ++i) {
				const RepField& field = layout.fields[i];
				if (!field.interp)
					continue;

				const Bytes& va = a->fields[i];
				const Bytes& vb = b->fields[i];
				if (va.empty() && vb.empty())
					continue;

				void* dst = field.getter(object.component);
				if (vb.empty())
					std::memcpy(dst, va.data(), field.size);
				else if (va.empty() || alpha >= 1.0f)
					std::memcpy(dst, vb.data(), field.size);
				else
					field.interp(dst, va.data(), vb.data(), alpha);
			}
		}

		return stats;
	}

}
