bool App::LoadModelFromFile(const std::wstring& pathW) {
	// 새 모델델 추가

	// 폴백 텍스처(화이트/블랙/노멀) 생성: 각각 최초 1회만 생성
	auto createFallbackIfNull = [&](ID3D11ShaderResourceView** targetSRV,
		UINT rgba) {
			if (*targetSRV)
				return;
			D3D11_TEXTURE2D_DESC td{};
			td.Width = 1;
			td.Height = 1;
			td.MipLevels = 1;
			td.ArraySize = 1;
			td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			td.SampleDesc.Count = 1;
			td.Usage = D3D11_USAGE_IMMUTABLE;
			td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			D3D11_SUBRESOURCE_DATA sd{};
			sd.pSysMem = &rgba;
			sd.SysMemPitch = sizeof(UINT);
			Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
			HR_T(m_->m_pDevice->CreateTexture2D(&td, &sd, tex.GetAddressOf()));
			D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
			srvd.Format = td.Format;
			srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvd.Texture2D.MipLevels = 1;
			srvd.Texture2D.MostDetailedMip = 0;
			HR_T(m_->m_pDevice->CreateShaderResourceView(tex.Get(), &srvd, targetSRV));
		};
	createFallbackIfNull(&m_->m_pFallbackWhite, 0xFFFFFFFF);
	createFallbackIfNull(&m_->m_pFallbackBlack, 0x000000FF); // a=1
	createFallbackIfNull(&m_->m_pFallbackNormal,
		0x8080FFFF); // (0.5,0.5,1,1) in RGBA8

	// 받은 경로에서 이름, 확장자 추출
	std::wstring ext{ L"" }, fileName{ L"" };
	if (!pathW.empty()) {
		size_t dot = pathW.find_last_of(L'.');
		if (dot != std::wstring::npos) {
			ext = pathW.substr(dot);
			std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
			size_t sep = pathW.find_last_of(L"\\/");
			if (sep != std::wstring::npos)
				fileName = pathW.substr(sep + 1, dot - sep - 1);
			else
				fileName = pathW.substr(0, dot);
		}
	}

	m_fLoadingProgress = 0.35f;

	bool ok = false;
	auto entry = std::make_unique<ModelEntry>();
	entry->modelName = fileName;

	// 캐시 확인
	std::shared_ptr<SharedModelData> shared;
	if (auto it = m_->m_ModelCache.find(pathW); it != m_->m_ModelCache.end())
		shared = it->second.lock();

	m_fLoadingProgress = 0.4f;
	m_sLoadingStr = L"3D 모델에 대한 Geometry, Material을 로드합니다.";

	if (!shared) {
		shared = std::make_shared<SharedModelData>();
		shared->pathW = pathW;
		// 로드 경로에 따라 매니저 준비
		if (ext == L".fbx" || ext == L".gltf" || ext == L".glb") {
			shared->source = ModelSource::FBX;
			// 공용 AssetManager를 통해 FBX 모델 공유/캐시
			shared->fbx = AssetManager::GetInstance().GetFbxModel(m_->m_pDevice, pathW);
			if (ok = (shared->fbx != nullptr)) {
				m_->PushLog("[OK] Loaded FBX(shared): " + Utf8FromWString(fileName));
				shared->stride = shared->fbx->GetVertexStride();
				shared->vb = shared->fbx->GetVertexBuffer();
				shared->ib = shared->fbx->GetIndexBuffer();
				shared->indexCount = shared->fbx->GetIndexCount();
				shared->subsets.clear();
				for (auto& s : shared->fbx->GetSubsets())
					shared->subsets.push_back(
						{ s.startIndex, s.indexCount, s.materialIndex });
				shared->materialSRVs = shared->fbx->GetMaterialSRVs();
				shared->normalSRVs = shared->fbx->GetNormalSRVs();
			}
			else {
				m_->PushLog("[ERR] Failed FBX: " + Utf8FromWString(fileName));
			}
		}
		else if (ext == L".obj") {
			shared->source = ModelSource::OBJ;
			shared->obj = std::make_shared<ObjManager>();
			if (ok = shared->obj->Load(m_->m_pDevice, pathW)) {
				m_->PushLog("[OK] Loaded OBJ(shared): " + Utf8FromWString(fileName));
				shared->stride = shared->obj->GetVertexStride();
				shared->vb = shared->obj->GetVertexBuffer();
				shared->ib = shared->obj->GetIndexBuffer();
				shared->indexCount = shared->obj->GetIndexCount();
				shared->subsets.clear();
				const auto& subs = shared->obj->GetSubsets();
				for (auto& s : subs)
					shared->subsets.push_back(
						{ s.startIndex, s.indexCount, s.materialIndex });
				shared->materialSRVs = shared->obj->GetMaterialSRVs();
			}
			else {
				m_->PushLog("[ERR] Failed OBJ: " + Utf8FromWString(fileName));
			}
		}
		else if (ext == L".pmx") {
			shared->source = ModelSource::PMX;
			shared->pmx = std::make_shared<PmxManager>();
			if (ok = shared->pmx->Load(m_->m_pDevice, pathW)) {
				m_->PushLog("[OK] Loaded PMX(shared): " + Utf8FromWString(fileName));
				shared->stride = shared->pmx->GetVertexStride();
				shared->vb = shared->pmx->GetVertexBuffer();
				shared->ib = shared->pmx->GetIndexBuffer();
				shared->indexCount = shared->pmx->GetIndexCount();
				shared->subsets.clear();
				const auto& subs = shared->pmx->GetSubsets();
				for (auto& s : subs)
					shared->subsets.push_back(
						{ s.startIndex, s.indexCount, s.materialIndex });
				shared->materialSRVs = shared->pmx->GetMaterialSRVs();
			}
			else {
				m_->PushLog("[ERR] Failed PMX: " + Utf8FromWString(fileName));
			}
		}

		if (ok) {
			m_->m_ModelCache[pathW] = shared; // 캐시 등록
		}
	}
	else {
		ok = true;
		m_->PushLog("[OK] Reused cached model: " + Utf8FromWString(fileName));
	}

	m_fLoadingProgress = 0.5f;
	m_sLoadingStr = L"3D 모델의 Skeletal, Bone, Animation을 로드합니다.";


	if (ok && shared) {
		entry->shared = shared;
		entry->source = shared->source;
		// 본 캐시/출력텍스트 일괄 구축
		BuildBoneCacheStructure(*entry, nullptr);
		// 메시 통계는 UI에서 필요할 때 계산
		entry->meshStatsValid = false;
		entry->meshStats = MeshStats{};
		// 모델별 셰이딩 초기값 = 현재 글로벌 셰이딩, 아웃라인 기본은 Toon일 때만 ON
		entry->modelShading = m_->m_ShadingMode;
		entry->outlineEnabled = (entry->modelShading == ShadingMode::ToonShading);
		entry->instancePbrMaterial = m_->m_DefaultPbrMaterial;

		// FBX 인스턴스 애니메이터를 로드 시점에 1회 초기화
		if (entry->source == ModelSource::FBX && entry->shared &&
			entry->shared->fbx) {
			entry->fbxBaseAnimator.InitMetadata(entry->shared->fbx->GetScenePtr());
			entry->fbxBaseAnimator.SetSharedContext(entry->shared->fbx->GetScenePtr(),
				entry->shared->fbx->GetNodeIndexOfName(),
				&entry->shared->fbx->GetBoneNames(),
				&entry->shared->fbx->GetBoneOffsets(),
				&entry->shared->fbx->GetGlobalInverse());
			auto t = entry->shared->fbx->GetCurrentAnimationType();
			entry->fbxBaseAnimator.SetType(t == FbxModel::AnimationType::Rigid
				? FbxAnimation::AnimType::Rigid
				: (t == FbxModel::AnimationType::Skinned
					? FbxAnimation::AnimType::Skinned
					: FbxAnimation::AnimType::None));
			entry->animatorInited = true;
		}

		m_->m_Models.push_back(std::move(entry));
	}
	m_fLoadingProgress = 0.7f;
	return ok;
}

void App::UnloadModel() {
	m_->m_Models.clear();
	m_->m_SelectedModelIdx = -1;
	m_->m_SelectedBoneIdx = -1;
}

