#version 450

layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform ShadowPushConstants {
    mat4 lightViewProjection;
    mat4 model;
} shadowData;

void main()
{
    gl_Position = shadowData.lightViewProjection * shadowData.model * vec4(inPosition, 1.0);
}
