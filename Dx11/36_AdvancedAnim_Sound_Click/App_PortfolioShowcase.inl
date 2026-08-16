// ============================================================================
// Portfolio showcase
//
// Project 36 opens on this rather than on an editor scene: four public MyAlice
// characters, each running its own CharacterAnimator, play the VRM_* clips
// embedded in the player model so the animation systems are the first thing the
// window shows.
//
// The clips are resolved by NAME out of the model's own animation table, never
// by index, so a re-export that reorders the animations fails loudly (a missing
// clip is logged and the slot goes quiet) instead of silently playing the wrong
// motion.
// ============================================================================

namespace
{
	constexpr int kPortfolioClipCount = 7;
	constexpr int kPortfolioSlotCount = 4;

	// Slot order - player, enemy 1, enemy 2, enemy 3, the order
	// InitializePortfolioShowcase() below builds showcase.slots in and
	// App_Lifecycle.inl's characterPositions is laid out in - is not left-to-right
	// screen order. The composition places the four slots at different depths and
	// x offsets (ndc +0.240 / -0.719 / -0.241 / +0.719 for slot 0/1/2/3
	// respectively; see App_Lifecycle.inl's characterPositions comment for the
	// derivation), so left to right on screen is slot 1, slot 2, slot 0, slot 3.
	// RenderPortfolioShowcaseHud orders its caption by this table so the printed
	// name lines up with the character a viewer actually sees at each position,
	// rather than with the order the slots were constructed in.
	constexpr int kPortfolioScreenOrderToSlot[kPortfolioSlotCount] = { 1, 2, 0, 3 };

	// The showcase clips, in the order they were authored. Animation 0 of
	// SampleModel.glb is a 0.042 s T-Pose and is deliberately not among them.
	// Declared without an explicit extent so kPortfolioClipCount can be checked
	// against it (see the static_asserts in InitializePortfolioShowcase).
	const char* const kPortfolioClipNames[] = {
		"VRM_1", "VRM_2", "VRM_3", "VRM_4", "VRM_5", "VRM_6", "VRM_7"
	};
	constexpr size_t kPortfolioClipNameCount =
		sizeof(kPortfolioClipNames) / sizeof(kPortfolioClipNames[0]);

	// One set of assignments lasts this long. Four slots drawn from a seven-clip
	// table means two sets put every clip on screen (0,1,2,3 then 4,5,6,0), and the
	// whole rotation closes after seven, by which time each clip has visited each
	// slot. Twelve seconds is a little longer than the longest clip (VRM_1, 11.9 s),
	// so a set never cuts a clip off before it has been seen through once.
	constexpr float kPortfolioSetSeconds = 12.0f;

	// Which set the show is in. Factored out because the motion and the caption must
	// never disagree about it: UpdatePortfolioShowcase picks the clips from this and
	// RenderPortfolioShowcaseHud names them from it, and two separate derivations of
	// "which set is this" is exactly how a HUD ends up announcing a clip the cast is
	// not playing. std::floor rather than a cast so the answer stays monotonic if the
	// clock is ever read at a negative time; truncation would round toward zero and
	// hand back set 0 twice.
	int PortfolioCycleForTime(float timeSec)
	{
		return (int)std::floor(timeSec / kPortfolioSetSeconds);
	}

	// Slot i plays clip ((cycle * 4 + i) % clipCount). Two cycles expose all seven.
	//
	// clipCount is the size of the COMPACTED table Task 2 built, not
	// kPortfolioClipCount. On a healthy model the two are the same 7 and the mapping
	// is exactly the designed one; if a re-export drops a name, indexing by 7 would
	// walk off the end of a shorter vector, so the rotation shortens to the clips
	// that actually resolved instead. kPortfolioClipCount stays the authority on how
	// many names the model is EXPECTED to carry - that is what the static_asserts and
	// the name table are pinned to - and clipCount is the authority on how many it
	// actually answered to.
	//
	// cycle may be negative: Task 4 asks for the outgoing line-up with cycle - 1, and
	// C++ '%' truncates toward zero, so a negative remainder is folded back into
	// [0, clipCount) rather than indexing backwards off the front of the vector.
	int PortfolioClipIndexForSlot(int cycle, int slot, int clipCount)
	{
		if (clipCount <= 0)
			return 0;
		const int raw = (cycle * kPortfolioSlotCount + slot) % clipCount;
		return raw < 0 ? raw + clipCount : raw;
	}

