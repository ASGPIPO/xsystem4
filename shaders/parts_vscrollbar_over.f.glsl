/* Straight-alpha source-over for independently layered scrollbar skins. */
uniform sampler2D tex;
uniform sampler2D background;
uniform vec4 source_rect;

in vec2 tex_coord;
out vec4 frag_color;

void main() {
	vec4 src = texture(tex, source_rect.xy + tex_coord * source_rect.zw);
	vec4 dst = texelFetch(background, ivec2(gl_FragCoord.xy), 0);
	float a = src.a + dst.a * (1.0 - src.a);
	vec3 rgb = src.rgb * src.a + dst.rgb * dst.a * (1.0 - src.a);
	frag_color = vec4(a > 0.0 ? rgb / a : vec3(0.0), a);
}
