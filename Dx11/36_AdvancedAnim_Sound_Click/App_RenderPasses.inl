void App::OnRender() {

	if (!m_bIsGameStarted)
	{
		// ============= 로딩 화면 ================
		// 이미지를 띄우고 텍스트 출력
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// 백버퍼(BackBuffer)를 검은색으로 클리어
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		m_->m_pDeviceContext->ClearRenderTargetView(m_->m_pRenderTargetView, clearColor);
		// 렌더 타겟을 '백버퍼'로 설정 Depth 버퍼는 2D UI에 필요 없으므로 nullptr
		m_->m_pDeviceContext->OMSetRenderTargets(1, &m_->m_pRenderTargetView, nullptr);

		RenderWaitingUI();
		SkyboxAssetManager::RenderStatusUI();

		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}
	else
	{
		// 1. 초기화 (HDR RT 및 Depth 클리어)
		PassClear();
		// 2. 렌더 패스 실행
		PassShadow();      // 섀도우 맵 생성
		PassMainScene();   // 3D 오브젝트, 스카이박스, 오버레이
		PassPostProcess(); // 톤매핑 (HDR -> BackBuffer)
		// 3. UI 및 Present
		PassUI();
		// README capture only: publish the finished frame from the swap chain
		// before it is presented, so the capture tool reads the true rendered
		// pixels instead of screen-scraping this window.
		WritePortfolioBackbufferPng();
	}

	m_->m_pSwapChain->Present(0, 0);
}

