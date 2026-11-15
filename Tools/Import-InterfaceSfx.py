"""Run with UnrealEditor-Cmd -run=pythonscript -script=<this script>. Imports only the eight generated cues."""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve();manifest=json.loads((root/'ArtSource/InterfaceSfx/manifest.json').read_text(encoding='utf-8'))
for cue in manifest['cues']:
 task=unreal.AssetImportTask();task.filename=str(root/'ArtSource/InterfaceSfx'/cue['file']);task.destination_path='/Game/Audio/Interface';task.destination_name='SFX_'+cue['id'];task.automated=True;task.replace_existing=True;task.save=True
 unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
 asset=unreal.load_asset(cue['asset']);assert asset and isinstance(asset,unreal.SoundWave),cue['id']
 asset.set_editor_property('looping',False)
 unreal.EditorAssetLibrary.save_loaded_asset(asset)
 unreal.log('InterfaceSfx imported '+cue['id']+' duration='+str(asset.get_editor_property('duration')))
