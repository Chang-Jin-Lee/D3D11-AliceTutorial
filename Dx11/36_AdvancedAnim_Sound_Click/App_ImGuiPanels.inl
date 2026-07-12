void App::RenderControlPannel() {
	// Control 패널
	ImGuiIO& ioUI = ImGui::GetIO();
	const float W = ioUI.DisplaySize.x;
	const float H = ioUI.DisplaySize.y;

	ImGui::SetNextWindowPos(ImVec2(10, 20), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 360), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Controls")) {
		ImGui::SeparatorText("Tone Mapping Parameter");
		ImGui::SliderFloat("Exposure", &m_->m_Exposure, -2.0f, 2.0f, "%.2f");
		ImGui::SliderFloat("Monitor Max Nits", &m_->m_MonitorMaxNits, 0.0f,
			50000.0f, "%.2f");

		ImGui::SeparatorText("Rendering Mode");
		ImGui::Checkbox("Use Deferred Rendering", &m_->m_UseDeferredRendering);

		ImGui::SeparatorText("PBR Parameter");
		// 텍스처 색 사용 여부 (PBR 전용)
		ImGui::Checkbox("Use Texture Color (PBR)", (bool*)&m_->m_UseTextureColor);
		// 노말맵 사용 여부 (PBR / 모델 공통)
		ImGui::Checkbox("Use Normal Map (PBR)", (bool*)&m_->m_EnableNormalMapForCube);

		ImGui::ColorEdit3("Base Color##PBR", &m_->m_DefaultPbrMaterial.baseColor.x);
		ImGui::SliderFloat("Metalness##PBR", &m_->m_DefaultPbrMaterial.metalness,
			0.0f, 1.0f, "%.2f");
		ImGui::SliderFloat("Roughness##PBR", &m_->m_DefaultPbrMaterial.roughness,
			0.0f, 1.0f, "%.2f");
		ImGui::SliderFloat("Ambient Occlusion##PBR",
			&m_->m_DefaultPbrMaterial.ambientOcclusion, 0.0f, 1.0f,
			"%.2f");
		if (ImGui::Button("Reset PBR Material")) {
			m_->m_DefaultPbrMaterial = PBRMaterialCPU{};
		}

		ImGui::Separator();

		// SkyBox 선택
		{
			int cur = static_cast<int>(m_->m_SkyBoxChoice);

			const char* items[] = { "Off", "bridge", "indoor", "baker" };

			// 2. 콤보 선택 UI
			if (ImGui::Combo("SkyBox Choice", &cur, items, IM_ARRAYSIZE(items))) {
				// 3. int → enum 캐스팅
				m_->m_SkyBoxChoice = static_cast<App::Impl::SkyBoxChoice>(cur);

				if (m_->m_SkyBoxChoice == App::Impl::SkyBoxChoice::Off) {
					// Off: 배경 단색 사용
				}
				else {
					switch (m_->m_SkyBoxChoice) {
					case App::Impl::SkyBoxChoice::bridge:
						ChangeIBLSkyBox(L"..\\Resource\\Skybox\\Bridge\\bridge");
						break;

					case App::Impl::SkyBoxChoice::indoor:
						ChangeIBLSkyBox(L"..\\Resource\\Skybox\\Indoor\\indoor");
						break;

					case App::Impl::SkyBoxChoice::Baker:
						ChangeIBLSkyBox(L"..\\Resource\\Skybox\\Sample\\BakerSample");
						break;

					case App::Impl::SkyBoxChoice::Off:
					default:
						break;
					}
				}
			}

			// 4. Off일 때만 배경색 편집
			if (m_->m_SkyBoxChoice == App::Impl::SkyBoxChoice::Off) {
				ImGui::ColorEdit4("Background Color", &m_->m_ClearColor.x);
			}
		}
		ImGui::Separator();
		ImGui::Text("Camera");
		{
			ImGui::Checkbox("Quick Guide", &m_->m_ShowQuickGuideWindow);
			if (ImGui::Button("Reset")) {
				m_Camera.Reset();
			}
			ImGui::SliderFloat("Camera Speed", &m_Camera.m_MoveSpeed, 1.0f, 500.0f,
				"%.1f");
			DirectX::XMFLOAT3 pos = m_Camera.GetPosition();
			if (ImGui::DragFloat3("Camera Pos (x,y,z)", &pos.x, 0.1f)) {
				m_Camera.SetPosition(pos);
			}
			float fovDeg = XMConvertToDegrees(m_Camera.GetFovYRad());
			if (ImGui::SliderFloat("Camera FOV (deg)", &fovDeg, 30.0f, 120.0f)) {
				m_Camera.SetFrustum(XMConvertToRadians(fovDeg), AspectRatio(),
					m_Camera.GetNearZ(), m_Camera.GetFarZ());
			}
			float nearZ = m_Camera.GetNearZ();
			float farZ = m_Camera.GetFarZ();
			if (ImGui::DragFloatRange2("Near/Far", &nearZ, &farZ, 0.1f, 0.01f,
				10000.0f, "Near: %.2f", "Far: %.2f")) {
				m_Camera.SetFrustum(m_Camera.GetFovYRad(), AspectRatio(), nearZ, farZ);
			}
			// 카메라 회전(도) 표시 및 편집: pitch, yaw, roll (API로 일원화)
			{
				XMFLOAT3 rotDeg = m_Camera.GetRotation();
				if (ImGui::DragFloat3("Camera Rot (deg)", &rotDeg.x, 1.0f, -180.0f,
					180.0f, "%.1f")) {
					m_Camera.SetRotation(rotDeg);
				}
			}
		}
		ImGui::Separator();
		ImGui::Text("VMD Camera");
		if (ImGui::Button("Load VMD Camera and Play")) {
			std::wstring vmdPath;
			if (OpenFileDialogVMD(vmdPath)) {
				if (mmd::LoadVmdCameraFromFile(vmdPath, m_->m_VmdCamera)) {
					m_->PushLog("[OK] Loaded VMD Camera");
				}
				else {
					m_->PushLog("[ERR] Failed to load VMD Camera");
				}
			}
		}
		if (!m_->m_VmdCamera.frames.empty()) {
			ImGui::Checkbox("Use VMD Camera", &m_->m_VmdCamera.use);
			ImGui::SameLine();
			if (ImGui::Button(m_->m_VmdCamera.playing ? "Pause##VMD" : "Play##VMD")) {
				m_->m_VmdCamera.playing = !m_->m_VmdCamera.playing;
			}
			ImGui::SameLine();
			if (ImGui::Button("Stop##VMD")) {
				m_->m_VmdCamera.playing = false;
				if (!m_->m_VmdCamera.frames.empty())
					m_->m_VmdCamera.currentFrame =
					(float)m_->m_VmdCamera.frames.front().frame;
			}

			int firstFrame = m_->m_VmdCamera.frames.front().frame;
			int lastFrame = m_->m_VmdCamera.frames.back().frame;
			float curFrame = m_->m_VmdCamera.currentFrame;
			ImGui::Text("Frame: %.1f (%d ~ %d)", curFrame, firstFrame, lastFrame);
			ImGui::SliderFloat("VMD Camera Scale", &m_->m_VmdCamera.scale, 0.1f, 5.0f,
				"%.2f");
		}
		ImGui::Separator();
		ImGui::Text("Shading");
		{
			int mode = (int)m_->m_ShadingMode;
			const char* modes[] = { "Phong",       "Blinn-Phong", "Lambert", "Unlit",
								   "TextureOnly", "ToonShading", "PBR" };
			if (ImGui::Combo("Shading Mode", &mode, modes, IM_ARRAYSIZE(modes))) {
				m_->m_ShadingMode = (ShadingMode)mode;
			}

			{
				ImGui::Separator();
				ImGui::Text("Outline (Toon + Multipass)");
				// Multipass 아웃라인 파라미터
				ImGui::DragFloat("Thickness", &m_->m_OutlineThickness, 0.0f, 2.0f);
				ImGui::ColorEdit3("Color", &m_->m_OutlineColor.x);
				ImGui::SliderFloat("Strength", &m_->m_OutlineStrength, 0.0f, 4.0f,
					"%.2f");
			}
		}
		ImGui::Separator();
		ImGui::Text("Light");
		ImGui::DragFloat3("Light Direction", &m_->m_DirLight.direction.x, 0.05f);
		ImGui::SliderFloat("Intensity", &m_->m_DirLight.intensity, 0.1f, 30.0f,
			"%.1f");
		ImGui::ColorEdit4("Ambient", &m_->m_DirLight.ambient.x);
		ImGui::ColorEdit4("Diffuse", &m_->m_DirLight.diffuse.x);
		ImGui::ColorEdit4("Specular", &m_->m_DirLight.specular.x);
		if (ImGui::Button("Reset Light")) {
			m_->m_DirLight = { XMFLOAT4(0, 0, 0, 1), XMFLOAT4(1, 1, 1, 1),
							  XMFLOAT4(0.7f, 0.7f, 0.7f, 1), XMFLOAT3(0, 0, 1), 1.0f };
		}
		ImGui::Separator();

		ImGui::Text("Material");
		ImGui::ColorEdit4("Ambient (ka)", &m_->m_Material.ambient.x);
		ImGui::ColorEdit4("Diffuse (kd)", &m_->m_Material.diffuse.x);
		ImGui::ColorEdit4("Specular (ks)", &m_->m_Material.specular.x);
		ImGui::DragFloat("Shininess (alpha)", &m_->m_Material.specular.w, 0.05f,
			1.0f, 256.0f);
		ImGui::ColorEdit4("Reflect (R=metal, A=roughness)",
			&m_->m_Material.reflect.x);
		if (ImGui::Button("Reset Material")) {
			m_->m_Material = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1),
							  XMFLOAT4(1, 1, 1, 32), XMFLOAT4(0, 0, 0, 0) };
		}
	}
	ImGui::End();

	auto& io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(10, 390), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(370, 360), ImGuiCond_FirstUseEver);
	// Shadow controls and debug view
	if (ImGui::Begin("ShadowMap (Directional)")) {
		ImGui::Checkbox("Enable Shadow", (bool*)&m_->m_ShadowEnabled);
		ImGui::DragFloat("Bias", &m_->m_ShadowBias, 0.0001f, 0.0f, .1f, "%.5f");
		ImGui::SliderFloat("PCF Radius(Texel)", &m_->m_ShadowPCFRadius, 0.0f, 3.0f,
			"%.2f");
		ImGui::SliderFloat("Ortho Radius(m)", &m_->m_ShadowOrthoRadius, 1.0f,
			3000.0f, "%.1f");
		if (m_->m_pShadowSRV) {
			ImGui::Separator();
			ImGui::Text("ShadowMap Debug");
			ImVec2 avail = ImGui::GetContentRegionAvail();
			float sz = std::min(avail.x, 256.0f);
			ImGui::Image((ImTextureID)m_->m_pShadowSRV, ImVec2(sz, sz));
		}
	}
	ImGui::End();
}

