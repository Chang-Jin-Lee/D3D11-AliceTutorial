# Enemy Idle and Public Sound Pack Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Project 36 shows all public replacement characters alive in the scene by applying the existing external Idle clip to the three enemy GLB models, and the tutorial uses a newly generated public-safe sound pack for shared gameplay cues.

**Architecture:** Keep runtime behavior local to `Dx11/36_AdvancedAnim_Sound_Click` and shared assets local to `Dx11/Resource/Sound/Public`. Reuse the existing `ExternalAnimationClipLibrary`, `CharacterAnimator`, `FbxAnimation`, and `Sound` systems instead of adding a new animation or audio stack.

**Tech Stack:** C++17, Direct3D 11, Assimp animation data, existing project `.inl` split, Python verification script, Dockerized `C:\Github\SunoAI_Downloader`, ffmpeg.

## Global Constraints

- Preserve every unrelated worktree change. Do not reset, checkout, delete, or rewrite files outside this task.
- Keep production code changes for runtime behavior in project 36 unless a shared system defect blocks the task.
- Keep generated/public sounds under `Dx11/Resource/Sound/Public`.
- Back up the current public sound files before replacing any file.
- Do not reference Nikke, Alice, Shift Up, copyrighted songs, or third-party character names in prompts, filenames, code comments, or UI text.
- Generated sounds must be instrumental or non-vocal sound effects.
- Missing optional BGM or aura files must not crash or block project 36 startup.
- Enemy idle must be applied after the existing generic FBX update pass so it overwrites bind-pose fallback palettes.
- Treat `anim_Idle.fbx` as the source clip for enemy idle because the enemy GLBs have no embedded animation tracks.
- Keep ImGui help text short and human-readable; no new long in-app tutorial copy is part of this task.

---

## Task 1: Add a Failing Verification Script for the Public Sound and Runtime Wiring

**Files:**

- Create `tools/verify_public_sound_pack.py`

**Steps:**

- [ ] Add a repo-local Python script that verifies required sound files and source wiring tokens.

```python
from pathlib import Path
import sys
import wave

ROOT = Path(__file__).resolve().parents[1]
SOUND_DIR = ROOT / "Dx11" / "Resource" / "Sound" / "Public"
APP_DIR = ROOT / "Dx11" / "36_AdvancedAnim_Sound_Click"

WAV_LIMITS = {
    "ui_advance.wav": (0.05, 1.00),
    "ui_done.wav": (0.10, 1.50),
    "step.wav": (0.05, 0.75),
    "run.wav": (0.05, 1.00),
    "action.wav": (0.10, 2.00),
    "reload.wav": (0.10, 2.00),
    "enemy_idle_aura.wav": (1.00, 12.00),
}

SOURCE_TOKENS = {
    "App_InternalTypes.inl": ["EnemyIdleRuntime", "m_EnemyIdleRuntimes", "m_PublicDemoAudioStarted"],
    "App.h": ["InitializeEnemyIdleRuntime", "UpdateEnemyIdleAnimations", "StartPublicDemoAudioOnce"],
    "App_Lifecycle.inl": ["PublicDemoBgm", "EnemyIdleAura", "bgm_public_demo.mp3", "enemy_idle_aura.wav"],
    "App_UpdateInput.inl": ["UpdateEnemyIdleAnimations", "StartPublicDemoAudioOnce"],
}


def fail(message: str) -> None:
    print(f"[FAIL] {message}")
    raise SystemExit(1)


def require_file(path: Path, min_bytes: int = 1024) -> None:
    if not path.exists():
        fail(f"missing file: {path}")
    if path.stat().st_size < min_bytes:
        fail(f"file too small: {path} ({path.stat().st_size} bytes)")


def wav_duration(path: Path) -> float:
    with wave.open(str(path), "rb") as wav:
        return wav.getnframes() / float(wav.getframerate())


def main() -> int:
    for name, (min_sec, max_sec) in WAV_LIMITS.items():
        path = SOUND_DIR / name
        require_file(path)
        duration = wav_duration(path)
        if duration < min_sec or duration > max_sec:
            fail(f"{name} duration {duration:.2f}s outside {min_sec:.2f}-{max_sec:.2f}s")

    bgm = SOUND_DIR / "bgm_public_demo.mp3"
    require_file(bgm, min_bytes=4096)

    for source_name, tokens in SOURCE_TOKENS.items():
        source_path = APP_DIR / source_name
        require_file(source_path, min_bytes=256)
        text = source_path.read_text(encoding="utf-8", errors="ignore")
        for token in tokens:
            if token not in text:
                fail(f"missing token {token!r} in {source_path}")

    print("[OK] public sound pack verification passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] Run the script before implementation and capture the expected failure.

```powershell
python tools\verify_public_sound_pack.py
```

Expected result before implementation:

```text
[FAIL] missing file: ...\Dx11\Resource\Sound\Public\enemy_idle_aura.wav
```

- [ ] Commit only this script if a checkpoint commit is appropriate for the current branch state.

```powershell
git add tools\verify_public_sound_pack.py
git commit -m "test: verify public sound pack wiring"
```

---

## Task 2: Generate and Process the Public Sound Pack

**Files and folders:**

- Read and back up `C:\Github\SunoAI_Downloader\Source\input\prompts.json`
- Read output from `C:\Github\SunoAI_Downloader\Source\output`
- Update files in `Dx11\Resource\Sound\Public`

**Steps:**

- [ ] Back up the existing public sounds to the user's desktop asset archive before replacing them.

```powershell
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$backupRoot = Join-Path $env:USERPROFILE "Desktop\애셋"
$backupDir = Join-Path $backupRoot "D3D11-AliceTutorial_PublicSound_Backup_$stamp"
New-Item -ItemType Directory -Force -Path $backupDir | Out-Null
Copy-Item -Path "Dx11\Resource\Sound\Public\*" -Destination $backupDir -Force
Write-Host $backupDir
```

- [ ] Back up the Suno downloader prompt file.

```powershell
Copy-Item `
  -Path "C:\Github\SunoAI_Downloader\Source\input\prompts.json" `
  -Destination "C:\Github\SunoAI_Downloader\Source\input\prompts.alice-public-sound-pack.backup.json" `
  -Force
```

