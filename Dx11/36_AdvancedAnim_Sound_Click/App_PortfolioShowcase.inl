// ============================================================================
// README capture-only portfolio showcase
//
// When DX11_README_CAPTURE is set, Project 36 stops being an interactive editor
// and becomes a deterministic eight-second animation reel: four public MyAlice
// characters, each running its own CharacterAnimator, are driven through
//   Dance -> Animation blend + upper layer -> CCD IK -> loop
// so the README GIF always shows the same feature tour.
//
// Outside capture mode every entry point here returns immediately: normal
// Project 36 behaviour is untouched.
// ============================================================================

namespace
{
	// Deterministic eight-second cycle. The manifest's frame-zero click resets
	// timeSec so the captured GIF always starts at the same pose.
	constexpr float kPortfolioCycleSec = 8.0f;
	constexpr float kPortfolioDanceEndSec = 3.0f;
	constexpr float kPortfolioBlendEndSec = 5.2f;
	constexpr float kPortfolioIkEndSec = 7.5f;
	// The blend window is split into Dance->Walk and Walk->Dance halves.
	constexpr float kPortfolioBlendMidSec = 4.1f;

	constexpr const char* kPortfolioIkTipBone = "J_Bip_L_Hand";
	constexpr const char* kPortfolioIkElbowBone = "J_Bip_L_LowerArm";
	constexpr const char* kPortfolioIkShoulderBone = "J_Bip_L_UpperArm";

	constexpr const char* kPortfolioHudPhaseDance = "DANCE / SKINNED ANIMATION";
	constexpr const char* kPortfolioHudPhaseBlend = "ANIMATION BLEND + LAYER";
	constexpr const char* kPortfolioHudPhaseIk = "CCD IK / LEFT HAND TARGET";
	constexpr const char* kPortfolioHudPhaseFinish = "SHOWCASE LOOP / REPLAY";
	constexpr const char* kPortfolioHudCastLine = "4 CHARACTERS / LIVE PALETTES";

