#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragPosition;
layout(location = 1) out vec3 vertexColor;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec2 fragTexCoord;
layout(location = 4) out vec4 fragLightSpacePosition;
layout(location = 5) out vec4 fragSpotLightSpacePosition;

layout(set = 0, binding = 0) uniform SceneUniformBuffer {
    mat4 view;
    mat4 projection;
    mat4 lightViewProjection;
    vec4 light_direction;
    mat4 spotLightViewProjection;
    vec4 light_color;
    vec4 ambient;
    vec4 camera_position;
} scene;

layout(set = 1, binding = 0) uniform ModelUniformBuffer {
    mat4 model;
    vec4 color;
    vec4 material_specular;
} modelData;

void main()
{
    vec4 position = modelData.model * vec4(inPosition, 1.0);

    fragPosition = position.xyz;

    fragNormal = normalize(
        transpose(inverse(mat3(modelData.model))) * inNormal
    );

    vertexColor = normalize(inPosition) * 0.5 + 0.5;

    fragLightSpacePosition =
        scene.lightViewProjection * position;

    fragSpotLightSpacePosition =
        scene.spotLightViewProjection * position;

    gl_Position = scene.projection * scene.view * position;

    fragTexCoord = inTexCoord;
}
