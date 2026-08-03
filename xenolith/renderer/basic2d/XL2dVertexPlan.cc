/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#include "XL2dVertexPlan.h"
#include "XLCoreObject.h"
#include "XLLinearGradient.h"
#include "SPFont.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

// Dispatch one command of the frame's list. CommandGroup carries no geometry of its own.
void VertexPlan::pushCommand(Context &ctx, const Command *cmd) {
	switch (cmd->type) {
	case CommandType::CommandGroup: break;
	case CommandType::VertexArray:
		pushVertexData(ctx, cmd, reinterpret_cast<const CmdVertexArray *>(cmd->data));
		break;
	case CommandType::Deferred:
		pushDeferred(ctx, cmd, reinterpret_cast<const CmdDeferred *>(cmd->data));
		break;
	case CommandType::ParticleEmitter:
		pushParticleEmitter(ctx, cmd, reinterpret_cast<const CmdParticleEmitter *>(cmd->data));
		break;
	}
}

VertexPlan::StatePlanInfo *VertexPlan::acquireStatePlan(
		FrameContextHandle2d *input, const core::Material *material,
		Map<core::MaterialId, MaterialWritePlan> &writePlan, const CmdInfo *cmd) {
	auto materialIt = writePlan.find(cmd->material);
	if (materialIt == writePlan.end()) {
		if (material) {
			materialIt = writePlan.emplace(cmd->material, MaterialWritePlan()).first;
			materialIt->second.material = material;
			if (auto atlas = materialIt->second.material->getAtlas()) {
				materialIt->second.atlas = atlas;
			}
		}
	}

	if (materialIt != writePlan.end() && materialIt->second.material) {
		auto stateIt = materialIt->second.states.find(cmd->state);
		if (stateIt == materialIt->second.states.end()) {
			stateIt = materialIt->second.states.emplace(cmd->state, StatePlanInfo()).first;

			if (cmd->state != StateIdNone) {
				auto state = input->getState(cmd->state);
				if (state) {
					auto stateData =
							dynamic_cast<StateData *>(state->data ? state->data.get() : nullptr);

					if (stateData) {
						stateIt->second.stateData = stateData;
						if (stateData->gradient) {
							globalWritePlan.vertexes += stateData->gradient->steps.size() + 2;
						}
					}
				}
			}
		}

		auto pathsIt = paths.find(cmd->zPath);
		if (pathsIt == paths.end()) {
			paths.emplace(cmd->zPath, 0.0f);
		}

		return &stateIt->second;
	}
	return nullptr;
}
void VertexPlan::emplaceWritePlan(FrameContextHandle2d *input,
		const core::Material *material, Map<core::MaterialId, MaterialWritePlan> &writePlan,
		const Command *c, const CmdInfo *cmd, SpanView<InstanceVertexData> vertexes) {
	auto statePlan = acquireStatePlan(input, material, writePlan, cmd);

	if (statePlan) {
		InstanceVertexData *packedStart = const_cast<InstanceVertexData *>(vertexes.data());
		size_t packedCommands = 0;

		for (auto &vIt : vertexes) {
			if (!vIt.data) {
				continue;
			}

			// count data objects
			globalWritePlan.vertexes += vIt.data->data.size();
			globalWritePlan.indexes += vIt.data->indexes.size();

			if (vIt.sdfIndexes > 0) {
				globalWritePlan.indexes += (vIt.sdfIndexes + vIt.fillIndexes);
			}

			if ((c->flags & CommandFlags::DoNotCount) != CommandFlags::None) {
				excludeVertexes += vIt.data->data.size();
				excludeIndexes += vIt.data->indexes.size();
			}

			maxShadowValue = sprt::max(maxShadowValue, cmd->depthValue);

			bool drawAsInstances = false;
			if (vIt.instances.size() > 1) {
				drawAsInstances = true;
			}

			// pack non-instanced blocks
			if (drawAsInstances) {
				globalWritePlan.transforms += vIt.instances.size();

				if (packedCommands > 0) {
					// write packed blocks
					auto vertexData = new (pool) VertexDataPlanInfo;
					vertexData->next = statePlan->packed;
					vertexData->vertexes = makeSpanView(packedStart, packedCommands);
					vertexData->zOrder = cmd->zPath;
					vertexData->depthValue = cmd->depthValue;
					vertexData->order = orderCounter++;
					statePlan->packed = vertexData;
				}

				auto vertexData = new (pool) VertexDataPlanInfo;
				vertexData->next = statePlan->instanced;
				vertexData->vertexes = makeSpanView(&vIt, 1);
				vertexData->zOrder = cmd->zPath;
				vertexData->depthValue = cmd->depthValue;
				vertexData->order = orderCounter++;
				statePlan->instanced = vertexData;

				packedCommands = 0;
				packedStart = const_cast<InstanceVertexData *>(&vIt + 1);
			} else {
				++globalWritePlan.transforms;
				++packedCommands;
			}
		}

		if (packedCommands > 0) {
			// write packed blocks
			auto vertexData = new (pool) VertexDataPlanInfo;
			vertexData->next = statePlan->packed;
			vertexData->vertexes = makeSpanView(packedStart, packedCommands);
			vertexData->zOrder = cmd->zPath;
			vertexData->depthValue = cmd->depthValue;
			vertexData->order = orderCounter++;
			statePlan->packed = vertexData;
		}
	}
}