	float PortfolioSmoothStep(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	// Assimp glTF2 clips are authored on a different tick base than the FBX
	// importer's, so seconds must always come from mDuration / mTicksPerSecond.
	float PortfolioClipDurationSec(const aiAnimation* clip)
	{
		if (!clip)
			return 0.0f;
		const double ticksPerSecond = (clip->mTicksPerSecond != 0.0) ? clip->mTicksPerSecond : 25.0;
		if (ticksPerSecond <= 0.0 || clip->mDuration <= 0.0)
			return 0.0f;
		return (float)(clip->mDuration / ticksPerSecond);
	}

	int FindPortfolioBoneIndex(const std::vector<std::string>& boneNames, const char* boneName)
	{
		if (!boneName)
			return -1;
		for (size_t i = 0; i < boneNames.size(); ++i)
		{
			if (boneNames[i] == boneName)
				return (int)i;
		}
		return -1;
	}

	bool PortfolioMatrixIsFinite(DirectX::FXMMATRIX source)
	{
		return !XMMatrixIsNaN(source) && !XMMatrixIsInfinite(source);
	}

	bool TryInvertPortfolioMatrix(DirectX::FXMMATRIX source, DirectX::XMMATRIX& out)
	{
		XMVECTOR determinant = XMMatrixDeterminant(source);
		if (std::fabs(XMVectorGetX(determinant)) < 1.0e-8f)
			return false;
		out = XMMatrixInverse(&determinant, source);
		return PortfolioMatrixIsFinite(out);
	}

	// CharacterAnimator keeps Assimp's column-vector convention, so a node
	// global's translation lives in the last column (see GetTranslation_Col in
	// Common/Animation/Animator.h), not in row 3.
	XMVECTOR PortfolioNodeTranslation(DirectX::FXMMATRIX nodeGlobal)
	{
		return XMMatrixTranspose(nodeGlobal).r[3];
	}
}

// ---------------------------------------------------------------------------
// Initialization: capture mode only.
// ---------------------------------------------------------------------------
bool App::InitializePortfolioShowcase()
{
	auto& showcase = m_->m_PortfolioShowcase;
	showcase.initialized = false;
	showcase.fallbackToIdle = false;
	showcase.timeSec = 0.0f;
	showcase.phase = Impl::PortfolioShowcasePhase::Dance;
	showcase.ikDebugValid = false;

	// Outside README capture mode the showcase stays completely inert.
	if (!IsReadmeCaptureMode())
		return false;

	const aiAnimation* danceClip = m_->m_ExternalAnimClips.Get("PortfolioDance");
	const aiAnimation* upperWaveClip = m_->m_ExternalAnimClips.Get("PortfolioUpperWave");
	if (!danceClip || !upperWaveClip)
	{
		showcase.fallbackToIdle = true;
		m_->PushLog("[WARN] Portfolio showcase: original clips unavailable, falling back to Idle");
	}
	else
	{
		m_->PushLog("[OK] Portfolio showcase clips: PortfolioDance=" +
			std::to_string(PortfolioClipDurationSec(danceClip)) + "s PortfolioUpperWave=" +
			std::to_string(PortfolioClipDurationSec(upperWaveClip)) + "s");
	}

	// Slot order: player first, then the three existing enemy indices.
	const std::array<int, 4> modelIndices = {
		m_->m_CharModelIndex,
		m_->m_EnemyModelIndices[0],
		m_->m_EnemyModelIndices[1],
		m_->m_EnemyModelIndices[2]
	};
	const std::array<float, 4> phaseOffsets = { 0.0f, 0.17f, 0.31f, 0.46f };

	int readySlots = 0;
	for (size_t i = 0; i < showcase.slots.size(); ++i)
	{
		auto& slot = showcase.slots[i];
		slot.modelIndex = -1;
		slot.phaseOffsetSec = phaseOffsets[i];
		slot.initialized = false;

		const int modelIndex = modelIndices[i];
		if (modelIndex < 0 || modelIndex >= (int)m_->m_Models.size())
		{
			m_->PushLog("[WARN] Portfolio showcase slot " + std::to_string(i) + ": model index unavailable");
			continue;
		}

		auto* entryPtr = m_->m_Models[(size_t)modelIndex].get();
		if (!entryPtr)
			continue;

		auto& entry = *entryPtr;
		if (entry.source != ModelSource::FBX || !entry.shared || !entry.shared->fbx || !entry.shared->fbx->HasSkeleton())
		{
			m_->PushLog("[WARN] Portfolio showcase slot " + std::to_string(i) + ": model has no usable skeleton");
			continue;
		}

		auto& fbx = *entry.shared->fbx;
		slot.animator.Initialize(
			m_->m_pDevice,
			fbx.GetScenePtr(),
			fbx.GetNodeIndexOfName(),
			fbx.GetGlobalInverse(),
			fbx.GetBoneNames(),
			fbx.GetBoneOffsets());

		// The showcase owns this entry's palette upload from now on, so the
		// bone constant buffer has to exist before the first render pass.
		entry.fbxBaseAnimator.EnsureBoneCB(m_->m_pDevice, MAX_BONES);

		slot.modelIndex = modelIndex;
		slot.initialized = true;
		++readySlots;
	}

	showcase.initialized = readySlots > 0;
	if (showcase.initialized)
	{
		m_->PushLog("[OK] Portfolio showcase initialized: slots=" + std::to_string(readySlots) + "/4");
	}
	else
	{
		m_->PushLog("[WARN] Portfolio showcase: no usable character slots");
	}
	return showcase.initialized;
}

// ---------------------------------------------------------------------------
// Frame-zero reset, driven by the capture manifest's synthetic left click.
// ---------------------------------------------------------------------------
void App::ResetPortfolioShowcase()
{
	auto& showcase = m_->m_PortfolioShowcase;
	showcase.timeSec = 0.0f;
	showcase.phase = Impl::PortfolioShowcasePhase::Dance;
	showcase.ikDebugValid = false;
}

// ---------------------------------------------------------------------------
// Deterministic eight-second update. Returns true only while the showcase owns
// the capture-mode character palettes; camera, rendering and input keep running
// normally either way.
// ---------------------------------------------------------------------------
bool App::UpdatePortfolioShowcase(float dt)
{
	auto& showcase = m_->m_PortfolioShowcase;
	if (!IsReadmeCaptureMode() || !showcase.initialized)
		return false;

	if (InputSystem::Instance &&
		InputSystem::Instance->m_MouseStateTracker.leftButton == Mouse::ButtonStateTracker::PRESSED)
	{
		ResetPortfolioShowcase();
	}

	showcase.timeSec += dt;
	const float cycleTime = std::fmod(showcase.timeSec, kPortfolioCycleSec);

	if (cycleTime < kPortfolioDanceEndSec)        showcase.phase = Impl::PortfolioShowcasePhase::Dance;
	else if (cycleTime < kPortfolioBlendEndSec)   showcase.phase = Impl::PortfolioShowcasePhase::BlendLayer;
	else if (cycleTime < kPortfolioIkEndSec)      showcase.phase = Impl::PortfolioShowcasePhase::Ik;
	else                                          showcase.phase = Impl::PortfolioShowcasePhase::Finish;

	const aiAnimation* idle = m_->m_ExternalAnimClips.Get("Idle");
	const aiAnimation* walk = m_->m_ExternalAnimClips.Get("Walk");
	const aiAnimation* dance = showcase.fallbackToIdle ? idle : m_->m_ExternalAnimClips.Get("PortfolioDance");
	const aiAnimation* upperWave = showcase.fallbackToIdle ? nullptr : m_->m_ExternalAnimClips.Get("PortfolioUpperWave");
	if (!dance)
		dance = idle;
	if (!walk)
		walk = dance;

	// Blend / layer / IK progress inside their own phase windows.
	const bool blendFirstHalf = cycleTime < kPortfolioBlendMidSec;
	const float blendHalfStart = blendFirstHalf ? kPortfolioDanceEndSec : kPortfolioBlendMidSec;
	const float blendHalfLength = blendFirstHalf
		? (kPortfolioBlendMidSec - kPortfolioDanceEndSec)
		: (kPortfolioBlendEndSec - kPortfolioBlendMidSec);
	const float localHalfTime = (blendHalfLength > 0.0f) ? ((cycleTime - blendHalfStart) / blendHalfLength) : 0.0f;
	const float layer01 = (kPortfolioBlendEndSec > kPortfolioDanceEndSec)
		? ((cycleTime - kPortfolioDanceEndSec) / (kPortfolioBlendEndSec - kPortfolioDanceEndSec))
		: 0.0f;
	const float ikTime = (kPortfolioIkEndSec > kPortfolioBlendEndSec)
		? ((cycleTime - kPortfolioBlendEndSec) / (kPortfolioIkEndSec - kPortfolioBlendEndSec))
		: 0.0f;
	// Ease the IK weight in and out so the arm never snaps at a phase edge.
	const float ikWeight = std::min(ikTime / 0.25f, (1.0f - ikTime) / 0.25f);

	showcase.ikDebugValid = false;

	for (size_t slotIndex = 0; slotIndex < showcase.slots.size(); ++slotIndex)
	{
		auto& slot = showcase.slots[slotIndex];
		if (!slot.initialized)
			continue;
		if (slot.modelIndex < 0 || slot.modelIndex >= (int)m_->m_Models.size())
			continue;

		auto* entryPtr = m_->m_Models[(size_t)slot.modelIndex].get();
		if (!entryPtr || !entryPtr->shared || !entryPtr->shared->fbx)
			continue;
		auto& entry = *entryPtr;

		const bool isMainSlot = (slotIndex == 0);
		const float danceTime = showcase.timeSec + slot.phaseOffsetSec;
		const float upperTime = danceTime;

		CharacterAnimator::UpdateDesc desc{};
		desc.dt = dt;
		desc.base.enabled = true;
		desc.base.animA = dance;
		desc.base.animB = dance;
		desc.base.timeA = desc.base.timeB = danceTime;

		if (isMainSlot && showcase.phase == Impl::PortfolioShowcasePhase::BlendLayer)
		{
			// 3.0-4.1: Dance -> Walk, 4.1-5.2: Walk -> Dance.
			desc.base.animA = blendFirstHalf ? dance : walk;
			desc.base.animB = blendFirstHalf ? walk : dance;
			desc.base.blend01 = PortfolioSmoothStep(localHalfTime);
		}

		if (isMainSlot && upperWave)
		{
			desc.upper.enabled = showcase.phase == Impl::PortfolioShowcasePhase::BlendLayer;
			desc.upper.animA = upperWave;
			desc.upper.animB = upperWave;
			desc.upper.timeA = desc.upper.timeB = upperTime;
			desc.upper.layerAlpha = std::sin(layer01 * XM_PI);
		}

		if (isMainSlot)
		{
			desc.ik.enabled = showcase.phase == Impl::PortfolioShowcasePhase::Ik;
			desc.ik.tipBone = kPortfolioIkTipBone;
			desc.ik.chainLen = 3;
			desc.ik.targetMS = XMVectorSet(
				-0.32f + 0.18f * std::cos(ikTime * XM_2PI),
				1.08f + 0.16f * std::sin(ikTime * XM_2PI),
				0.20f, 1.0f);
			desc.ik.weight = PortfolioSmoothStep(ikWeight);
			XMStoreFloat3(&showcase.ikTargetMS, desc.ik.targetMS);
		}

		slot.animator.Update(desc);

		entry.fbxBaseAnimator.EnsureBoneCB(m_->m_pDevice, MAX_BONES);
		entry.fbxBaseAnimator.UploadPalette(m_->m_pDeviceContext, slot.animator.finalTransforms);

		if (isMainSlot && desc.ik.enabled)
		{
			// Recover the arm chain's node globals from the uploaded palette:
			//   final = globalInverse * nodeGlobal * boneOffset
			// so nodeGlobal = globalInverse^-1 * final * boneOffset^-1.
			const auto& fbx = *entry.shared->fbx;
			const auto& boneNames = fbx.GetBoneNames();
			const auto& boneOffsets = fbx.GetBoneOffsets();
			const auto& palette = slot.animator.finalTransforms;

			const int shoulderIndex = FindPortfolioBoneIndex(boneNames, kPortfolioIkShoulderBone);
			const int elbowIndex = FindPortfolioBoneIndex(boneNames, kPortfolioIkElbowBone);
			const int handIndex = FindPortfolioBoneIndex(boneNames, kPortfolioIkTipBone);

			XMMATRIX globalInverseInv{};
			const bool globalInverseOk =
				TryInvertPortfolioMatrix(XMLoadFloat4x4(&fbx.GetGlobalInverse()), globalInverseInv);

			const XMMATRIX modelWorld = entry.GetWorldMatrix();
			XMFLOAT3 resolved[3]{};
			bool allResolved = globalInverseOk;
			const int boneIndices[3] = { shoulderIndex, elbowIndex, handIndex };

			for (int i = 0; i < 3 && allResolved; ++i)
			{
				const int boneIndex = boneIndices[i];
				if (boneIndex < 0 ||
					(size_t)boneIndex >= palette.size() ||
					(size_t)boneIndex >= boneOffsets.size())
				{
					allResolved = false;
					break;
				}

				XMMATRIX boneOffsetInv{};
				if (!TryInvertPortfolioMatrix(XMLoadFloat4x4(&boneOffsets[(size_t)boneIndex]), boneOffsetInv))
				{
					allResolved = false;
					break;
				}

				const XMMATRIX nodeGlobal = globalInverseInv * palette[(size_t)boneIndex] * boneOffsetInv;
				if (!PortfolioMatrixIsFinite(nodeGlobal))
				{
					allResolved = false;
					break;
				}

				XMStoreFloat3(&resolved[i], XMVector3TransformCoord(PortfolioNodeTranslation(nodeGlobal), modelWorld));
			}

			if (allResolved)
			{
				showcase.ikShoulderWS = resolved[0];
				showcase.ikElbowWS = resolved[1];
				showcase.ikHandWS = resolved[2];
				XMStoreFloat3(&showcase.ikTargetWS,
					XMVector3TransformCoord(XMLoadFloat3(&showcase.ikTargetMS), modelWorld));
				showcase.ikDebugValid = true;
			}
		}
	}

	return true;
}

// ---------------------------------------------------------------------------
// IK evidence: the solved arm chain and its target, drawn with LineRenderer.
// ---------------------------------------------------------------------------
void App::RenderPortfolioShowcaseDebug()
{
	const auto& showcase = m_->m_PortfolioShowcase;
	if (!IsReadmeCaptureMode() || !showcase.initialized || !showcase.ikDebugValid)
		return;
	if (showcase.phase != Impl::PortfolioShowcasePhase::Ik)
		return;
	if (!m_->m_LineRenderer || !m_->m_pLineVS || !m_->m_pLineInputLayout)
		return;

	// The recovered positions are already world space, so the line pass draws
	// with an identity world matrix.
	ConstantBuffer lineCB = m_->m_ConstantBuffer;
	const XMMATRIX identity = XMMatrixIdentity();
	lineCB.world = XMMatrixTranspose(identity);
	lineCB.worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, identity));
	lineCB.view = m_->m_baseProjection.view;
	lineCB.proj = m_->m_baseProjection.proj;
	lineCB.pad = 3.0f; // line marker path
	lineCB.boundsBoneIndex = -1;

	D3D11_MAPPED_SUBRESOURCE mappedLine{};
	if (FAILED(m_->m_pDeviceContext->Map(m_->m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedLine)))
		return;
	memcpy_s(mappedLine.pData, sizeof(ConstantBuffer), &lineCB, sizeof(ConstantBuffer));
	m_->m_pDeviceContext->Unmap(m_->m_pConstantBuffer, 0);
	m_->m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
	m_->m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);

	ID3D11VertexShader* previousVS = nullptr;
	m_->m_pDeviceContext->VSGetShader(&previousVS, nullptr, nullptr);
	ID3D11Buffer* previousVSb1 = nullptr;
	m_->m_pDeviceContext->VSGetConstantBuffers(1, 1, &previousVSb1);
	ID3D11InputLayout* previousIL = nullptr;
	m_->m_pDeviceContext->IAGetInputLayout(&previousIL);

	m_->m_pDeviceContext->VSSetShader(m_->m_pLineVS, nullptr, 0);
	m_->m_pDeviceContext->IASetInputLayout(m_->m_pLineInputLayout);
	ID3D11Buffer* nullBoneCB = nullptr;
	m_->m_pDeviceContext->VSSetConstantBuffers(1, 1, &nullBoneCB);

	auto drawLine = [&](const XMFLOAT3& from, const XMFLOAT3& to, const XMFLOAT4& color) {
		m_->m_LineRenderer->DrawLine(m_->m_pDeviceContext, from, to, color,
			m_->m_pLineInputLayout, m_->m_pLineVS, m_->m_pPixelShader, m_->m_pConstantBuffer);
		};

	const XMFLOAT4 chainColor(0.15f, 1.0f, 0.35f, 1.0f);
	const XMFLOAT4 reachColor(1.0f, 0.85f, 0.1f, 1.0f);
	const XMFLOAT4 targetColor(1.0f, 0.25f, 0.25f, 1.0f);

	drawLine(showcase.ikShoulderWS, showcase.ikElbowWS, chainColor);
	drawLine(showcase.ikElbowWS, showcase.ikHandWS, chainColor);
	drawLine(showcase.ikHandWS, showcase.ikTargetWS, reachColor);

	// Three-axis cross at the IK target.
	const float crossHalfLength = 8.0f;
	const XMFLOAT3& target = showcase.ikTargetWS;
	drawLine(XMFLOAT3(target.x - crossHalfLength, target.y, target.z),
		XMFLOAT3(target.x + crossHalfLength, target.y, target.z), targetColor);
	drawLine(XMFLOAT3(target.x, target.y - crossHalfLength, target.z),
		XMFLOAT3(target.x, target.y + crossHalfLength, target.z), targetColor);
	drawLine(XMFLOAT3(target.x, target.y, target.z - crossHalfLength),
		XMFLOAT3(target.x, target.y, target.z + crossHalfLength), targetColor);

	if (previousVS) { m_->m_pDeviceContext->VSSetShader(previousVS, nullptr, 0); previousVS->Release(); }
	if (previousVSb1) { m_->m_pDeviceContext->VSSetConstantBuffers(1, 1, &previousVSb1); previousVSb1->Release(); }
	if (previousIL) { m_->m_pDeviceContext->IASetInputLayout(previousIL); previousIL->Release(); }
}

