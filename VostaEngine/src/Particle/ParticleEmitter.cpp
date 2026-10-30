#include "vepch.h"
#include "ParticleEmitter.h"

#include "Core/Log.h"
#include "VM.h"

namespace ve {

	namespace {

		constexpr uint64_t kInitialRngState = 0x2545F4914F6CDD1Dull;

		void resolveParamValue(const EmitterAsset& asset, Stage stage, const ParamKey& key, float* out) {
			for (int c = 0; c < 4; ++c)
				out[c] = 0.0f;

			for (const ModuleEntry& entry : asset.stages[static_cast<uint32_t>(stage)]) {
				if (entry.guid.a != key.guid.a || entry.guid.b != key.guid.b)
					continue;
				if (!entry.module)
					return;

				const ModuleParamDecl* decl = findParamDecl(*entry.module, key.name);
				if (!decl)
					return;

				const uint16_t offset = paramComponentOffset(*entry.module, key.name);
				for (uint8_t c = 0; c < decl->comps; ++c) {
					const size_t index = static_cast<size_t>(offset) + c;
					out[c] = index < entry.params.size() ? entry.params[index] : decl->value[c];
				}
				return;
			}
		}

	}

	bool ParticleEmitterInstance::initialize(const EmitterAsset& asset) {
		m_compiled = asset.compiled;
		if (!m_compiled) {
			VE_CORE_ERROR_PRINT("ParticleEmitterInstance: asset has no compiled program");
			return false;
		}

		const uint32_t particleWidth = asset.particleLayout.totalComponentsCount();
		for (ParticleDataSet& set : m_particles) {
			set.stride = particleWidth;
			set.num = 0;
			set.ensureCapacity(1);
			set.zeroAll();
		}

		const uint32_t emitterWidth = asset.emitterLayout.totalComponentsCount();
		m_emitter.stride = emitterWidth;
		m_emitter.capacity = 1;
		m_emitter.num = 1;
		m_emitter.data.assign(emitterWidth, 0.0f);

		m_cur = 0;
		m_spawnDone = false;
		m_emitterAge = 0.0f;
		m_rng = kInitialRngState;

		rebuildParamBlock(asset);
		return true;
	}

	void ParticleEmitterInstance::tick(float dt) {
		if (!m_compiled)
			return;

		if (!m_spawnDone) {
			initializeEmitter();
			m_spawnDone = true;
		}

		m_emitterAge += dt;
		updateEmitter(dt);
		updateParticle(dt);
		initializeParticle(dt);
		commit();
	}

	bool ParticleEmitterInstance::applyParams(const EmitterAsset& asset) {
		if (!m_compiled || !asset.compiled)
			return false;
		if (m_compiled->structureHash != asset.compiled->structureHash)
			return false;

		rebuildParamBlock(asset);
		return true;
	}

	void ParticleEmitterInstance::reset() {
		for (ParticleDataSet& set : m_particles) {
			set.num = 0;
			set.zeroAll();
		}
		m_emitter.zeroAll();
		m_cur = 0;
		m_spawnDone = false;
		m_emitterAge = 0.0f;
		m_rng = kInitialRngState;
	}

	void ParticleEmitterInstance::initializeEmitter() {
		m_emitter.zeroAll();
		runEmitterStage(Stage::EmitterSpawn, 0.0f);
	}

	void ParticleEmitterInstance::updateEmitter(float dt) {
		runEmitterStage(Stage::EmitterUpdate, dt);
	}

	void ParticleEmitterInstance::updateParticle(float dt) {
		ParticleDataSet& src = m_particles[m_cur];
		ParticleDataSet& dst = m_particles[1 - m_cur];
		const Program* program = stageProgram(Stage::ParticleUpdate);

		dst.ensureCapacity(src.num);
		dst.num = 0;

		uint32_t out = 0;
		for (uint32_t p = 0; p < src.num; ++p) {
			dst.copyInstance(src, p, out);

			if (program) {
				ExecContext context;
				context.program = program;
				context.src = &src;
				context.dst = &dst;
				context.srcIndex = p;
				context.dstIndex = out;
				context.dt = dt;
				context.paramBlock = m_paramBlock[static_cast<uint32_t>(Stage::ParticleUpdate)].data();
				if (!VM::run(context))
					continue;
			}

			++out;
		}

		dst.num = out;
	}