void App::PassClear() {
	float clearColor[4] = { m_->m_ClearColor.x, m_->m_ClearColor.y,
						   m_->m_ClearColor.z, m_->m_ClearColor.w };
	m_->m_pDeviceContext->ClearRenderTargetView(m_->m_pHdrRenderTargetView,
		clearColor);
	m_->m_pDeviceContext->ClearDepthStencilView(m_->m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	m_->m_pDeviceContext->OMSetRenderTargets(1, &m_->m_pHdrRenderTargetView, m_->m_pDepthStencilView);
	m_->m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

// ====================================== 디버그 박스: 각 3D 모델의 로컬 AABB를 선으로 표시 ======================================
void App::PassDebugDraw()
{
	for (auto& mdlPtr : m_->m_Models)
	{
		// AABB 계산 및 그리기

		if (!mdlPtr->boundsValid && mdlPtr->shared) {
			// AABB 계산 로직
			XMFLOAT3 mn, mx;
			if (ComputeLocalAABB(m_->m_pDevice, m_->m_pDeviceContext, mdlPtr->shared->vb, mdlPtr->shared->stride, mn, mx))
			{
				mdlPtr->boundsMin = mn; mdlPtr->boundsMax = mx; mdlPtr->boundsValid = true;
			}
		}
		if (mdlPtr->boundsValid && mdlPtr->source == ModelSource::FBX && mdlPtr->shared && mdlPtr->shared->fbx) {
			// 애니메이션 샘플링
			int curClip = mdlPtr->fbxBaseAnimator.GetCurrentIndex();
			const aiScene* sc = mdlPtr->shared->fbx->GetScenePtr();
			if (sc && curClip >= 0 && (size_t)curClip < sc->mNumAnimations)
			{
				if (mdlPtr->animAabbClip != curClip || mdlPtr->animAabbMinSamples.empty() || mdlPtr->animAabbMaxSamples.empty())
				{
					mdlPtr->animAabbClip = curClip;
					mdlPtr->animAabbMinSamples.clear();
					mdlPtr->animAabbMaxSamples.clear();
					const aiAnimation* a = sc->mAnimations[(size_t)curClip];
					double tps = (a && a->mTicksPerSecond != 0.0) ? a->mTicksPerSecond : 25.0;
					double dur = a ? (a->mDuration / (tps != 0.0 ? tps : 25.0)) : 0.0;
					int sps = 30; // 샘플링 주파수
					int numSamples = (dur > 0.0) ? (int)std::ceil(dur * sps) : 1;
					if (numSamples < 1) numSamples = 1;
					mdlPtr->animAabbSampleDt = (float)(1.0 / (double)sps);
					// 타겟 채널 선택: 루트 노드 이름이 있으면 우선, 없으면 첫 채널
					const char* rootName = sc->mRootNode ? sc->mRootNode->mName.C_Str() : nullptr;
					const aiNodeAnim* ch = nullptr;
					if (a)
					{
						for (unsigned i = 0; i < a->mNumChannels; ++i)
						{
							if (!a->mChannels[i]) continue;
							if (rootName && strcmp(a->mChannels[i]->mNodeName.C_Str(), rootName) == 0) { ch = a->mChannels[i]; break; }
						}
						if (!ch && a->mNumChannels > 0) ch = a->mChannels[0];
					}
					auto vInterp = [](const aiVectorKey* k, unsigned n, double t) { if (n == 0) return aiVector3D(0, 0, 0); if (n == 1) return k[0].mValue; unsigned i = 0; while (i + 1 < n && t >= k[i + 1].mTime) ++i; unsigned j = (i + 1 < n) ? i + 1 : i; double dt = k[j].mTime - k[i].mTime; double a = (dt > 0.0) ? (t - k[i].mTime) / dt : 0.0; aiVector3D v0 = k[i].mValue, v1 = k[j].mValue; return v0 + (float)a * (v1 - v0); };
					auto qInterp = [](const aiQuatKey* k, unsigned n, double t) { if (n == 0) return aiQuaternion(); if (n == 1) return k[0].mValue; unsigned i = 0; while (i + 1 < n && t >= k[i + 1].mTime) ++i; unsigned j = (i + 1 < n) ? i + 1 : i; double dt = k[j].mTime - k[i].mTime; double a = (dt > 0.0) ? (t - k[i].mTime) / dt : 0.0; aiQuaternion q; aiQuaternion::Interpolate(q, k[i].mValue, k[j].mValue, (float)a); q.Normalize(); return q; };
					mdlPtr->animAabbMinSamples.resize((size_t)numSamples);
					mdlPtr->animAabbMaxSamples.resize((size_t)numSamples);
					// 8 코너 미리 구성(로컬)
					auto mn0 = mdlPtr->boundsMin; auto mx0 = mdlPtr->boundsMax;
					XMFLOAT3 corners[8] = {
						{mn0.x, mn0.y, mn0.z}, {mx0.x, mn0.y, mn0.z}, {mx0.x, mn0.y, mx0.z}, {mn0.x, mn0.y, mx0.z},
						{mn0.x, mx0.y, mn0.z}, {mx0.x, mx0.y, mn0.z}, {mx0.x, mx0.y, mx0.z}, {mn0.x, mx0.y, mx0.z}
					};
					for (int si = 0; si < numSamples; ++si)
					{
						double tSec = (double)si * (1.0 / (double)sps);
						double tt = tSec * (tps != 0.0 ? tps : 25.0);
						aiVector3D S = ch ? ((ch->mNumScalingKeys > 0) ? vInterp(ch->mScalingKeys, ch->mNumScalingKeys, tt) : aiVector3D(1, 1, 1)) : aiVector3D(1, 1, 1);
						aiVector3D T = ch ? ((ch->mNumPositionKeys > 0) ? vInterp(ch->mPositionKeys, ch->mNumPositionKeys, tt) : aiVector3D(0, 0, 0)) : aiVector3D(0, 0, 0);
						aiQuaternion R = ch ? ((ch->mNumRotationKeys > 0) ? qInterp(ch->mRotationKeys, ch->mNumRotationKeys, tt) : aiQuaternion()) : aiQuaternion();
						aiMatrix4x4 mS; mS.Scaling(S, mS); aiMatrix4x4 mR = aiMatrix4x4(R.GetMatrix()); aiMatrix4x4 mT; mT.Translation(T, mT);
						aiMatrix4x4 mA = mT * mR * mS;
						XMFLOAT4X4 am; am._11 = (float)mA.a1; am._12 = (float)mA.a2; am._13 = (float)mA.a3; am._14 = (float)mA.a4;
						am._21 = (float)mA.b1; am._22 = (float)mA.b2; am._23 = (float)mA.b3; am._24 = (float)mA.b4;
						am._31 = (float)mA.c1; am._32 = (float)mA.c2; am._33 = (float)mA.c3; am._34 = (float)mA.c4;
						am._41 = (float)mA.d1; am._42 = (float)mA.d2; am._43 = (float)mA.d3; am._44 = (float)mA.d4;
						XMMATRIX A = XMLoadFloat4x4(&am);
						// 8 코너 변환 후 min/max
						XMFLOAT3 mn{ FLT_MAX, FLT_MAX, FLT_MAX };
						XMFLOAT3 mx{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
						for (int ci = 0; ci < 8; ++ci)
						{
							XMVECTOR p = XMVectorSet(corners[ci].x, corners[ci].y, corners[ci].z, 1.0f);
							p = XMVector4Transform(p, A);
							XMFLOAT4 pf; XMStoreFloat4(&pf, p);
							mn.x = (std::min)(mn.x, pf.x); mn.y = (std::min)(mn.y, pf.y); mn.z = (std::min)(mn.z, pf.z);
							mx.x = (std::max)(mx.x, pf.x); mx.y = (std::max)(mx.y, pf.y); mx.z = (std::max)(mx.z, pf.z);
						}
						mdlPtr->animAabbMinSamples[(size_t)si] = mn;
						mdlPtr->animAabbMaxSamples[(size_t)si] = mx;
					}
				}
			}

		}

		if (mdlPtr->boundsValid && m_->m_LineRenderer && m_->m_pLineVS && m_->m_pLineInputLayout)
		{
			//라인 렌더링
			XMMATRIX S = XMMatrixScaling(mdlPtr->scale.x, mdlPtr->scale.y, mdlPtr->scale.z);
			XMMATRIX R = XMMatrixRotationQuaternion(XMQuaternionRotationRollPitchYaw(
				XMConvertToRadians(mdlPtr->rotDeg.x),
				XMConvertToRadians(mdlPtr->rotDeg.y),
				XMConvertToRadians(mdlPtr->rotDeg.z)
			));
			XMMATRIX T = XMMatrixTranslation(mdlPtr->pos.x, mdlPtr->pos.y, mdlPtr->pos.z);
			XMMATRIX W = S * R * T;
			// ConstantBuffer 설정, DrawLine 호출 등
			ID3D11Buffer* cbBones = mdlPtr->fbxBaseAnimator.GetBoneCB();
			// 라인용 상수버퍼: 이 모델의 월드 행렬 사용
			ConstantBuffer lineCB = m_->m_ConstantBuffer;
			lineCB.world = XMMatrixTranspose(W);
			lineCB.worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranspose(W)));
			lineCB.view = m_->m_baseProjection.view;
			lineCB.proj = m_->m_baseProjection.proj;
			lineCB.pad = 3.0f; // 라인 마커용
			int useBoneIdx = -1;
			if (cbBones != nullptr)
			{
				useBoneIdx = (mdlPtr->boundsBoneIndex >= 0) ? mdlPtr->boundsBoneIndex : -1;
			}
			lineCB.boundsBoneIndex = useBoneIdx;
			D3D11_MAPPED_SUBRESOURCE mappedLine;
			HR_T(m_->m_pDeviceContext->Map(m_->m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedLine));
			memcpy_s(mappedLine.pData, sizeof(ConstantBuffer), &lineCB, sizeof(ConstantBuffer));
			m_->m_pDeviceContext->Unmap(m_->m_pConstantBuffer, 0);
			m_->m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
			m_->m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);

			// 라인 VS/IL/b1 바인딩 저장/설정
			ID3D11VertexShader* prevVS = nullptr; m_->m_pDeviceContext->VSGetShader(&prevVS, nullptr, nullptr);
			ID3D11Buffer* prevVSb1 = nullptr; m_->m_pDeviceContext->VSGetConstantBuffers(1, 1, &prevVSb1);
			ID3D11InputLayout* prevIL = nullptr; m_->m_pDeviceContext->IAGetInputLayout(&prevIL);
			m_->m_pDeviceContext->VSSetShader(m_->m_pLineVS, nullptr, 0);
			m_->m_pDeviceContext->IASetInputLayout(m_->m_pLineInputLayout);
			// 선택 본 인덱스가 유효하면 스키닝 팔레트(b1)를 바인딩하여 본 변환을 적용
			if (useBoneIdx >= 0 && cbBones) m_->m_pDeviceContext->VSSetConstantBuffers(1, 1, &cbBones);

			// 로컬 AABB 8 코너 계산
			auto mn = mdlPtr->boundsMin; auto mx = mdlPtr->boundsMax;
			// 팔레트 기반 본 선택(-1 아님)일 때는 샘플 AABB를 사용하지 않음
			if (useBoneIdx < 0 && !mdlPtr->animAabbMinSamples.empty() && !mdlPtr->animAabbMaxSamples.empty() && mdlPtr->animAabbSampleDt > 0.0f)
			{
				double t = mdlPtr->fbxBaseAnimator.GetTimeSec();
				int idx = (int)std::floor(t / (double)mdlPtr->animAabbSampleDt + 0.5);
				if (idx < 0) idx = 0; if (idx >= (int)mdlPtr->animAabbMinSamples.size()) idx = (int)mdlPtr->animAabbMinSamples.size() - 1;
				mn = mdlPtr->animAabbMinSamples[(size_t)idx];
				mx = mdlPtr->animAabbMaxSamples[(size_t)idx];
			}
			XMFLOAT3 c[8] = {
				{mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z}, {mx.x, mn.y, mx.z}, {mn.x, mn.y, mx.z},
				{mn.x, mx.y, mn.z}, {mx.x, mx.y, mn.z}, {mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z}
			};
			XMFLOAT4 col = XMFLOAT4(1, 1, 0, 1); // 노란색
			// 12 엣지 드로우
			auto Draw = [&](const int& i, const int& j) {
				m_->m_LineRenderer->DrawLine(m_->m_pDeviceContext, c[i], c[j], col, m_->m_pLineInputLayout, m_->m_pLineVS, m_->m_pPixelShader, m_->m_pConstantBuffer);
				};
			// 박스 면 4개 선 그리기 (밑면)
			Draw(0, 1); Draw(1, 2); Draw(2, 3); Draw(3, 0);
			// 박스 면 4개 선 그리기 (윗면)
			Draw(4, 5); Draw(5, 6); Draw(6, 7); Draw(7, 4);
			// 박스 세로 4개 선 그리기 (밑면과 윗면 연결)
			Draw(0, 4); Draw(1, 5); Draw(2, 6); Draw(3, 7);

			// VS/IL/b1 복원
			if (prevVS) { m_->m_pDeviceContext->VSSetShader(prevVS, nullptr, 0); prevVS->Release(); }
			if (prevVSb1) { m_->m_pDeviceContext->VSSetConstantBuffers(1, 1, &prevVSb1); prevVSb1->Release(); }
			if (prevIL) { m_->m_pDeviceContext->IASetInputLayout(prevIL); prevIL->Release(); }
		}
	}

	// ====================================== SoundBox 디버그 드로우 ======================================
	if (m_->m_LineRenderer && m_->m_pLineVS && m_->m_pLineInputLayout)
	{
		const auto& soundBoxes = m_->m_SoundBoxSystem.GetBoxes();
		for (const auto& box : soundBoxes)
		{
			// SoundBox의 World AABB 계산
			XMFLOAT3 worldMin, worldMax;

			float x0 = box.boundsMin.x * box.scale.x + box.position.x;
			float x1 = box.boundsMax.x * box.scale.x + box.position.x;
			worldMin.x = std::min(x0, x1);
			worldMax.x = (std::max)(x0, x1);

			float y0 = box.boundsMin.y * box.scale.y + box.position.y;
			float y1 = box.boundsMax.y * box.scale.y + box.position.y;
			worldMin.y = std::min(y0, y1);
			worldMax.y = (std::max)(y0, y1);

			float z0 = box.boundsMin.z * box.scale.z + box.position.z;
			float z1 = box.boundsMax.z * box.scale.z + box.position.z;
			worldMin.z = std::min(z0, z1);
			worldMax.z = (std::max)(z0, z1);

			// World AABB의 8개 코너 계산
			XMFLOAT3 corners[8] = {
				{worldMin.x, worldMin.y, worldMin.z}, {worldMax.x, worldMin.y, worldMin.z},
				{worldMax.x, worldMin.y, worldMax.z}, {worldMin.x, worldMin.y, worldMax.z},
				{worldMin.x, worldMax.y, worldMin.z}, {worldMax.x, worldMax.y, worldMin.z},
				{worldMax.x, worldMax.y, worldMax.z}, {worldMin.x, worldMax.y, worldMax.z}
			};

			// SoundBox는 빨간색으로 그리기 (모델 AABB와 구분)
			XMFLOAT4 boxColor = XMFLOAT4(1, 0, 0, 1); // 빨간색

			// 라인 VS/IL 바인딩 저장
			ID3D11VertexShader* prevVS = nullptr;
			m_->m_pDeviceContext->VSGetShader(&prevVS, nullptr, nullptr);
			ID3D11InputLayout* prevIL = nullptr;
			m_->m_pDeviceContext->IAGetInputLayout(&prevIL);

			// 라인 렌더링용 셰이더 바인딩
			m_->m_pDeviceContext->VSSetShader(m_->m_pLineVS, nullptr, 0);
			m_->m_pDeviceContext->IASetInputLayout(m_->m_pLineInputLayout);

			// SoundBox는 월드 공간이므로 단위 행렬 사용
			ConstantBuffer lineCB = m_->m_ConstantBuffer;
			XMMATRIX identity = XMMatrixIdentity();
			lineCB.world = XMMatrixTranspose(identity);
			lineCB.worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, identity));
			lineCB.view = m_->m_baseProjection.view;
			lineCB.proj = m_->m_baseProjection.proj;
			lineCB.pad = 3.0f; // 라인 마커용

			D3D11_MAPPED_SUBRESOURCE mappedLine;
			HR_T(m_->m_pDeviceContext->Map(m_->m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedLine));
			memcpy_s(mappedLine.pData, sizeof(ConstantBuffer), &lineCB, sizeof(ConstantBuffer));
			m_->m_pDeviceContext->Unmap(m_->m_pConstantBuffer, 0);
			m_->m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
			m_->m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);

			// 12개 엣지 그리기
			auto DrawEdge = [&](const int& i, const int& j) {
				m_->m_LineRenderer->DrawLine(m_->m_pDeviceContext, corners[i], corners[j], boxColor,
					m_->m_pLineInputLayout, m_->m_pLineVS, m_->m_pPixelShader, m_->m_pConstantBuffer);
				};

			// 밑면 4개 선
			DrawEdge(0, 1); DrawEdge(1, 2); DrawEdge(2, 3); DrawEdge(3, 0);
			// 윗면 4개 선
			DrawEdge(4, 5); DrawEdge(5, 6); DrawEdge(6, 7); DrawEdge(7, 4);
			// 세로 4개 선 (밑면과 윗면 연결)
			DrawEdge(0, 4); DrawEdge(1, 5); DrawEdge(2, 6); DrawEdge(3, 7);

			// VS/IL 복원
			if (prevVS) { m_->m_pDeviceContext->VSSetShader(prevVS, nullptr, 0); prevVS->Release(); }
			if (prevIL) { m_->m_pDeviceContext->IASetInputLayout(prevIL); prevIL->Release(); }
		}
	}
}

