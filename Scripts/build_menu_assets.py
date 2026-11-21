from pathlib import Path
import unreal
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.SyncToBrowser 0')
source=Path(unreal.Paths.project_dir())/'SourceArt'/'UI'
if '-MenuMaterialOnly' not in unreal.SystemLibrary.get_command_line():
    for filename,name in [('MenuResume.png','T_MenuResume'),('MenuGoldGlow.png','T_MenuGoldGlow'),('MenuLogo.png','T_MenuLogo'),('MenuReference.png','T_MenuReference'),('MenuBackground.png','T_MenuBackground')]:
        task=unreal.AssetImportTask()
        for k,v in [('filename',str(source/filename)),('destination_path','/Game/Voxel/UI'),('destination_name',name),('automated',True),('replace_existing',True),('save',True)]: task.set_editor_property(k,v)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture=unreal.load_asset('/Game/Voxel/UI/'+name)
        if not texture: raise RuntimeError('Missing '+name)
        texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property('never_stream',True)
        unreal.EditorAssetLibrary.save_loaded_asset(texture)
unreal.log('VOXEL_MENU_ASSETS_OK')


# The hover recolors only blue face pixels and warms the existing sculpted gold.
# Cream lettering and its black extrusion retain the reference's original shapes.
path='/Game/Voxel/UI/M_MenuButtonArt'
mat=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_MenuButtonArt','/Game/Voxel/UI',unreal.Material,unreal.MaterialFactoryNew())
edit=unreal.MaterialEditingLibrary
edit.delete_all_material_expressions(mat)
mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property('two_sided',True)
mat.set_editor_property('disable_depth_test',True)
tex=edit.create_material_expression(mat,unreal.MaterialExpressionTextureSampleParameter2D,-600,0)
tex.set_editor_property('parameter_name','Art')
tex.set_editor_property('texture',unreal.load_asset('/Game/Voxel/UI/T_MenuReference'))
custom=edit.create_material_expression(mat,unreal.MaterialExpressionCustom,0,0)
inputs=[]
for name in ['Color','Alpha','Highlight','BorderOnly']:
    entry=unreal.CustomInput();entry.set_editor_property('input_name',name);inputs.append(entry)
custom.set_editor_property('inputs',inputs)
custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
custom.set_editor_property('code',r"""
float h=saturate(Highlight);
float cool=saturate((Color.b-Color.r)*8)*(1-smoothstep(.5,.8,Color.r));
float warm=saturate((Color.r-Color.b)*6);
float3 blue=float3(Color.r*.92,Color.g*1.85+.045,Color.b*2.05+.10);
float3 lit=lerp(Color.rgb,blue,cool*h);
lit+=float3(.32,.22,.035)*warm*h*.7;
if(BorderOnly>.5)return float4(float3(1.8,1.05,.14),Alpha*warm*h*.78);
// Canvas material colors bypass the texture draw path gamma conversion.
return float4(pow(saturate(lit),1.0/2.2),Alpha*h);
""")
assert edit.connect_material_expressions(tex,'',custom,'Color')
assert edit.connect_material_expressions(tex,'A',custom,'Alpha')
for i,name in enumerate(['Highlight','BorderOnly']):
    param=edit.create_material_expression(mat,unreal.MaterialExpressionScalarParameter,-500,200+i*180)
    param.set_editor_property('parameter_name',name);param.set_editor_property('default_value',0)
    edit.connect_material_expressions(param,'',custom,name)
rgb=edit.create_material_expression(mat,unreal.MaterialExpressionComponentMask,260,0)
rgb.set_editor_property('r',True);rgb.set_editor_property('g',True);rgb.set_editor_property('b',True);rgb.set_editor_property('a',False)
a=edit.create_material_expression(mat,unreal.MaterialExpressionComponentMask,260,180)
a.set_editor_property('r',False);a.set_editor_property('g',False);a.set_editor_property('b',False);a.set_editor_property('a',True)
assert edit.connect_material_expressions(custom,'',rgb,'');assert edit.connect_material_expressions(custom,'',a,'')
edit.connect_material_property(rgb,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
edit.connect_material_property(a,'',unreal.MaterialProperty.MP_OPACITY)
edit.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat)
unreal.log('VOXEL_MENU_V2_ASSETS_OK')
