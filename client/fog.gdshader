shader_type spatial;
render_mode unshaded, blend_mix, depth_draw_never, shadows_disabled, fog_disabled;
uniform sampler2D fog_map : filter_nearest, repeat_disable;
uniform vec2 map_extent;
varying vec2 world_xz;
void vertex() { world_xz = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xz; }
void fragment() {
    vec4 fog = texture(fog_map, world_xz / map_extent);
    ALBEDO = fog.rgb;
    ALPHA = fog.a;
}