void App::PassShadow() {
	// 그림자 옵션이 꺼져있거나 모델이 없으면 패스
	if (!m_->m_ShadowEnabled || m_->m_Models.empty() || !m_->m_pShadowDSV)
		return;

	// 4번 슬롯에 묶인 섀도우 맵 SRV를 제거해주고 시작함.
	// 이전 프레임의 PassMainScene 단계에서 섀도우 맵 텍스처가
	// Pixel Shader의 4번 슬롯(Input)에 바인딩 되어 있음.
	// 다음 프레임의 PassShadow 단계가 시작될 때, 이 텍스처를 Depth Stencil로 output으로 설정하려고 하니
	// DirectX가 읽고 있는 중인 자원에 쓸 수 없다며 경고를 띄움. 이를 해결하기 위해 여기서 4번 슬롯을 해제하는걸로 함.
	ID3D11ShaderResourceView* const nullSRV[1] = { nullptr };
	m_->m_pDeviceContext->PSSetShaderResources(4, 1, nullSRV);

	// 1. 섀도우 맵 깊이 타겟 설정 (Color Target은 불필요하므로 nullptr)
	ID3D11RenderTargetView* nullRTV = nullptr;
	m_->m_pDeviceContext->OMSetRenderTargets(0, &nullRTV, m_->m_pShadowDSV);
	m_->m_pDeviceContext->ClearDepthStencilView(m_->m_pShadowDSV,
		D3D11_CLEAR_DEPTH, 1.0f, 0);

	// 2. 뷰포트 및 라스터라이저 설정
	m_->m_pDeviceContext->RSSetViewports(1, &m_->m_ShadowViewport);
	if (m_->RSShadowBias)
		m_->m_pDeviceContext->RSSetState(m_->RSShadowBias);

	// 3. 셰이더 설정 (PS는 필요 없음)
	m_->m_pDeviceContext->PSSetShader(nullptr, nullptr, 0);

	// 4. 모델 렌더링
	for (const auto& mdl : m_->m_Models) {
		if (!mdl->shared || !mdl->shared->vb)
			continue;

		// 월드 행렬 계산
		XMMATRIX S = XMMatrixScaling(mdl->scale.x, mdl->scale.y, mdl->scale.z);
		XMMATRIX R = XMMatrixRotationQuaternion(XMQuaternionRotationRollPitchYaw(
			XMConvertToRadians(mdl->rotDeg.x), XMConvertToRadians(mdl->rotDeg.y),
			XMConvertToRadians(mdl->rotDeg.z)));
		XMMATRIX T = XMMatrixTranslation(mdl->pos.x, mdl->pos.y, mdl->pos.z);
		XMMATRIX W = S * R * T;

		// 상수 버퍼 업데이트 (Light ViewProj 사용)
		ConstantBuffer cb = m_->m_ConstantBuffer;
		cb.world = XMMatrixTranspose(W);
		cb.lightViewProj = m_->m_baseProjection.lightViewProj;
		UpdateCB(m_->m_pConstantBuffer, cb);
		m_->m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);

		// 스키닝 여부 확인 및 셰이더 선택
		bool hasSkeleton = false;
		if (mdl->source == ModelSource::FBX && mdl->shared->fbx)
			hasSkeleton = mdl->shared->fbx->HasSkeleton();
		else if (mdl->source == ModelSource::PMX && mdl->shared->pmx)
			hasSkeleton = mdl->shared->pmx->HasSkeleton();

		bool useSkin =
			hasSkeleton && m_->m_pVSSkinnedShadow && mdl->fbxBaseAnimator.GetBoneCB();

		if (useSkin) {
			m_->m_pDeviceContext->IASetInputLayout(m_->m_pInputLayoutSkinned);
			m_->m_pDeviceContext->VSSetShader(m_->m_pVSSkinnedShadow, nullptr, 0);
			ID3D11Buffer* boneCB = (mdl->source == ModelSource::PMX)
				? mdl->shared->pmx->GetBoneConstantBuffer()
				: mdl->fbxBaseAnimator.GetBoneCB();
			m_->m_pDeviceContext->VSSetConstantBuffers(1, 1, &boneCB);
		}
		else {
			m_->m_pDeviceContext->IASetInputLayout(m_->m_pInputLayout);
			m_->m_pDeviceContext->VSSetShader(m_->m_pVSShadow, nullptr, 0);
		}

		// 그리기
		UINT stride = mdl->shared->stride;
		UINT offset = 0;
		m_->m_pDeviceContext->IASetVertexBuffers(0, 1, &mdl->shared->vb, &stride,
			&offset);
		m_->m_pDeviceContext->IASetIndexBuffer(mdl->shared->ib,
			DXGI_FORMAT_R32_UINT, 0);

		for (const auto& sub : mdl->shared->subsets)
			m_->m_pDeviceContext->DrawIndexed(sub.count, sub.start, 0);
	}

	// 상태 복구 (RSState만 nullptr로, 뷰포트는 다음 패스에서 재설정)
	m_->m_pDeviceContext->RSSetState(nullptr);
}