	// The CCD IK evidence path: the chain RenderPortfolioShowcaseDebug() draws, and
	// the helpers below that recover it from the uploaded palette. The chain is
	// held here, unsolved, while the showcase plays base clips only; the IK
	// windows that drive it are not part of this change.
	constexpr const char* kPortfolioIkTipBone = "J_Bip_L_Hand";
	constexpr const char* kPortfolioIkElbowBone = "J_Bip_L_LowerArm";
	constexpr const char* kPortfolioIkShoulderBone = "J_Bip_L_UpperArm";

	constexpr const char* kPortfolioHudTitleLine = "VRM ANIMATION SHOWCASE";
	// The second HUD line is now built per frame from the clips actually playing (see
	// RenderPortfolioShowcaseHud), so there is no fixed cast-line format any more. It
	// still reports only slots that came up, which is what the old "%d CHARACTERS"
	// line was for: a partial initialization cannot publish a caption the frame does
	// not support.

	// The showcase's observable diagnostics channel. PushLog() records into
	// m_LogLines, whose only renderer was the editor console panel - which this
	// build no longer draws - so a line that must survive a plain launch is also
	// written to the debugger's output stream, where DebugView, WinDbg or the
	// Visual Studio Output window picks it up without a rebuild.
	void PortfolioDebugOut(const std::string& line)
	{
		OutputDebugStringA(line.c_str());
		OutputDebugStringA("\n");
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

	// ------------------------------------------------------------------------
	// README backbuffer publication helpers.
	// ------------------------------------------------------------------------

	// At most twelve published frames per second. The capture tool samples the
	// file far more slowly than that, so anything faster would only spend the
	// showcase's frame time re-encoding pixels nobody reads.
	constexpr ULONGLONG kPortfolioBackbufferMinIntervalMs = 1000ull / 12ull;

	std::wstring ReadPortfolioBackbufferPathFromEnvironment()
	{
		const DWORD required = GetEnvironmentVariableW(L"DX11_README_BACKBUFFER_PNG", nullptr, 0);
		if (required == 0)
			return std::wstring();

		std::wstring value(required, L'\0');
		const DWORD copied = GetEnvironmentVariableW(L"DX11_README_BACKBUFFER_PNG", value.data(), required);
		if (copied == 0 || copied >= required)
			return std::wstring();
		value.resize(copied);
		return value;
	}

	// The one format capture mode can produce. ResolveSwapChainFormat in
	// App_Utilities.inl pins the swap chain to DXGI_FORMAT_R8G8B8A8_UNORM
	// whenever DX11_README_CAPTURE is set, precisely so the bytes read back here
	// are already the gamma-encoded sRGB a PNG stores.
	//
	// Anything else is refused rather than reinterpreted. A wider table would let
	// an HDR10 back buffer (PQ-encoded Rec.2020) through a plain numeric 10->8
	// bit conversion and publish a washed-out, wrong-hue frame that every
	// automated check still accepts; refusing means the writer skips the frame,
	// the capture tool's timeout fires, and a future format change fails visibly
	// instead of publishing colour garbage.
	const GUID* PortfolioBackbufferWicFormat(DXGI_FORMAT format) noexcept
	{
		if (format == DXGI_FORMAT_R8G8B8A8_UNORM)
			return &GUID_WICPixelFormat32bppRGBA;
		return nullptr;
	}

	// Encodes mapped staging rows into a complete in-memory PNG. Encoding away
	// from the file system is what keeps the temporary sibling below alive for a
	// single WriteFile instead of for the whole encode, so a capture run that is
	// killed mid-frame has almost no window in which to leave one behind.
	bool EncodePortfolioBackbufferPng(
		IWICImagingFactory* factory,
		const void* rows,
		UINT rowPitch,
		UINT width,
		UINT height,
		DXGI_FORMAT format,
		std::vector<uint8_t>& encoded)
	{
		encoded.clear();
		const GUID* sourceFormat = PortfolioBackbufferWicFormat(format);
		if (!factory || !rows || !sourceFormat || width == 0 || height == 0 || rowPitch == 0)
			return false;

		// The mapped RowPitch is the driver's, never assumed to be width * 4.
		Microsoft::WRL::ComPtr<IWICBitmap> sourceBitmap;
		if (FAILED(factory->CreateBitmapFromMemory(
			width, height, *sourceFormat, rowPitch, rowPitch * height,
			static_cast<BYTE*>(const_cast<void*>(rows)), sourceBitmap.GetAddressOf())))
			return false;

		Microsoft::WRL::ComPtr<IStream> stream;
		if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, stream.GetAddressOf())))
			return false;

		Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
		if (FAILED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.GetAddressOf())) ||
			FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)))
			return false;

		Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
		Microsoft::WRL::ComPtr<IPropertyBag2> frameProperties;
		if (FAILED(encoder->CreateNewFrame(frame.GetAddressOf(), frameProperties.GetAddressOf())) ||
			FAILED(frame->Initialize(frameProperties.Get())) ||
			FAILED(frame->SetSize(width, height)))
			return false;

		// A screenshot has no meaningful alpha channel, and the back buffer's
		// alpha is whatever the last blend left behind. Publishing 24bpp BGR keeps
		// the PNG opaque so the downstream GDI+ and ffmpeg stages cannot composite
		// the capture against an unexpected background.
		GUID targetFormat = GUID_WICPixelFormat24bppBGR;
		if (FAILED(frame->SetPixelFormat(&targetFormat)))
			return false;

		Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
		BOOL canConvert = FALSE;
		if (FAILED(factory->CreateFormatConverter(converter.GetAddressOf())) ||
			FAILED(converter->CanConvert(*sourceFormat, targetFormat, &canConvert)) ||
			!canConvert ||
			FAILED(converter->Initialize(sourceBitmap.Get(), targetFormat,
				WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeMedianCut)))
			return false;

		if (FAILED(frame->WriteSource(converter.Get(), nullptr)) ||
			FAILED(frame->Commit()) ||
			FAILED(encoder->Commit()))
			return false;

		// GlobalSize would report the allocation, not the encoded length.
		STATSTG streamStats{};
		if (FAILED(stream->Stat(&streamStats, STATFLAG_NONAME)) ||
			streamStats.cbSize.QuadPart <= 0 ||
			streamStats.cbSize.QuadPart > (LONGLONG)(64ull * 1024ull * 1024ull))
			return false;

		HGLOBAL encodedMemory = nullptr;
		if (FAILED(GetHGlobalFromStream(stream.Get(), &encodedMemory)) || !encodedMemory)
			return false;

		const uint8_t* encodedBytes = static_cast<const uint8_t*>(GlobalLock(encodedMemory));
		if (!encodedBytes)
			return false;
		encoded.assign(encodedBytes, encodedBytes + (size_t)streamStats.cbSize.QuadPart);
		GlobalUnlock(encodedMemory);
		return true;
	}

	// The publication itself: one private temporary sibling, written in a single
	// shot, then renamed onto the requested path. MoveFileExW's replace is atomic,
	// so a tool polling that path only ever opens a whole PNG - never the bytes of
	// one still being written.
	bool PublishPortfolioBackbufferPng(
		const std::wstring& temporaryPath,
		const std::wstring& requestedPath,
		const std::vector<uint8_t>& encoded)
	{
		if (temporaryPath.empty() || requestedPath.empty() || encoded.empty())
			return false;

		const HANDLE temporaryFile = CreateFileW(temporaryPath.c_str(), GENERIC_WRITE, 0, nullptr,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (temporaryFile == INVALID_HANDLE_VALUE)
			return false;

		DWORD written = 0;
		const BOOL wrote = WriteFile(temporaryFile, encoded.data(), (DWORD)encoded.size(), &written, nullptr);
		CloseHandle(temporaryFile);

		if (!wrote || written != (DWORD)encoded.size() ||
			!MoveFileExW(temporaryPath.c_str(), requestedPath.c_str(),
				MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			// A reader holding the published PNG open blocks the replace. Drop this
			// frame - the next throttled one republishes - but never leave the
			// temporary sibling behind.
			DeleteFileW(temporaryPath.c_str());
			return false;
		}
		return true;
	}
}

// ---------------------------------------------------------------------------
// Clip lookup by name.
//
// GetAnimationNames() is index-aligned with the scene's mAnimations, so walking
// the names and taking the animation at the matching index resolves a clip by
// what it is called rather than by where it happens to sit. Models with no scene
// or no animations - the ground plane, an asset that failed to import - are
// skipped, so only a model that really carries the named clip can answer.
// ---------------------------------------------------------------------------
const aiAnimation* App::FindPortfolioClip(const std::string& name) const
{
	for (const auto& entry : m_->m_Models)
	{
		if (!entry || entry->source != ModelSource::FBX || !entry->shared || !entry->shared->fbx)
			continue;

		const auto& fbx = *entry->shared->fbx;
		const aiScene* scene = fbx.GetScenePtr();
		if (!scene || !scene->mAnimations)
			continue;

		const auto& names = fbx.GetAnimationNames();
		for (size_t i = 0; i < names.size(); ++i)
		{
			if (names[i] == name && i < scene->mNumAnimations)
				return scene->mAnimations[i];
		}
	}
	return nullptr;
}

// ---------------------------------------------------------------------------
// Initialization.
// ---------------------------------------------------------------------------
bool App::InitializePortfolioShowcase()
{
	auto& showcase = m_->m_PortfolioShowcase;

	// Each count and the table it indexes, pinned together at compile time. Raising
	// kPortfolioClipCount would walk off kPortfolioClipNames in the resolver loop
	// below, and changing the slot count would disagree with either the slots array
	// or the phaseOffsets table the slot loop reads in lock-step with it.
	static_assert(kPortfolioClipNameCount == (size_t)kPortfolioClipCount,
		"kPortfolioClipCount must equal the number of entries in kPortfolioClipNames");
	static_assert(std::tuple_size<decltype(showcase.slots)>::value == (size_t)kPortfolioSlotCount,
		"kPortfolioSlotCount must equal PortfolioShowcaseRuntime::slots.size()");

	showcase.initialized = false;
	showcase.timeSec = 0.0f;
	showcase.clips.clear();
	showcase.clipNames.clear();
	showcase.ikDebugValid = false;

	auto& backbuffer = m_->m_PortfolioBackbuffer;
	backbuffer.requestedPath.clear();
	backbuffer.temporaryPath.clear();
	backbuffer.nextWriteTickMs = 0;

	// The backbuffer publisher keeps its own gate: README capture mode plus a named
	// path. Read once, here, and never again, so a capture run cannot change where
	// its frames are published half way through. An unset variable leaves the path
	// empty, which is what keeps the writer off for the projects that did not opt in.
	if (IsReadmeCaptureMode())
	{
		backbuffer.requestedPath = ReadPortfolioBackbufferPathFromEnvironment();
		if (!backbuffer.requestedPath.empty())
		{
			backbuffer.temporaryPath = backbuffer.requestedPath + L".tmp.png";
			m_->PushLog("[OK] Portfolio backbuffer capture enabled");
		}
	}

	// Resolution compacts as it goes: a name that does not resolve contributes
	// nothing rather than leaving a hole. That is what keeps clips.size() and the
	// index space the slot loop wraps around describing the same thing, so a partial
	// re-export costs the cast variety, never motion.
	showcase.clips.reserve(kPortfolioClipNameCount);
	showcase.clipNames.reserve(kPortfolioClipNameCount);
	for (int i = 0; i < kPortfolioClipCount; ++i)
	{
		const aiAnimation* clip = FindPortfolioClip(kPortfolioClipNames[i]);
		if (!clip)
		{
			const std::string missing = std::string("[WARN] Portfolio clip missing: ") + kPortfolioClipNames[i];
			m_->PushLog(missing);
			PortfolioDebugOut(missing);
			continue;
		}
		showcase.clips.push_back(clip);
		showcase.clipNames.emplace_back(kPortfolioClipNames[i]);
		m_->PushLog(std::string("[OK] Portfolio clip ") + kPortfolioClipNames[i] + ": " +
			std::to_string(PortfolioClipDurationSec(clip)) + "s");
	}

	// The names are reported, not just the count: with the table compacted, "which
	// clips is the cast actually rotating through" is no longer inferable from the
	// count alone, and it is what a partial re-export changes.
	std::string resolvedSummary = "[OK] Portfolio clips resolved: " +
		std::to_string(showcase.clips.size()) + "/" + std::to_string(kPortfolioClipCount) + " [";
	for (size_t i = 0; i < showcase.clipNames.size(); ++i)
	{
		if (i > 0)
			resolvedSummary += ", ";
		resolvedSummary += showcase.clipNames[i];
	}
	resolvedSummary += "]";
	m_->PushLog(resolvedSummary);
	// Reported on every launch, not only on failure: a full roster is the only cheap
	// way to tell a healthy run from one quietly playing a shorter rotation.
	PortfolioDebugOut(resolvedSummary);

	if (showcase.clips.empty())
	{
		const std::string disabled = "[WARN] Portfolio showcase disabled: no VRM clips resolved";
		m_->PushLog(disabled);
		PortfolioDebugOut(disabled);
		return false;
	}

	// Slot order: player first, then the three existing enemy indices.
	const std::array<int, kPortfolioSlotCount> modelIndices = {
		m_->m_CharModelIndex,
		m_->m_EnemyModelIndices[0],
		m_->m_EnemyModelIndices[1],
		m_->m_EnemyModelIndices[2]
	};
	const std::array<float, kPortfolioSlotCount> phaseOffsets = { 0.0f, 0.17f, 0.31f, 0.46f };

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
		m_->PushLog("[OK] Portfolio showcase initialized: slots=" + std::to_string(readySlots) +
			"/" + std::to_string(kPortfolioSlotCount));
	}
	else
	{
		m_->PushLog("[WARN] Portfolio showcase: no usable character slots");
	}
	return showcase.initialized;
}

