void App::OnInputProcess(const Keyboard::State& KeyState,
	const Keyboard::KeyboardStateTracker& KeyTracker,
	const Mouse::State& MouseState,
	const Mouse::ButtonStateTracker& MouseTracker)
{
	// 1) V키 토글 (ImGui가 키보드 잡고 있으면 무시)
	if (!ImGui::GetIO().WantCaptureKeyboard && KeyTracker.IsKeyPressed(Keyboard::V))
	{
		m_->m_TpsCamAttached = !m_->m_TpsCamAttached;

		// 켤 때 현재 카메라 각도에서 이어서 시작
		if (m_->m_TpsCamAttached)
		{
			XMFLOAT3 rotDeg = m_Camera.GetRotation();
			m_->m_TpsYawRad = XMConvertToRadians(rotDeg.y);
			m_->m_TpsPitchRad = XMConvertToRadians(rotDeg.x);
			m_->m_TpsLastWheel = MouseState.scrollWheelValue;
		}
	}

	// 2) TPS 부착 모드면: 자유카메라 입력은 막고, TPS 입력만 처리
	if (m_->m_TpsCamAttached)
	{
		// RMB 누르면 relative
		if (InputSystem::Instance && InputSystem::Instance->m_Mouse)
		{
			InputSystem::Instance->m_Mouse->SetMode(MouseState.rightButton ? Mouse::MODE_RELATIVE : Mouse::MODE_ABSOLUTE);
		}

		// RMB 드래그로 yaw/pitch
		if (!ImGui::GetIO().WantCaptureMouse && MouseState.positionMode == Mouse::MODE_RELATIVE)
		{
			m_->m_TpsYawRad += float(MouseState.x) * m_->m_TpsRotSpeed;
			m_->m_TpsPitchRad += float(MouseState.y) * m_->m_TpsRotSpeed;
			m_->m_TpsPitchRad = std::clamp(m_->m_TpsPitchRad, m_->m_TpsPitchMin, m_->m_TpsPitchMax);
		}

		// 휠 줌
		if (!ImGui::GetIO().WantCaptureMouse)
		{
			int wheel = MouseState.scrollWheelValue;
			int delta = wheel - m_->m_TpsLastWheel;
			m_->m_TpsLastWheel = wheel;

			if (delta != 0)
			{
				// DirectXTK wheel: 보통 120 단위
				float notches = float(delta) / 120.0f;
				m_->m_TpsDist -= notches * m_->m_TpsZoomStep;
				m_->m_TpsDist = std::clamp(m_->m_TpsDist, m_->m_TpsDistMin, m_->m_TpsDistMax);
			}
		}

		// TPS 모드에서는 기본 카메라 입력 처리 X
		(void)KeyState; (void)MouseTracker;
		return;
	}

	// 3) TPS 꺼져 있으면 원래대로(자유 카메라)
	GameApp::OnInputProcess(KeyState, KeyTracker, MouseState, MouseTracker);
}

