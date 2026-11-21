"""Run once in Unreal Editor to create the prototype map and its shared material."""
import unreal

unreal.EditorAssetLibrary.make_directory('/Game/Voxel/Materials')
unreal.EditorAssetLibrary.make_directory('/Game/Voxel/Maps')
material_path = '/Game/Voxel/Materials/M_Voxel'
if not unreal.EditorAssetLibrary.does_asset_exist(material_path):
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_Voxel', '/Game/Voxel/Materials', unreal.Material, unreal.MaterialFactoryNew())
    tint = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -300, 0)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', unreal.LinearColor(0.3, 0.5, 0.4, 1))
    unreal.MaterialEditingLibrary.connect_material_property(tint, '', unreal.MaterialProperty.MP_BASE_COLOR)
    rough = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 200)
    rough.set_editor_property('r', 0.85)
    unreal.MaterialEditingLibrary.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(material_path)

mat = unreal.load_asset(material_path)
mat.set_editor_property('used_with_instanced_static_meshes', True)
unreal.MaterialEditingLibrary.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(material_path)

map_path = '/Game/Voxel/Maps/Riverlands'
if not unreal.EditorAssetLibrary.does_asset_exist(map_path):
    world = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    world.new_level(map_path)
    world.save_current_level()
unreal.log('VOXEL_BOOTSTRAP_OK')