void App::RenderModelPannel() {
	// Models 독립 창
	auto& io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 330, 380),
		ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(320, 360), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Details")) {
		if (m_->m_SelectedItem >= 0 &&
			m_->m_SelectedItem < (int)m_->m_Objects.size()) {
			auto* obj = m_->m_Objects[m_->m_SelectedItem].get();
			if (obj->kind == ObjectKind::Cube) {
				auto* co = static_cast<CubeObject*>(obj);
				ImGui::Text("Cube : %s", Utf8FromWString(co->name).c_str());
				ImGui::Separator();
				auto& t = co->cubeTransform;
				// Maps (per cube, Texture type only)
				if (co->cubeType == ECubeType::Texture) {
					bool nm = (co->useNormalMap != 0);
					if (ImGui::Checkbox("Enable Normal Map", &nm)) {
						co->useNormalMap = nm ? 1 : 0;
					}
					bool sm = (co->useSpecularMap != 0);
					if (ImGui::Checkbox("Use Specular Map", &sm)) {
						co->useSpecularMap = sm ? 1 : 0;
					}
				}
				ImGui::DragFloat3("Position", &t.position.x, 0.1f);
				ImGui::DragFloat3("Rotation (deg)", &t.rotationDeg.x, 1.0f, -360.0f,
					360.0f, "%.1f");
				ImGui::DragFloat3("Scale", &t.scale.x, 0.01f, 0.001f, 100.0f, "%.3f");

				ImGui::Separator();
				ImGui::TextUnformatted("Material");
				ImGui::ColorEdit4("Ambient (ka)", &co->matAmbient.x);
				ImGui::ColorEdit4("Diffuse (kd)", &co->matDiffuse.x);
				ImGui::ColorEdit4("Specular (ks)", &co->matSpecular.x);
				ImGui::DragFloat("Shininess (alpha)", &co->matSpecular.w, 0.05f, 1.0f,
					256.0f);
				ImGui::ColorEdit4("Reflect (kr,a)", &co->matReflect.x);
			}
			else {
				auto* mo = static_cast<ModelObject*>(obj);
				int i = mo->modelIndex;
				if (i >= 0 && i < (int)m_->m_Models.size()) {
					auto& mdl = *m_->m_Models[i];
					ImGui::Text("Model #%d : %s", i,
						Utf8FromWString(m_->m_Models[i]->modelName).c_str());
					ImGui::Separator();
					ImGui::PushID(i);
					ImGui::DragFloat3("Position", &mdl.pos.x, 0.1f);
					ImGui::DragFloat3("Rotation (deg)", &mdl.rotDeg.x, 1.0f, -360.0f,
						360.0f, "%.1f");
					ImGui::DragFloat3("Scale", &mdl.scale.x, 0.01f, 0.001f, 100.0f,
						"%.3f");
					ImGui::Checkbox("Auto Rotate (Yaw)", &mdl.autoRotate);
					if (mdl.source == ModelSource::FBX && mdl.shared && mdl.shared->fbx) {
						if (mdl.shared->fbx->HasAnimations()) {
							const auto& names = mdl.shared->fbx->GetAnimationNames();
							if (mdl.uiSelectedAnim < 0 ||
								mdl.uiSelectedAnim >= (int)names.size())
								mdl.uiSelectedAnim = mdl.fbxBaseAnimator.GetCurrentIndex();

							ImGui::Text("FBX Animations");
							if (ImGui::BeginListBox(
								"##AnimList",
								ImVec2(-FLT_MIN,
									4 * ImGui::GetTextLineHeightWithSpacing()))) {
								for (int a = 0; a < (int)names.size(); ++a) {
									bool sel = (a == mdl.uiSelectedAnim);
									if (ImGui::Selectable(names[a].c_str(), sel)) {
										mdl.uiSelectedAnim = a;
										mdl.fbxBaseAnimator.SetCurrentIndex(a);
										m_->PushLog(std::string("[OK] FBX Anim -> ") + names[a]);
									}
									if (sel)
										ImGui::SetItemDefaultFocus();
								}
								ImGui::EndListBox();
							}

							// FBX용 오디오 로드 (FMOD)
							if (ImGui::Button("Load Audio (FMOD)...")) {
								wchar_t file[MAX_PATH] = { 0 };
								OPENFILENAMEW ofn{};
								ofn.lStructSize = sizeof(ofn);
								ofn.hwndOwner = GameApp::m_hWnd;
								ofn.lpstrFilter =
									L"Audio Files (*.wav;*.mp3;*.ogg)\0*.wav;*.mp3;*.ogg\0All "
									L"Files\0*.*\0\0";
								ofn.lpstrFile = file;
								ofn.nMaxFile = MAX_PATH;
								ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
								if (GetOpenFileNameW(&ofn)) {
									mdl.audioPath = file;
									// 파일명 추출 (확장자 제외)
									std::wstring audioKey;
									size_t sep = mdl.audioPath.find_last_of(L"\\/");
									size_t dot = mdl.audioPath.find_last_of(L'.');
									if (sep != std::wstring::npos && dot != std::wstring::npos && dot > sep) {
										audioKey = mdl.audioPath.substr(sep + 1, dot - sep - 1);
									}
									else if (dot != std::wstring::npos) {
										audioKey = mdl.audioPath.substr(0, dot);
									}
									else {
										audioKey = mdl.audioPath;
									}
									mdl.audioLoaded = Sound::Load(audioKey, mdl.audioPath, Sound::Type::BGM);
									if (mdl.audioLoaded) {
										mdl.audioKey = audioKey; // key 저장
										m_->PushLog("[OK] Audio loaded (FMOD)");
									}
									else {
										m_->PushLog("[ERR] Audio load failed (FMOD)");
									}
								}
							}

							bool playFBX = mdl.fbxBaseAnimator.IsPlaying();
							if (ImGui::Checkbox("Play", &playFBX)) {
								mdl.fbxBaseAnimator.SetPlaying(playFBX);
								mdl.uiAnimPlaying = playFBX;

								// 애니메이션 재생 상태에 맞춰 사운드도 같이 제어
								if (mdl.audioLoaded && !mdl.audioKey.empty()) {
									if (playFBX) {
										Sound::PlayBGM(mdl.audioKey);
									}
									else {
										Sound::PauseBGM(true);
									}
								}
							}

							double cur = mdl.fbxBaseAnimator.GetTimeSec();
							double dur = mdl.fbxBaseAnimator.GetClipDurationSec(
								mdl.fbxBaseAnimator.GetCurrentIndex());
							float curF = (float)cur, durF = (float)dur;
							if (durF > 0.0f) {
								// 애니메이션 타임라인과 사운드 재생 위치를 동일한 초 단위로
								// 맞춘다.
								if (ImGui::SliderFloat("Time (s)", &curF, 0.0f, durF)) {
									mdl.fbxBaseAnimator.SetTimeSec((double)curF);
									if (mdl.audioLoaded) {
										Sound::SetBGMTimeSeconds(curF);
									}
								}
							}

							// 정지 / 현재 시간 출력 (애니+오디오 동시 제어)
							if (mdl.audioLoaded) {
								ImGui::SeparatorText("Audio Sync (FMOD)");
								if (ImGui::Button("Stop (Anim + Audio)##FBX")) {
									mdl.fbxBaseAnimator.SetPlaying(false);
									mdl.fbxBaseAnimator.SetTimeSec(0.0);
									Sound::StopBGM();
									Sound::SetBGMTimeSeconds(0.0f);
								}
								float curAudio = Sound::GetBGMTimeSeconds();
								float lenAudio = Sound::GetBGMLengthSeconds();
								ImGui::Text("Audio Time: %.2f / %.2f sec", curAudio, lenAudio);
							}
						}
					}
					else if (mdl.source == ModelSource::PMX && mdl.shared &&
						mdl.shared->pmx) {
						if (ImGui::Button("Load VMD...")) {
							std::wstring vmdPath;
							wchar_t file[MAX_PATH] = { 0 };
							OPENFILENAMEW ofn{};
							ofn.lStructSize = sizeof(ofn);
							ofn.hwndOwner = GameApp::m_hWnd;
							ofn.lpstrFilter = L"VMD Files (*.vmd)\0*.vmd\0All Files\0*.*\0\0";
							ofn.lpstrFile = file;
							ofn.nMaxFile = MAX_PATH;
							ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
							if (GetOpenFileNameW(&ofn)) {
								if (mdl.shared->pmx->LoadVMD(m_->m_pDevice, file)) {
									m_->PushLog("[OK] VMD loaded");
									mdl.uiAnimPlaying = true;
								}
								else {
									m_->PushLog("[ERR] VMD load failed");
								}
							}
						}

						bool playPMX = mdl.shared->pmx->IsAnimationPlaying();
						if (ImGui::Checkbox("Play##PMX", &playPMX)) {
							mdl.shared->pmx->SetAnimationPlaying(playPMX);
						}

						double cur = mdl.shared->pmx->GetAnimationTimeSeconds();
						double dur = mdl.shared->pmx->GetClipDurationSec();
						float curF = (float)cur, durF = (float)dur;
						if (durF > 0.0f) {
							if (ImGui::SliderFloat("Time (s)##PMX", &curF, 0.0f, durF)) {
								mdl.shared->pmx->SetAnimationTimeSeconds((double)curF);
							}
						}
					}

					// 셰이딩 모드 선택 UI
					{
						int modePer = (int)mdl.modelShading;
						const char* modes[] = { "Phong", "Blinn-Phong", "Lambert",
											   "Unlit", "TextureOnly", "ToonShading",
											   "PBR" };
						if (ImGui::Combo("Shading Mode", &modePer, modes,
							IM_ARRAYSIZE(modes))) {
							mdl.modelShading = (ShadingMode)modePer;
						}
					}
					// Outline 적용 토글
					ImGui::Checkbox("Outline", &mdl.outlineEnabled);

					// 인스턴스 머티리얼
					ImGui::Checkbox("Use Instance Material", &mdl.useInstanceMaterial);
					if (mdl.useInstanceMaterial) {
						ImGui::ColorEdit4("Ambient (ka)##inst", &mdl.instanceMaterial.ambient.x);
						ImGui::ColorEdit4("Diffuse (kd)##inst", &mdl.instanceMaterial.diffuse.x);
						ImGui::ColorEdit4("Specular (ks)##inst", &mdl.instanceMaterial.specular.x);
						ImGui::DragFloat("Shininess (alpha)##inst", &mdl.instanceMaterial.specular.w, 0.05f, 1.0f, 256.0f);
						ImGui::ColorEdit4("Reflect (kr,a)##inst", &mdl.instanceMaterial.reflect.x);
					}

					ImGui::SeparatorText("PBR Material");
					ImGui::Checkbox("Use Instance PBR Material", &mdl.useInstancePbrMaterial);
					if (mdl.useInstancePbrMaterial) {
						ImGui::ColorEdit3("Base Color##instPBR", &mdl.instancePbrMaterial.baseColor.x);
						ImGui::SliderFloat("Metalness##instPBR", &mdl.instancePbrMaterial.metalness, 0.0f, 1.0f, "%.2f");
						ImGui::SliderFloat("Roughness##instPBR", &mdl.instancePbrMaterial.roughness, 0.04f, 1.0f, "%.2f");
						ImGui::SliderFloat("Ambient Occlusion##instPBR", &mdl.instancePbrMaterial.ambientOcclusion, 0.0f, 1.0f, "%.2f");
						ImGui::Checkbox("Use NormalMap", &mdl.useNormalMap);
						ImGui::Checkbox("Use SpecularMap", &mdl.useSpecularMap);
					}
					else {
						const auto& defPbr = m_->m_DefaultPbrMaterial;
						ImGui::Text("Base Color: (%.2f, %.2f, %.2f)", defPbr.baseColor.x, defPbr.baseColor.y, defPbr.baseColor.z);
						ImGui::Text("Metalness: %.2f", defPbr.metalness);
						ImGui::Text("Roughness: %.2f", defPbr.roughness);
						ImGui::Text("AO: %.2f", defPbr.ambientOcclusion);
					}

					ImGui::Separator();
					// 디버그 AABB 기준 본 인덱스 설정 (-1: Auto)
					ImGui::TextUnformatted("Debug AABB");
					ImGui::DragInt("Bounds Bone Index (-1:auto)", &mdl.boundsBoneIndex,
						1.0f, -1, 1023);
					// 선택된 인덱스의 본 이름 표시 (FBX만)
					if (mdl.source == ModelSource::FBX && mdl.shared && mdl.shared->fbx) {
						const auto& boneNames = mdl.shared->fbx->GetBoneNames();
						const char* name = "<auto>";
						if (mdl.boundsBoneIndex >= 0 &&
							(size_t)mdl.boundsBoneIndex < boneNames.size()) {
							name = boneNames[(size_t)mdl.boundsBoneIndex].c_str();
						}
						ImGui::Text("Bone: %s", name);
					}

					ImGui::Separator();
					// 메시 통계는 최초에 한 번만 계산
					if (!mdl.meshStatsValid && mdl.shared && mdl.shared->vb &&
						mdl.shared->ib && mdl.shared->indexCount > 0) {
						mdl.meshStatsValid = ComputeMeshStats(
							m_->m_pDevice, m_->m_pDeviceContext, mdl.shared->vb,
							mdl.shared->stride, mdl.shared->ib, mdl.shared->indexCount,
							mdl.meshStats);
					}
					if (mdl.meshStatsValid) {
						ImGui::Text("Vertex: %u   Edge: %u   Face: %u   Tri: %u",
							mdl.meshStats.vertices, mdl.meshStats.edges,
							mdl.meshStats.faces, mdl.meshStats.triangles);
					}

					// 본 구조 카드
					const auto& cache = mdl.boneCache;
					int rootIdx = mdl.boneRoot;
					bool hasSkeleton =
						mdl.boneCacheValid && rootIdx >= 0 && rootIdx < (int)cache.size();
					if (hasSkeleton && rootIdx >= 0) {
						ImGui::Checkbox("Show Bone Details", &mdl.showBoneDetails);
						if (mdl.showBoneDetails) {
							ImGui::BeginChild("BoneCard", ImVec2(0, 240), true,
								ImGuiWindowFlags_HorizontalScrollbar);
							ImGui::TextUnformatted(mdl.boneDisplayText.c_str());
							ImGui::EndChild();
						}
					}

					ImGui::PopID();
				}
			}
		}
		else {
			ImGui::TextUnformatted("Select an object in Scene Collection.");
		}
	}
	ImGui::End();
}