	void ParticleEmitterInstance::initializeParticle(float dt) {
		const Program* program = stageProgram(Stage::ParticleSpawn);
		if (!program)
			return;

		ParticleDataSet& dst = m_particles[1 - m_cur];

		SpawnInfo infos[kMaxSpawnSlots];
		uint32_t total = 0;
		for (uint32_t slot = 0; slot < kMaxSpawnSlots; ++slot) {
			infos[slot] = readSpawnInfo(slot);
			if (infos[slot].count > 0)
				total += static_cast<uint32_t>(infos[slot].count);
		}

		const uint32_t room = dst.num < kMaxParticles ? kMaxParticles - dst.num : 0;
		total = std::min(total, room);
		dst.ensureCapacity(dst.num + total);

		for (uint32_t slot = 0; slot < kMaxSpawnSlots; ++slot) {
			const SpawnInfo& info = infos[slot];
			for (uint32_t j = 0; j < static_cast<uint32_t>(info.count); ++j) {
				if (dst.num >= kMaxParticles)
					return;

				const uint32_t index = dst.num;
				dst.zeroInstance(index);

				ExecContext context;
				context.program = program;
				context.src = &dst;
				context.dst = &dst;
				context.srcIndex = index;
				context.dstIndex = index;
				context.dt = dt;
				context.spawnTime = m_emitterAge - info.interpStartDt - static_cast<float>(j) * info.intervalDt;
				context.spawnIndex = j;
				context.spawnCount = static_cast<uint32_t>(info.count);
				context.spawnGroup = static_cast<uint32_t>(info.spawnGroup);
				context.spawnRandom = nextSpawnRandom();
				context.paramBlock = m_paramBlock[static_cast<uint32_t>(Stage::ParticleSpawn)].data();
				if (VM::run(context))
					++dst.num;
			}
		}
	}

	void ParticleEmitterInstance::commit() {
		m_cur = 1 - m_cur;
	}

	void ParticleEmitterInstance::rebuildParamBlock(const EmitterAsset& asset) {
		for (uint32_t s = 0; s < kStageCount; ++s) {
			std::vector<float>& block = m_paramBlock[s];
			block.clear();

			const Program* program = stageProgram(static_cast<Stage>(s));
			if (!program)
				continue;

			uint32_t width = 0;
			for (const ProgramParam& param : program->params)
				width = std::max(width, static_cast<uint32_t>(param.offset) + param.comps);
			block.assign(width, 0.0f);

			for (const ProgramParam& param : program->params) {
				float value[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				resolveParamValue(asset, static_cast<Stage>(s), param.key, value);
				for (uint8_t c = 0; c < param.comps; ++c)
					block[param.offset + c] = value[c];
			}
		}
	}

	float ParticleEmitterInstance::nextSpawnRandom() {
		// SplitMix64: cheap, well-distributed, and trivially replayable.
		uint64_t z = (m_rng += 0x9E3779B97F4A7C15ull);
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
		z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
		z ^= z >> 31;
		return static_cast<float>(z >> 40) * (1.0f / 16777216.0f);
	}

	SpawnInfo ParticleEmitterInstance::readSpawnInfo(uint32_t slot) const {
		SpawnInfo info;
		const SpawnInfoOffsets& offsets = m_compiled->spawnOffsets[slot];
		if (!offsets.valid)
			return info;

		const float rawCount = *m_emitter.attr(offsets.count, 0);
		if (rawCount > 0.0f)
			info.count = static_cast<int32_t>(std::min(rawCount, static_cast<float>(kMaxParticles)));
		info.interpStartDt = *m_emitter.attr(offsets.interpStartDt, 0);
		info.intervalDt = *m_emitter.attr(offsets.intervalDt, 0);
		info.spawnGroup = static_cast<int32_t>(*m_emitter.attr(offsets.spawnGroup, 0));
		return info;
	}

	void ParticleEmitterInstance::runEmitterStage(Stage stage, float dt) {
		const Program* program = stageProgram(stage);
		if (!program)
			return;

		ExecContext context;
		context.program = program;
		context.src = &m_emitter;
		context.dst = &m_emitter;
		context.srcIndex = 0;
		context.dstIndex = 0;
		context.dt = dt;
		context.paramBlock = m_paramBlock[static_cast<uint32_t>(stage)].data();
		VM::run(context);
	}

	const Program* ParticleEmitterInstance::stageProgram(Stage stage) const {
		return m_compiled ? m_compiled->stages[static_cast<uint32_t>(stage)].get() : nullptr;
	}

}