// ---------------------------------------------------------------------------
// Restart the show from t = 0.
//
// Now that the showcase runs ungated, the left click that drives this is an
// ordinary interactive affordance as well as the capture manifest's synthetic
// one: clicking the window restarts all four characters together, which is the
// intended way to re-watch the opening of the rotation. It is deliberately NOT
// scoped to capture mode - a viewer who can only ever join the show mid-clip
// would be worse off, and every slot keeps its own phase offset across the
// restart, so the cast stays staggered rather than snapping into lock-step.
// ---------------------------------------------------------------------------
void App::ResetPortfolioShowcase()
{
	auto& showcase = m_->m_PortfolioShowcase;
	showcase.timeSec = 0.0f;
	showcase.ikDebugValid = false;
}

// ---------------------------------------------------------------------------
// Per-frame update. Returns true while the showcase owns the character palettes,
// which is every frame it is initialized; camera, rendering and input keep
// running normally either way.
// ---------------------------------------------------------------------------
bool App::UpdatePortfolioShowcase(float dt)
{
	auto& showcase = m_->m_PortfolioShowcase;
	if (!showcase.initialized)
		return false;

	if (InputSystem::Instance &&
		InputSystem::Instance->m_MouseStateTracker.leftButton == Mouse::ButtonStateTracker::PRESSED)
	{
		ResetPortfolioShowcase();
	}

	showcase.timeSec += dt;
	showcase.ikDebugValid = false;

	const int cycle = PortfolioCycleForTime(showcase.timeSec);
	// Seconds elapsed inside the current set. Nothing reads it yet: Task 4 consumes
	// it to cross-fade the outgoing line-up into the incoming one at a set boundary.
	// It is computed here, beside the cycle it belongs to, so that blend and this
	// assignment can only ever be driven off the same clock.
	const float setTime = showcase.timeSec - (float)cycle * kPortfolioSetSeconds;
	(void)setTime;

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

		// The rotation. Every set advances the whole cast by four clips, so a viewer
		// who watches two sets has seen all seven, and one who stays for seven has
		// seen each of them in each of the four positions. The modulus is taken over
		// the size of the very table it indexes, and initialization refused to run
		// with an empty one, so every slot always receives a clip that resolved -
		// even when only some of the seven names did.
		const int clipIndex = PortfolioClipIndexForSlot(cycle, (int)slotIndex, (int)showcase.clips.size());
		const aiAnimation* clip = showcase.clips[(size_t)clipIndex];
		if (!clip)
			continue; // unreachable: clips is compacted, so it holds no nulls

		const float clipTime = showcase.timeSec + slot.phaseOffsetSec;

		CharacterAnimator::UpdateDesc desc{};
		desc.dt = dt;
		desc.base.enabled = true;
		desc.base.animA = clip;
		desc.base.animB = clip;
		desc.base.timeA = clipTime;
		desc.base.timeB = clipTime;
		desc.base.blend01 = 0.0f;
		desc.base.layerAlpha = 1.0f;

		slot.animator.Update(desc);

		entry.fbxBaseAnimator.EnsureBoneCB(m_->m_pDevice, MAX_BONES);
		entry.fbxBaseAnimator.UploadPalette(m_->m_pDeviceContext, slot.animator.finalTransforms);
	}

	return true;
}

