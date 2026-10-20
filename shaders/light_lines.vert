#version 450

layout(location = 0) in vec3 inPosition;

layout(set = 0, binding = 0) uniform SceneUniformBuffer {
    mat4 view;
    mat4 projection;
    vec4 light_direction;
    vec4 light_color;
    vec4 ambient;
    vec4 camera_position;
} scene;

void main()
{
    gl_Position = scene.projection * scene.view * vec4(inPosition, 1.0);
}
