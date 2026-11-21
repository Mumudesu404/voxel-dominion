"""Import the original weapon WAVs and build the terrain range-overlay material."""
from pathlib import Path
import runpy
import unreal

ROOT = Path(unreal.Paths.project_dir())
range_only = '-RangeMaterialOnly' in unreal.SystemLibrary.get_command_line()
if not range_only:
    runpy.run_path(str(ROOT / 'Scripts' / 'generate_tower_audio.py'), run_name='__main__')
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.SyncToBrowser 0')
    unreal.EditorAssetLibrary.make_directory('/Game/Voxel/Audio')
    for source in sorted((ROOT / 'SourceArt' / 'Audio').glob('S_*.wav')):
        task = unreal.AssetImportTask()
        task.set_editor_property('filename', str(source))
        task.set_editor_property('destination_path', '/Game/Voxel/Audio')
        task.set_editor_property('destination_name', source.stem)
        task.set_editor_property('automated', True)
        task.set_editor_property('replace_existing', True)
        task.set_editor_property('save', True)
        task.set_editor_property('factory', unreal.SoundFactory())
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        asset_path = '/Game/Voxel/Audio/' + source.stem
        sound = unreal.load_asset(asset_path)
        if not isinstance(sound, unreal.SoundWave):
            raise RuntimeError('Sound import failed: ' + asset_path)
        sound.set_editor_property('looping', False)
        sound.set_editor_property('compression_quality', 85)
        sound.set_editor_property('loading_behavior', unreal.SoundWaveLoadingBehavior.FORCE_INLINE)
        unreal.EditorAssetLibrary.save_asset(asset_path)

path = '/Game/Voxel/Materials/M_TowerRange'
mat = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else \
    unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_TowerRange', '/Game/Voxel/Materials', unreal.Material, unreal.MaterialFactoryNew())
edit = unreal.MaterialEditingLibrary
edit.delete_all_material_expressions(mat)
mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property('two_sided', True)
mat.set_editor_property('disable_depth_test', True)

custom = edit.create_material_expression(mat, unreal.MaterialExpressionCustom, 0, 0)
names = ['WorldPos', 'Origin', 'BaseRange', 'Tint', 'FillTint', 'FillOpacity', 'BattleClock', 'RotationAngle', 'BadgePosition', 'BadgeSize']
inputs = []
for name in names:
    entry = unreal.CustomInput()
    entry.set_editor_property('input_name', name)
    inputs.append(entry)