- [ ] Replace `C:\Github\SunoAI_Downloader\Source\input\prompts.json` with a five-entry instrumental prompt batch.

```json
[
  {
    "id": "d3d11_public_demo_bgm_20260708",
    "prompt": "Instrumental seamless game demo background loop, warm hopeful adventure mood, light percussion, soft synth pads, clean mix, no vocals, no copyrighted melodies, 90 seconds",
    "filename": "d3d11_public_demo_bgm.mp3",
    "tags": "instrumental, game background, loop, hopeful, no vocals"
  },
  {
    "id": "d3d11_public_ui_cues_20260708",
    "prompt": "A sequence of clean UI sound effects separated by silence: soft confirm click, bright completion chime, minimal digital interface cues, no vocals, no melody quote, no copyrighted material",
    "filename": "d3d11_public_ui_cues.mp3",
    "tags": "sound effects, ui, minimal, no vocals"
  },
  {
    "id": "d3d11_public_movement_cues_20260708",
    "prompt": "A sequence of short stylized movement sound effects separated by silence: light cloth step, quick run footstep, subtle fantasy character movement, dry mix, no vocals, no copyrighted material",
    "filename": "d3d11_public_movement_cues.mp3",
    "tags": "sound effects, footsteps, movement, no vocals"
  },
  {
    "id": "d3d11_public_action_cues_20260708",
    "prompt": "A sequence of compact fantasy action sound effects separated by silence: magic shot burst, charged action hit, reload or ready-up mechanical cloth cue, no vocals, no copyrighted material",
    "filename": "d3d11_public_action_cues.mp3",
    "tags": "sound effects, action, fantasy, no vocals"
  },
  {
    "id": "d3d11_public_enemy_aura_20260708",
    "prompt": "Instrumental seamless ambient enemy aura loop, low magical shimmer, quiet tension, soft pulsing texture, no vocals, no copyrighted melodies, 20 seconds",
    "filename": "d3d11_public_enemy_aura.mp3",
    "tags": "ambient, loop, enemy aura, no vocals"
  }
]
```

- [ ] Run the existing Dockerized downloader batch.

```powershell
docker exec suno-automation-bot python suno_bot.py batch
```

- [ ] Confirm the expected source MP3 files exist.

```powershell
Get-ChildItem "C:\Github\SunoAI_Downloader\Source\output" -Filter "*.mp3" |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 10 FullName,Length,LastWriteTime
```

- [ ] Process generated MP3s into the exact repo filenames. Use ffmpeg from `C:\ffmpeg\bin\ffmpeg.exe`.