void VertexPlan::pushVertexData(Context &ctx, const Command *c,
		const CmdVertexArray *cmd) {
	auto material = ctx.getMaterialById(cmd->material);
	if (!material) {
		return;
	}

	if (ctx.collectDamage) {
		for (auto &iv : cmd->vertexes) { ctx.damage->addInstances(c, cmd, iv); }
	}

	if (material->getPipeline()->isSolid()) {
		emplaceWritePlan(ctx.input, material, solidWritePlan, c, cmd, cmd->vertexes);
	} else if (cmd->renderingLevel == RenderingLevel::Surface) {
		emplaceWritePlan(ctx.input, material, surfaceWritePlan, c, cmd, cmd->vertexes);
	} else {
		auto v = transparentWritePlan.find(cmd->zPath);
		if (v == transparentWritePlan.end()) {
			v = transparentWritePlan.emplace(cmd->zPath, Map<core::MaterialId, MaterialWritePlan>())
						.first;
		}
		emplaceWritePlan(ctx.input, material, v->second, c, cmd, cmd->vertexes);
	}
};

void VertexPlan::applyNormalized(SpanView<InstanceVertexData> &vertexes,
		const CmdDeferred *cmd) {
	// apply transforms;
	if (cmd->normalized) {
		for (auto &it : vertexes) {
			if (it.instances.size() > 0) {
				const_cast<SpanView<TransformData> &>(it.instances) = it.instances.pdup();
			} else {
				TransformData instance;
				instance.transform = Mat4::IDENTITY;
				const_cast<SpanView<TransformData> &>(it.instances) =
						makeSpanView(&instance, 1).pdup();
			}
			for (auto &inst : it.instances) {
				auto modelTransform = cmd->modelTransform * inst.transform;

				Mat4 newMV;
				newMV.m[12] = sprt::floor(modelTransform.m[12]);
				newMV.m[13] = sprt::floor(modelTransform.m[13]);
				newMV.m[14] = sprt::floor(modelTransform.m[14]);

				const_cast<TransformData &>(inst).transform = cmd->viewTransform * newMV;
			}
		}
	} else {
		for (auto &it : vertexes) {
			if (it.instances.size() > 0) {
				const_cast<SpanView<TransformData> &>(it.instances) = it.instances.pdup();
			} else {
				TransformData instance;
				instance.transform = Mat4::IDENTITY;
				const_cast<SpanView<TransformData> &>(it.instances) =
						makeSpanView(&instance, 1).pdup();
			}
			for (auto &inst : it.instances) {
				const_cast<TransformData &>(inst).transform =
						cmd->viewTransform * cmd->modelTransform * inst.transform;
			}
		}
	}
}

void VertexPlan::pushDeferred(Context &ctx, const Command *c,
		const CmdDeferred *cmd) {
	auto material = ctx.getMaterialById(cmd->material);
	if (!material) {
		return;
	}

	if (!cmd->deferred->isWaitOnReady()) {
		if (!cmd->deferred->isReady()) {
			return;
		}
	}

	SpanView<InstanceVertexData> storedVertexes;

	cmd->deferred->acquireResult(
			[&](SpanView<InstanceVertexData> vertexes, DeferredVertexResult::Flags flags) {
		auto v = vertexes.pdup();
		applyNormalized(v, cmd);
		storedVertexes = v;
	});

	// the result is resolved by now, so a deferred command is bounded exactly like an immediate one
	if (ctx.collectDamage) {
		for (auto &iv : storedVertexes) { ctx.damage->addInstances(c, cmd, iv); }
	}

	if (cmd->renderingLevel == RenderingLevel::Solid) {
		emplaceWritePlan(ctx.input, material, solidWritePlan, c, cmd, storedVertexes);
	} else if (cmd->renderingLevel == RenderingLevel::Surface) {
		emplaceWritePlan(ctx.input, material, surfaceWritePlan, c, cmd, storedVertexes);
	} else {
		auto v = transparentWritePlan.find(cmd->zPath);
		if (v == transparentWritePlan.end()) {
			v = transparentWritePlan.emplace(cmd->zPath, Map<core::MaterialId, MaterialWritePlan>())
						.first;
		}
		emplaceWritePlan(ctx.input, material, v->second, c, cmd, storedVertexes);
	}
}