void App::PassMainScene() {
	// 1. 뷰포트를 클라이언트 윈도우 크기로 복구
	D3D11_VIEWPORT vp{};
	vp.Width = static_cast<float>(m_ClientWidth);
	vp.Height = static_cast<float>(m_ClientHeight);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	m_->m_pDeviceContext->RSSetViewports(1, &vp);

	// 디퍼드 렌더링 모드 확인
	if (m_->m_UseDeferredRendering) {
		// ========== G-Buffer 패스 ==========
		PassGBuffer();

		// ========== 라이트 패스 ==========
		PassDeferredLight();

		m_->m_pDeviceContext->OMSetDepthStencilState(m_->m_pDepthStencilState, 0);

		// ========== 스카이박스 렌더링 (포워드) ==========
		// Light 패스가 끝난 후 깊이 버퍼는 G-Buffer의 깊이 정보를 유지하고 있음
		// 스카이박스는 빈 공간(Depth=1.0)에만 그려짐
		if (m_->m_SkyBoxChoice != App::Impl::SkyBoxChoice::Off && m_->m_Skybox) {
			// 혹시 모를 타겟 해제 방지를 위해 렌더 타겟 재설정
			// m_->m_pDeviceContext->OMSetRenderTargets(1,
			// &m_->m_pHdrRenderTargetView, m_->m_pDepthStencilView);
			m_->m_Skybox->Render(
				m_->m_pDeviceContext, m_->m_pVertexBuffer, m_->m_pIndexBuffer,
				m_->m_nIndices, m_->m_VertextBufferStride, m_->m_VertextBufferOffset,
				m_->m_baseProjection.view, m_->m_baseProjection.proj);
		}

		// ========== Axis Overlay ==========
		{
			ConstantBuffer overlayCB = m_->m_ConstantBuffer;
			overlayCB.world = XMMatrixTranspose(XMMatrixIdentity());
			overlayCB.view = XMMatrixTranspose(XMMatrixIdentity());
			overlayCB.proj = XMMatrixTranspose(XMMatrixIdentity());
			overlayCB.worldInvTranspose = XMMatrixIdentity();
			overlayCB.pad = 3.0f;
			overlayCB.shadingMode = (int)m_->m_ShadingMode;

			UpdateCB(m_->m_pConstantBuffer, overlayCB);
			m_->m_LineRenderer->DrawAxesOverlay(
				m_->m_pDeviceContext, XMMatrixTranspose(m_->m_baseProjection.view),
				DirectX::XMFLOAT2(-0.9f, 0.85f), 0.08f, m_->m_pLineInputLayout,
				m_->m_pLineVS, m_->m_pPixelShader, m_->m_pConstantBuffer);
		}
	}

	m_->m_pDeviceContext->OMSetRenderTargets(1, &m_->m_pHdrRenderTargetView,
		m_->m_pDepthStencilView);
	m_->m_pDeviceContext->ClearDepthStencilView(
		m_->m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f,
		0);
	// 포워드 렌더링
	m_->m_pDeviceContext->VSSetShader(m_->m_pVertexShader, nullptr, 0);
	m_->m_pDeviceContext->PSSetShader(m_->m_pPixelShader, nullptr, 0);

	// 1. 전역 상수 버퍼 설정
	{
		ConstantBuffer& cb = m_->m_ConstantBuffer;
		cb.world = m_->m_baseProjection.world;
		cb.view = m_->m_baseProjection.view;
		cb.proj = m_->m_baseProjection.proj;
		cb.lightViewProj = m_->m_baseProjection.lightViewProj;
		cb.shadowBias = m_->m_ShadowBias;
		cb.shadowMapSize = (float)m_->m_ShadowSize;
		cb.shadowPCFRadius = m_->m_ShadowPCFRadius;
		cb.shadowEnabled = m_->m_ShadowEnabled;
		cb.worldInvTranspose =
			XMMatrixTranspose(XMMatrixInverse(nullptr, m_->m_baseProjection.world));

		XMFLOAT3 lightDir = m_->m_DirLight.direction;
		XMStoreFloat3(&lightDir, XMVector3Normalize(XMLoadFloat3(&lightDir)));
		cb.dirLight = m_->m_DirLight;
		cb.dirLight.direction = lightDir;

		cb.eyePos = m_Camera.GetPosition();
		cb.pad = 0.0f;
		cb.useTextureColor =
			m_->m_UseTextureColor; // 리팩토링 시 누락되었던 부분 복구

		const PBRMaterialCPU& defPbr = m_->m_DefaultPbrMaterial;
		cb.pbrBaseColor = defPbr.baseColor;
		cb.pbrMetalness = defPbr.metalness;
		cb.pbrRoughness = defPbr.roughness;
		cb.pbrAO = defPbr.ambientOcclusion;
		cb.shadingMode = (int)m_->m_ShadingMode;
		// 툰은 모델별로만 켜진다. 여기(전역 기본값)는 항상 꺼둬야 바닥/큐브/
		// 오버레이처럼 이 상수 버퍼를 그대로 물려받는 드로우콜이 툰에 물들지 않는다.
		cb.toonEnabled = 0;
		cb.enableNormalMap = 0;
		cb.useSpecularMap = 0;
		cb.useDiffuseMap = 1;

		cb.outlineThickness = m_->m_OutlineThickness;
		cb.outlineColor = m_->m_OutlineColor;
		cb.outlineStrength = m_->m_OutlineStrength;
		cb.material = m_->m_Material;

		UpdateCB(m_->m_pConstantBuffer, cb);
	}

	// 공통 리소스 바인딩
	m_->m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
	m_->m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
	m_->m_pDeviceContext->PSSetSamplers(0, 1, &m_->m_pSamplerState);
	if (m_->m_pShadowSampler) m_->m_pDeviceContext->PSSetSamplers(1, 1, &m_->m_pShadowSampler);
	if (m_->m_pShadowSRV) m_->m_pDeviceContext->PSSetShaderResources(4, 1, &m_->m_pShadowSRV);

	// 큐브맵(t1)과 IBL(t5~7) 바인딩.
	// 이후 루프에서 t1은 건드리면 안 됨.
	m_->m_pDeviceContext->PSSetShaderResources(1, 1, &m_->m_pTextureSRV);
	if (m_->m_pIblDiffuseSRV) m_->m_pDeviceContext->PSSetShaderResources(5, 1, &m_->m_pIblDiffuseSRV);
	if (m_->m_pIblSpecularSRV) m_->m_pDeviceContext->PSSetShaderResources(6, 1, &m_->m_pIblSpecularSRV);
	if (m_->m_pIblBrdfLutSRV) m_->m_pDeviceContext->PSSetShaderResources(7, 1, &m_->m_pIblBrdfLutSRV);

	// 2. 큐브 렌더링
	UINT stride = m_->m_VertextBufferStride;
	UINT offset = m_->m_VertextBufferOffset;
	for (auto& objPtr : m_->m_Objects) {
		if (!objPtr || objPtr->kind != ObjectKind::Cube)
			continue;
		auto* cubeObj = static_cast<CubeObject*>(objPtr.get());
		const Transform& mc = cubeObj->cubeTransform;

		m_->m_pDeviceContext->IASetVertexBuffers(0, 1, &m_->m_pVertexBuffer,
			&stride, &offset);
		m_->m_pDeviceContext->IASetInputLayout(m_->m_pInputLayout);
		m_->m_pDeviceContext->IASetIndexBuffer(m_->m_pIndexBuffer,
			DXGI_FORMAT_R32_UINT, 0);

		ConstantBuffer cb = m_->m_ConstantBuffer;
		XMMATRIX Sm = XMMatrixScaling(mc.scale.x, mc.scale.y, mc.scale.z);
		XMMATRIX Rm = XMMatrixRotationQuaternion(
			XMQuaternionRotationRollPitchYaw(XMConvertToRadians(mc.rotationDeg.x),
				XMConvertToRadians(mc.rotationDeg.y),
				XMConvertToRadians(mc.rotationDeg.z)));
		XMMATRIX Tm = XMMatrixTranslation(mc.position.x, mc.position.y, mc.position.z);
		XMMATRIX W = Sm * Rm * Tm;

		cb.world = XMMatrixTranspose(W);
		cb.worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranspose(W)));
		cb.material.ambient = cubeObj->matAmbient;
		cb.material.diffuse = cubeObj->matDiffuse;
		cb.material.specular = cubeObj->matSpecular;
		cb.material.reflect = cubeObj->matReflect;
		cb.pad = 0.0f;
		cb.shadingMode = (int)m_->m_ShadingMode;
		cb.enableNormalMap = (cubeObj->useNormalMap != 0) ? 1 : 0;
		cb.useSpecularMap = (cubeObj->useSpecularMap != 0) ? 1 : 0;
		const PBRMaterialCPU& cubePbr = m_->m_DefaultPbrMaterial;
		cb.pbrBaseColor = cubePbr.baseColor;
		cb.pbrMetalness = cubePbr.metalness;
		cb.pbrRoughness = cubePbr.roughness;
		cb.pbrAO = cubePbr.ambientOcclusion;

		for (int face = 0; face < 6; ++face) {
			bool isTexCube = (cubeObj->cubeType == ECubeType::Texture);
			ID3D11ShaderResourceView* srvDiffuse = cubeObj->faceSRV[face]
				? cubeObj->faceSRV[face] : m_->m_pFallbackWhite;
			ID3D11ShaderResourceView* srvNormal =
				(isTexCube && cubeObj->useNormalMap != 0)
				? (cubeObj->normalSRV[face] ? cubeObj->normalSRV[face] : m_->m_pFallbackNormal)
				: nullptr;
			ID3D11ShaderResourceView* srvSpec =
				(isTexCube && cubeObj->useSpecularMap != 0)
				? (cubeObj->specSRV[face] ? cubeObj->specSRV[face] : m_->m_pFallbackWhite) : nullptr;

			cb.useDiffuseMap = (cubeObj->faceSRV[face] != nullptr) ? 1 : 0;
			cb.enableNormalMap = (isTexCube && cubeObj->useNormalMap != 0) ? 1 : 0;
			cb.useSpecularMap = (isTexCube && cubeObj->useSpecularMap != 0) ? 1 : 0;

			UpdateCB(m_->m_pConstantBuffer, cb);

			// 여기서 0번부터 4개를 한번에 바인딩하면 t1(큐브맵)이 nullptr로
			// 덮어씌워져 반사가 깨짐. 반드시 t0, t2, t3를 개별(혹은 t1을 피해) 바인딩
			// 해야 함.
			m_->m_pDeviceContext->PSSetShaderResources(0, 1, &srvDiffuse);
			m_->m_pDeviceContext->PSSetShaderResources(2, 1, &srvNormal);
			m_->m_pDeviceContext->PSSetShaderResources(3, 1, &srvSpec);

			m_->m_pDeviceContext->DrawIndexed(6, face * 6, 0);
		}
	}

	// 3. 3D 모델 렌더링
	if (!m_->m_Models.empty()) {
		for (auto& mdlPtr : m_->m_Models) {
			if (!mdlPtr->shared || !mdlPtr->shared->vb || !mdlPtr->shared->ib)
				continue;
			UINT s = mdlPtr->shared->stride;
			UINT o = 0;
			m_->m_pDeviceContext->IASetVertexBuffers(0, 1, &mdlPtr->shared->vb, &s, &o);

			// 스켈레톤/쉐이더 선택
			ID3D11Buffer* cbBones = nullptr;
			bool hasSkeleton = false;
			if (mdlPtr->source == ModelSource::FBX && mdlPtr->shared->fbx) {
				cbBones = mdlPtr->fbxBaseAnimator.GetBoneCB();
				hasSkeleton = mdlPtr->shared->fbx->HasSkeleton();
			}
			else if (mdlPtr->source == ModelSource::PMX && mdlPtr->shared->pmx) {
				cbBones = mdlPtr->shared->pmx->GetBoneConstantBuffer();
				hasSkeleton = mdlPtr->shared->pmx->HasSkeleton();
			}
			bool useSkinned = hasSkeleton && (s == sizeof(VertexSkinnedTBN)) && m_->m_pInputLayoutSkinned && m_->m_pVertexShaderSkinned && cbBones;

			if (useSkinned) {
				m_->m_pDeviceContext->IASetInputLayout(m_->m_pInputLayoutSkinned);
				m_->m_pDeviceContext->VSSetShader(m_->m_pVertexShaderSkinned, nullptr, 0);
				if (cbBones) {
					if (mdlPtr->source == ModelSource::PMX && mdlPtr->shared->pmx && !mdlPtr->shared->pmx->HasAnimations())
						mdlPtr->shared->pmx->UploadIdentityPalette(m_->m_pDeviceContext);
					m_->m_pDeviceContext->VSSetConstantBuffers(1, 1, &cbBones);
				}
			}
			else {
				m_->m_pDeviceContext->IASetInputLayout(m_->m_pInputLayout);
				m_->m_pDeviceContext->VSSetShader(m_->m_pVertexShader, nullptr, 0);
				ID3D11Buffer* nullCB = nullptr;
				m_->m_pDeviceContext->VSSetConstantBuffers(1, 1, &nullCB);
			}
			m_->m_pDeviceContext->IASetIndexBuffer(mdlPtr->shared->ib,
				DXGI_FORMAT_R32_UINT, 0);

			XMMATRIX W = mdlPtr->GetWorldMatrix();

			ConstantBuffer cb = m_->m_ConstantBuffer;
			cb.world = XMMatrixTranspose(W);
			cb.worldInvTranspose =
				XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranspose(W)));
			cb.material = (mdlPtr->useInstanceMaterial ? mdlPtr->instanceMaterial : m_->m_Material);
			cb.shadingMode = (int)mdlPtr->modelShading;
			cb.toonEnabled = mdlPtr->useToonShading ? 1 : 0;
			if (!mdlPtr->useInstancePbrMaterial)
			{
				cb.enableNormalMap = m_->m_EnableNormalMapForCube;
				cb.useSpecularMap = m_->m_EnableNormalMapForCube;
			}
			else
			{
				cb.enableNormalMap = mdlPtr->useNormalMap;
				cb.useSpecularMap = mdlPtr->useSpecularMap;
			}

			const PBRMaterialCPU& activePbr = mdlPtr->useInstancePbrMaterial ? mdlPtr->instancePbrMaterial : m_->m_DefaultPbrMaterial;
			cb.pbrBaseColor = activePbr.baseColor;
			cb.pbrMetalness = activePbr.metalness;
			cb.pbrRoughness = activePbr.roughness;
			cb.pbrAO = activePbr.ambientOcclusion;

			const auto& subsets = mdlPtr->shared->subsets;
			const auto& passOrder = mdlPtr->shared->materialPassOrder;
			const bool hasPassOrder = passOrder.size() == subsets.size();
			const uint32_t firstBlendSubset = hasPassOrder
				? mdlPtr->shared->firstBlendSubset
				: static_cast<uint32_t>(subsets.size());

			m_->m_pDeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffffu);
			m_->m_pDeviceContext->OMSetDepthStencilState(
				m_->m_pDepthStencilState, 0);

			for (uint32_t passIndex = 0;
				passIndex < static_cast<uint32_t>(subsets.size());
				++passIndex) {
				if (passIndex == firstBlendSubset) {
					m_->m_pDeviceContext->OMSetBlendState(
						m_->m_pAlphaBlendState, nullptr, 0xffffffffu);
					m_->m_pDeviceContext->OMSetDepthStencilState(
						m_->m_pDepthStencilStateReadOnly, 0);
				}

				const uint32_t subsetIndex =
					hasPassOrder ? passOrder[passIndex] : passIndex;
				const auto& sub = subsets[subsetIndex];
				ID3D11ShaderResourceView* srvDiffuse = nullptr;
				if (mdlPtr->shared && sub.materialIndex < mdlPtr->shared->materialSRVs.size())
					srvDiffuse = mdlPtr->shared->materialSRVs[sub.materialIndex];
				if (!srvDiffuse)
					srvDiffuse = m_->m_pFallbackWhite;

				ID3D11ShaderResourceView* srvNormal = nullptr;
				if (mdlPtr->shared && sub.materialIndex < mdlPtr->shared->normalSRVs.size())
					srvNormal = mdlPtr->shared->normalSRVs[sub.materialIndex];
				if (!srvNormal)
					srvNormal = m_->m_pFallbackNormal;
				ID3D11ShaderResourceView* srvSpec = (m_->m_UseSpecularMapForCube != 0) ? m_->m_pFallbackWhite : nullptr;

				const ModelMaterialProcessing::MaterialAlphaInfo alphaInfo =
					sub.materialIndex < mdlPtr->shared->materialAlphaInfos.size()
					? mdlPtr->shared->materialAlphaInfos[sub.materialIndex]
					: ModelMaterialProcessing::MaterialAlphaInfo{};
				cb.materialAlphaMode = static_cast<int>(alphaInfo.mode);
				cb.materialAlphaCutoff = alphaInfo.cutoff;
				UpdateCB(m_->m_pConstantBuffer, cb);

				// 개별로 바인딩함
				m_->m_pDeviceContext->PSSetShaderResources(0, 1, &srvDiffuse);
				m_->m_pDeviceContext->PSSetShaderResources(2, 1, &srvNormal);
				m_->m_pDeviceContext->PSSetShaderResources(3, 1, &srvSpec);
				m_->m_pDeviceContext->DrawIndexed(sub.count, sub.start, 0);
			}

			// 뒤의 스카이박스/오버레이 패스에 불투명 상태를 돌려준다.
			m_->m_pDeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffffu);
			m_->m_pDeviceContext->OMSetDepthStencilState(
				m_->m_pDepthStencilState, 0);
		}
	}

	// VS 복구
	m_->m_pDeviceContext->VSSetShader(m_->m_pVertexShader, nullptr, 0);
	ID3D11Buffer* nullCB = nullptr;
	m_->m_pDeviceContext->VSSetConstantBuffers(1, 1, &nullCB);

	// 4. SkyBox 렌더링
	if (m_->m_SkyBoxChoice != App::Impl::SkyBoxChoice::Off && m_->m_Skybox) {
		m_->m_Skybox->Render(m_->m_pDeviceContext, m_->m_pVertexBuffer,
			m_->m_pIndexBuffer, m_->m_nIndices,
			m_->m_VertextBufferStride, m_->m_VertextBufferOffset,
			m_->m_baseProjection.view, m_->m_baseProjection.proj);
	}

	// 5. Axis Overlay
	{
		ConstantBuffer overlayCB = m_->m_ConstantBuffer;
		overlayCB.world = XMMatrixTranspose(XMMatrixIdentity());
		overlayCB.view = XMMatrixTranspose(XMMatrixIdentity());
		overlayCB.proj = XMMatrixTranspose(XMMatrixIdentity());
		overlayCB.worldInvTranspose = XMMatrixIdentity();
		overlayCB.pad = 3.0f;
		overlayCB.shadingMode = (int)m_->m_ShadingMode;

		UpdateCB(m_->m_pConstantBuffer, overlayCB);
		m_->m_LineRenderer->DrawAxesOverlay(
			m_->m_pDeviceContext, XMMatrixTranspose(m_->m_baseProjection.view),
			DirectX::XMFLOAT2(-0.9f, 0.85f), 0.08f, m_->m_pLineInputLayout,
			m_->m_pLineVS, m_->m_pPixelShader, m_->m_pConstantBuffer);
	}
}