// ---------------------------------------------------------------------------
// Capture-mode HUD: the only ImGui surface Project 36 draws while capturing.
// ---------------------------------------------------------------------------
void App::RenderPortfolioShowcaseHud()
{
	const auto& showcase = m_->m_PortfolioShowcase;
	if (!IsReadmeCaptureMode())
		return;

	const char* phaseLabel = kPortfolioHudPhaseDance;
	switch (showcase.phase)
	{
	case Impl::PortfolioShowcasePhase::Dance:      phaseLabel = kPortfolioHudPhaseDance;  break;
	case Impl::PortfolioShowcasePhase::BlendLayer: phaseLabel = kPortfolioHudPhaseBlend;  break;
	case Impl::PortfolioShowcasePhase::Ik:         phaseLabel = kPortfolioHudPhaseIk;     break;
	case Impl::PortfolioShowcasePhase::Finish:     phaseLabel = kPortfolioHudPhaseFinish; break;
	}

	// Fixed geometry and an opaque panel keep the captured frames deterministic.
	ImGui::SetNextWindowPos(ImVec2(24.0f, 24.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(320.0f, 64.0f), ImGuiCond_Always);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.05f, 0.08f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

	const ImGuiWindowFlags hudFlags =
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;

	if (ImGui::Begin("PortfolioShowcaseHud", nullptr, hudFlags))
	{
		ImGui::TextUnformatted(phaseLabel);
		ImGui::TextUnformatted(kPortfolioHudCastLine);
	}
	ImGui::End();
	ImGui::PopStyleColor(2);
}