void App::RenderSceneCollection() {
	auto& io = ImGui::GetIO();
	// Scene Collection 블렌더의 Hierarchy창
	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 330, 20),
		ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(320, 360), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Scene Collection")) {
		if (ImGui::Button("Browse Model...")) {
			std::wstring pathW;
			if (OpenFileDialogModel(pathW)) {
				if (LoadModelFromFile(pathW)) {
					m_->PushLog("[OK] Model Selected " + Utf8FromWString(pathW));
					m_->m_SelectedModelIdx = (int)m_->m_Models.size() - 1;
					int newIndex = (int)m_->m_Models.size() - 1;
					auto mo = std::make_unique<ModelObject>(
						m_->m_Models[(size_t)newIndex]->modelName, newIndex);
					m_->m_Objects.push_back(std::move(mo));
				}
				else {
					m_->PushLog("[ERR] Load failed (Browse) | file extension must be "
						"fbx, obj, pmx");
				}
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Unload Model All")) {
			UnloadModel();
			m_->PushLog("[OK] Unloaded models All");
		}

		// ======================== Audio (FMOD) 전역 제어 ========================
		ImGui::Separator();
		ImGui::TextUnformatted("Audio (FMOD)");

		// 노래(오디오) 로드
		if (ImGui::Button("Load Audio...")) {
			wchar_t file[MAX_PATH] = { 0 };
			OPENFILENAMEW ofn{};
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = GameApp::m_hWnd;
			ofn.lpstrFilter =
				L"Audio Files (*.wav;*.mp3;*.ogg)\0*.wav;*.mp3;*.ogg\0All "
				L"Files\0*.*\0\0";
			ofn.lpstrFile = file;
			ofn.nMaxFile = MAX_PATH;
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
			if (GetOpenFileNameW(&ofn)) {
				m_->m_AudioPath = file;
				// 파일명 추출 (확장자 제외)
				std::wstring audioKey;
				size_t sep = m_->m_AudioPath.find_last_of(L"\\/");
				size_t dot = m_->m_AudioPath.find_last_of(L'.');
				if (sep != std::wstring::npos && dot != std::wstring::npos && dot > sep) {
					audioKey = m_->m_AudioPath.substr(sep + 1, dot - sep - 1);
				}
				else if (dot != std::wstring::npos) {
					audioKey = m_->m_AudioPath.substr(0, dot);
				}
				else {
					audioKey = m_->m_AudioPath;
				}
				m_->m_AudioLoaded = Sound::Load(audioKey, m_->m_AudioPath, Sound::Type::BGM);
				if (m_->m_AudioLoaded) {
					m_->m_AudioKey = audioKey; // key 저장
					m_->PushLog("[OK] Audio loaded (FMOD) : " +
						Utf8FromWString(m_->m_AudioPath));
				}
				else {
					m_->PushLog("[ERR] Audio load failed (FMOD)");
				}
			}
		}

		ImGui::SameLine();
		bool audioLoaded = m_->m_AudioLoaded;
		ImGui::BeginDisabled(!audioLoaded);
		if (ImGui::Button("Play##GlobalAudio")) {
			if (!m_->m_AudioKey.empty()) {
				Sound::PlayBGM(m_->m_AudioKey);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Pause##GlobalAudio")) {
			Sound::PauseBGM(true);
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop##GlobalAudio")) {
			Sound::StopBGM();
			Sound::SetBGMTimeSeconds(0.0f);
		}
		ImGui::EndDisabled();

		if (audioLoaded) {
			float cur = Sound::GetBGMTimeSeconds();
			float len = Sound::GetBGMLengthSeconds();
			ImGui::Text("Time: %.2f / %.2f sec", cur, len);
			if (len > 0.0f) {
				if (ImGui::SliderFloat("Time (s)##GlobalAudio", &cur, 0.0f, len)) {
					Sound::SetBGMTimeSeconds(cur);
				}
			}
		}

		ImGui::Separator();
		ImGui::Text("Scene Collection");
		// 통합 목록: m_Objects 자체가 소스이므로 매 프레임 재구성하지 않음

		ImGui::BeginChild("##SceneList", ImVec2(0, 0), true,
			ImGuiWindowFlags_HorizontalScrollbar);
		for (int i = 0; i < (int)m_->m_Objects.size(); ++i) {
			ImGui::PushID(i);
			std::wstring wlabel = std::to_wstring(i) + L"  " + m_->m_Objects[i]->name;
			std::string label = Utf8FromWString(wlabel);
			bool sel = (i == m_->m_SelectedItem);
			if (ImGui::Selectable(label.c_str(), sel,
				ImGuiSelectableFlags_AllowItemOverlap))
				m_->m_SelectedItem = i;
			ImGui::SameLine();
			ImGui::SetItemAllowOverlap();
			if (ImGui::SmallButton("Unload")) {
				if (m_->m_Objects[i]->kind == ObjectKind::Cube) {
					// 큐브는 통합 컨테이너에서만 제거
					m_->m_Objects.erase(m_->m_Objects.begin() + i);
				}
				else {
					// 모델은 렌더 소스인 m_Models도 함께 제거
					auto* mo = static_cast<ModelObject*>(m_->m_Objects[i].get());
					int removed = mo->modelIndex;
					if (removed >= 0 && removed < (int)m_->m_Models.size())
						m_->m_Models.erase(m_->m_Models.begin() + removed);
					// 인덱스 재정렬
					for (auto& up : m_->m_Objects) {
						if (up && up->kind == ObjectKind::Model) {
							auto* om = static_cast<ModelObject*>(up.get());
							if (om->modelIndex > removed)
								om->modelIndex -= 1;
						}
					}
					m_->m_Objects.erase(m_->m_Objects.begin() + i);
				}
				m_->m_SelectedItem = -1;
				ImGui::PopID();
				break;
			}
			ImGui::PopID();
		}
		ImGui::EndChild();
	}
	ImGui::End();
}