// G-Buffer 패스: 지오메트리 정보를 G-Buffer에 렌더링
void App::PassGBuffer() {
	// 2. G-Buffer 클리어 및 타겟 설정
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };  // BaseColor Alpha=0
	float clearNormal[4] = { 0.5f, 0.5f, 0.5f, 1.0f }; // Normal 0 vector

	// 루프 대신 memset이나 fill을 사용하지 않고 처리
	m_->m_pDeviceContext->ClearRenderTargetView(m_->m_pGBufferRTVs[0].Get(), clearColor); // Pos
	m_->m_pDeviceContext->ClearRenderTargetView(m_->m_pGBufferRTVs[1].Get(), clearNormal); // Normal
	m_->m_pDeviceContext->ClearRenderTargetView(m_->m_pGBufferRTVs[2].Get(), clearColor); // Metal
	m_->m_pDeviceContext->ClearRenderTargetView(m_->m_pGBufferRTVs[3].Get(), clearColor); // Rough
	m_->m_pDeviceContext->ClearRenderTargetView(m_->m_pGBufferRTVs[4].Get(), clearColor); // Color

	m_->m_pDeviceContext->ClearDepthStencilView(m_->m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	ID3D11RenderTargetView* rtvs[Impl::GBufferCount] = {
		m_->m_pGBufferRTVs[0].Get(), m_->m_pGBufferRTVs[1].Get(),
		m_->m_pGBufferRTVs[2].Get(), m_->m_pGBufferRTVs[3].Get(),
		m_->m_pGBufferRTVs[4].Get() };
	m_->m_pDeviceContext->OMSetRenderTargets(Impl::GBufferCount, rtvs,
		m_->m_pDepthStencilView);
	m_->m_pDeviceContext->OMSetDepthStencilState(m_->m_pDepthStencilState, 0);

	// 3. 파이프라인 설정
	m_->m_pDeviceContext->VSSetShader(m_->m_pGBufferVS, nullptr, 0);
	m_->m_pDeviceContext->PSSetShader(m_->m_pGBufferPS, nullptr, 0);
	m_->m_pDeviceContext->IASetPrimitiveTopology(
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 4. 전역 상수 버퍼 업데이트
	ConstantBuffer& cb = m_->m_ConstantBuffer;
	cb.world = m_->m_baseProjection.world;
	cb.view = m_->m_baseProjection.view;
	cb.proj = m_->m_baseProjection.proj;
	cb.worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, m_->m_baseProjection.world));
	cb.eyePos = m_Camera.GetPosition();
	cb.useTextureColor = m_->m_UseTextureColor;

	// 기본 재질 설정
	const PBRMaterialCPU& defPbr = m_->m_DefaultPbrMaterial;
	cb.pbrBaseColor = defPbr.baseColor;
	cb.pbrMetalness = defPbr.metalness;
	cb.pbrRoughness = defPbr.roughness;
	cb.pbrAO = defPbr.ambientOcclusion;
	cb.enableNormalMap = 0;
	cb.useDiffuseMap = 1;

	UpdateCB(m_->m_pConstantBuffer, cb);

	m_->m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
	m_->m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
	m_->m_pDeviceContext->PSSetSamplers(0, 1, &m_->m_pSamplerState);

	// 큐브 렌더링
	UINT stride = m_->m_VertextBufferStride;
	UINT offset = m_->m_VertextBufferOffset;
	for (auto& objPtr : m_->m_Objects) {
		if (!objPtr || objPtr->kind != ObjectKind::Cube)
			continue;
		auto* cubeObj = static_cast<CubeObject*>(objPtr.get());
		const Transform& mc = cubeObj->cubeTransform;

		m_->m_pDeviceContext->IASetVertexBuffers(0, 1, &m_->m_pVertexBuffer, &stride, &offset);
		m_->m_pDeviceContext->IASetIndexBuffer(m_->m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

		ConstantBuffer cb = m_->m_ConstantBuffer;
		XMMATRIX Sm = XMMatrixScaling(mc.scale.x, mc.scale.y, mc.scale.z);
		XMMATRIX Rm = XMMatrixRotationQuaternion(
			XMQuaternionRotationRollPitchYaw(XMConvertToRadians(mc.rotationDeg.x),
				XMConvertToRadians(mc.rotationDeg.y),
				XMConvertToRadians(mc.rotationDeg.z)));
		XMMATRIX Tm = XMMatrixTranslation(mc.position.x, mc.position.y, mc.position.z);
		XMMATRIX W = Sm * Rm * Tm;

		cb.world = XMMatrixTranspose(W);
		cb.worldInvTranspose =
			XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranspose(W)));
		const PBRMaterialCPU& cubePbr = m_->m_DefaultPbrMaterial;
		cb.pbrBaseColor = cubePbr.baseColor;
		cb.pbrMetalness = cubePbr.metalness;
		cb.pbrRoughness = cubePbr.roughness;
		cb.enableNormalMap = (cubeObj->useNormalMap != 0) ? 1 : 0;

		for (int face = 0; face < 6; ++face) {
			bool isTexCube = (cubeObj->cubeType == ECubeType::Texture);
			ID3D11ShaderResourceView* srvDiffuse = cubeObj->faceSRV[face]
				? cubeObj->faceSRV[face] : m_->m_pFallbackWhite;
			ID3D11ShaderResourceView* srvNormal =
				(isTexCube && cubeObj->useNormalMap != 0)
				? (cubeObj->normalSRV[face] ? cubeObj->normalSRV[face] : m_->m_pFallbackNormal) : nullptr;

			cb.useDiffuseMap = (cubeObj->faceSRV[face] != nullptr) ? 1 : 0;
			cb.enableNormalMap = (isTexCube && cubeObj->useNormalMap != 0) ? 1 : 0;

			UpdateCB(m_->m_pConstantBuffer, cb);
			m_->m_pDeviceContext->PSSetShaderResources(0, 1, &srvDiffuse);
			m_->m_pDeviceContext->PSSetShaderResources(2, 1, &srvNormal);
			m_->m_pDeviceContext->DrawIndexed(6, face * 6, 0);
		}
	}

	// 3D 모델 렌더링
	if (!m_->m_Models.empty()) {
		for (auto& mdlPtr : m_->m_Models) {
			if (!mdlPtr->shared || !mdlPtr->shared->vb)
				continue;

			// VS, InputLayout 설정 (스키닝 여부 체크)
			bool hasSkeleton = false;
			if (mdlPtr->source == ModelSource::FBX && mdlPtr->shared->fbx)
				hasSkeleton = mdlPtr->shared->fbx->HasSkeleton();
			else if (mdlPtr->source == ModelSource::PMX && mdlPtr->shared->pmx)
				hasSkeleton = mdlPtr->shared->pmx->HasSkeleton();

			ID3D11Buffer* cbBones = mdlPtr->fbxBaseAnimator.GetBoneCB();
			bool useSkinned = hasSkeleton && m_->m_pVertexShaderSkinned && cbBones;

			if (useSkinned) {
				m_->m_pDeviceContext->IASetInputLayout(m_->m_pInputLayoutSkinned);
				// G-Buffer용 스키닝 VS가 별도로 없다면 기존 스키닝 VS 사용하되
				// G-Buffer PS와 호환되는지 확인 필요. 보통 G-Buffer 출력을 위한 별도의
				// Skinned VS를 만듭니다. 만약 없다면 기본 Skinned VS가 VertexOut
				// 구조체를 공유하는지 확인하세요.
				m_->m_pDeviceContext->VSSetShader(m_->m_pVertexShaderSkinned, nullptr,
					0);
				m_->m_pDeviceContext->VSSetConstantBuffers(1, 1, &cbBones);
			}
			else {
				m_->m_pDeviceContext->IASetInputLayout(m_->m_pGBufferInputLayout);
				m_->m_pDeviceContext->VSSetShader(m_->m_pGBufferVS, nullptr, 0);
			}

			// 버퍼 바인딩
			UINT s = mdlPtr->shared->stride, o = 0;
			m_->m_pDeviceContext->IASetVertexBuffers(0, 1, &mdlPtr->shared->vb, &s,
				&o);
			m_->m_pDeviceContext->IASetIndexBuffer(mdlPtr->shared->ib,
				DXGI_FORMAT_R32_UINT, 0);

			// 월드 행렬
			XMMATRIX W = mdlPtr->GetWorldMatrix(); // (Scale * Rot * Trans)
			ConstantBuffer cbM = m_->m_ConstantBuffer;
			cbM.world = XMMatrixTranspose(W);
			cbM.worldInvTranspose =
				XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranspose(W)));

			const PBRMaterialCPU& activePbr = mdlPtr->useInstancePbrMaterial
				? mdlPtr->instancePbrMaterial
				: m_->m_DefaultPbrMaterial;
			cbM.pbrBaseColor = activePbr.baseColor;
			cbM.pbrMetalness = activePbr.metalness;
			cbM.pbrRoughness = activePbr.roughness;
			cbM.enableNormalMap =
				m_->m_EnableNormalMapForCube; // 모델별 설정 확인 필요함

			UpdateCB(m_->m_pConstantBuffer, cbM);

			// 서브셋 그리기
			for (const auto& sub : mdlPtr->shared->subsets) {
				// 재질 바인딩 (Diffuse, Normal)
				ID3D11ShaderResourceView* srvD =
					(sub.materialIndex < mdlPtr->shared->materialSRVs.size())
					? mdlPtr->shared->materialSRVs[sub.materialIndex]
					: m_->m_pFallbackWhite;
				ID3D11ShaderResourceView* srvN =
					(sub.materialIndex < mdlPtr->shared->normalSRVs.size())
					? mdlPtr->shared->normalSRVs[sub.materialIndex]
					: m_->m_pFallbackNormal;

				m_->m_pDeviceContext->PSSetShaderResources(0, 1, &srvD);
				m_->m_pDeviceContext->PSSetShaderResources(2, 1, &srvN);
				m_->m_pDeviceContext->DrawIndexed(sub.count, sub.start, 0);
			}
		}

		// RTV 해제
		ID3D11RenderTargetView* nullRTVs[Impl::GBufferCount] = { nullptr };
		m_->m_pDeviceContext->OMSetRenderTargets(Impl::GBufferCount, nullRTVs,
			nullptr);
	}
}