void App::OnUpdate(const float& dt) {
	if (!m_bIsLoaded)
	{
		if (m_->m_ShowScenePopup) {
			m_->m_ScenePopupTimer -= dt;
			if (m_->m_ScenePopupTimer <= 0.0f) {
				m_->m_ShowScenePopup = false;
				m_->m_ScenePopupTimer = 0.0f;

				// 임시 이미지 사용 중이었다면 원본 이미지로 복원
				if (m_->m_IsUsingTempImage) {
					m_->m_IsUsingTempImage = false;
					LoadSceneImage(m_->m_OriginalSceneImagePath);
					m_->m_CurrentSceneImagePath = m_->m_OriginalSceneImagePath;
				}
			}
		}
		return;
	}

	// ====================================== 사운드 중첩 재생 테스트 (1, 2, 3 숫자 키) ======================================
	if (InputSystem::Instance)
	{
		// 1 key: play a neutral UI advance sound
		if (InputSystem::Instance->m_KeyboardStateTracker.IsKeyPressed(Keyboard::Keys::D1))
		{
			Sound::PlaySFX(L"UiAdvance");
			m_->PushLog("[Sound] UI advance sound played");
		}
		// 2 키: 현재 재생 중인 모든 SFX 정지 (BGM은 계속 재생)
		if (InputSystem::Instance->m_KeyboardStateTracker.IsKeyPressed(Keyboard::Keys::D2))
		{
			Sound::StopAllSFX();
			m_->PushLog("[Sound] All SFX stopped");
		}
		// 3 키: BGM 정지
		if (InputSystem::Instance->m_KeyboardStateTracker.IsKeyPressed(Keyboard::Keys::D3))
		{
			Sound::StopLastSFX();
			m_->PushLog("[Sound] Last SFX stopped");
		}
	}

	// ====================================== SoundBox 시스템 업데이트 ======================================
	// Use the loaded player slot when available; otherwise listen from the camera.
	const int listenerModelIndex = m_->m_CharModelIndex;
	if (listenerModelIndex >= 0 && listenerModelIndex < (int)m_->m_Models.size())
	{
		auto& player = *m_->m_Models[(size_t)listenerModelIndex];
		XMFLOAT3 playerPos = player.pos;

		// ===== 3D 사운드 리스너 업데이트 =====
		//   - (playerPos - prevPos) / dt 를 그대로 사용할때 dt가 매우 작거나 튀면
		//     속도가 폭주하여 도플러 계산 시 피치가 '찌직'거리며 심하게 변조됨
		//   - 그래서 다음처럼 함. safeDt + 속도 클램프를 넣고, forward/up 벡터는 정규화해서 사용
		static XMFLOAT3 prevPos = { 0, 0, 0 };

		// dt가 0 또는 너무 작은 경우를 방지 (도플러/속도 폭주 방지)
		const float safeDt = (dt > 1.0e-4f) ? dt : 1.0e-4f;

		XMFLOAT3 rawVel = {
			(playerPos.x - prevPos.x) / safeDt,
			(playerPos.y - prevPos.y) / safeDt,
			(playerPos.z - prevPos.z) / safeDt
		};
		prevPos = playerPos;

		// 속도 벡터의 크기를 일정 값으로 클램프해서 이상치(스파이크) 제거
		auto ClampVel = [](XMFLOAT3 v, float maxSpeed)
		{
			const float s2 = v.x * v.x + v.y * v.y + v.z * v.z;
			const float maxS2 = maxSpeed * maxSpeed;
			if (s2 > maxS2)
			{
				const float inv = maxSpeed / std::sqrt(s2);
				v.x *= inv;
				v.y *= inv;
				v.z *= inv;
			}
			return v;
		};
		XMFLOAT3 vel = ClampVel(rawVel, 50.0f); // 필요시 50.0f 값을 튜닝

		// 카메라 방향 사용 (FMOD에 넣기 전에 정규화해서 전달)
		auto Normalize3 = [](XMFLOAT3 v)
		{
			XMVECTOR x = XMLoadFloat3(&v);
			x = XMVector3Normalize(x);
			XMStoreFloat3(&v, x);
			return v;
		};

		XMFLOAT3 forward = Normalize3(m_Camera.GetForward());
		XMFLOAT3 up = Normalize3(m_Camera.GetUp());

		Sound::SetListener(playerPos, vel, forward, up);

		// SoundBox 업데이트(3D 소스/볼륨 갱신)
		m_->m_SoundBoxSystem.Update(playerPos);
	}
	else
	{
		// 모델이 없을 때는 카메라를 리스너로 사용
		XMFLOAT3 camPos = m_Camera.GetPosition();
		static XMFLOAT3 prevCamPos = { 0, 0, 0 };

		// 위와 동일하게 safeDt/클램프 적용
		const float safeDt = (dt > 1.0e-4f) ? dt : 1.0e-4f;
		XMFLOAT3 rawVel = {
			(camPos.x - prevCamPos.x) / safeDt,
			(camPos.y - prevCamPos.y) / safeDt,
			(camPos.z - prevCamPos.z) / safeDt
		};
		prevCamPos = camPos;

		auto ClampVel = [](XMFLOAT3 v, float maxSpeed)
		{
			const float s2 = v.x * v.x + v.y * v.y + v.z * v.z;
			const float maxS2 = maxSpeed * maxSpeed;
			if (s2 > maxS2)
			{
				const float inv = maxSpeed / std::sqrt(s2);
				v.x *= inv;
				v.y *= inv;
				v.z *= inv;
			}
			return v;
		};
		XMFLOAT3 vel = ClampVel(rawVel, 50.0f);

		auto Normalize3 = [](XMFLOAT3 v)
		{
			XMVECTOR x = XMLoadFloat3(&v);
			x = XMVector3Normalize(x);
			XMStoreFloat3(&v, x);
			return v;
		};

		XMFLOAT3 forward = Normalize3(m_Camera.GetForward());
		XMFLOAT3 up = Normalize3(m_Camera.GetUp());

		Sound::SetListener(camPos, vel, forward, up);
		m_->m_SoundBoxSystem.Update(camPos);
	}

	// Shadow light view-projection (directional, orthographic, scene-anchored
	// with texel snapping)
	auto UpdateShadow = [this](XMFLOAT3& focusF) {
		// 모델 중심을 포커스로 사용하여 카메라 움직임과 무관하게 안정화
		XMVECTOR focus = XMLoadFloat3(&focusF);

		// 1. 원본 방향 벡터 로드
		XMVECTOR rawDir = XMLoadFloat3(&m_->m_DirLight.direction);

		// 2. 벡터의 길이(제곱)를 구해서 0에 가까운지 확인 (Epsilon 체크)
		XMVECTOR lenSq = XMVector3LengthSq(rawDir);
		float lenSqVal = XMVectorGetX(lenSq);
		XMVECTOR fwd;
		// 1e-6f는 0에 매우 가까운 작은 수 (FLT_EPSILON 등을 써도 됨)
		if (lenSqVal < 1.0e-6f) {
			// 예외 처리: 방향이 0이면 기본값(예: 수직 아래)으로 강제 설정
			fwd = XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);
		}
		else {
			// 정상이면 정규화 수행
			fwd = XMVector3Normalize(rawDir);
		}

		float r = m_->m_ShadowOrthoRadius;
		// 큰 오브젝트에서도 라이트 카메라가 항상 AABB 뒤쪽에 위치하도록 반경만큼
		// 뒤로 물린다
		float backDist = r;
		XMVECTOR lightPos = XMVectorSubtract(focus, XMVectorScale(fwd, backDist));
		XMVECTOR up = XMVectorSet(0, 1, 0, 0);
		if (fabsf(XMVectorGetX(XMVector3Dot(up, fwd))) > 0.99f)
			up = XMVectorSet(0, 0, 1, 0);
		XMMATRIX LView = XMMatrixLookToLH(lightPos, fwd, up);

		// 라이트 뷰 공간에서 포커스 주변 AABB(±r)를 투영해 동적으로 near/far 계산
		float minZ = 1e9f, maxZ = -1e9f;
		for (int sx = -1; sx <= 1; sx += 2)
			for (int sy = -1; sy <= 1; sy += 2)
				for (int sz = -1; sz <= 1; sz += 2) {
					XMVECTOR cornerWS = XMVectorSet(focusF.x + sx * r, focusF.y + sy * r,
						focusF.z + sz * r, 1.0f);
					XMVECTOR cornerLS = XMVector3TransformCoord(cornerWS, LView);
					float z = XMVectorGetZ(cornerLS);
					minZ = (z < minZ) ? z : minZ;
					maxZ = (z > maxZ) ? z : maxZ;
				}
		// 여유 패딩(5%)을 주고 near는 0.01 이상으로 고정
		float zPad = r * 0.05f;
		float zn = (minZ - zPad);
		if (zn < 0.01f)
			zn = 0.01f;
		float zf = maxZ + zPad;
		if (zf <= zn)
			zf = zn + 0.01f;
		XMMATRIX LProj = XMMatrixOrthographicOffCenterLH(-r, r, -r, r, zn, zf);

		// 텍셀 스냅: 라이트 뷰 공간 XY를 섀도우맵 텍셀 그리드에 정렬(near/far에는
		// 영향 없음)
		XMVECTOR focusLS = XMVector3TransformCoord(focus, LView);
		float texelWorld = (2.0f * r) / (float)m_->m_ShadowSize;
		float fx = XMVectorGetX(focusLS);
		float fy = XMVectorGetY(focusLS);
		float snapX = floorf(fx / texelWorld) * texelWorld;
		float snapY = floorf(fy / texelWorld) * texelWorld;
		XMMATRIX snap = XMMatrixTranslation(snapX - fx, snapY - fy, 0.0f);
		LView = snap * LView;

		XMMATRIX LVP = XMMatrixMultiply(LView, LProj);
		m_->m_baseProjection.lightViewProj = XMMatrixTranspose(LVP);
		};

	// 3D 모델 애니메이션 업데이트 (공유 데이터는 중복 업데이트 안함)
	// 동시에 섀도우용 포커스 위치를 누적해서 한 번만 섀도우 행렬 계산
	{
		std::unordered_set<SharedModelData*> updated;
		const int totalModels = (int)m_->m_Models.size();
		// 애니메이션 업데이트 간격. 모델이 많을수록 업데이트 주기를 늘려 CPU 부하를
		// 제한.
		float animStep = 1.0f / 120.0f;
		if (totalModels > 60)
			animStep = 1.0f / 60.0f;
		if (totalModels > 120)
			animStep = 1.0f / 30.0f;

		// 섀도우용 씬 중심 누적
		XMFLOAT3 sceneCenter = { 0.0f, 0.0f, 0.0f };
		int sceneModelCount = 0;

		// ===================== AdvancedRig: player animation and optional socket attachment =====================
		// - This block handles the player model separately from the shared-data update path.
		// - The general loop below skips the player model's default FbxAnimation::UpdateAndUpload.
		if (m_->m_UseAdvancedRig && m_->m_CharRigInited) {
			const int ci = m_->m_CharModelIndex;
			const int wi = m_->m_WeaponModelIndex;
			if (ci < 0 || ci >= (int)m_->m_Models.size()) {
				// invalid indices -> fallback to normal update path
			}
			else {
				auto& player = *m_->m_Models[(size_t)ci];
				ModelEntry* weapon = nullptr;
				if (wi >= 0 && wi < (int)m_->m_Models.size())
					weapon = m_->m_Models[(size_t)wi].get();

				// 입력 수집
				CharacterInputState input{};

				XMVECTOR moveDirWS = XMVectorZero();
				bool wantMove = false;
				bool wantRun = false;

				if (InputSystem::Instance)
				{
					const auto& ks = InputSystem::Instance->m_KeyboardState;
					const auto& kt = InputSystem::Instance->m_KeyboardStateTracker;
					const auto& ms = InputSystem::Instance->m_MouseState;
					const auto& mt = InputSystem::Instance->m_MouseStateTracker;

					const bool lockMove = m_->m_CharCtrl.IsMovementLocked();
					const bool inStance = m_->m_CharCtrl.IsInShootStance();

					if (!ImGui::GetIO().WantCaptureKeyboard)
					{
						input.crouchTogglePressed = kt.IsKeyPressed(Keyboard::LeftControl) || kt.IsKeyPressed(Keyboard::RightControl);

						// TPS 카메라가 붙어 있을 때만 WASD로 캐릭터를 조작 (자유카메라와 충돌 방지)
						if (m_->m_TpsCamAttached && !lockMove)
						{
							// 카메라 yaw 기준 이동(전형적인 TPS)
							const float yaw = m_->m_TpsYawRad;
							XMVECTOR fwd = XMVectorSet(std::sinf(yaw), 0.0f, std::cosf(yaw), 0.0f);
							fwd = XMVector3Normalize(fwd);
							XMVECTOR right = XMVector3Normalize(XMVector3Cross(XMVectorSet(0, 1, 0, 0), fwd));

							if (ks.IsKeyDown(Keyboard::W)) moveDirWS = XMVectorAdd(moveDirWS, fwd);
							if (ks.IsKeyDown(Keyboard::S)) moveDirWS = XMVectorSubtract(moveDirWS, fwd);
							if (ks.IsKeyDown(Keyboard::D)) moveDirWS = XMVectorAdd(moveDirWS, right);
							if (ks.IsKeyDown(Keyboard::A)) moveDirWS = XMVectorSubtract(moveDirWS, right);

							const float lenSq = XMVectorGetX(XMVector3LengthSq(moveDirWS));
							wantMove = (lenSq > 1.0e-6f);
							if (wantMove) moveDirWS = XMVector3Normalize(moveDirWS);

							const bool shift = ks.IsKeyDown(Keyboard::LeftShift) || ks.IsKeyDown(Keyboard::RightShift);
							wantRun = wantMove && shift;

							input.move = wantMove;
							input.run = wantRun;

							// ===== 실제 캐릭터 이동 적용 =====
							float speed = m_->m_CharWalkSpeed * (wantRun ? m_->m_CharRunMul : 1.0f);

							XMVECTOR delta = XMVectorScale(moveDirWS, speed * dt);
							XMFLOAT3 d{}; XMStoreFloat3(&d, delta);

							player.pos.x += d.x;
							player.pos.z += d.z;

							// ===== 이동 방향으로 캐릭터 회전(선택) =====
							if (m_->m_CharRotateToMove)
							{
								auto WrapPi = [](float a)
									{
										while (a > XM_PI)  a -= XM_2PI;
										while (a < -XM_PI) a += XM_2PI;
										return a;
									};

								const float targetYaw = std::atan2(
									XMVectorGetX(moveDirWS),
									XMVectorGetZ(moveDirWS)) + XM_PI;

								float curYaw = XMConvertToRadians(player.rotDeg.y);
								float diff = WrapPi(targetYaw - curYaw);

								float k = 1.0f - std::exp(-m_->m_CharTurnSpeed * dt);
								curYaw = curYaw + diff * k;

								player.rotDeg.y = XMConvertToDegrees(curYaw);
							}
						}
						else
						{
							// TPS OFF 또는 lockMove면 캐릭터 locomotion 입력을 꺼서 "카메라 WASD"와 충돌 방지
							input.move = false;
							input.run = false;
						}

						// R은 Shoot_Stance에서만 의미
						input.reloadPressed = inStance && kt.IsKeyPressed(Keyboard::R);
					}

					if (!ImGui::GetIO().WantCaptureMouse)
					{
						// 스나이퍼 모드: 눌러서 차지, 떼면 발사
						if (m_->m_SniperEnabled && inStance)
						{
							// 마우스 위치(absolute면 마우스 위치, relative면 화면 중앙)
							ImVec2 curPos;
							if (ms.positionMode == Mouse::MODE_RELATIVE)
								curPos = ImVec2(m_ClientWidth * 0.5f, m_ClientHeight * 0.5f);
							else
								curPos = ImVec2((float)ms.x, (float)ms.y);

							if (mt.leftButton == Mouse::ButtonStateTracker::PRESSED)
							{
								m_->m_SniperCharging = true;
								m_->m_SniperCharge01 = 0.0f;
								m_->m_SniperAimPos = curPos;

								// 마우스를 누르기 시작하는 순간: ShootCharged 루프 시작
								if (!Sound::IsSfxPlaying(L"ShootCharged"))
									Sound::PlaySFX(L"ShootCharged", 1.0f, 0.85f, true);
							}

							if (m_->m_SniperCharging)
							{
								// 조준점은 현재 마우스 위치를 따라가게(원하면 press 순간 고정도 가능)
								m_->m_SniperAimPos = curPos;

								if (mt.leftButton == Mouse::ButtonStateTracker::HELD)
								{
									float t = (m_->m_SniperChargeTimeSec > 0.001f) ? (dt / m_->m_SniperChargeTimeSec) : 1.0f;
									m_->m_SniperCharge01 = std::clamp(m_->m_SniperCharge01 + t, 0.0f, 1.0f);
								}

								if (mt.leftButton == Mouse::ButtonStateTracker::RELEASED)
								{
									m_->m_SniperLastShotCharged = (m_->m_SniperCharge01 >= 1.0f - 1e-4f);

									// 떼는 순간: ShootCharged 정지하고 Shoot 재생
									Sound::StopSfx(L"ShootCharged");
									Sound::PlaySFX(L"Shoot", 1.0f, 1.0f, false);

									// 떼는 순간 "발사 트리거"
									input.firePressed = true;

									// 차지 종료
									m_->m_SniperCharging = false;
									m_->m_SniperCharge01 = 0.0f;
								}
							}
						}
					}
				}

				// ===== 이동 SFX =====
				if (m_->m_TpsCamAttached) // TPS 조작 중일 때만
				{
					if (input.move)
					{
						// 발자국 루프 재생(이미 재생 중이면 유지)
						float pitch = input.run ? m_->m_CharRunMul : 1.0f;
						// loop=true: 이미 재생 중이면 재시작하지 않음 루프는 유지함
						Sound::PlaySFX(L"Walk", 0.9f, pitch, true);

						// 달리기 시작 순간 목소리 1회
						if (input.run && !m_->m_LastRun)
						{
							Sound::PlaySFX(L"RunVoice", 1.0f, 1.0f, false);
						}
					}
					else
					{
						// 멈추면 발자국 정지
						Sound::StopSfx(L"Walk");
					}

					m_->m_LastMove = input.move;
					m_->m_LastRun = input.run;
				}

				// ===== 리로드/사격 SFX =====
				if (input.reloadPressed)
				{
					Sound::PlaySFX(L"Reload", 1.0f, 1.0f, false);
				}

				// firePressed는 마우스를 떼는 순간에만 true가 되고,
				// 그때 이미 Shoot 사운드가 재생되므로 여기서는 처리하지 않음

				// ===  TPS 카메라 붙어있으면, 이번 프레임 카메라 pose + aim yaw 계산 ===
				AimInputState aim{};
				if (m_->m_TpsCamAttached)
				{
					const XMMATRIX charWorld = player.GetWorldMatrix();
					const XMVECTOR charPosWS = XMVector3TransformCoord(XMVectorZero(), charWorld);
					const XMVECTOR upWS = XMVectorSet(0, 1, 0, 0);

					// 카메라가 바라볼 피벗(서있을 때/앉을 때 높이 다르게)
					const bool crouchLike = m_->m_CharCtrl.IsInShootStance(); // 이전 프레임 기준이라도 충분히 자연스럽습니다.
					const float pivotH = crouchLike ? m_->m_TpsTargetHeightCrouch : m_->m_TpsTargetHeightStand;
					const XMVECTOR pivotWS = XMVectorAdd(charPosWS, XMVectorScale(upWS, pivotH));

					// 카메라 방향(저장된 yaw/pitch로)
					const XMMATRIX camR = XMMatrixRotationRollPitchYaw(m_->m_TpsPitchRad, m_->m_TpsYawRad, 0.0f);
					const XMVECTOR camForwardWS = XMVector3Normalize(camR.r[2]);

					// 카메라 위치: 피벗에서 뒤로 dist만큼
					const XMVECTOR camPosWS = XMVectorSubtract(pivotWS, XMVectorScale(camForwardWS, m_->m_TpsDist));

					// 카메라 실제 적용
					XMFLOAT3 cp{}; XMStoreFloat3(&cp, camPosWS);
					m_Camera.SetPosition(cp);

					XMFLOAT3 rotDeg{
						XMConvertToDegrees(m_->m_TpsPitchRad),
						XMConvertToDegrees(m_->m_TpsYawRad),
						0.0f
					};
					m_Camera.SetRotation(rotDeg);
					m_Camera.Update(0.0f); // world 재계산

					// ---- AimYaw 계산(±70도 제한) ----
					auto WrapPi = [](float a)
						{
							while (a > XM_PI)  a -= XM_2PI;
							while (a < -XM_PI) a += XM_2PI;
							return a;
						};

					// 캐릭터 yaw(몸통 방향)
					float charYaw = XMConvertToRadians(player.rotDeg.y);

					// 카메라 yaw(조준 방향)
					float camYaw = m_->m_TpsYawRad;

					// 차이 = 상체가 돌아야 할 yaw
					float yaw = WrapPi(camYaw - charYaw);

					const float maxYawRad = XMConvertToRadians(m_->m_AimMaxYawDeg);
					yaw = std::clamp(yaw, -maxYawRad, +maxYawRad);

					// 스무딩(튀는 것 방지)
					float a = 1.0f - std::exp(-m_->m_AimSmoothing * dt);
					m_->m_AimYawSmoothed = m_->m_AimYawSmoothed + (yaw - m_->m_AimYawSmoothed) * a;

					aim.enabled = true;
					aim.yawRad = m_->m_AimYawSmoothed;
					aim.weight = 1.0f;
				}
				else
				{
					// TPS 끄면 aim도 0으로 복귀
					float a = 1.0f - std::exp(-m_->m_AimSmoothing * dt);
					m_->m_AimYawSmoothed = m_->m_AimYawSmoothed + (0.0f - m_->m_AimYawSmoothed) * a;
				}

				// UI로 조절하는 리코일 파라미터를 컨트롤러에 반영
				m_->m_CharCtrl.config.recoil.kick = m_->m_RecoilKickUi;
				m_->m_CharCtrl.config.recoil.decay = m_->m_RecoilDecayUi;

				// update + palette upload + optional socket attachment
				m_->m_CharCtrl.TickAndApply(dt, input, player, weapon, m_->m_pDevice, m_->m_pDeviceContext,
					m_->m_TpsCamAttached ? &aim : nullptr);
			}
		}

		size_t i = 0;
		for (auto& mdlPtr : m_->m_Models) {
			auto& mdl = *mdlPtr;
			if (mdl.autoRotate && m_bIsGameStarted) {
				mdl.rotDeg.y += 45.0f * dt;
				mdl.rotDeg.y = std::fmod(mdl.rotDeg.y + 180.0f, 360.0f) - 180.0f;
			}
			if (!mdl.shared)
				continue;
			if (updated.find(mdl.shared.get()) != updated.end())
				continue;

			// 섀도우용 씬 중심 누적
			sceneCenter.x += mdl.pos.x;
			sceneCenter.y += mdl.pos.y;
			sceneCenter.z += mdl.pos.z;
			sceneModelCount++;

			// AdvancedRig 대상 캐릭터는 여기서 기본 업데이트를 하지 않는다.
			const int charIndex = m_->m_CharModelIndex;
			const bool isRigCharacter =
				charIndex >= 0 &&
				charIndex < (int)m_->m_Models.size() &&
				(size_t)charIndex == i &&
				mdlPtr.get() == m_->m_Models[(size_t)charIndex].get();
			if (isRigCharacter && m_->m_UseAdvancedRig && m_->m_CharRigInited) {
				// shared 중복 업데이트 방지 마킹은 유지
				//updated.insert(mdl.shared.get());
				++i;
				continue;
			}

			if (mdl.source == ModelSource::FBX && mdl.shared->fbx) {
				// 인스턴스별 애니메이션 업데이트 (공유 지오메트리/스켈레톤 사용)
				if (!mdl.animatorInited) {
					mdl.fbxBaseAnimator.InitMetadata(mdl.shared->fbx->GetScenePtr());
					mdl.fbxBaseAnimator.SetSharedContext(mdl.shared->fbx->GetScenePtr(),
						mdl.shared->fbx->GetNodeIndexOfName(),
						&mdl.shared->fbx->GetBoneNames(),
						&mdl.shared->fbx->GetBoneOffsets(),
						&mdl.shared->fbx->GetGlobalInverse());
					auto t = mdl.shared->fbx->GetCurrentAnimationType();
					mdl.fbxBaseAnimator.SetType(t == FbxModel::AnimationType::Rigid
						? FbxAnimation::AnimType::Rigid
						: (t == FbxModel::AnimationType::Skinned
							? FbxAnimation::AnimType::Skinned
							: FbxAnimation::AnimType::None));
					mdl.animatorInited = true;
				}
				// 애니메이션 LOD: 누적 시간 기반으로 일정 주기마다만 팔레트 평가/업로드
				mdl.animUpdateAccum += dt;
				if (mdl.animUpdateAccum >= animStep) {
					const double dtAnim = (double)mdl.animUpdateAccum;
					mdl.animUpdateAccum = 0.0f;

					mdl.fbxBaseAnimator.SetPlaying(mdl.uiAnimPlaying);
					mdl.fbxBaseAnimator.EnsureBoneCB(m_->m_pDevice, 1023);
					mdl.fbxBaseAnimator.UpdateAndUpload(m_->m_pDeviceContext, dtAnim,
						mdl.shared->fbx->GetScenePtr(),
						mdl.shared->fbx->GetNodeIndexOfName(),
						mdl.shared->fbx->GetBoneNames(),
						mdl.shared->fbx->GetBoneOffsets(),
						mdl.shared->fbx->GetGlobalInverse());
				}
			}
			else if (mdl.source == ModelSource::PMX && mdl.shared->pmx) {
				// PMX도 적용 팔레트이므로 1회만 업데이트
				auto it = updated.find(mdl.shared.get());
				if (it == updated.end()) {
					mdl.animUpdateAccum += dt;
					if (mdl.animUpdateAccum >= animStep) {
						const double dtAnim = (double)mdl.animUpdateAccum;
						mdl.animUpdateAccum = 0.0f;
						mdl.shared->pmx->UpdateAnimation(m_->m_pDeviceContext, dtAnim);
						updated.insert(mdl.shared.get());
					}
				}
			}
			++i;

		}

		UpdateEnemyIdleAnimations(dt);
		StartPublicDemoAudioOnce();

		// FMOD 갱신 (매 프레임) - 모델 루프 밖에서 1회만 호출해야 한다.
		Sound::Update();

		// 섀도우 행렬은 씬 전체에 대해 한 번만 계산 (성능 최적화)
		if (sceneModelCount > 0) {
			sceneCenter.x /= (float)sceneModelCount;
			sceneCenter.y /= (float)sceneModelCount;
			sceneCenter.z /= (float)sceneModelCount;
			UpdateShadow(sceneCenter);
		}
		else {
			// 모델이 없을 경우 기본 위치 사용
			XMFLOAT3 defaultPos = { 0.0f, 0.0f, 0.0f };
			UpdateShadow(defaultPos);
		}
	}

	// Camera의 View/Proj
	XMMATRIX model = XMMatrixIdentity();
	m_->m_baseProjection.world = XMMatrixTranspose(model);

	// VMD 카메라 업데이트 및 적용
	mmd::UpdateVmdCamera(dt, m_->m_VmdCamera);
	XMMATRIX view{}, proj{};
	XMFLOAT3 eye{};
	bool useVmdCam = mmd::EvaluateVmdCamera(
		m_->m_VmdCamera, m_Camera.GetFovYRad(), AspectRatio(),
		m_Camera.GetNearZ(), m_Camera.GetFarZ(), view, proj, eye);

	if (m_->m_TpsCamAttached)
		useVmdCam = false;

	if (useVmdCam) {
		m_->m_baseProjection.view = XMMatrixTranspose(view);
		m_->m_baseProjection.proj = XMMatrixTranspose(proj);
		m_->m_baseProjection.eyePos = eye;
	}
	else {
		m_->m_baseProjection.view = XMMatrixTranspose(m_Camera.GetViewMatrixXM());
		m_->m_baseProjection.proj = XMMatrixTranspose(m_Camera.GetProjMatrixXM());
		m_->m_baseProjection.eyePos = m_Camera.GetPosition();
	}
	m_->m_baseProjection.worldInvTranspose =
		XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranspose(model)));

	XMFLOAT3 lightDir = m_->m_DirLight.direction;
	XMVECTOR v = XMVector3Normalize(XMLoadFloat3(&lightDir));
	XMStoreFloat3(&lightDir, v);

	// DirectionalLight 정규화된 방향으로 대입
	m_->m_baseProjection.dirLight = m_->m_DirLight;
	m_->m_baseProjection.dirLight.direction = lightDir;
	m_->m_baseProjection.pad = 0.0f;

	// 머티리얼을 기본 캐시에 반영해 둔다
	m_->m_baseProjection.material = m_->m_Material;

	m_->m_SystemInfo.Tick(dt);

	// 씬 변경 팝업 타이머 업데이트
	// 씬 이미지 팝업 및 임시 이미지 타이머 업데이트
	if (m_->m_ShowScenePopup) {
		m_->m_ScenePopupTimer -= dt;
		if (m_->m_ScenePopupTimer <= 0.0f) {
			m_->m_ShowScenePopup = false;
			m_->m_ScenePopupTimer = 0.0f;

			// 임시 이미지 사용 중이었다면 원본 이미지로 복원
			if (m_->m_IsUsingTempImage) {
				m_->m_IsUsingTempImage = false;
				LoadSceneImage(m_->m_OriginalSceneImagePath);
				m_->m_CurrentSceneImagePath = m_->m_OriginalSceneImagePath;
			}
		}
	}

	// ====================================== 간단 마우스 피킹 (FBX/모델 위주) ======================================
	if (m_->m_UseAdvancedRig && m_->m_CharRigInited && m_->m_CharCtrl.IsMovementLocked())
	{
		// 앉아있는(혹은 앉기/서기/사격/리로드 중) LMB는 게임 입력으로 사용 => picking 스킵
	}
	else if (InputSystem::Instance && !ImGui::GetIO().WantCaptureMouse) {
		auto& input = *InputSystem::Instance;
		if (input.m_MouseStateTracker.leftButton ==
			Mouse::ButtonStateTracker::PRESSED) {
			PickingRay ray = PickingRay::ScreenPointToRay(
				m_Camera, (float)input.m_MouseState.x, (float)input.m_MouseState.y,
				(float)m_ClientWidth, (float)m_ClientHeight);

			int pickedModel = -1;
			float bestT = FLT_MAX;

			for (auto it = m_->m_Models.begin(); it != m_->m_Models.end(); ++it) {
				auto& mdl = **it;

				XMFLOAT3 mn = mdl.boundsMin, mx = mdl.boundsMax;
				XMFLOAT3 bmin{}, bmax{};

				// 로컬 AABB(min,max)에 스케일과 위치를 그대로 적용해서 월드 AABB를 만든다 (회전은 무시)
				float x0 = mn.x * mdl.scale.x + mdl.pos.x;
				float x1 = mx.x * mdl.scale.x + mdl.pos.x;
				bmin.x = std::min(x0, x1);
				bmax.x = (std::max)(x0, x1);

				float y0 = mn.y * mdl.scale.y + mdl.pos.y;
				float y1 = mx.y * mdl.scale.y + mdl.pos.y;
				bmin.y = std::min(y0, y1);
				bmax.y = (std::max)(y0, y1);

				float z0 = mn.z * mdl.scale.z + mdl.pos.z;
				float z1 = mx.z * mdl.scale.z + mdl.pos.z;
				bmin.z = std::min(z0, z1);
				bmax.z = (std::max)(z0, z1);

				float t;
				if (ray.HitAABB(bmin, bmax, t) && t < bestT) {
					bestT = t;
					pickedModel = (int)(it - m_->m_Models.begin());
				}
			}

			if (pickedModel >= 0) {
				m_->m_SelectedModelIdx = pickedModel;
				auto itObj = std::find_if(m_->m_Objects.begin(), m_->m_Objects.end(),
					[pickedModel](const std::unique_ptr<BaseObject>& up) {
						if (!up || up->kind != ObjectKind::Model) return false;
						return static_cast<ModelObject*>(up.get())->modelIndex == pickedModel;
					});
				if (itObj != m_->m_Objects.end())
					m_->m_SelectedItem = (int)std::distance(m_->m_Objects.begin(), itObj);
				else
					m_->m_SelectedItem = -1;
			}
		}
	}
}

inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) {
	return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
}
inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs) {
	return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y);
}

// 상수 버퍼 업데이트
template <typename T> void App::UpdateCB(ID3D11Buffer* buffer, const T& data) {
	D3D11_MAPPED_SUBRESOURCE mapped;
	HR_T(m_->m_pDeviceContext->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0,
		&mapped));
	memcpy(mapped.pData, &data, sizeof(T));
	m_->m_pDeviceContext->Unmap(buffer, 0);
}
