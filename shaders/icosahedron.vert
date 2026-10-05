#version 450

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 fragPosition;
layout(location = 1) out vec3 vertexColor;

layout(set = 0, binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 view;
    mat4 projection;
} ubo;

void main()
{
    vec4 position =
        ubo.model *
        vec4(inPosition, 1.0);

    fragPosition = position.xyz;

    vertexColor = normalize(inPosition) * 0.5 + 0.5;

    gl_Position =
        ubo.projection *
        ubo.view *
        position;
}