// ---------------------------------------------------------------------------
// IK evidence: the solved arm chain and its target, drawn with LineRenderer.
// ---------------------------------------------------------------------------
void App::RenderPortfolioShowcaseDebug()
{
	const auto& showcase = m_->m_PortfolioShowcase;
	if (!showcase.initialized || !showcase.ikDebugValid)
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
// Showcase HUD: the only ImGui surface Project 36 draws.
// ---------------------------------------------------------------------------
void App::RenderPortfolioShowcaseHud()
{
	const auto& showcase = m_->m_PortfolioShowcase;
	// No showcase, no caption: the HUD only ever describes a cast that is running.
	if (!showcase.initialized)
		return;

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

	// The line-up currently playing, ordered left to right on screen via
	// kPortfolioScreenOrderToSlot (slot construction order and screen order are not
	// the same - see that table's comment). Three things keep it honest:
	//
	//   - the cycle comes from PortfolioCycleForTime() reading the same
	//     showcase.timeSec that UpdatePortfolioShowcase() drove the assignment from,
	//     and the index from the same PortfolioClipIndexForSlot(), so the caption
	//     cannot name a clip the cast is not playing;
	//   - the names come from showcase.clipNames, which records what each entry of
	//     the compacted table actually answered to, so a partial re-export prints
	//     the shorter rotation it really got rather than the seven it asked for;
	//   - the iteration order comes from kPortfolioScreenOrderToSlot rather than
	//     the raw slot index, so the name printed at each position is the one
	//     actually running on the character standing there.
	//
	// A slot that never came up contributes nothing, which preserves the old cast
	// line's property: the HUD only ever describes characters the frame supports.
	const int cycle = PortfolioCycleForTime(showcase.timeSec);
	const int clipCount = (int)showcase.clips.size();
	std::string lineUp;
	for (int screenIndex = 0; screenIndex < kPortfolioSlotCount; ++screenIndex)
	{
		const int slot = kPortfolioScreenOrderToSlot[screenIndex];
		if ((size_t)slot >= showcase.slots.size() || !showcase.slots[(size_t)slot].initialized)
			continue;
		if (!lineUp.empty())
			lineUp += "  ";
		lineUp += showcase.clipNames[(size_t)PortfolioClipIndexForSlot(cycle, slot, clipCount)];
	}

	if (ImGui::Begin("PortfolioShowcaseHud", nullptr, hudFlags))
	{
		ImGui::TextUnformatted(kPortfolioHudTitleLine);
		ImGui::TextUnformatted(lineUp.c_str());
	}
	ImGui::End();
	ImGui::PopStyleColor(2);
}

// ---------------------------------------------------------------------------
// Reliable capture source: publish the finished frame straight from the swap
// chain.
//
// Graphics.CopyFromScreen captures whatever is physically on screen, so an
// overlapping window, a screensaver or a locked workstation corrupts the shot.
// Reading swap-chain buffer 0 after PassUI() and before Present() gives the true
// rendered frame instead. Every failure below skips the frame silently: a capture
// run must never crash or stall because a publication did not work out.
// ---------------------------------------------------------------------------
void App::WritePortfolioBackbufferPng()
{
	auto& writer = m_->m_PortfolioBackbuffer;
	if (!IsReadmeCaptureMode() || writer.requestedPath.empty() || writer.temporaryPath.empty())
		return;
	if (writer.wicUnavailable || !m_->m_pDevice || !m_->m_pDeviceContext || !m_->m_pSwapChain)
		return;

	const ULONGLONG nowMs = GetTickCount64();
	if (nowMs < writer.nextWriteTickMs)
		return;
	writer.nextWriteTickMs = nowMs + kPortfolioBackbufferMinIntervalMs;

	if (!writer.wicFactory)
	{
		if (!writer.comInitializeAttempted)
		{
			writer.comInitializeAttempted = true;
			// Nothing else in this application initializes COM on the render
			// thread and WIC cannot be created without it. RPC_E_CHANGED_MODE
			// means COM is already up in the other apartment model, which serves
			// just as well - and must not be balanced with CoUninitialize here.
			writer.comUninitializeNeeded =
				SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE));
		}

		// Same factory creation as Dx11/third_party/DirectXTK/src/WICTextureLoader.cpp:
		// prefer the WIC2 factory, fall back to WIC1.
		if (FAILED(CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER,
				__uuidof(IWICImagingFactory2),
				reinterpret_cast<void**>(writer.wicFactory.ReleaseAndGetAddressOf()))) &&
			FAILED(CoCreateInstance(CLSID_WICImagingFactory1, nullptr, CLSCTX_INPROC_SERVER,
				__uuidof(IWICImagingFactory),
				reinterpret_cast<void**>(writer.wicFactory.ReleaseAndGetAddressOf()))))
		{
			writer.wicFactory.Reset();
			writer.wicUnavailable = true;
			m_->PushLog("[WARN] Portfolio backbuffer capture: WIC imaging factory unavailable");
			return;
		}
	}

	Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
	if (FAILED(m_->m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()))))
		return;

	D3D11_TEXTURE2D_DESC frameDesc{};
	backBuffer->GetDesc(&frameDesc);

	// Both of these are refusals, not repairs. This swap chain is created
	// single-sampled and, in capture mode, always as DXGI_FORMAT_R8G8B8A8_UNORM,
	// so neither can fire today; if either ever does, skipping every frame makes
	// the capture tool time out and fail the run loudly, which is the only safe
	// outcome. Copying a multisampled back buffer into a staging texture is
	// invalid, and would leave the staging texture holding the previous frame -
	// exactly the silently-wrong publication this writer must never make.
	if (frameDesc.SampleDesc.Count != 1)
		return;
	if (!PortfolioBackbufferWicFormat(frameDesc.Format))
		return;

	// The staging texture lives for the whole capture run; it is rebuilt only if
	// the swap chain's size or format ever changes underneath it.
	if (!writer.staging ||
		writer.stagingWidth != frameDesc.Width ||
		writer.stagingHeight != frameDesc.Height ||
		writer.stagingFormat != frameDesc.Format)
	{
		writer.staging.Reset();
		writer.stagingWidth = 0;
		writer.stagingHeight = 0;
		writer.stagingFormat = DXGI_FORMAT_UNKNOWN;

		D3D11_TEXTURE2D_DESC stagingDesc{};
		stagingDesc.Width = frameDesc.Width;
		stagingDesc.Height = frameDesc.Height;
		stagingDesc.MipLevels = 1;
		stagingDesc.ArraySize = 1;
		stagingDesc.Format = frameDesc.Format;
		stagingDesc.SampleDesc.Count = 1;
		stagingDesc.SampleDesc.Quality = 0;
		stagingDesc.Usage = D3D11_USAGE_STAGING;
		stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		if (FAILED(m_->m_pDevice->CreateTexture2D(&stagingDesc, nullptr, writer.staging.GetAddressOf())))
		{
			writer.staging.Reset();
			return;
		}
		writer.stagingWidth = frameDesc.Width;
		writer.stagingHeight = frameDesc.Height;
		writer.stagingFormat = frameDesc.Format;
	}

	m_->m_pDeviceContext->CopyResource(writer.staging.Get(), backBuffer.Get());

	D3D11_MAPPED_SUBRESOURCE mappedFrame{};
	if (FAILED(m_->m_pDeviceContext->Map(writer.staging.Get(), 0, D3D11_MAP_READ, 0, &mappedFrame)))
		return;

	// The encode grows a std::vector, so it can in principle throw std::bad_alloc.
	// The Map above must be balanced whatever happens, and a capture run must
	// never die because one frame could not be published, so the exception is
	// turned back into the same skipped frame every other failure produces.
	bool encoded = false;
	try
	{
		encoded = EncodePortfolioBackbufferPng(
			writer.wicFactory.Get(), mappedFrame.pData, mappedFrame.RowPitch,
			writer.stagingWidth, writer.stagingHeight, writer.stagingFormat, writer.encodedScratch);
	}
	catch (...)
	{
		encoded = false;
	}
	m_->m_pDeviceContext->Unmap(writer.staging.Get(), 0);
	if (!encoded)
		return;

	PublishPortfolioBackbufferPng(writer.temporaryPath, writer.requestedPath, writer.encodedScratch);
}

// ---------------------------------------------------------------------------
// Shutdown: hand back the D3D and COM objects the writer owns before the device
// is released, and make sure no temporary sibling outlives the process.
// ---------------------------------------------------------------------------
void App::ShutdownPortfolioBackbufferWriter()
{
	auto& writer = m_->m_PortfolioBackbuffer;
	if (!writer.temporaryPath.empty())
	{
		DeleteFileW(writer.temporaryPath.c_str());
	}

	writer.staging.Reset();
	writer.stagingWidth = 0;
	writer.stagingHeight = 0;
	writer.stagingFormat = DXGI_FORMAT_UNKNOWN;
	writer.encodedScratch.clear();
	writer.encodedScratch.shrink_to_fit();
	writer.wicFactory.Reset();

	if (writer.comUninitializeNeeded)
	{
		writer.comUninitializeNeeded = false;
		CoUninitialize();
	}
}
