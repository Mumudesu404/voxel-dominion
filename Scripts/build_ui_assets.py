"""Import the user's exact transparent UI art and original vector numeral atlas."""
from pathlib import Path
import unreal

source = Path(unreal.Paths.project_dir()) / 'SourceArt' / 'UI'
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.SyncToBrowser 0')
unreal.EditorAssetLibrary.make_directory('/Game/Voxel/UI')
for filename, asset_name in [('MoneyBag.png', 'T_MoneyBag'), ('SelectionBlade.png', 'T_SelectionBlade'),
                             ('HandCursor.png', 'T_HandCursor'), ('GoldDigits.png', 'T_GoldDigits')]:
    task = unreal.AssetImportTask()
    for key, value in [('filename', str(source / filename)), ('destination_path', '/Game/Voxel/UI'),
                       ('destination_name', asset_name), ('automated', True), ('replace_existing', True), ('save', True)]:
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset('/Game/Voxel/UI/' + asset_name)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError('Missing UI texture: ' + asset_name)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('never_stream', True)
    texture.set_editor_property('srgb', True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
unreal.log('VOXEL_UI_ASSETS_OK: exact user art, transparent textures and original gold numeral atlas')
