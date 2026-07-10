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
