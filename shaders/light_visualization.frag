#version 450

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform ModelUniformBuffer {
    mat4 model;
    vec4 color;
    vec4 material_specular;
} modelData;

void main()
{
    outColor = vec4(modelData.color.rgb, 1.0);
}
