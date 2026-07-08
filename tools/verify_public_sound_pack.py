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
