void App::InitializeEnemyIdleRuntime(int modelIndex)
{
	if (modelIndex < 0 || modelIndex >= static_cast<int>(m_->m_Models.size()))
		return;

	const aiAnimation* idleClip = m_->m_ExternalAnimClips.Get("Idle");
	if (!idleClip)
	{
		m_->PushLog("[WARN] Enemy idle skipped: Idle clip missing");
		return;
	}

	auto* entryPtr = m_->m_Models[(size_t)modelIndex].get();
	if (!entryPtr)
		return;

	auto& entry = *entryPtr;
	if (entry.source != ModelSource::FBX || !entry.shared || !entry.shared->fbx || !entry.shared->fbx->HasSkeleton())
	{
		m_->PushLog("[WARN] Enemy idle skipped: model has no usable skeleton");
		return;
	}

	auto& fbx = *entry.shared->fbx;
	auto& runtime = m_->m_EnemyIdleRuntimes.emplace_back();
	runtime.modelIndex = modelIndex;
	runtime.idleTimeSec = 0.0f;
	runtime.sourceChannelCount = static_cast<int>(idleClip->mNumChannels);
	const auto& nodeIndexMap = fbx.GetNodeIndexOfName();
	for (unsigned channelIndex = 0; channelIndex < idleClip->mNumChannels; ++channelIndex)
	{
		const aiNodeAnim* channel = idleClip->mChannels[channelIndex];
		if (channel && nodeIndexMap.find(channel->mNodeName.C_Str()) != nodeIndexMap.end())
			++runtime.matchedChannelCount;
	}
	runtime.animator.Initialize(
		m_->m_pDevice,
		fbx.GetScenePtr(),
		fbx.GetNodeIndexOfName(),
		fbx.GetGlobalInverse(),
		fbx.GetBoneNames(),
		fbx.GetBoneOffsets());
	runtime.initialized = true;

	entry.fbxBaseAnimator.EnsureBoneCB(m_->m_pDevice, MAX_BONES);
	m_->PushLog("[OK] Enemy idle initialized");
	m_->PushLog("[OK] Enemy idle channels: model=" + std::to_string(modelIndex) +
		" matched=" + std::to_string(runtime.matchedChannelCount) + "/" +
		std::to_string(runtime.sourceChannelCount));
}

void App::UpdateEnemyIdleAnimations(float dt)
{
	const aiAnimation* idleClip = m_->m_ExternalAnimClips.Get("Idle");
	if (!idleClip)
		return;

	for (auto& runtime : m_->m_EnemyIdleRuntimes)
	{
		if (!runtime.initialized)
			continue;
		if (runtime.modelIndex < 0 || runtime.modelIndex >= static_cast<int>(m_->m_Models.size()))
			continue;

		auto* entryPtr = m_->m_Models[(size_t)runtime.modelIndex].get();
		if (!entryPtr)
			continue;

		auto& entry = *entryPtr;
		if (!entry.shared || !entry.shared->fbx)
			continue;

		runtime.idleTimeSec += dt;

		CharacterAnimator::UpdateDesc desc{};
		desc.dt = dt;
		desc.base.enabled = true;
		desc.base.animA = idleClip;
		desc.base.animB = idleClip;
		desc.base.timeA = runtime.idleTimeSec;
		desc.base.timeB = runtime.idleTimeSec;
		desc.base.blend01 = 0.0f;
		desc.base.layerAlpha = 1.0f;

		runtime.animator.Update(desc);
		const auto& finalTransforms = runtime.animator.finalTransforms;
		if (runtime.previousPalette.size() != finalTransforms.size())
		{
			runtime.previousPalette.resize(finalTransforms.size());
			for (size_t boneIndex = 0; boneIndex < finalTransforms.size(); ++boneIndex)
				XMStoreFloat4x4(&runtime.previousPalette[boneIndex], finalTransforms[boneIndex]);
		}
		else
		{
			float paletteDelta = 0.0f;
			for (size_t boneIndex = 0; boneIndex < finalTransforms.size(); ++boneIndex)
			{
				const XMMATRIX previous = XMLoadFloat4x4(&runtime.previousPalette[boneIndex]);
				const XMMATRIX current = finalTransforms[boneIndex];
				for (int row = 0; row < 4; ++row)
					paletteDelta += XMVectorGetX(XMVector4LengthSq(XMVectorSubtract(current.r[row], previous.r[row])));
				XMStoreFloat4x4(&runtime.previousPalette[boneIndex], current);
			}

			if (paletteDelta > 1e-4f)
			{
				runtime.paletteNoMotionSec = 0.0f;
				if (!runtime.paletteMotionLogged)
				{
					m_->PushLog("[OK] Enemy palette moving: model=" + std::to_string(runtime.modelIndex) +
						" delta=" + std::to_string(paletteDelta));
					runtime.paletteMotionLogged = true;
				}
			}
			else
			{
				runtime.paletteNoMotionSec += dt;
				if (runtime.paletteNoMotionSec >= 2.0f && !runtime.paletteStaticWarningLogged)
				{
					m_->PushLog("[WARN] Enemy palette static: model=" + std::to_string(runtime.modelIndex) +
						" matched=" + std::to_string(runtime.matchedChannelCount) + "/" +
						std::to_string(runtime.sourceChannelCount));
					runtime.paletteStaticWarningLogged = true;
				}
			}
		}
		entry.fbxBaseAnimator.EnsureBoneCB(m_->m_pDevice, MAX_BONES);
		entry.fbxBaseAnimator.UploadPalette(m_->m_pDeviceContext, runtime.animator.finalTransforms);
	}
}

void App::StartPublicDemoAudioOnce()
{
	if (!m_bIsLoaded || !m_bIsGameStarted || m_->m_PublicDemoAudioStarted)
		return;

	m_->m_PublicDemoAudioStarted = true;

	if (m_->m_PublicDemoBgmLoaded && !Sound::IsBGMPlaying())
	{
		Sound::SetBGMVolume(0.28f);
		Sound::PlayBGM(L"PublicDemoBgm");
	}

	if (m_->m_EnemyIdleAuraLoaded && !Sound::IsSfxPlaying(L"EnemyIdleAura"))
	{
		Sound::PlaySFX(L"EnemyIdleAura", 0.14f, 1.0f, true);
	}
}
