# Enemy Idle Animation and Public Sound Pack Design

## Goal

Apply idle animation to the three enemy characters in project 36 and create a reusable public sound pack with Suno-generated source audio, while keeping the implementation focused on project 36 and leaving the other projects able to reuse the files from `Dx11/Resource/Sound/Public`.

## Scope

- Project 36 (`Dx11/36_AdvancedAnim_Sound_Click`) will load and play the new audio in code.
- The shared public sound directory (`Dx11/Resource/Sound/Public`) will receive the generated and processed sound files.
- Other projects will not have their code paths changed in this task.
- Existing sound files must remain usable until replacement files are generated and verified.
- Suno generation will use `C:\Github\SunoAI_Downloader`, its existing `Source/input/cookies.json`, and the already running `suno-automation-bot` container.

## Enemy Idle Animation

The enemy GLB files do not contain embedded animation clips. They will use the existing external `Idle` clip loaded from `Dx11/Resource/fbx/Public/MyAlice/Animations/anim_Idle.fbx`.

Implementation will add a small 36-only runtime state for enemy idle animation:

- Store enemy model indices after loading `AliceEnemy1.glb`, `AliceEnemy2.glb`, and `AliceEnemy3.glb`.
- For each enemy, initialize a `CharacterAnimator` with that enemy model's skeleton context.
- Advance a per-enemy `idleTimeSec` each frame.
- Evaluate the external `Idle` clip through `CharacterAnimator::Update`.
- Upload the resulting palette into the enemy's existing `fbxBaseAnimator` with `UploadPalette`.

The normal player `CharacterAnimController` remains unchanged. Enemy idle updates will run after the existing generic FBX model update inside `OnUpdate`, so the idle palette is the final palette uploaded for each enemy that frame.

## Sound Pack

Generate a compact, public reusable sound pack with Suno. Prompts must be instrumental, non-vocal, and not reference any copyrighted character, franchise, or existing song.

Target files:

- `bgm_public_demo.mp3`: soft anime action demo loop, suitable for the public asset scene.
- `ui_advance.wav`: short clean UI navigation tick.
- `ui_done.wav`: short positive completion chime.
- `step.wav`: soft cloth/boot footstep.
- `run.wav`: short movement effort or fast step accent, without voice.
- `action.wav`: compact stylized energy/action hit.
- `reload.wav`: short mechanical reload/prepare sound.
- `enemy_idle_aura.wav`: subtle looping aura usable for enemies or ambience.

The existing keys loaded in project 36 will continue to work:

- `UiAdvance` -> `ui_advance.wav`
- `UiDone` -> `ui_done.wav`
- `Walk` -> `step.wav`
- `RunVoice` -> `run.wav`
- `Shoot` -> `action.wav`
- `ShootCharged` -> `action.wav`
- `Reload` -> `reload.wav`

New keys in project 36:

- `PublicDemoBgm` -> `bgm_public_demo.mp3`
- `EnemyIdleAura` -> `enemy_idle_aura.wav`

## Audio Generation and Processing

`C:\Github\SunoAI_Downloader\Source\input\prompts.json` will be rewritten with the sound-pack prompts. Before rewriting it, the existing prompt file will be backed up inside the Suno downloader workspace.

Run generation with:

```powershell
docker exec suno-automation-bot python suno_bot.py batch
```

Generated MP3 files will be copied from `C:\Github\SunoAI_Downloader\Source\output` into a working folder, then processed with `C:\ffmpeg\bin\ffmpeg.exe`.

Processing rules:

- Keep BGM as MP3 for FMOD streaming.
- Convert SFX to WAV, stereo or mono PCM, 44.1 kHz.
- Trim SFX to short one-shot lengths.
- Normalize SFX volume conservatively to avoid clipping.
- If a generated file is missing or unusable, keep the existing public sound file in place and log that the replacement was skipped.

## Runtime Audio Behavior

During project 36 loading:

- Load the existing SFX keys from `Dx11/Resource/Sound/Public`.
- Load `PublicDemoBgm` if `bgm_public_demo.mp3` exists.
- Load `EnemyIdleAura` if `enemy_idle_aura.wav` exists.

When the public demo starts:

- Play `PublicDemoBgm` at low volume if it loaded successfully.
- Play `EnemyIdleAura` as a very quiet loop if it loaded successfully.
- Do not interrupt manually loaded BGM from the existing audio UI after the user chooses one.

The demo will keep one-shot state flags so the BGM and aura loop start once when `m_bIsGameStarted` first becomes true.

## Error Handling

- Missing Suno output must not break the build.
- Missing BGM or aura files must only produce app console warnings.
- Existing SFX keys must still load, either from the new WAV files or the prior checked-in WAV files.
- Enemy idle animation must fall back to the bind-pose palette if the external idle clip or skeleton initialization fails.

## Testing

Automated or script-level tests:

- Add a small repository-local verification script that checks required sound files exist and that each generated WAV has nonzero duration.
- Add a lightweight source assertion script or test that project 36 references the new `PublicDemoBgm` and `EnemyIdleAura` keys.

Manual verification:

- Build `Dx11/36_AdvancedAnim_Sound_Click/36_AdvancedAnim_Sound_Click.vcxproj` Debug x64.
- Run project 36, start the public demo, and confirm enemy characters visibly idle instead of staying in a static bind pose.
- Confirm the app console logs loaded public SFX plus BGM/aura when those files are present.
- Build `Dx11/TutorialApp.sln` Debug x64 before completion.

## Non-Goals

- Do not update code in project 32 or other sound demos.
- Do not introduce a new audio middleware or replace FMOD.
- Do not create new animation assets in Blender.
- Do not use copyrighted music, character names, or franchise references in Suno prompts.