custom.set_editor_property('inputs', inputs)
custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
custom.set_editor_property('description', 'Complete tactical circle, translucent blue fill, synchronized rotating ticks and crossed-sword crest')
custom.set_editor_property('code', r'''
float2 d = WorldPos.xy - Origin.xy;
float r = length(d);
float radius = BaseRange;
float aa = max(fwidth(r), 1.1);
float rim = abs(r - radius);
float core = 1 - smoothstep(1.8, 1.8 + aa, rim);
float inner = (1 - smoothstep(.8, .8 + aa, abs(r - radius + 10))) * .30;
float pulse = .91 + .09 * sin(BattleClock * 2.0);
float glow = exp(-rim / max(15, aa * 2.0)) * .35 * pulse;
float coloredEdge = (1 - smoothstep(5, 5 + aa, rim)) * .88;
float inside = 1 - smoothstep(radius - aa, radius + aa, r);
float fill = FillOpacity * inside;
float angle = atan2(d.y, d.x) - RotationAngle;
float tickIndex = floor(angle / 6.2831853 * 64 + .5);
float tickDistance = abs(frac(angle / 6.2831853 * 64 + .5) - .5) * 6.2831853 * max(r, 1) / 64;
float major = 1 - step(.5, abs(frac(tickIndex / 8) * 8));
float tickBand = smoothstep(radius - lerp(32, 58, major) - aa, radius - lerp(32, 58, major), r)
              * (1 - smoothstep(radius - 13, radius - 13 + aa, r));
float ticks = (1 - smoothstep(1.1, 1.1 + aa, tickDistance)) * tickBand * .78;
float cardinalDistance = abs(frac(angle / 6.2831853 * 4 + .5) - .5) * 6.2831853 * max(r, 1) / 4;
float diamond = abs(r - radius) + cardinalDistance;
float diamondEdge = 1 - smoothstep(1.2, 1.2 + aa, abs(diamond - 18));
float lineAlpha = max(coloredEdge, max(glow, max(max(core * .95, inner), max(ticks, diamondEdge * .9))));
float alpha = max(fill, lineAlpha);
float sweep = pow(.5 + .5 * cos(angle), 12);
float3 edgeRGB = lerp(Tint.rgb * (2.0 + sweep * .45), float3(2.0, 2.2, 2.25), max(core, diamondEdge) * .82);
float3 rgb = lerp(FillTint.rgb, edgeRGB, saturate(lineAlpha / max(alpha, .001)));

// Evaluate the crest in the same rotating dial space as the ticks and diamonds.
// Its position and swords rotate together around the tower without rebuilding geometry.
float2 front = normalize(float2(-3300, -4000));
float2 side = float2(-front.y, front.x);
float rotationCos = cos(RotationAngle), rotationSin = sin(RotationAngle);
float2 dialPosition = float2(rotationCos * d.x + rotationSin * d.y, -rotationSin * d.x + rotationCos * d.y);
float2 q = dialPosition - (BadgePosition.xy - Origin.xy);
float2 b = float2(dot(q, side), dot(q, front)) / BadgeSize;
float h = max(abs(b.x) * .8660254 + abs(b.y) * .5, abs(b.y));
float badgeAA = max(aa / BadgeSize, .014);
float badge = 1 - smoothstep(1, 1 + badgeAA, h);
float border = smoothstep(.82, .86, h);
float fine = 1 - smoothstep(.012, .012 + badgeAA, abs(h - .98));
float3 crest = lerp(float3(.025, .065, .12), Tint.rgb * 1.9, border);
crest = lerp(crest, float3(2.1, 2.2, 2.2), fine * .7);
for (int k = 0; k < 2; ++k)
{
    float2 dir = normalize(float2(k == 0 ? 1 : -1, -1));
    float2 across = float2(-dir.y, dir.x);
    float u = dot(b, dir), v = dot(b, across);
    float bladeWidth = .07 * saturate((.60 - u) / .18);
    float blade = (1 - smoothstep(bladeWidth, bladeWidth + badgeAA, abs(v)))
                * smoothstep(-.24 - badgeAA, -.24, u) * (1 - smoothstep(.60, .60 + badgeAA, u));
    float handle = (1 - smoothstep(.05, .05 + badgeAA, abs(v)))
                 * smoothstep(-.59 - badgeAA, -.59, u) * (1 - smoothstep(-.23, -.23 + badgeAA, u));
    float guard = (1 - smoothstep(.20, .20 + badgeAA, abs(v)))
                * (1 - smoothstep(.045, .045 + badgeAA, abs(u + .26)));
    crest = lerp(crest, float3(.43, .19, .045), handle);
    crest = lerp(crest, float3(.85, .55, .13), guard);
    crest = lerp(crest, float3(.85, 1.05, 1.2), blade);
}
rgb = lerp(rgb, crest, badge);
alpha = lerp(alpha, .94, badge);
return float4(rgb, saturate(alpha));
''')

position = edit.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -450, -300)
edit.connect_material_expressions(position, '', custom, 'WorldPos')
for index, (name, value) in enumerate((('Origin', (0, 0, 70)), ('Tint', (.06, .48, 1.)), ('FillTint', (.16, .65, 1.75)), ('BadgePosition', (0, -1200, 0)))):
    node = edit.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -450, -100 + index * 110)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', unreal.LinearColor(*value, 1))
    edit.connect_material_expressions(node, '', custom, name)
for index, (name, value) in enumerate((('BaseRange', 1250), ('BattleClock', 0), ('RotationAngle', 0), ('FillOpacity', .18), ('BadgeSize', 90))):
    node = edit.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -450, 270 + index * 90)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', value)
    edit.connect_material_expressions(node, '', custom, name)
rgb = edit.create_material_expression(mat, unreal.MaterialExpressionComponentMask, 300, -50)
rgb.set_editor_property('r', True); rgb.set_editor_property('g', True); rgb.set_editor_property('b', True); rgb.set_editor_property('a', False)
alpha = edit.create_material_expression(mat, unreal.MaterialExpressionComponentMask, 300, 100)
alpha.set_editor_property('r', False); alpha.set_editor_property('g', False); alpha.set_editor_property('b', False); alpha.set_editor_property('a', True)
edit.connect_material_expressions(custom, '', rgb, '')
edit.connect_material_expressions(custom, '', alpha, '')
edit.connect_material_property(rgb, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
edit.connect_material_property(alpha, '', unreal.MaterialProperty.MP_OPACITY)
edit.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(path)
unreal.log('VOXEL_RANGE_MATERIAL_OK: complete circle, blue translucent fill, rotating dial; audio ' + ('unchanged' if range_only else 'imported'))