void VertexPlan::pushParticleEmitter(Context &ctx, const Command *c,
		const CmdParticleEmitter *cmd) {
	if (flatOrder) {
		// FlatPass has no particle compute pass - drop the command instead of emitting a draw
		// span that would reference a missing emitter attachment
		return;
	}

	auto material = ctx.getMaterialById(cmd->material);
	if (!material) {
		return;
	}

	if (ctx.collectDamage) {
		// particles are simulated on the GPU and carry no CPU geometry at all, so they can be
		// neither versioned nor bounded: the whole frame has to be treated as damaged
		ctx.damage->escalate();
	}

	auto emplacePlan = [&](Map<core::MaterialId, MaterialWritePlan> &writePlan) {
		auto statePlan = acquireStatePlan(ctx.input, material, writePlan, cmd);
		if (statePlan) {
			statePlan->particles.emplace_back(cmd);
		}
	};

	if (material->getPipeline()->isSolid()) {
		emplacePlan(solidWritePlan);
	} else if (cmd->renderingLevel == RenderingLevel::Surface) {
		emplacePlan(surfaceWritePlan);
	} else {
		auto v = transparentWritePlan.find(cmd->zPath);
		if (v == transparentWritePlan.end()) {
			v = transparentWritePlan.emplace(cmd->zPath, Map<core::MaterialId, MaterialWritePlan>())
						.first;
		}
		emplacePlan(v->second);
	}
}

void VertexPlan::updatePathsDepth() {
	float depthScale = 1.0f / float(paths.size() + 1);
	float depthOffset = 1.0f - depthScale;
	for (auto &it : paths) {
		it.second = depthOffset;
		depthOffset -= depthScale;
	}
}

void VertexPlan::pushInitial(WriteTarget &writeTarget) {
	if (writeTarget.transform) {
		TransformData nullTransforml;
		nullTransforml.offset = Vec4::ZERO;
		sprt::memcpy(writeTarget.transform, &nullTransforml, sizeof(TransformData));
		++writeTarget.transtormOffset;
	}

	if (writeTarget.indexes) {
		Vector<uint32_t> indexes{0, 2, 1, 0, 3, 2, 4, 6, 5, 4, 7, 6};
		sprt::memcpy(writeTarget.indexes, indexes.data(), indexes.size() * sizeof(uint32_t));
		writeTarget.indexOffset += indexes.size();
	}

	if (writeTarget.vertexes) {
		Vector<Vertex> vertexes{// full screen quad data
			Vertex{Vec4(-1.0f, -1.0f, 0.0f, 1.0f), Vec4::ONE, Vec2::ZERO, 0, 0},
			Vertex{Vec4(-1.0f, 1.0f, 0.0f, 1.0f), Vec4::ONE, Vec2::UNIT_Y, 0, 0},
			Vertex{Vec4(1.0f, 1.0f, 0.0f, 1.0f), Vec4::ONE, Vec2::ONE, 0, 0},
			Vertex{Vec4(1.0f, -1.0f, 0.0f, 1.0f), Vec4::ONE, Vec2::UNIT_X, 0, 0},

			// shadow quad data
			Vertex{Vec4(-1.0f, -1.0f, 0.0f, 1.0f), Vec4::ONE, Vec2(0.0f, 1.0f - shadowSize.y), 0,
				0},
			Vertex{Vec4(-1.0f, 1.0f, 0.0f, 1.0f), Vec4::ONE, Vec2(0.0f, 1.0f), 0, 0},
			Vertex{Vec4(1.0f, 1.0f, 0.0f, 1.0f), Vec4::ONE, Vec2(shadowSize.x, 1.0f), 0, 0},
			Vertex{Vec4(1.0f, -1.0f, 0.0f, 1.0f), Vec4::ONE,
				Vec2(shadowSize.x, 1.0f - shadowSize.y), 0, 0}};

		switch (core::getPureTransform(transform)) {
		case core::SurfaceTransformFlags::Rotate90:
			vertexes[0].tex = Vec2::UNIT_Y;
			vertexes[1].tex = Vec2::ONE;
			vertexes[2].tex = Vec2::UNIT_X;
			vertexes[3].tex = Vec2::ZERO;
			vertexes[4].tex = Vec2(0.0f, shadowSize.y);
			vertexes[5].tex = shadowSize;
			vertexes[6].tex = Vec2(shadowSize.x, 0.0f);
			vertexes[7].tex = Vec2::ZERO;
			break;
		case core::SurfaceTransformFlags::Rotate180:
			vertexes[0].tex = Vec2::ONE;
			vertexes[1].tex = Vec2::UNIT_X;
			vertexes[2].tex = Vec2::ZERO;
			vertexes[3].tex = Vec2::UNIT_Y;
			vertexes[4].tex = shadowSize;
			vertexes[5].tex = Vec2(shadowSize.x, 0.0f);
			vertexes[6].tex = Vec2::ZERO;
			vertexes[7].tex = Vec2(0.0f, shadowSize.y);
			break;
		case core::SurfaceTransformFlags::Rotate270:
			vertexes[0].tex = Vec2::UNIT_X;
			vertexes[1].tex = Vec2::ZERO;
			vertexes[2].tex = Vec2::UNIT_Y;
			vertexes[3].tex = Vec2::ONE;
			vertexes[4].tex = Vec2(shadowSize.x, 0.0f);
			vertexes[5].tex = Vec2::ZERO;
			vertexes[6].tex = Vec2(0.0f, shadowSize.y);
			vertexes[7].tex = shadowSize;
			break;
		default: break;
		}

		sprt::memcpy(writeTarget.vertexes, vertexes.data(), vertexes.size() * sizeof(Vertex));
		writeTarget.vertexOffset += vertexes.size();
	}
}