```powershell
$ffmpeg = "C:\ffmpeg\bin\ffmpeg.exe"
$out = "C:\Github\SunoAI_Downloader\Source\output"
$dst = "Dx11\Resource\Sound\Public"

Copy-Item "$out\d3d11_public_demo_bgm.mp3" "$dst\bgm_public_demo.mp3" -Force

& $ffmpeg -y -i "$out\d3d11_public_ui_cues.mp3"       -ss 0.00 -t 0.35 -af "afade=t=out:st=0.30:d=0.05,loudnorm=I=-18:TP=-2:LRA=7" -ar 44100 -ac 1 "$dst\ui_advance.wav"
& $ffmpeg -y -i "$out\d3d11_public_ui_cues.mp3"       -ss 2.00 -t 0.80 -af "afade=t=out:st=0.72:d=0.08,loudnorm=I=-18:TP=-2:LRA=7" -ar 44100 -ac 1 "$dst\ui_done.wav"
& $ffmpeg -y -i "$out\d3d11_public_movement_cues.mp3" -ss 0.00 -t 0.25 -af "afade=t=out:st=0.20:d=0.05,loudnorm=I=-20:TP=-2:LRA=7" -ar 44100 -ac 1 "$dst\step.wav"
& $ffmpeg -y -i "$out\d3d11_public_movement_cues.mp3" -ss 2.00 -t 0.45 -af "afade=t=out:st=0.38:d=0.07,loudnorm=I=-20:TP=-2:LRA=7" -ar 44100 -ac 1 "$dst\run.wav"
& $ffmpeg -y -i "$out\d3d11_public_action_cues.mp3"   -ss 0.00 -t 0.90 -af "afade=t=out:st=0.80:d=0.10,loudnorm=I=-17:TP=-2:LRA=7" -ar 44100 -ac 1 "$dst\action.wav"
& $ffmpeg -y -i "$out\d3d11_public_action_cues.mp3"   -ss 2.00 -t 0.90 -af "afade=t=out:st=0.80:d=0.10,loudnorm=I=-18:TP=-2:LRA=7" -ar 44100 -ac 1 "$dst\reload.wav"
& $ffmpeg -y -i "$out\d3d11_public_enemy_aura.mp3"    -ss 0.00 -t 6.00 -af "afade=t=in:st=0:d=0.25,afade=t=out:st=5.75:d=0.25,loudnorm=I=-24:TP=-3:LRA=8" -ar 44100 -ac 1 "$dst\enemy_idle_aura.wav"
```

- [ ] Run verification and expect it to still fail on source tokens until the C++ wiring is added.

```powershell
python tools\verify_public_sound_pack.py
```

Expected result after audio assets but before C++:

```text
[FAIL] missing token 'EnemyIdleRuntime' in ...\App_InternalTypes.inl
```

- [ ] Commit the sound assets after verifying file presence and durations.

```powershell
git add Dx11\Resource\Sound\Public tools\verify_public_sound_pack.py
git commit -m "assets: add public demo sound pack"
```

---

## Task 3: Add Project 36 Enemy Idle Runtime State

**Files:**

- Update `Dx11/36_AdvancedAnim_Sound_Click/App_InternalTypes.inl`
- Update `Dx11/36_AdvancedAnim_Sound_Click/App.h`
- Update `Dx11/36_AdvancedAnim_Sound_Click/App.cpp`
- Create `Dx11/36_AdvancedAnim_Sound_Click/App_PublicDemoRuntime.inl`

**Steps:**

- [ ] Add enemy idle and audio state to `App::Impl` in `App_InternalTypes.inl`.

```cpp
struct EnemyIdleRuntime
{
    int modelIndex = -1;
    CharacterAnimator animator;
    float idleTimeSec = 0.0f;
    bool initialized = false;
};

std::array<int, 3> m_EnemyModelIndices{ -1, -1, -1 };
std::vector<EnemyIdleRuntime> m_EnemyIdleRuntimes;
bool m_PublicDemoAudioStarted = false;
bool m_PublicDemoBgmLoaded = false;
bool m_EnemyIdleAuraLoaded = false;
```

- [ ] Add private helper declarations to `App.h`.

```cpp
void InitializeEnemyIdleRuntime(int modelIndex);
void UpdateEnemyIdleAnimations(float dt);
void StartPublicDemoAudioOnce();
```

- [ ] Include the new implementation file from `App.cpp` after internal types are available.

```cpp
#include "App_PublicDemoRuntime.inl"
```

- [ ] Create `App_PublicDemoRuntime.inl` with helper implementations.

```cpp
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

    auto& entry = *m_->m_Models[modelIndex];
    auto& runtime = m_->m_EnemyIdleRuntimes.emplace_back();
    runtime.modelIndex = modelIndex;
    runtime.animator.Initialize(m_->m_device.Get(), entry.shared->fbx.scene.get());
    runtime.initialized = true;

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

        auto& entry = *m_->m_Models[runtime.modelIndex];
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
        entry.fbxBaseAnimator.EnsureBoneCB(m_->m_device.Get(), 1024);
        entry.fbxBaseAnimator.UploadPalette(m_->m_context.Get(), runtime.animator.finalTransforms);
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

    if (m_->m_EnemyIdleAuraLoaded)
    {
        Sound::PlaySFX(L"EnemyIdleAura", 0.14f, 1.0f, true);
    }
}
```