// 디퍼드 라이트 패스: G-Buffer를 읽어서 조명 계산
void App::PassDeferredLight() {
	// 1. HDR 타겟 설정 (Depth Stencil View는 바인딩하되 쓰기는 금지해야 함)
	m_->m_pDeviceContext->OMSetRenderTargets(1, &m_->m_pHdrRenderTargetView, m_->m_pDepthStencilView);

	// Depth Write 끄기: Quad가 깊이 버퍼(z=0)를 덮어쓰면 스카이박스(z=1)가 가려짐
	// Impl에 명시적인 DepthDisable 상태가 없다면 nullptr로 테스트 자체를 끄는
	// 것이 안전함
	m_->m_pDeviceContext->OMSetDepthStencilState(m_->m_pDepthStencilStateReadOnly, 0);

	// 2. 텍스처 바인딩 (G-Buffer 5개 + IBL 3개 + ShadowMap 1개)
	std::vector<ID3D11ShaderResourceView*> srvs = {
		m_->m_pGBufferSRVs[0].Get(),
		m_->m_pGBufferSRVs[1].Get(),
		m_->m_pGBufferSRVs[2].Get(),
		m_->m_pGBufferSRVs[3].Get(),
		m_->m_pGBufferSRVs[4].Get(),
		m_->m_pIblDiffuseSRV,
		m_->m_pIblSpecularSRV,
		m_->m_pIblBrdfLutSRV,
		m_->m_pShadowSRV // t8: 섀도우 맵 바인딩 필수
	};
	m_->m_pDeviceContext->PSSetShaderResources(0, static_cast<UINT>(srvs.size()), srvs.data());

	// 3. 샘플러 설정 (Shadow Sampler 포함)
	ID3D11SamplerState* samplers[] = { m_->m_pSamplerState, m_->m_pShadowSampler, m_->m_pSamplerLinear };
	m_->m_pDeviceContext->PSSetSamplers(0, 3, samplers);

	// 4. 상수 버퍼 업데이트 (CameraPos, ShadowInfo, LightMatrix)
	ConstantBuffer cb = m_->m_ConstantBuffer;
	cb.eyePos = m_Camera.GetPosition();
	cb.lightViewProj = m_->m_baseProjection.lightViewProj; // 그림자 판별용 행렬
	cb.shadowMapSize = (float)m_->m_ShadowSize;
	cb.shadowEnabled = m_->m_ShadowEnabled;
	cb.shadowPCFRadius = m_->m_ShadowPCFRadius;
	cb.shadowBias = m_->m_ShadowBias;
	UpdateCB(m_->m_pConstantBuffer, cb);
	m_->m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);

	// Directional Light 정보 업데이트
	{
		XMFLOAT3 L = m_->m_DirLight.direction;
		XMStoreFloat3(&L, XMVector3Normalize(XMLoadFloat3(&L)));
		struct DirLightCB {
			XMFLOAT4 dir;
			XMFLOAT4 color;
			float intensity;
			float padding[3];
		} lcb;
		lcb.dir = { L.x, L.y, L.z, 1.0f };
		lcb.color = { m_->m_DirLight.diffuse.x, m_->m_DirLight.diffuse.y, m_->m_DirLight.diffuse.z, 1.0f };
		lcb.intensity = m_->m_DirLight.intensity;
		UpdateCB(m_->m_pDirectionalLightBuffer, lcb);
		m_->m_pDeviceContext->PSSetConstantBuffers(3, 1, &m_->m_pDirectionalLightBuffer);
	}

	// 5. FullScreen Quad 그리기
	m_->m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_->m_pDeviceContext->IASetInputLayout(m_->m_pQuadInputLayout);
	m_->m_pDeviceContext->IASetVertexBuffers(0, 1, &m_->m_pQuadVertexBuffer,
		&m_->m_QuadVertexBufferStride,
		&m_->m_QuadVertexBufferOffset);
	m_->m_pDeviceContext->IASetIndexBuffer(m_->m_pQuadIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

	m_->m_pDeviceContext->VSSetShader(m_->m_pQuadVertexShader, nullptr, 0);
	m_->m_pDeviceContext->PSSetShader(m_->m_pDeferredLightPS, nullptr, 0);
	m_->m_pDeviceContext->DrawIndexed(m_->m_nQuadIndices, 0, 0);

	// 6. 리소스 해제 (다음 패스 쓰기 충돌 방지)
	std::vector<ID3D11ShaderResourceView*> nullSRVs(9, nullptr);
	m_->m_pDeviceContext->PSSetShaderResources(0, 9, nullSRVs.data());

	// Depth Stencil 복구 (Skybox 렌더링 등을 위해)
	m_->m_pDeviceContext->OMSetDepthStencilState(m_->m_pDepthStencilState, 0);
}