void App::RenderConsolPannel() {
	// Console
	{
		auto& io = ImGui::GetIO();
		ImGui::SetNextWindowPos(
			ImVec2(io.DisplaySize.x / 2 - 415, io.DisplaySize.y - 210),
			ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(830, 200), ImGuiCond_FirstUseEver);

		if (ImGui::Begin("Console")) {
			ImGui::Checkbox("Auto Scroll", &m_->m_LogAutoScroll);
			ImGui::SameLine();
			ImGui::InputTextWithHint("##LogFilter", "filter...", m_->m_LogFilter, 70);
			ImGui::SameLine();
			if (ImGui::Button("Clear"))
				m_->m_LogLines.clear();
			ImGui::Separator();
			ImGui::BeginChild("ConsoleScroll", ImVec2(0, 0), false,
				ImGuiWindowFlags_HorizontalScrollbar);
			for (const auto& ln : m_->m_LogLines) {
				if (m_->m_LogFilter[0] != '\0') {
					if (ln.find(m_->m_LogFilter) == std::string::npos)
						continue;
				}
				ImGui::TextUnformatted(ln.c_str());
			}
			if (m_->m_LogAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
				ImGui::SetScrollHereY(1.0f);
			ImGui::EndChild();
		}
		ImGui::End();
	}

	// 현재 카메라 포워드 기준 스카이박스 면 이미지를 표시 (스카이박스 On일 때만)
	if (m_->m_SkyBoxChoice != App::Impl::SkyBoxChoice::Off) {
		int face = 0;
		using namespace DirectX;
		XMFLOAT3 fwd = m_Camera.GetForward();
		XMVECTOR f = XMLoadFloat3(&fwd);
		XMVECTOR fn = XMVector3Normalize(f);
		XMFLOAT3 v;
		XMStoreFloat3(&v, fn);
		float ax = fabsf(v.x), ay = fabsf(v.y), az = fabsf(v.z);
		if (ax >= ay && ax >= az)
			face = (v.x >= 0.0f) ? 0 : 1; // +X / -X
		else if (ay >= ax && ay >= az)
			face = (v.y >= 0.0f) ? 2 : 3; // +Y / -Y
		else
			face = (v.z >= 0.0f) ? 4 : 5; // +Z / -Z

		ID3D11ShaderResourceView* faceSRV =
			(face >= 0 && face < 6) ? m_->m_pSkyFaceSRV[face] : nullptr;
		if (faceSRV) {
			ImGuiIO& io = ImGui::GetIO();
			ImVec2 pos(io.DisplaySize.x - m_->m_HanakoDrawSize.x - 330.0f, 20.0f);
			ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(
				ImVec2(m_->m_HanakoDrawSize.x + 50, m_->m_HanakoDrawSize.y + 80),
				ImGuiCond_Once);
			if (ImGui::Begin("Skybox Face")) {
				ImGui::BeginChild("SkyFaceView", ImVec2(0, 0), true,
					ImGuiWindowFlags_NoScrollbar |
					ImGuiWindowFlags_NoScrollWithMouse);
				const ImVec2 tex =
					(m_->m_HanakoDrawSize.x > 0 && m_->m_HanakoDrawSize.y > 0)
					? m_->m_HanakoDrawSize
					: m_->m_SkyFaceSize;
				ImVec2 avail = ImGui::GetContentRegionAvail();
				float sx = (tex.x > 0.f) ? (avail.x / tex.x) : 1.f;
				float sy = (tex.y > 0.f) ? (avail.y / tex.y) : 1.f;
				float scale = (sx > 0.f && sy > 0.f) ? std::min(sx, sy) : 1.f;
				ImVec2 draw = ImVec2(tex.x * scale, tex.y * scale);
				ImVec2 start = ImGui::GetCursorPos();
				ImVec2 offset =
					ImVec2((avail.x - draw.x) * 0.5f, (avail.y - draw.y) * 0.5f);
				ImGui::SetCursorPos(start + offset);
				ImGui::Image((ImTextureID)faceSRV, draw);
				ImVec2 r0 = ImGui::GetItemRectMin();
				ImVec2 r1 = ImGui::GetItemRectMax();
				ImGui::GetWindowDrawList()->AddRect(
					r0 - ImVec2(2, 2), r1 + ImVec2(2, 2), IM_COL32(255, 255, 255, 160),
					8.0f, 0, 2.0f);
				ImGui::EndChild();
			}
			ImGui::End();
		}
	}
}

// ============================================================
// UI 헬퍼 함수들
// ============================================================
static void DrawCrossFadeEditor(const char* label, CrossFadeParams& f)
{
	ImGui::PushID(&f);
	ImGui::SeparatorText(label);

	ImGui::DragFloat("Duration (sec)", &f.durationSec, 0.01f, 0.0f, 2.0f, "%.3f");
	ImGui::Checkbox("Use Exit Time", &f.useExitTime);
	if (f.useExitTime)
		ImGui::SliderFloat("Exit Norm", &f.exitNorm, 0.0f, 1.0f, "%.3f");

	ImGui::SliderFloat("Entry Norm", &f.entryNorm, 0.0f, 1.0f, "%.3f");
	ImGui::Checkbox("SmoothStep", &f.smoothStep);

	ImGui::PopID();
}

static void DrawBlendCurve(bool smoothStep, float blend01, ImVec2 size)
{
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 p0 = ImGui::GetCursorScreenPos();
	ImVec2 p1 = ImVec2(p0.x + size.x, p0.y + size.y);

	// 배경
	dl->AddRectFilled(p0, p1, IM_COL32(30, 30, 30, 255), 4.0f);
	dl->AddRect(p0, p1, IM_COL32(90, 90, 90, 255), 4.0f);

	auto Eval = [&](float t)
		{
			t = std::clamp(t, 0.0f, 1.0f);
			if (!smoothStep) return t;
			return t * t * (3.0f - 2.0f * t);
		};

	// 곡선
	const int N = 64;
	ImVec2 prev{};
	for (int i = 0; i <= N; ++i)
	{
		float x = (float)i / (float)N;
		float y = Eval(x);

		ImVec2 p = ImVec2(
			p0.x + x * size.x,
			p1.y - y * size.y
		);

		if (i > 0)
			dl->AddLine(prev, p, IM_COL32(220, 220, 220, 255), 2.0f);
		prev = p;
	}

	// 현재 점(blend01)
	float yb = Eval(blend01);
	ImVec2 dot = ImVec2(p0.x + blend01 * size.x, p1.y - yb * size.y);
	dl->AddCircleFilled(dot, 5.0f, IM_COL32(255, 200, 0, 255));

	ImGui::Dummy(size);
}

static void DrawStateMachineGraph(const char* id, AnimStateMachine& sm)
{
	const auto& states = sm.GetStates();
	const auto& trans = sm.GetTransitions();

	ImVec2 avail = ImGui::GetContentRegionAvail();
	ImVec2 size = ImVec2(avail.x, 180.0f);
	ImGui::BeginChild(id, size, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 origin = ImGui::GetCursorScreenPos();

	const float nodeW = 120.0f;
	const float nodeH = 36.0f;
	const float gapX = 40.0f;
	const float gapY = 28.0f;

	// 간단 배치(가로로 쭉)
	std::vector<ImVec2> nodePos(states.size());
	for (int i = 0; i < (int)states.size(); ++i)
	{
		nodePos[(size_t)i] = ImVec2(
			origin.x + 20.0f + i * (nodeW + gapX),
			origin.y + 40.0f
		);
	}

	int cur = sm.GetCurrentStateIndex();
	bool inTr = sm.IsInTransition();
	int from = sm.GetFromStateIndex();
	int to = sm.GetToStateIndex();
	float b01 = sm.GetBlend01();

	// 링크 그리기
	for (int ti = 0; ti < (int)trans.size(); ++ti)
	{
		const auto& t = trans[(size_t)ti];

		int a = t.from;
		int b = t.to;
		if (b < 0 || b >= (int)states.size()) continue;

		ImVec2 pA;
		if (a >= 0 && a < (int)states.size())
			pA = ImVec2(nodePos[(size_t)a].x + nodeW, nodePos[(size_t)a].y + nodeH * 0.5f);
		else
			pA = ImVec2(origin.x + 10.0f, origin.y + 40.0f + nodeH * 0.5f); // AnyState

		ImVec2 pB = ImVec2(nodePos[(size_t)b].x, nodePos[(size_t)b].y + nodeH * 0.5f);

		bool activeEdge = inTr && (a == from) && (b == to);
		ImU32 col = activeEdge ? IM_COL32(255, 200, 0, 255) : IM_COL32(120, 120, 120, 255);
		float thickness = activeEdge ? (2.0f + 4.0f * b01) : 2.0f;

		dl->AddLine(pA, pB, col, thickness);

		if (activeEdge)
		{
			ImVec2 mid = ImVec2((pA.x + pB.x) * 0.5f, (pA.y + pB.y) * 0.5f - 14.0f);
			char buf[64];
			snprintf(buf, 64, "blend=%.2f", b01);
			dl->AddText(mid, IM_COL32(255, 220, 120, 255), buf);
		}
	}

	// 노드 그리기 클릭 강제전환도 가능하게 하고자 함
	// 그래야 바로바로 변하는거 볼 수 있음
	for (int i = 0; i < (int)states.size(); ++i)
	{
		ImVec2 p = nodePos[(size_t)i];
		ImVec2 p2 = ImVec2(p.x + nodeW, p.y + nodeH);

		bool activeNode = (i == cur);
		bool fromNode = inTr && (i == from);
		bool toNode = inTr && (i == to);

		ImU32 fill = IM_COL32(60, 60, 60, 255);
		ImU32 border = IM_COL32(120, 120, 120, 255);

		if (activeNode) border = IM_COL32(120, 255, 120, 255);
		if (fromNode)   border = IM_COL32(255, 200, 0, 255);
		if (toNode)     border = IM_COL32(255, 200, 0, 255);

		dl->AddRectFilled(p, p2, fill, 6.0f);
		dl->AddRect(p, p2, border, 6.0f, 0, activeNode ? 3.0f : 2.0f);

		dl->AddText(ImVec2(p.x + 8, p.y + 8), IM_COL32(220, 220, 220, 255), states[(size_t)i].name.c_str());

		// 클릭 히트박스
		ImGui::SetCursorScreenPos(p);
		ImGui::InvisibleButton((std::string("node##") + id + std::to_string(i)).c_str(), ImVec2(nodeW, nodeH));
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
			sm.ForceState(i);
	}

	// 상단 텍스트
	ImGui::SetCursorScreenPos(ImVec2(origin.x + 10, origin.y + 8));
	ImGui::Text("Current: %s  |  InTransition: %s", sm.CurrentStateName().c_str(), inTr ? "true" : "false");

	ImGui::EndChild();
}

static void DrawTransitionsEditor(const char* layerName, AnimStateMachine& sm)
{
	ImGui::PushID(layerName); // ★ 충돌 제거 핵심

	auto& trans = sm.GetTransitions();
	const auto& states = sm.GetStates();

	ImGui::SeparatorText(layerName);

	for (int i = 0; i < (int)trans.size(); ++i)
	{
		ImGui::PushID(i);

		auto& t = trans[(size_t)i];
		auto NameOf = [&](int idx)->const char*
			{
				if (idx < 0) return "*";
				if (idx >= 0 && idx < (int)states.size()) return states[(size_t)idx].name.c_str();
				return "?";
			};

		std::string header = std::string(NameOf(t.from)) + " -> " + NameOf(t.to) +
			"  (pri=" + std::to_string(t.priority) + ")";

		if (ImGui::TreeNode("##trnode", "%s", header.c_str()))
		{
			ImGui::DragInt("Priority", &t.priority, 1, -1000, 1000);

			DrawCrossFadeEditor("Fade", t.fade);

			ImGui::SeparatorText("Conditions");
			for (int ci = 0; ci < (int)t.conditions.size(); ++ci)
			{
				const auto& c = t.conditions[(size_t)ci];
				ImGui::BulletText("cond[%d] type=%d param=%s (f=%.2f i=%d)",
					ci, (int)c.type, c.param.c_str(), c.f, c.i);
			}

			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	ImGui::PopID();
}

void App::RenderQuickGuideUI()
{
	if (!m_->m_ShowQuickGuideWindow) return;

	ImGui::SetNextWindowPos(ImVec2(620, 240), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(330, 300), ImGuiCond_Always);
	if (ImGui::Begin("Quick Guide##36QuickGuide", &m_->m_ShowQuickGuideWindow, ImGuiWindowFlags_NoSavedSettings)) {
		const bool attached = m_->m_TpsCamAttached;

		if (ImGui::Button(attached ? "Detach Camera (V)" : "Attach Camera (V)", ImVec2(-1.0f, 0.0f))) {
			m_->m_TpsCamAttached = !m_->m_TpsCamAttached;
			if (m_->m_TpsCamAttached) {
				const XMFLOAT3 rotDeg = m_Camera.GetRotation();
				m_->m_TpsYawRad = XMConvertToRadians(rotDeg.y);
				m_->m_TpsPitchRad = std::clamp(XMConvertToRadians(rotDeg.x),
					m_->m_TpsPitchMin, m_->m_TpsPitchMax);
				if (InputSystem::Instance) {
					m_->m_TpsLastWheel = InputSystem::Instance->m_MouseState.scrollWheelValue;
				}
			}
		}

		if (ImGui::Button("Reset Follow View", ImVec2(-1.0f, 0.0f))) {
			m_->m_TpsCamAttached = true;
			m_->m_TpsYawRad = 0.0f;
			m_->m_TpsPitchRad = XMConvertToRadians(15.0f);
			m_->m_TpsDist = 220.0f;
			if (InputSystem::Instance) {
				m_->m_TpsLastWheel = InputSystem::Instance->m_MouseState.scrollWheelValue;
			}
		}

		ImGui::Text("Camera: %s", m_->m_TpsCamAttached ? "attached to player" : "free");
		ImGui::SeparatorText("Controls");
		ImGui::BulletText("V: attach camera");
		ImGui::BulletText("WASD: move, Shift: run");
		ImGui::BulletText("RMB drag: look, wheel: zoom");
		ImGui::BulletText("Ctrl: stance, LMB / R: action");
	}
	ImGui::End();
}

void App::RenderAdvancedRigUI()
{
	if (!m_->m_CharRigInited) return;

	auto& ctrl = m_->m_CharCtrl;

	ImGui::Begin("AnimGraph / Blend Debug##AdvancedRig");

	ImGui::Checkbox("Enable AdvancedRig", &m_->m_UseAdvancedRig);
	ImGui::Text("Rig Inited: %s", m_->m_CharRigInited ? "true" : "false");
	ImGui::Separator();

	// ----- Weapon Socket Transform -----
	ImGui::SeparatorText("Weapon Socket Transform");
	{
		auto& sock = m_->m_CharCtrl.config.weaponSocket;
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "[%s] attached to [%s]", sock.socketName.c_str(), sock.parentBone.c_str());
		bool changed = false;

		// |= 연산자를 사용해 하나라도 변경되면 changed가 true가 됨
		changed |= ImGui::DragFloat3("Socket Pos", &sock.pos.x, 1.00f, -500.0f, 500.0f, "%.4f");
		changed |= ImGui::DragFloat3("Socket Rot", &sock.rotDeg.x, 1.0f, -360.0f, 360.0f);
		changed |= ImGui::DragFloat3("Socket Scale", &sock.scale.x, 0.01f, 0.1f, 10.0f);

		// 변경 사항이 있을 때만 업데이트 함수 호출
		if (changed)
		{
			m_->m_CharCtrl.AdjustSocket();
		}
	}

	// ----- Base/Upper/Add Runtime -----
	ImGui::SeparatorText("Runtime States");
	ImGui::Text("Base : %s", ctrl.DebugBaseState().c_str());
	ImGui::Text("Upper: %s", ctrl.DebugUpperState().c_str());
	ImGui::Text("Add  : %s", ctrl.DebugAddState().c_str());

	// 전환 진행률 + 커브
	{
		auto& b = ctrl.BaseSM();
		ImGui::SeparatorText("Base Transition");
		ImGui::Text("InTransition=%s  blend=%.3f  from=%d to=%d",
			b.IsInTransition() ? "true" : "false",
			b.GetBlend01(), b.GetFromStateIndex(), b.GetToStateIndex());
		ImGui::ProgressBar(b.IsInTransition() ? b.GetBlend01() : 0.0f, ImVec2(-1, 0));
		DrawBlendCurve(b.GetActiveFade().smoothStep, b.IsInTransition() ? b.GetBlend01() : 0.0f, ImVec2(ImGui::GetContentRegionAvail().x, 70));
	}

	{
		auto& u = ctrl.UpperSM();
		ImGui::SeparatorText("Upper Transition");
		ImGui::Text("InTransition=%s  blend=%.3f", u.IsInTransition() ? "true" : "false", u.GetBlend01());
		ImGui::ProgressBar(u.IsInTransition() ? u.GetBlend01() : 0.0f, ImVec2(-1, 0));
		DrawBlendCurve(u.GetActiveFade().smoothStep, u.IsInTransition() ? u.GetBlend01() : 0.0f, ImVec2(ImGui::GetContentRegionAvail().x, 70));
	}

	{
		auto& a = ctrl.AddSM();
		ImGui::SeparatorText("Additive Transition");
		ImGui::Text("InTransition=%s  blend=%.3f", a.IsInTransition() ? "true" : "false", a.GetBlend01());
		ImGui::ProgressBar(a.IsInTransition() ? a.GetBlend01() : 0.0f, ImVec2(-1, 0));
		DrawBlendCurve(a.GetActiveFade().smoothStep, a.IsInTransition() ? a.GetBlend01() : 0.0f, ImVec2(ImGui::GetContentRegionAvail().x, 70));
	}

	// ----- Graph View -----
	ImGui::SeparatorText("Graph View (click node = Force State)");
	DrawStateMachineGraph("BaseGraph", ctrl.BaseSM());
	DrawStateMachineGraph("UpperGraph", ctrl.UpperSM());
	DrawStateMachineGraph("AddGraph", ctrl.AddSM());

	// ----- Transition Parameter Edit -----
	ImGui::SeparatorText("Edit Transitions (live)");
	DrawTransitionsEditor("Base Transitions", ctrl.BaseSM());
	DrawTransitionsEditor("Upper Transitions", ctrl.UpperSM());
	DrawTransitionsEditor("Add Transitions", ctrl.AddSM());

	// ----- Slot(Key) -> Animation Index Mapping -----
	ImGui::SeparatorText("Slot Mapping (key -> animation index)");
	{
		auto& map = ctrl.GetAnimIndexMap();
		const auto& animNames = ctrl.GetAnimNames();

		// 키 정렬해서 보여주기
		std::vector<std::string> keys;
		keys.reserve(map.size());
		for (auto& kv : map) keys.push_back(kv.first);
		std::sort(keys.begin(), keys.end());

		for (auto& k : keys)
		{
			int& idx = map[k];
			ImGui::PushID(k.c_str());

			ImGui::Text("%s", k.c_str());
			ImGui::SameLine(140);

			// 콤보로 애니 선택
			int cur = idx;
			const char* preview = (cur >= 0 && cur < (int)animNames.size()) ? animNames[(size_t)cur].c_str() : "<none>";
			if (ImGui::BeginCombo("##AnimCombo", preview))
			{
				// none
				if (ImGui::Selectable("<none>", cur < 0)) idx = -1;

				for (int i = 0; i < (int)animNames.size(); ++i)
				{
					bool sel = (i == cur);
					if (ImGui::Selectable(animNames[(size_t)i].c_str(), sel))
						idx = i;
					if (sel) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::PopID();
		}

		if (ImGui::Button("AutoBind (contains)"))
			ctrl.AutoBindCommonSlotsByContains();

		ImGui::SameLine();
		if (ImGui::Button("Rebuild Default Graph"))
		{
			// 그래프를 다시 만들고 싶으면:
			// - config나 slot mapping 바꾼 뒤 적용하려면 rebuild가 필요할 때가 있음
			ctrl.BuildDefaultGraph();
			ctrl.ResetRuntime();
		}
	}

	// ----- Recoil (Shake) -----
	ImGui::SeparatorText("Recoil (Shake)");
	ImGui::SliderFloat("Recoil Kick", &m_->m_RecoilKickUi, 0.0f, 1.5f);
	ImGui::SliderFloat("Recoil Decay", &m_->m_RecoilDecayUi, 0.0f, 30.0f);

	// ----- Character Move (TPS) -----
	ImGui::SeparatorText("Character Move (TPS)");
	ImGui::Checkbox("Rotate to Move Dir", &m_->m_CharRotateToMove);
	ImGui::SliderFloat("Walk Speed", &m_->m_CharWalkSpeed, 10.0f, 800.0f);
	ImGui::SliderFloat("Run Mul", &m_->m_CharRunMul, 1.0f, 3.0f);
	ImGui::SliderFloat("Turn Speed", &m_->m_CharTurnSpeed, 1.0f, 25.0f);

	// ----- Sniper -----
	ImGui::SeparatorText("Sniper");
	ImGui::Checkbox("Sniper Enabled", &m_->m_SniperEnabled);
	ImGui::SliderFloat("Charge Time (sec)", &m_->m_SniperChargeTimeSec, 0.1f, 3.0f);
	ImGui::SliderFloat("Aim Radius", &m_->m_SniperAimRadius, 4.0f, 30.0f);



	ImGui::End();
}

void App::RenderWaitingUI()
{
	if (!m_->m_ShowSceneImageWindow) return;

	ImGuiIO& io = ImGui::GetIO();
	ImVec2 size(740.0f, 900.0f); // 프로그레스 바 공간만큼 높이 약간 증가

	ImGui::SetNextWindowPos(
		ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
		ImGuiCond_Always,
		ImVec2(0.5f, 0.5f)
	);

	//ImVec2 pos(io.DisplaySize.x - size.x - 20.0f, 20.0f);
	//ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);

	//if (ImGui::Begin("Loading...", &m_->m_ShowSceneImageWindow, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
	//if (ImGui::Begin("Loading...", &m_->m_ShowSceneImageWindow, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
	if (ImGui::Begin("Loading...", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
	{
		if (m_bIsLoaded) // atomic 읽기
		{
			// Play the public loading-complete cue once.
			if (!m_->m_LoadingDoneSoundPlayed)
			{
				Sound::PlaySFX(L"UiDone", 1.0f, 1.0f, false);
				m_->m_LoadingDoneSoundPlayed = true;
				m_->m_CurrentSceneImagePath = L"..\\Resource\\Image\\Public\\LoadingDone.png";
				LoadSceneImage(m_->m_CurrentSceneImagePath);
			}

			// "눌러보세요!" 펄스 효과 텍스트
			float time = (float)ImGui::GetTime();
			float pulse = (sinf(time * 5.0f) + 1.0f) * 0.5f;
			ImGui::SetWindowFontScale(1.5f);
			ImGui::Text("%s", Utf8FromWString(L"로딩이 완료되었습니다.").c_str());

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f + pulse * 0.1f, 0.5f + pulse * 0.5f, 0.0f + pulse * 0.8f, 1.0f));
			if (m_pFontLarge)ImGui::PushFont(m_pFontLarge);
			ImGui::Text("%s", Utf8FromWString(L"마우스 좌클릭으로 시작하세요!").c_str());
			ImGui::PopStyleColor();
			ImGui::SetWindowFontScale(1.0f);
			if (m_pFontLarge)ImGui::PopFont();

			ImGui::Separator();
			ImGui::Spacing();

			// Public loading-complete image; click to start.
			if (m_->m_pSceneImageSRV)
			{
				ImVec2 avail = ImGui::GetContentRegionAvail();
				float aspect = (m_->m_SceneImageSize.y > 0) ? (m_->m_SceneImageSize.x / m_->m_SceneImageSize.y) : 1.0f;
				float w = avail.x;
				float h = w / aspect;
				if (h > avail.y) { h = avail.y; w = h * aspect; }

				// 가로 중앙 정렬 계산함. (남은 공간 - 이미지 너비)의 절반만큼 커서를 이동함
				if (avail.x > w)
				{
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail.x - w) * 0.5f);
				}

				ImVec2 imgPos = ImGui::GetCursorScreenPos(); // 커서 이동 후의 위치를 저장해야 함
				ImVec2 imgSize(w, h);

				// 투명 버튼을 이미지 위에 깔아서 클릭 감지
				ImGui::PushID("DoneClickArea");
				if (ImGui::InvisibleButton("##ClickArea", imgSize))
				{
					// 클릭 시 게임 시작
					m_bIsGameStarted = true;
				}
				ImGui::PopID();

				// 실제 이미지 그리기 (저장해둔 중앙 위치 사용)
				ImGui::SetItemAllowOverlap();
				ImGui::SetCursorScreenPos(imgPos);
				ImGui::Image((ImTextureID)m_->m_pSceneImageSRV, imgSize);
			}

			if (InputSystem::Instance->m_MouseState.leftButton)
			{
				m_bIsGameStarted = true;
			}
		}
		else
		{
			// "눌러보세요!" 펄스 효과 텍스트
			float time = (float)ImGui::GetTime();
			float pulse = (sinf(time * 5.0f) + 1.0f) * 0.5f;
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f + pulse * 0.1f, 0.5f + pulse * 0.5f, 0.0f + pulse * 0.8f, 1.0f));
			if (m_pFontLarge)ImGui::PushFont(m_pFontLarge);

			ImGui::Text("%s", Utf8FromWString(L"샘플을 눌러보세요!").c_str());
			ImGui::PopStyleColor();
			ImGui::SetWindowFontScale(1.0f);

			ImGui::Separator();
			ImGui::Spacing();

			if (m_pFontLarge)ImGui::PopFont();


			ImGui::Spacing();

			// 3. 로딩 프로그레스 바
			// ImVec2(-1, 0)은 가로 폭을 꽉 채운다는 의미
			ImGui::ProgressBar(m_fLoadingProgress, ImVec2(-1.0f, 0.0f),
				std::format("Loading... {:.0f}%", m_fLoadingProgress * 100.0f).c_str());
			ImGui::Spacing();
			const wchar_t* currentPtr = m_sLoadingStr.load(); // UI 스레드에서 acquire 또는 relaxed로 로드
			ImGui::Text("%s", Utf8FromWString(currentPtr).c_str());

			ImGui::Spacing();

			// 4. Public comic preview shown during loading.
			if (ImGui::Button(Utf8FromWString(L"샘플 보기").c_str()))
			{
				m_->m_MangaIndex = 0;
				m_->m_CurrentSceneImagePath = L"..\\Resource\\Image\\Public\\Loading.png";
				LoadSceneImage(m_->m_CurrentSceneImagePath);
			}
			ImGui::Spacing();
			/*
			if (m_->m_pSceneImageSRV)
			{
				ImVec2 avail = ImGui::GetContentRegionAvail();
				float aspect = (m_->m_SceneImageSize.y > 0) ? (m_->m_SceneImageSize.x / m_->m_SceneImageSize.y) : 1.0f;
				float w = avail.x;
				float h = w / aspect;
				if (h > avail.y) { h = avail.y; w = h * aspect; }

				// 가로 중앙 정렬 계산함. (남은 공간 - 이미지 너비)의 절반만큼 커서를 이동함
				if (avail.x > w)
				{
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail.x - w) * 0.5f);
				}

				ImVec2 imgPos = ImGui::GetCursorScreenPos(); // 커서 이동 후의 위치를 저장해야 함
				ImVec2 imgSize(w, h);

				// 투명 버튼을 이미지 위에 깔아서 클릭 감지
				ImGui::PushID("ImgBtn");
				if (ImGui::InvisibleButton("##ClickArea", imgSize))
				{
					// 클릭 시 동작 (기존 유지)
					if (!m_->m_IsUsingTempImage && !m_->m_ShowScenePopup)
					{
						m_->m_OriginalSceneImagePath = m_->m_CurrentSceneImagePath;

						bool isSceneA = (m_SceneIndex == 0);
						m_->m_TempSceneImagePath = isSceneA ? L"..\\Resource\\Image\\SceneB.png" : L"..\\Resource\\Image\\SceneA.png";
						m_->m_ScenePopupMessage = isSceneA ? Utf8FromWString(L"안녕하세요 토끼씨!") : Utf8FromWString(L"기뻐요 토끼씨!");

						LoadSceneImage(m_->m_TempSceneImagePath);
						m_->m_CurrentSceneImagePath = m_->m_TempSceneImagePath;
						m_->m_IsUsingTempImage = true;
						m_->m_ShowScenePopup = true;
						m_->m_ScenePopupTimer = 2.0f;
					}
				}
				ImGui::PopID();

				// 실제 이미지 그리기 (저장해둔 중앙 위치 사용)
				ImGui::SetItemAllowOverlap();
				ImGui::SetCursorScreenPos(imgPos);
				ImGui::Image((ImTextureID)m_->m_pSceneImageSRV, imgSize);

				// 팝업 그리기 (이미지 위에 오버레이)
				if (m_->m_ShowScenePopup)
				{
					ImDrawList* dl = ImGui::GetWindowDrawList();
					ImVec2 pSize(300.0f, 80.0f);
					ImVec2 pPos(imgPos.x + (w - pSize.x) * 0.5f, imgPos.y + (h - pSize.y) * 0.5f);

					// 반투명 배경 박스
					dl->AddRectFilled(pPos, ImVec2(pPos.x + pSize.x, pPos.y + pSize.y), IM_COL32(0, 0, 0, 200), 10.0f);
					dl->AddRect(pPos, ImVec2(pPos.x + pSize.x, pPos.y + pSize.y), IM_COL32(255, 255, 255, 255), 10.0f, 0, 2.0f);

					// 텍스트 중앙 정렬
					ImVec2 txtSz = ImGui::CalcTextSize(m_->m_ScenePopupMessage.c_str());
					dl->AddText(ImVec2(pPos.x + (pSize.x - txtSz.x) * 0.5f, pPos.y + (pSize.y - txtSz.y) * 0.5f),
						IM_COL32(255, 255, 255, 255), m_->m_ScenePopupMessage.c_str());
				}
			}
			*/

			// Public comic viewer: loading placeholder followed by public pages.
			if (m_->m_pSceneImageSRV)
			{
				ImVec2 avail = ImGui::GetContentRegionAvail();
				float aspect = (m_->m_SceneImageSize.y > 0) ? (m_->m_SceneImageSize.x / m_->m_SceneImageSize.y) : 1.0f;
				float w = avail.x;
				float h = w / aspect;
				if (h > avail.y) { h = avail.y; w = h * aspect; }

				// 가로 중앙 정렬 계산함. (남은 공간 - 이미지 너비)의 절반만큼 커서를 이동함
				if (avail.x > w)
				{
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail.x - w) * 0.5f);
				}

				ImVec2 imgPos = ImGui::GetCursorScreenPos(); // 커서 이동 후의 위치를 저장해야 함
				ImVec2 imgSize(w, h);

				// 투명 버튼을 이미지 위에 깔아서 클릭 감지
				ImGui::PushID("ComicClickArea");
				if (ImGui::InvisibleButton("##ClickArea", imgSize))
				{
					// Advance to the next public comic image.
					if (m_->m_MangaIndex == 0)
					{
						m_->m_MangaIndex = 1;
						m_->m_CurrentSceneImagePath = std::format(L"..\\Resource\\Image\\Public\\Comic\\{:02d}.png", m_->m_MangaIndex);
						LoadSceneImage(m_->m_CurrentSceneImagePath);
						Sound::PlaySFX(L"UiAdvance", 1.0f, 1.0f, false);
					}
					else if (m_->m_MangaIndex < 3)
					{
						m_->m_MangaIndex++;
						m_->m_CurrentSceneImagePath = std::format(L"..\\Resource\\Image\\Public\\Comic\\{:02d}.png", m_->m_MangaIndex);
						LoadSceneImage(m_->m_CurrentSceneImagePath);

						Sound::PlaySFX(L"UiAdvance", 1.0f, 1.0f, false);
					}
					// Stop advancing after the staged public pages.
				}
				ImGui::PopID();

				// 실제 이미지 그리기 (저장해둔 중앙 위치 사용)
				ImGui::SetItemAllowOverlap();
				ImGui::SetCursorScreenPos(imgPos);
				ImGui::Image((ImTextureID)m_->m_pSceneImageSRV, imgSize);
			}
			else
			{
				ImGui::Text("Loading Image...");
			}
		}

	}
	ImGui::End();
}

