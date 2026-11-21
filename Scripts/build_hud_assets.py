from pathlib import Path
import unreal
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.SyncToBrowser 0')
source=Path(unreal.Paths.project_dir())/'SourceArt'/'UI'
for name in ['HUDBar','HUDOrders','HUDReference']:
    task=unreal.AssetImportTask()
    for k,v in [('filename',str(source/(name+'.png'))),('destination_path','/Game/Voxel/UI'),('destination_name','T_'+name),('automated',True),('replace_existing',True),('save',True)]:task.set_editor_property(k,v)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture=unreal.load_asset('/Game/Voxel/UI/T_'+name)
    if not isinstance(texture,unreal.Texture2D):raise RuntimeError('Missing HUD texture '+name)
    texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('never_stream',True)
    texture.set_editor_property('srgb',True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
unreal.log('VOXEL_BATTLE_HUD_ASSETS_OK')