void VertexPlan::pushPlanVertexes(WriteTarget &writeTarget,
		Map<core::MaterialId, MaterialWritePlan> &writePlan) {
	auto pushVertexes = [&](core::MaterialId materialId, const MaterialWritePlan &plan,
								uint32_t transform, const InstanceVertexData &vertexes) {
		auto target = reinterpret_cast<Vertex *>(writeTarget.vertexes) + writeTarget.vertexOffset;
		sprt::memcpy(target, vertexes.data->data.data(),
				vertexes.data->data.size() * sizeof(Vertex));

		size_t idx = 0;
		if (plan.atlas) {
			if (hasGpuSideAtlases) {
				for (; idx < vertexes.data->data.size(); ++idx) {
					target[idx].material = materialId | transform << 16;
				}
			} else {
				auto ext = plan.atlas->getImageExtent();
				float atlasScaleX = 1.0f / ext.width;
				float atlasScaleY = 1.0f / ext.height;

				for (; idx < vertexes.data->data.size(); ++idx) {
					auto &t = target[idx];
					t.material = materialId | transform << 16;

					struct AtlasData {
						Vec2 pos;
						Vec2 tex;
					};

					if (auto d = reinterpret_cast<const AtlasData *>(
								plan.atlas->getObjectByName(t.object))) {
						t.pos += Vec4(d->pos.x, d->pos.y, 0, 0);
						t.tex = d->tex;
						t.object = 0;
					} else {
#if DEBUG
						log::source().warn("VertexMaterialDrawPlan", "Object not found: ", t.object,
								" ", string::toUtf8<Interface>(char16_t(t.object)));
#endif
						auto anchor = font::CharId::getAnchorForChar(t.object);
						switch (anchor) {
						case font::CharAnchor::BottomLeft:
							t.tex = Vec2(1.0f - atlasScaleX, 0.0f);
							break;
						case font::CharAnchor::TopLeft:
							t.tex = Vec2(1.0f - atlasScaleX, 0.0f + atlasScaleY);
							break;
						case font::CharAnchor::TopRight:
							t.tex = Vec2(1.0f, 0.0f + atlasScaleY);
							break;
						case font::CharAnchor::BottomRight: t.tex = Vec2(1.0f, 0.0f); break;
						}
					}
				}
			}
		} else {
			for (; idx < vertexes.data->data.size(); ++idx) {
				//target[idx].pos = transform.transform * target[idx].pos * transform.mask + transform.offset;
				target[idx].material = materialId | transform << 16;
			}
		}

		writeTarget.vertexOffset += vertexes.data->data.size();
	};

	auto writeTransform = [&](const TransformData &inst, float zOffset, float depthValue,
								  const StateData *stateData, uint32_t preTransform) -> uint32_t {
		auto ret = preTransform ? preTransform : writeTarget.transtormOffset;
		auto instanceTarget = writeTarget.transform + ret;
		sprt::memcpy(instanceTarget, &inst, sizeof(TransformData));
		instanceTarget->offset.z = zOffset;
		instanceTarget->shadowValue = depthValue;
		if (stateData) {
			instanceTarget->outlineColor = stateData->outlineColor;
			instanceTarget->outlineOffset = stateData->outlineOffset;
		} else {
			instanceTarget->outlineOffset = 0.0f;
		}

		if (!preTransform) {
			++writeTarget.transtormOffset;
		}
		return ret;
	};

	auto pushVertexList = [&](core::MaterialId mId, MaterialWritePlan &plan,
								  const StatePlanInfo &state, VertexDataPlanInfo *packedInstance,
								  bool instances) {
		while (packedInstance) {
			packedInstance->vertexOffset = writeTarget.vertexOffset;

			// used as firstInstance for instanced drawing to access transform array
			packedInstance->transformOffset = writeTarget.transtormOffset;

			float zOffset = 0.0f;
			float depthValue = 0.0f;
			auto pathIt = paths.find(packedInstance->zOrder);
			if (pathIt != paths.end()) {
				zOffset = pathIt->second;
			}

			if (packedInstance->depthValue > 0.0f) {
				auto f16 = sprt::halffloat::encode(packedInstance->depthValue);
				auto value = sprt::halffloat::decode(f16);
				depthValue = value;
			}

			for (auto &iit : packedInstance->vertexes) {
				if (instances) {
					for (auto &inst : iit.instances) {
						writeTransform(inst, zOffset, depthValue, state.stateData, 0);
					}

					pushVertexes(mId, plan, 0, iit);
				} else {
					auto transform = writeTransform(iit.instances.front(), zOffset, depthValue,
							state.stateData, 0);
					pushVertexes(mId, plan, transform, iit);
				}
			}

			packedInstance->vertexCount = writeTarget.vertexOffset - packedInstance->vertexOffset;
			packedInstance->transformCount =
					writeTarget.transtormOffset - packedInstance->transformOffset;

			packedInstance = packedInstance->next;
		}
	};

	for (auto &plan : writePlan) {
		for (auto &state : plan.second.states) {
			// write gradient vertexes (2 + n: start, end, anchors)
			if (state.second.stateData && state.second.stateData->gradient) {
				auto target =
						reinterpret_cast<Vertex *>(writeTarget.vertexes) + writeTarget.vertexOffset;

				Vec2 start =
						state.second.stateData->transform * state.second.stateData->gradient->start;
				Vec2 end =
						state.second.stateData->transform * state.second.stateData->gradient->end;

				start.y = surfaceExtent.height - start.y;
				end.y = surfaceExtent.height - end.y;

				Vec2 norm = end - start;

				float d = norm.y * norm.y / (norm.x * norm.x + norm.y * norm.y);

				Vec2 axisAngle;
				if (sprt::abs(norm.y) > sprt::abs(norm.x)) {
					axisAngle.x = sprt::copysign(norm.length(), norm.y);
					axisAngle.y = d;
				} else {
					axisAngle.x = sprt::copysign(norm.length(), norm.x);
					axisAngle.y = d;
				}

				target->pos = Vec4(start, 0.0f, 0.0f);
				target->tex = axisAngle;
				++target;

				target->pos = Vec4(end, 1.0f, 0.0f);
				target->tex = axisAngle;
				++target;

				for (auto &it : state.second.stateData->gradient->steps) {
					target->pos = Vec4(math::lerp(start, end, it.value), it.value, it.factor);
					target->tex = axisAngle;
					target->color = Vec4(it.color.r, it.color.g, it.color.b, it.color.a);
					++target;
				}

				state.second.gradientStart = writeTarget.vertexOffset;
				state.second.gradientCount =
						uint32_t(state.second.stateData->gradient->steps.size());

				writeTarget.vertexOffset += state.second.stateData->gradient->steps.size() + 2;
			}

			pushVertexList(plan.first, plan.second, state.second, state.second.instanced, true);
			pushVertexList(plan.first, plan.second, state.second, state.second.packed, false);

			for (auto &it : state.second.particles) {
				TransformData inst(it->transform);

				float zOffset = 0.0f;
				float depthValue = 0.0f;
				auto pathIt = paths.find(it->zPath);
				if (pathIt != paths.end()) {
					zOffset = pathIt->second;
				}

				if (it->depthValue > 0.0f) {
					auto f16 = sprt::halffloat::encode(it->depthValue);
					auto value = sprt::halffloat::decode(f16);
					depthValue = value;
				}

				writeTransform(inst, zOffset, depthValue, state.second.stateData,
						it->transformIndex);
			}
		}
	}
}