void App::RenderSceneImageWindow() {
	if (!m_->m_ShowSceneImageWindow)
		return;

	// 씬 이미지 창 위치/크기 설정 (08_ImguiSystemInfo 참고)
	ImGuiIO& io = ImGui::GetIO();
	ImVec2 size(130.0f, 260.0f);
	ImVec2 pos(io.DisplaySize.x - size.x - 330.0f, 20.0f);
	ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);

	if (ImGui::Begin("Scene Image", &m_->m_ShowSceneImageWindow)) {
		// 빛나는 글씨 효과로 "눌러보세요!" 텍스트 표시
		// 펄스 효과를 위한 시간 기반 색상 계산 (더 빠르고 눈에 띄게)
		float time = (float)ImGui::GetTime();
		float pulse =
			(sinf(time * 5.0f) + 1.0f) * 0.5f; // 0.0 ~ 1.0 사이 값 (속도 증가)

		// 밝은 노란색에서 흰색으로 강렬한 펄스 효과
		// 색상 범위를 넓혀서 더 눈에 띄게 만듦
		float r = 0.9f + pulse * 0.1f; // 0.9 ~ 1.0 (약간 변동)
		float g = 0.5f + pulse * 0.5f; // 0.5 ~ 1.0 (넓은 범위)
		float b = 0.0f + pulse * 0.8f; // 0.0 ~ 0.8 (더 넓은 범위)
		float a = 0.5f + pulse * 0.5f; // 0.5 ~ 1.0 (더 넓은 범위)

		// 글씨 크기 키우기
		ImGui::SetWindowFontScale(1.5f); // 기본 크기의 1.5배
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(r, g, b, a));
		ImGui::Text("%s", Utf8FromWString(L"눌러보세요!").c_str());
		ImGui::PopStyleColor();
		ImGui::SetWindowFontScale(1.0f); // 원래 크기로 복원
		ImGui::Separator();

		// 이미지 표시
		if (m_->m_pSceneImageSRV) {
			ImVec2 avail = ImGui::GetContentRegionAvail();
			float aspect = (m_->m_SceneImageSize.y > 0)
				? (m_->m_SceneImageSize.x / m_->m_SceneImageSize.y)
				: 1.0f;
			float displayWidth = avail.x;
			float displayHeight = displayWidth / aspect;
			if (displayHeight > avail.y) {
				displayHeight = avail.y;
				displayWidth = displayHeight * aspect;
			}

			// 이미지 위치 계산
			ImVec2 imagePos = ImGui::GetCursorScreenPos();
			ImVec2 imageSize(displayWidth, displayHeight);

			// 이미지를 클릭 가능한 버튼처럼 표시 (투명 버튼 위에 이미지)
			ImGui::PushID("SceneImageButton");
			if (ImGui::InvisibleButton("##SceneImage", imageSize)) {
				// 이미지 클릭 시 임시 이미지로 변경
				if (!m_->m_IsUsingTempImage && !m_->m_ShowScenePopup) {
					// 원본 이미지 경로 저장
					m_->m_OriginalSceneImagePath = m_->m_CurrentSceneImagePath;

					// 현재 씬에 따라 다른 임시 이미지와 메시지 설정
					if (m_SceneIndex == 0) {
						// SceneA일 때 SceneB 이미지로 변경
						m_->m_TempSceneImagePath = L"..\\Resource\\Image\\SceneB.png";
						m_->m_ScenePopupMessage = Utf8FromWString(L"안녕하세요 토끼씨!");
					}
					else {
						// SceneB일 때 SceneA 이미지로 변경
						m_->m_TempSceneImagePath = L"..\\Resource\\Image\\SceneA.png";
						m_->m_ScenePopupMessage = Utf8FromWString(L"기뻐요 토끼씨!");
					}

					// 임시 이미지 로드
					LoadSceneImage(m_->m_TempSceneImagePath);
					m_->m_CurrentSceneImagePath = m_->m_TempSceneImagePath;
					m_->m_IsUsingTempImage = true;

					// 팝업 표시 및 타이머 시작 (2초 후 원본으로 복원)
					m_->m_ShowScenePopup = true;
					m_->m_ScenePopupTimer = 2.0f;
				}
			}
			ImGui::PopID();

			// 이미지 표시 (버튼 위에)
			ImGui::SetItemAllowOverlap();
			ImGui::SetCursorScreenPos(imagePos);
			ImGui::Image((ImTextureID)m_->m_pSceneImageSRV, imageSize);

			// 팝업 표시 (이미지 위에 오버레이)
			if (m_->m_ShowScenePopup) {
				ImDrawList* drawList = ImGui::GetWindowDrawList();

				// 팝업 배경 (반투명)
				ImVec2 popupSize(300.0f, 80.0f);
				ImVec2 popupPos(imagePos.x + (imageSize.x - popupSize.x) * 0.5f,
					imagePos.y + (imageSize.y - popupSize.y) * 0.5f);
				ImVec2 popupEnd =
					ImVec2(popupPos.x + popupSize.x, popupPos.y + popupSize.y);

				// 배경 그리기
				drawList->AddRectFilled(popupPos, popupEnd, IM_COL32(0, 0, 0, 200),
					10.0f);
				drawList->AddRect(popupPos, popupEnd, IM_COL32(255, 255, 255, 255),
					10.0f, 0, 2.0f);

				// 텍스트 그리기 (중앙 정렬)
				ImVec2 textSize = ImGui::CalcTextSize(m_->m_ScenePopupMessage.c_str());
				ImVec2 textPos(popupPos.x + (popupSize.x - textSize.x) * 0.5f,
					popupPos.y + (popupSize.y - textSize.y) * 0.5f);
				drawList->AddText(textPos, IM_COL32(255, 255, 255, 255),
					m_->m_ScenePopupMessage.c_str());
			}
		}
		else {
			ImGui::TextColored(ImVec4(1, 1, 0, 1), "Image not loaded");
		}
	}
	ImGui::End();
}

