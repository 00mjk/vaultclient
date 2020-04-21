#version 330
#extension GL_ARB_separate_shader_objects : require

layout(std140) uniform type_u_cameraPlaneParams
{
    float s_CameraNearPlane;
    float s_CameraFarPlane;
    float u_clipZNear;
    float u_clipZFar;
} u_cameraPlaneParams;

layout(std140) uniform type_u_EveryFrameFrag
{
    vec4 u_specularDir;
    layout(row_major) mat4 u_eyeNormalMatrix;
    layout(row_major) mat4 u_inverseViewMatrix;
} u_EveryFrameFrag;

uniform sampler2D SPIRV_Cross_CombinednormalMapTexturenormalMapSampler;
uniform sampler2D SPIRV_Cross_CombinedskyboxTextureskyboxSampler;

layout(location = 0) in vec2 in_var_TEXCOORD0;
layout(location = 1) in vec2 in_var_TEXCOORD1;
layout(location = 2) in vec4 in_var_COLOR0;
layout(location = 3) in vec4 in_var_COLOR1;
layout(location = 4) in vec2 in_var_TEXCOORD2;
layout(location = 0) out vec4 out_var_SV_Target;

void main()
{
    vec3 _90 = normalize(((texture(SPIRV_Cross_CombinednormalMapTexturenormalMapSampler, in_var_TEXCOORD0).xyz * vec3(2.0)) - vec3(1.0)) + ((texture(SPIRV_Cross_CombinednormalMapTexturenormalMapSampler, in_var_TEXCOORD1).xyz * vec3(2.0)) - vec3(1.0)));
    vec3 _92 = normalize(in_var_COLOR0.xyz);
    vec3 _108 = normalize((vec4(_90, 0.0) * u_EveryFrameFrag.u_eyeNormalMatrix).xyz);
    vec3 _110 = normalize(reflect(_92, _108));
    vec3 _134 = normalize((vec4(_110, 0.0) * u_EveryFrameFrag.u_inverseViewMatrix).xyz);
    float _162 = log2(in_var_TEXCOORD2.x) * (1.0 / log2(u_cameraPlaneParams.s_CameraFarPlane + 1.0));
    vec4 _165 = (vec4(mix(texture(SPIRV_Cross_CombinedskyboxTextureskyboxSampler, vec2(atan(_134.x, _134.y) + 3.1415927410125732421875, acos(_134.z)) * vec2(0.15915493667125701904296875, 0.3183098733425140380859375)).xyz, in_var_COLOR1.xyz * mix(vec3(1.0, 1.0, 0.60000002384185791015625), vec3(0.3499999940395355224609375), vec3(pow(max(0.0, _90.z), 5.0))), vec3(((dot(_108, _92) * (-0.5)) + 0.5) * 0.75)) + vec3(pow(abs(dot(_110, normalize((vec4(normalize(u_EveryFrameFrag.u_specularDir.xyz), 0.0) * u_EveryFrameFrag.u_eyeNormalMatrix).xyz))), 50.0) * 0.5), 1.0) * 0.300000011920928955078125) + vec4(0.20000000298023223876953125, 0.4000000059604644775390625, 0.699999988079071044921875, 1.0);
    _165.w = _162;
    out_var_SV_Target = _165;
    gl_FragDepth = _162;
}

