"""Build locally authored meteor meshes and materials. Run with UnrealEditor-Cmd."""
import math
import os
import random
import unreal

# Commandlets have no content-browser Slate window; disable post-import browser sync.
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.SyncToBrowser 0')

ROOT = '/Game/Voxel'
SOURCE = os.path.join(unreal.Paths.project_dir(), 'SourceArt', 'Meteor')
os.makedirs(SOURCE, exist_ok=True)
unreal.EditorAssetLibrary.make_directory(ROOT + '/Effects')
unreal.EditorAssetLibrary.make_directory(ROOT + '/Materials')


def obj_file(name, verts, faces):
    filename = os.path.join(SOURCE, name + '.obj')
    with open(filename, 'w', encoding='ascii') as handle:
        handle.write('o ' + name + '\ns off\n')
        for v in verts:
            handle.write('v %.6f %.6f %.6f\n' % tuple(v))
        for face in faces:
            a, b, c = [verts[i] for i in face]
            ab, ac = [b[i]-a[i] for i in range(3)], [c[i]-a[i] for i in range(3)]
            normal = (ab[1]*ac[2]-ab[2]*ac[1], ab[2]*ac[0]-ab[0]*ac[2], ab[0]*ac[1]-ab[1]*ac[0])
            length = max(1e-9, math.sqrt(sum(n*n for n in normal)))
            handle.write('vn %.6f %.6f %.6f\n' % tuple(n/length for n in normal))
        for index, face in enumerate(faces, 1):
            handle.write('f ' + ' '.join('%d//%d' % (v+1, index) for v in face) + '\n')
    return filename


phi = (1 + math.sqrt(5)) / 2
verts = [(-1,phi,0),(1,phi,0),(-1,-phi,0),(1,-phi,0),(0,-1,phi),(0,1,phi),(0,-1,-phi),(0,1,-phi),(phi,0,-1),(phi,0,1),(-phi,0,-1),(-phi,0,1)]
faces = [(0,11,5),(0,5,1),(0,1,7),(0,7,10),(0,10,11),(1,5,9),(5,11,4),(11,10,2),(10,7,6),(7,1,8),(3,9,4),(3,4,2),(3,2,6),(3,6,8),(3,8,9),(4,9,5),(2,4,11),(6,2,10),(8,6,7),(9,8,1)]
midpoints = {}
def midpoint(a,b):
    key = tuple(sorted((a,b)))
    if key not in midpoints:
        midpoints[key] = len(verts)
        verts.append(tuple((verts[a][i]+verts[b][i])*.5 for i in range(3)))
    return midpoints[key]
subdivided = []
for a,b,c in faces:
    ab,bc,ca = midpoint(a,b),midpoint(b,c),midpoint(c,a)
    subdivided.extend([(a,ab,ca),(b,bc,ab),(c,ca,bc),(ab,bc,ca)])
rng = random.Random(615)
rock_verts = []
for v in verts:
    length = math.sqrt(sum(n*n for n in v)); radius=50*rng.uniform(.84,1.12)
    rock_verts.append(tuple(n/length*radius for n in v))
rock_file = obj_file('SM_MeteorRock', rock_verts, subdivided)
ring_verts, ring_faces = [], []
for i in range(80):
    a = i*math.tau/80
    ring_verts.extend([(math.cos(a)*50,math.sin(a)*50,0),(math.cos(a)*47,math.sin(a)*47,0)])
for i in range(80):
    a,b,c,d=2*i,2*i+1,2*((i+1)%80),2*((i+1)%80)+1
    ring_faces.extend([(a,c,b),(b,c,d)])
ring_file = obj_file('SM_MeteorRing', ring_verts, ring_faces)

for name, filename in [('SM_MeteorRock',rock_file),('SM_MeteorRing',ring_file)]:
    path = ROOT + '/Effects/' + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        task=unreal.AssetImportTask()
        task.set_editor_property('filename', filename)
        task.set_editor_property('destination_path', ROOT + '/Effects')
        task.set_editor_property('destination_name', name)
        task.set_editor_property('automated', True)
        task.set_editor_property('replace_existing', True)
        task.set_editor_property('save', True)
        options=unreal.FbxImportUI()
        options.set_editor_property('import_materials', False)
        options.set_editor_property('import_textures', False)
        options.static_mesh_import_data.set_editor_property('auto_generate_collision', False)
        options.static_mesh_import_data.set_editor_property('generate_lightmap_u_vs', False)
        task.set_editor_property('options', options)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh=unreal.load_asset(path)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError('Missing mesh after import: ' + path)
    base=unreal.load_asset(ROOT + '/Materials/M_Voxel')
    if base:
        mesh.set_material(0, base)
    unreal.EditorAssetLibrary.save_asset(path)


def material(name, blend):
    path=ROOT + '/Materials/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        mat=unreal.load_asset(path)
        unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
    else:
        mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ROOT+'/Materials', unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('blend_mode', blend)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided', True)
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    edit=unreal.MaterialEditingLibrary
    tint=edit.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -600, -100)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', unreal.LinearColor(1,.2,.01,1))
    fade=edit.create_material_expression(mat, unreal.MaterialExpressionPerInstanceCustomData, -600, 150)
    fade.set_editor_property('data_index', 0)
    fade.set_editor_property('const_default_value', 1.0)
    # Transparent particles fade through opacity once. Multiplying emission by
    # fade as well makes the late fire/smoke disappear much too early.
    if blend == unreal.BlendMode.BLEND_OPAQUE:
        emission=edit.create_material_expression(mat, unreal.MaterialExpressionMultiply, -200, -100)
        edit.connect_material_expressions(tint, '', emission, 'A')
        edit.connect_material_expressions(fade, '', emission, 'B')
        edit.connect_material_property(emission, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        edit.connect_material_property(tint, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if blend != unreal.BlendMode.BLEND_OPAQUE:
        fresnel=edit.create_material_expression(mat, unreal.MaterialExpressionFresnel, -600, 350)
        fresnel.set_editor_property('exponent', 2.5)
        fresnel.set_editor_property('base_reflect_fraction', 0.0)
        soft=edit.create_material_expression(mat, unreal.MaterialExpressionOneMinus, -400, 350)
        if not edit.connect_material_expressions(fresnel, '', soft, ''):
            raise RuntimeError('Failed to connect soft particle edge: ' + name)
        opacity=edit.create_material_expression(mat, unreal.MaterialExpressionMultiply, -200, 250)
        edit.connect_material_expressions(soft, '', opacity, 'A')
        edit.connect_material_expressions(fade, '', opacity, 'B')
        edit.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY)
    edit.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(path)

material('M_MeteorGlow', unreal.BlendMode.BLEND_OPAQUE)
material('M_MeteorFire', unreal.BlendMode.BLEND_ADDITIVE)
material('M_MeteorSmoke', unreal.BlendMode.BLEND_TRANSLUCENT)
unreal.log('VOXEL_METEOR_ASSETS_OK: 2 meshes, 3 materials; original OBJ sources saved in SourceArt/Meteor')