// G-Buffer 디버그 뷰
void App::RenderGBufferDebug() {
	if (!m_->m_UseDeferredRendering)
		return;

	ImGuiIO& io = ImGui::GetIO();
	ImVec2 pos(io.DisplaySize.x - 350.0f, 20.0f);
	ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(330.0f, 400.0f), ImGuiCond_FirstUseEver);

	if (ImGui::Begin("G-Buffer Debug View")) {
		ImGui::Text("G-Buffer Contents");
		ImGui::Separator();

		// 각 G-Buffer 텍스처 표시
		const char* gbufferNames[] = { "PositionWS", "NormalWS", "Metalness", "Roughness", "BaseColor" };

		for (int i = 0; i < Impl::GBufferCount; ++i) {
			ImGui::Text("%s", gbufferNames[i]);
			ImGui::Image((ImTextureID)m_->m_pGBufferSRVs[i].Get(), ImVec2(128, 128));
			ImGui::Separator();
		}
	}
	ImGui::End();
}

// Deferred Rendering UI - 설정 및 G-Buffer 디버그 뷰
void App::RenderDeferredUI() {
	// Deferred Rendering이 활성화되지 않았으면 표시하지 않음
	if (!m_->m_UseDeferredRendering)
		return;

	// Deferred Rendering Settings 윈도우
	ImGuiIO& io = ImGui::GetIO();
	ImVec2 settingsPos(20.0f, 20.0f);
	ImGui::SetNextWindowPos(settingsPos, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(350.0f, 500.0f), ImGuiCond_FirstUseEver);

	// G-Buffer Debug View 윈도우
	ImVec2 gbufferPos(io.DisplaySize.x - 350.0f, 20.0f);
	ImGui::SetNextWindowPos(gbufferPos, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(330.0f, 600.0f), ImGuiCond_FirstUseEver);

	if (ImGui::Begin("G-Buffer Debug View")) {
		ImGui::Text("G-Buffer Contents");
		ImGui::Separator();

		// 각 G-Buffer 텍스처 이름
		const char* gbufferNames[] = { "PositionWS", "NormalWS", "Metalness",
									  "Roughness", "BaseColor" };

		// G-Buffer 텍스처들을 2x3 그리드로 표시
		for (int i = 0; i < Impl::GBufferCount; ++i) {
			ImGui::BeginGroup();
			ImGui::Text("%s", gbufferNames[i]);
			ImGui::Image((ImTextureID)m_->m_pGBufferSRVs[i].Get(), ImVec2(128, 128));
			ImGui::EndGroup();

			// 2개씩 한 줄에 배치
			if ((i + 1) % 2 != 0 && i < Impl::GBufferCount - 1) {
				ImGui::SameLine();
			}
			else {
				ImGui::Separator();
			}
		}

		// Depth Buffer 표시 (있는 경우)
		if (m_->m_pDepthStencilView) {
			ImGui::Separator();
			ImGui::Text("Depth Buffer");
			// Depth SRV가 별도로 있는지 확인 필요
			// 일반적으로 Depth는 별도 SRV로 접근하므로, 여기서는 주석 처리
			// ImGui::Image((ImTextureID)m_->m_pDepthSRV.Get(), ImVec2(128, 128));
		}
	}
	ImGui::End();
}