void VertexPlan::drawWritePlan(Context &ctx, WriteTarget &writeTarget,
		Map<core::MaterialId, MaterialWritePlan> &writePlan) {
	// optimize draw order, minimize switching pipeline, textureSet and descriptors
	Vector<const sprt::pair<const core::MaterialId, MaterialWritePlan> *> drawOrder;

	// optimize pipeline switching strategy
	for (auto &it : writePlan) {
		if (drawOrder.empty()) {
			drawOrder.emplace_back(&it);
		} else {
			auto lb = sprt::lower_bound(drawOrder.begin(), drawOrder.end(), &it,
					[](const sprt::pair<const core::MaterialId, MaterialWritePlan> *l,
							const sprt::pair<const core::MaterialId, MaterialWritePlan> *r) {
				if (l->second.material->getPipeline() != r->second.material->getPipeline()) {
					return core::GraphicPipeline::comparePipelineOrdering(
							*l->second.material->getPipeline(), *r->second.material->getPipeline());
				} else if (l->second.material->getLayoutIndex()
						!= r->second.material->getLayoutIndex()) {
					return l->second.material->getLayoutIndex()
							< r->second.material->getLayoutIndex();
				} else {
					return l->first < r->first;
				}
			});
			if (lb == drawOrder.end()) {
				drawOrder.emplace_back(&it);
			} else {
				drawOrder.emplace(lb, &it);
			}
		}
	}

	auto writeIndexes = [](uint32_t *indexTarget, const uint32_t *indexSource, uint32_t indexCount,
								uint32_t vertexOffset) {
		if (vertexOffset == 0) {
			sprt::memcpy(indexTarget, indexSource, indexCount * sizeof(uint32_t));
		} else {
			for (size_t i = 0; i < indexCount; ++i) {
				*(indexTarget++) = *(indexSource++) + vertexOffset;
			}
		}
		return indexCount;
	};

	enum StatePlanPhase {
		StatePlanGeneral,
		StatePlanShadowSolid,
		StatePlanShadowVolumes
	};

	auto processStatePlanIndexes = [&](const InstanceVertexData &vertexes, StatePlanPhase phase,
										   uint32_t localVertexOffset) {
		switch (phase) {
		case StatePlanGeneral:
			writeTarget.indexOffset += writeIndexes(
					reinterpret_cast<uint32_t *>(writeTarget.indexes) + writeTarget.indexOffset,
					vertexes.data->indexes.data(),
					uint32_t(vertexes.data->indexes.size() - vertexes.sdfIndexes),
					localVertexOffset);
			break;
		case StatePlanShadowSolid:
			if (vertexes.sdfIndexes > 0 && vertexes.fillIndexes > 0) {
				writeTarget.indexOffset += writeIndexes(
						reinterpret_cast<uint32_t *>(writeTarget.indexes) + writeTarget.indexOffset,
						vertexes.data->indexes.data(), vertexes.fillIndexes, localVertexOffset);
			}
			break;
		case StatePlanShadowVolumes:
			if (vertexes.sdfIndexes > 0) {
				writeTarget.indexOffset += writeIndexes(
						reinterpret_cast<uint32_t *>(writeTarget.indexes) + writeTarget.indexOffset,
						vertexes.data->indexes.data() + vertexes.fillIndexes
								+ vertexes.strokeIndexes,
						vertexes.sdfIndexes, localVertexOffset);
			}
			break;
		}
	};

	auto processStatePlan = [&](core::MaterialId materialId, StateId stateId,
									const StatePlanInfo &statePlan, StatePlanPhase phase,
									mem_std::Vector<VertexSpan> &target) {
		size_t localVertexOffset = 0;
		auto materialIndexes = writeTarget.indexOffset;

		auto packedInstance = statePlan.instanced;
		while (packedInstance) {
			for (auto &vertexes : packedInstance->vertexes) {
				processStatePlanIndexes(vertexes, phase, 0);
				if (writeTarget.indexOffset > materialIndexes) {
					target.emplace_back(VertexSpan{.material = materialId,
						.indexCount = writeTarget.indexOffset - materialIndexes,
						.instanceCount = packedInstance->transformCount,
						.firstIndex = materialIndexes,
						.vertexOffset = packedInstance->vertexOffset,
						.firstInstance = packedInstance->transformOffset,
						.state = stateId,
						.gradientOffset = statePlan.gradientStart,
						.gradientCount = statePlan.gradientCount,
						.outlineOffset =
								(statePlan.stateData ? statePlan.stateData->outlineOffset : 0.0f)});
				}
				materialIndexes = writeTarget.indexOffset;
			}
			packedInstance = packedInstance->next;
		}

		materialIndexes = writeTarget.indexOffset;
		packedInstance = statePlan.packed;
		while (packedInstance) {
			for (auto &vertexes : packedInstance->vertexes) {
				processStatePlanIndexes(vertexes, phase, uint32_t(localVertexOffset));
				localVertexOffset += vertexes.data->data.size();
			}
			packedInstance = packedInstance->next;
		}

		if (writeTarget.indexOffset > materialIndexes) {
			target.emplace_back(VertexSpan{.material = materialId,
				.indexCount = writeTarget.indexOffset - materialIndexes,
				.instanceCount = 1,
				.firstIndex = materialIndexes,
				.vertexOffset = statePlan.packed->vertexOffset,
				.firstInstance = 0,
				.state = stateId,
				.gradientOffset = statePlan.gradientStart,
				.gradientCount = statePlan.gradientCount,
				.outlineOffset =
						(statePlan.stateData ? statePlan.stateData->outlineOffset : 0.0f)});
		}

		// do not draw shadows for a particles for now
		if (phase == StatePlanPhase::StatePlanGeneral) {
			for (auto &it : statePlan.particles) {
				target.emplace_back(VertexSpan{.material = materialId,
					.indexCount = 0,
					.instanceCount = 1,
					.firstIndex = 0,
					.vertexOffset = 0,
					.firstInstance = 0,
					.state = stateId,
					.gradientOffset = statePlan.gradientStart,
					.gradientCount = statePlan.gradientCount,
					.outlineOffset =
							(statePlan.stateData ? statePlan.stateData->outlineOffset : 0.0f),
					.particleSystemId = it->id});
			}
		}
	};

	// General drawing
	for (auto &it : drawOrder) {
		for (auto &state : it->second.states) {
			processStatePlan(it->first, state.first, state.second, StatePlanGeneral,
					ctx.materialSpans);
		}
	}

	// Shadow solids
	for (auto &it : drawOrder) {
		for (auto &state : it->second.states) {
			processStatePlan(it->first, state.first, state.second, StatePlanShadowSolid,
					ctx.shadowSolidSpans);
		}
	}

	// Shadow volumes
	for (auto &it : drawOrder) {
		for (auto &state : it->second.states) {
			processStatePlan(it->first, state.first, state.second, StatePlanShadowVolumes,
					ctx.shadowSdfSpans);
		}
	}
}

