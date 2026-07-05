import pathlib
import unreal

OUTPUT_DIR = pathlib.Path(r"C:\Users\k2503200021\AppData\Local\Temp\D3D11AssetInspection\UEExportAnimOnly")
ASSETS = [
    "/Game/Assets/Characters/MyAlice/Animation/anim_Idle",
    "/Game/Assets/Characters/MyAlice/Animation/Walk_Loop_F_0_Seq",
    "/Game/Assets/Characters/MyAlice/Animation/Run_Combat_Loop_F_0_Seq",
    "/Game/Assets/Characters/MyAlice/Animation/Roll_F_0_Seq",
]


def export_anim(asset_path: str) -> None:
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if asset is None:
        raise RuntimeError(f"Could not load {asset_path}")

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    out_file = OUTPUT_DIR / f"{asset.get_name()}.fbx"

    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = str(out_file)
    task.automated = True
    task.replace_identical = True
    task.prompt = False

    options = unreal.FbxExportOption()
    options.ascii = False
    options.export_preview_mesh = False
    task.options = options

    ok = unreal.Exporter.run_asset_export_task(task)
    if not ok:
        raise RuntimeError(f"Export failed for {asset_path}")
    unreal.log(f"Exported {asset_path} -> {out_file}")


for path in ASSETS:
    export_anim(path)