// Sound Debug UI - 3D 사운드 및 Pan 테스트
void App::RenderSoundDebugUI() {
	ImGuiIO& io = ImGui::GetIO();
	ImVec2 pos(io.DisplaySize.x - 400.0f, 200.0f);
	ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(380.0f, 600.0f), ImGuiCond_FirstUseEver);

	if (ImGui::Begin("Sound Debug"))
	{
		// ===== 2D Pan 테스트 =====
		ImGui::SeparatorText("Pan Test (2D)");
		static float pan = 0.0f;
		if (ImGui::Button("Play Pan Test"))
		{
			// Use a public SFX key loaded during sample initialization.
			Sound::PlayPanTest2D(L"UiAdvance", true);
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop Pan Test"))
		{
			Sound::StopPanTest2D();
		}
		ImGui::SliderFloat("Pan (-1=L, +1=R)", &pan, -1.0f, 1.0f);
		Sound::SetPanTest2D(pan);

		// ===== SoundBox 파라미터 =====
		ImGui::SeparatorText("SoundBoxes");
		auto& boxes = m_->m_SoundBoxSystem.GetBoxes();
		for (int i = 0; i < (int)boxes.size(); ++i)
		{
			auto& b = boxes[i];
			ImGui::PushID(i);
			if (ImGui::TreeNode("SoundBox", "SoundBox #%d", i))
			{
				ImGui::Text("Key: %ls", b.bgmKey.c_str());
				ImGui::DragFloat3("Pos", &b.position.x, 0.1f);
				ImGui::DragFloat3("Scale", &b.scale.x, 0.1f);

				ImGui::SliderFloat("Edge Volume", &b.edgeVolume, 0.0f, 1.0f);
				ImGui::SliderFloat("Center Volume", &b.centerVolume, 0.0f, 1.0f);
				ImGui::SliderFloat("Curve", &b.curve, 0.1f, 8.0f);

				ImGui::DragFloat("MinDist", &b.minDist, 0.1f, 0.01f, 1000.0f);
				ImGui::DragFloat("MaxDist", &b.maxDist, 0.1f, 0.01f, 5000.0f);

				ImGui::SliderFloat("LR Weight", &b.lrWeight, -1.0f, 1.0f);
				ImGui::DragFloat("LR Max Meters", &b.lrMaxMeters, 0.1f, 0.0f, 100.0f);

				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		ImGui::End();
	}
}