// Painter-order span emission for queues without a depth buffer.
//
// Routing into solid/surface/transparent plans stays untouched (that is what keeps geometry batched
// per material); only the order in which spans are emitted changes. Every VertexDataPlanInfo across
// all three plans is collected, sorted by (zPath, traversal order), and written out with absolute
// vertex indexes so that adjacent entries sharing a material+state collapse into a single draw.
void VertexPlan::drawWritePlanFlat(Context &ctx,
		WriteTarget &writeTarget) {
	struct FlatDrawEntry {
		SpanView<ZOrder> zOrder;
		core::MaterialId material;
		StateId state;
		const StatePlanInfo *statePlan;
		const VertexDataPlanInfo *data;
		bool instanced;
		uint32_t order;
	};

	Vector<FlatDrawEntry> entries;

	auto collectPlan = [&](Map<core::MaterialId, MaterialWritePlan> &writePlan) {
		for (auto &plan : writePlan) {
			for (auto &state : plan.second.states) {
				auto collectChain = [&](const VertexDataPlanInfo *it, bool instanced) {
					while (it) {
						entries.emplace_back(FlatDrawEntry{it->zOrder, plan.first, state.first,
							&state.second, it, instanced, it->order});
						it = it->next;
					}
				};

				collectChain(state.second.instanced, true);
				collectChain(state.second.packed, false);
			}
		}
	};

	collectPlan(solidWritePlan);
	collectPlan(surfaceWritePlan);
	for (auto &it : transparentWritePlan) { collectPlan(it.second); }

	if (entries.empty()) {
		return;
	}

	ZOrderLess zLess;
	sprt::sort(entries.begin(), entries.end(), [&](const FlatDrawEntry &l, const FlatDrawEntry &r) {
		if (zLess(l.zOrder, r.zOrder)) {
			return true;
		}
		if (zLess(r.zOrder, l.zOrder)) {
			return false;
		}
		return l.order < r.order;
	});

	auto writeIndexes = [](uint32_t *indexTarget, const uint32_t *indexSource, uint32_t indexCount,
								uint32_t vertexOffset) {
		for (size_t i = 0; i < indexCount; ++i) {
			*(indexTarget++) = *(indexSource++) + vertexOffset;
		}
		return indexCount;
	};

	// index of the span that a following entry may be merged into
	size_t currentIdx = maxOf<size_t>();
	const StatePlanInfo *currentStatePlan = nullptr;

	for (auto &entry : entries) {
		auto firstIndex = writeTarget.indexOffset;

		// absolute vertex base, so the span itself can use vertexOffset = 0 and stay mergeable
		uint32_t vertexBase = entry.data->vertexOffset;

		for (auto &vertexes : entry.data->vertexes) {
			writeTarget.indexOffset += writeIndexes(
					reinterpret_cast<uint32_t *>(writeTarget.indexes) + writeTarget.indexOffset,
					vertexes.data->indexes.data(),
					uint32_t(vertexes.data->indexes.size() - vertexes.sdfIndexes), vertexBase);
			vertexBase += uint32_t(vertexes.data->data.size());
		}

		auto indexCount = writeTarget.indexOffset - firstIndex;
		if (indexCount == 0) {
			continue;
		}

		// indexes are written sequentially, so a non-instanced entry that follows a non-instanced
		// span of the same material+state just extends it
		if (!entry.instanced && currentIdx != maxOf<size_t>() && currentStatePlan == entry.statePlan
				&& ctx.materialSpans[currentIdx].material == entry.material
				&& ctx.materialSpans[currentIdx].state == entry.state
				&& ctx.materialSpans[currentIdx].instanceCount == 1) {
			ctx.materialSpans[currentIdx].indexCount += indexCount;
			continue;
		}

		ctx.materialSpans.emplace_back(VertexSpan{.material = entry.material,
			.indexCount = indexCount,
			.instanceCount = entry.instanced ? entry.data->transformCount : 1,
			.firstIndex = firstIndex,
			.vertexOffset = 0,
			.firstInstance = entry.instanced ? entry.data->transformOffset : 0,
			.state = entry.state,
			.gradientOffset = entry.statePlan->gradientStart,
			.gradientCount = entry.statePlan->gradientCount,
			.outlineOffset = (entry.statePlan->stateData ? entry.statePlan->stateData->outlineOffset
														 : 0.0f)});

		if (entry.instanced) {
			// instanced spans carry their own instanceCount/firstInstance and cannot absorb others
			currentIdx = maxOf<size_t>();
			currentStatePlan = nullptr;
		} else {
			currentIdx = ctx.materialSpans.size() - 1;
			currentStatePlan = entry.statePlan;
		}
	}
}