- [ ] Build project 36 to catch compile issues from struct placement or includes.

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" `
  "Dx11\36_AdvancedAnim_Sound_Click\36_AdvancedAnim_Sound_Click.vcxproj" `
  /p:Configuration=Debug /p:Platform=x64 /m
```

Expected result:

```text
Build succeeded.
```

---

## Task 4: Wire Enemy Idle, Optional BGM, and Optional Aura in Project 36

**Files:**

- Update `Dx11/36_AdvancedAnim_Sound_Click/App_Lifecycle.inl`
- Update `Dx11/36_AdvancedAnim_Sound_Click/App_UpdateInput.inl`

**Steps:**

- [ ] Load optional public BGM and enemy aura after the existing project 36 sound initialization succeeds.

```cpp
m_->m_PublicDemoBgmLoaded = Sound::Load(
    L"PublicDemoBgm",
    L"..\\Resource\\Sound\\Public\\bgm_public_demo.mp3",
    Sound::Type::BGM);
m_->m_EnemyIdleAuraLoaded = Sound::Load(
    L"EnemyIdleAura",
    L"..\\Resource\\Sound\\Public\\enemy_idle_aura.wav",
    Sound::Type::SFX);
```

- [ ] Store enemy model indices and initialize enemy idle runtimes after the three enemy GLBs and external clips have loaded.

```cpp
m_->m_EnemyModelIndices = { enemy1Index, enemy2Index, enemy3Index };
m_->m_EnemyIdleRuntimes.clear();
for (int enemyIndex : m_->m_EnemyModelIndices)
{
    InitializeEnemyIdleRuntime(enemyIndex);
}
```

- [ ] Call enemy idle and audio start from `OnUpdate` after the generic model update loop.

```cpp
UpdateEnemyIdleAnimations(dt);
StartPublicDemoAudioOnce();
```

- [ ] Run the public sound verification script and expect it to pass.

```powershell
python tools\verify_public_sound_pack.py
```

Expected result:

```text
[OK] public sound pack verification passed
```

- [ ] Build project 36 again.

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" `
  "Dx11\36_AdvancedAnim_Sound_Click\36_AdvancedAnim_Sound_Click.vcxproj" `
  /p:Configuration=Debug /p:Platform=x64 /m
```

Expected result:

```text
Build succeeded.
```

- [ ] Run project 36 and verify all three enemies remain visible and use the idle palette instead of the static bind pose.

```powershell
& "Dx11\x64\Debug\36_AdvancedAnim_Sound_Click.exe"
```

Manual verification:

```text
The player animates normally.
Enemy1, Enemy2, and Enemy3 remain visible.
Enemies have idle motion rather than frozen bind pose.
No crash occurs if BGM or aura is unavailable.
```

- [ ] Commit project 36 runtime wiring and sound verification script if the build and verification pass.

```powershell
git add `
  Dx11\36_AdvancedAnim_Sound_Click\App.h `
  Dx11\36_AdvancedAnim_Sound_Click\App.cpp `
  Dx11\36_AdvancedAnim_Sound_Click\App_InternalTypes.inl `
  Dx11\36_AdvancedAnim_Sound_Click\App_Lifecycle.inl `
  Dx11\36_AdvancedAnim_Sound_Click\App_PublicDemoRuntime.inl `
  Dx11\36_AdvancedAnim_Sound_Click\App_UpdateInput.inl `
  tools\verify_public_sound_pack.py
git commit -m "feat: animate public enemies in project 36"
```

---

## Task 5: Full Regression Verification

**Files:**

- No new source files expected

**Steps:**

- [ ] Run the verification script.

```powershell
python tools\verify_public_sound_pack.py
```

Expected result:

```text
[OK] public sound pack verification passed
```

- [ ] Build the full solution in Debug x64.

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" `
  "D3D11_AliceTutorial.sln" `
  /p:Configuration=Debug /p:Platform=x64 /m
```

Expected result:

```text
Build succeeded.
```

- [ ] Launch project 36 for a final smoke test.

```powershell
& "Dx11\x64\Debug\36_AdvancedAnim_Sound_Click.exe"
```

Final manual checks:

```text
Camera near remains 0.01.
Camera speed default remains 15.
Quick guide window remains visible and short.
Player animation is not broken.
All three enemies are loaded.
Enemies use idle animation.
Public demo BGM starts once after the scene starts.
Enemy aura sound is subtle and does not dominate.
Existing click, walk, run, action, reload cues still play.
```

- [ ] Inspect `git diff --stat` and confirm only intended files are part of this task.

```powershell
git diff --stat
git status --short
```

- [ ] Create a final checkpoint commit if there are uncommitted intended changes.

```powershell
git add Dx11\Resource\Sound\Public Dx11\36_AdvancedAnim_Sound_Click tools\verify_public_sound_pack.py
git commit -m "feat: add public enemy idle audio pass"
```