void App::PassPostProcess() {
	// 1. 렌더 타겟 설정 (BackBuffer)
	ID3D11RenderTargetView* nullRTV = nullptr;
	m_->m_pDeviceContext->OMSetRenderTargets(1, &m_->m_pRenderTargetView,
		nullptr);

	// 2. 뷰포트 재설정 혹시 다른 패스에서 뷰포트가 변경되었을 경우
	D3D11_VIEWPORT vp{};
	vp.Width = static_cast<float>(m_ClientWidth);
	vp.Height = static_cast<float>(m_ClientHeight);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	m_->m_pDeviceContext->RSSetViewports(1, &vp);

	// 3. 톤매핑 상수 버퍼 업데이트
	m_->m_PostProcessConstantBuffer.g_Exposure = m_->m_Exposure;
	m_->m_PostProcessConstantBuffer.g_MaxHDRNits = m_->m_MonitorMaxNits;
	UpdateCB(m_->m_pPostProcessConstantBuffer, m_->m_PostProcessConstantBuffer);
	m_->m_pDeviceContext->PSSetConstantBuffers(2, 1,
		&m_->m_pPostProcessConstantBuffer);

	// 4. Quad 렌더링 준비
	m_->m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_->m_pDeviceContext->IASetVertexBuffers(0, 1, &m_->m_pQuadVertexBuffer,
		&m_->m_QuadVertexBufferStride,
		&m_->m_QuadVertexBufferOffset);
	m_->m_pDeviceContext->IASetInputLayout(m_->m_pQuadInputLayout);
	m_->m_pDeviceContext->IASetIndexBuffer(m_->m_pQuadIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

	// 5. 셰이더 선택 HDR 지원 모니터 여부 확인
	m_->m_pDeviceContext->VSSetShader(m_->m_pQuadVertexShader, nullptr, 0);
	if (m_->m_format == DXGI_FORMAT_R16G16B16A16_FLOAT || m_->m_format == DXGI_FORMAT_R10G10B10A2_UNORM)
		m_->m_pDeviceContext->PSSetShader(m_->m_pPS_ToneMappingHDR, nullptr, 0);
	else
		m_->m_pDeviceContext->PSSetShader(m_->m_pPS_ToneMappingLDR, nullptr, 0);

	// 6. HDR 텍스처 바인딩 및 그리기
	m_->m_pDeviceContext->PSSetShaderResources(8, 1, &m_->m_pHdrShaderResourceView);
	m_->m_pDeviceContext->PSSetSamplers(2, 1, &m_->m_pSamplerLinear);

	m_->m_pDeviceContext->DrawIndexed(m_->m_nQuadIndices, 0, 0);

	// 7. 리소스 해제
	ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
	m_->m_pDeviceContext->PSSetShaderResources(8, 1, nullSRV);
}

void App::PassUI() {
	// Refresh resource-owning skybox state before ImGui records raw SRV pointers.
	const unsigned int skyboxGeneration = SkyboxAssetManager::GetCompletedGeneration();
	if (skyboxGeneration != m_->m_SkyboxAssetGeneration) {
		m_->m_SkyboxAssetGeneration = skyboxGeneration;
		if (!m_->m_CurrentIBLPath.empty())
			ChangeIBLSkyBox(m_->m_CurrentIBLPath);
	}

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	const bool readmeCaptureMode = IsReadmeCaptureMode();
	if (readmeCaptureMode) {
		// Published media stays focused on the animation techniques and their timing.
		RenderPortfolioShowcaseHud();
	}
	else {
		// Ordinary launches retain the tutorial's interactive editor and diagnostics.
		RenderControlPannel();
		RenderSceneCollection();
		RenderModelPannel();
		RenderQuickGuideUI();
		RenderConsolPannel();
		m_->m_SystemInfo.RenderUI();
		RenderSceneImageWindow();
		RenderDeferredUI();
		RenderSoundDebugUI();
		// Drawn last so the showcase palette-ownership explanation is not hidden by
		// the restored System Info and Sound Debug windows on high-DPI desktops.
		RenderAdvancedRigUI();
	}

	if (!readmeCaptureMode) {
		SkyboxAssetManager::RenderStatusUI();
	}

	// Sniper UI Overlay
	if (!readmeCaptureMode && m_->m_SniperEnabled && m_->m_SniperCharging)
	{
		ImDrawList* dl = ImGui::GetForegroundDrawList();
		ImVec2 p = m_->m_SniperAimPos;

		const float r = m_->m_SniperAimRadius;
		dl->AddCircle(p, r, IM_COL32(255, 255, 255, 220), 32, 2.0f);

		ImVec2 barSize(80.0f, 7.0f);
		ImVec2 barPos(p.x - barSize.x * 0.5f, p.y + r + 10.0f);
		dl->AddRectFilled(barPos, ImVec2(barPos.x + barSize.x, barPos.y + barSize.y),
			IM_COL32(0, 0, 0, 160), 2.0f);
		dl->AddRectFilled(barPos, ImVec2(barPos.x + barSize.x * m_->m_SniperCharge01, barPos.y + barSize.y),
			IM_COL32(255, 255, 255, 220), 2.0f);
		dl->AddRect(barPos, ImVec2(barPos.x + barSize.x, barPos.y + barSize.y),
			IM_COL32(255, 255, 255, 220), 2.0f);
	}

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
