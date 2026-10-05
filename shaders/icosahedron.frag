#version 450

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 vertexColor;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 view;
    mat4 projection;
    vec4 color;
} ubo;

void main()
{
    vec3 dx = dFdx(fragPosition);
    vec3 dy = dFdy(fragPosition);

    vec3 normal = normalize(cross(dy, dx));

    vec3 lightDirection =
        normalize(vec3(0.5, 0.8, 1.0));

    float lighting =
        max(dot(normal, lightDirection), 0.0);

    vec3 baseColor = ubo.color.rgb * vertexColor;

    vec3 color =
        baseColor * (0.55 + 0.45 * lighting);

    outColor = vec4(color, 1.0);
}