void VertexPlan::pushAll(Context &ctx, WriteTarget &writeTarget) {
	pushInitial(writeTarget);
	pushPlanVertexes(writeTarget, solidWritePlan);
	pushPlanVertexes(writeTarget, surfaceWritePlan);
	for (auto &it : transparentWritePlan) { pushPlanVertexes(writeTarget, it.second); }

	if (flatOrder) {
		drawWritePlanFlat(ctx, writeTarget);

		// everything is drawn in painter's order, so there is no solid/surface split to report
		ctx.transparentCmds = uint32_t(ctx.materialSpans.size());
		return;
	}

	uint32_t counter = 0;
	drawWritePlan(ctx, writeTarget, solidWritePlan);

	ctx.solidCmds = uint32_t(ctx.materialSpans.size() - counter);
	counter = uint32_t(ctx.materialSpans.size());

	drawWritePlan(ctx, writeTarget, surfaceWritePlan);

	ctx.surfaceCmds = uint32_t(ctx.materialSpans.size() - counter);
	counter = uint32_t(ctx.materialSpans.size());

	for (auto &it : transparentWritePlan) { drawWritePlan(ctx, writeTarget, it.second); }

	ctx.transparentCmds = uint32_t(ctx.materialSpans.size() - counter);
}
} // namespace stappler::xenolith::basic2d
