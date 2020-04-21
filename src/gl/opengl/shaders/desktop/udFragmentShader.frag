#version 330
#extension GL_ARB_separate_shader_objects : require

uniform sampler2D SPIRV_Cross_CombinedsceneColourTexturesceneColourSampler;
uniform sampler2D SPIRV_Cross_CombinedsceneDepthTexturesceneDepthSampler;

layout(location = 0) in vec2 in_var_TEXCOORD0;
layout(location = 0) out vec4 out_var_SV_Target;

void main()
{
    vec4 _34 = texture(SPIRV_Cross_CombinedsceneDepthTexturesceneDepthSampler, in_var_TEXCOORD0);
    float _35 = _34.x;
    vec4 _40 = vec4(texture(SPIRV_Cross_CombinedsceneColourTexturesceneColourSampler, in_var_TEXCOORD0).zyx, 1.0);
    _40.w = _35;
    out_var_SV_Target = _40;
    gl_FragDepth = _35;
}

