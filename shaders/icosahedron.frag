#version 450

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 vertexColor;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform SceneUniformBuffer {
    mat4 view;
    mat4 projection;
    vec4 light_direction;
    vec4 light_color;
    vec4 ambient;
    vec4 camera_position;
} scene;

struct PointLight {
    vec4 position;
    vec4 color;
};

layout(set = 0, binding = 1, std430) readonly buffer PointLightBuffer {
    uint count;
    uint padding0;
    uint padding1;
    uint padding2;
    PointLight lights[];
} pointLightBuffer;

struct SpotLight {
    vec4 position;
    vec4 direction;
    vec4 color;
    vec4 cone_angles;
};

layout(set = 0, binding = 2, std430) readonly buffer SpotLightBuffer {
    uint count;
    uint padding0;
    uint padding1;
    uint padding2;
    SpotLight lights[];
} spotLightBuffer;

layout(set = 1, binding = 0) uniform ModelUniformBuffer {
    mat4 model;
    vec4 color;
    vec4 material_specular;
} modelData;

layout(set = 1, binding = 1) uniform sampler2D albedoTexture;
layout(set = 1, binding = 2) uniform sampler2D specularTexture;
layout(set = 1, binding = 3) uniform sampler2D emissiveTexture;

layout(push_constant) uniform SamplingPushConstants {
    int enabled;
    float time;
    float strength;
} sampling;

void main()
{
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(scene.camera_position.xyz - fragPosition);

    vec2 uv = fragTexCoord;

    if (sampling.enabled != 0) {
        uv.x += sin(uv.y * 20.0 + sampling.time * 2.0)
              * sampling.strength;

        uv.y += cos(uv.x * 20.0 + sampling.time * 2.0)
              * sampling.strength;
    }

    vec3 baseColor = texture(albedoTexture, uv).rgb
                   * modelData.color.rgb;

    vec3 specularMap = texture(specularTexture, uv).rgb;
    vec3 emissiveMap = texture(emissiveTexture, uv).rgb;

    vec3 ambient = baseColor
                 * scene.ambient.rgb
                 * scene.ambient.a;

    vec3 L = normalize(-scene.light_direction.xyz);
    float diffuseFactor = max(dot(N, L), 0.0);

    vec3 diffuse = baseColor
                 * scene.light_color.rgb
                 * scene.light_color.a
                 * diffuseFactor;

    vec3 specular = vec3(0.0);

    if (diffuseFactor > 0.0) {
        vec3 H = normalize(L + V);

        float specularFactor = pow(
            max(dot(N, H), 0.0),
            max(modelData.material_specular.a, 1.0)
        );

        specular = specularMap
                 * modelData.material_specular.rgb
                 * scene.light_color.rgb
                 * scene.light_color.a
                 * specularFactor;
    }

    vec3 pointLighting = vec3(0.0);

    for (uint i = 0; i < pointLightBuffer.count; ++i) {
        PointLight light = pointLightBuffer.lights[i];

        vec3 toLight = light.position.xyz - fragPosition;
        float distanceToLight = length(toLight);

        if (distanceToLight > 0.001) {
            vec3 Lp = toLight / distanceToLight;
            float diffusePoint = max(dot(N, Lp), 0.0);

            vec3 pointSpecular = vec3(0.0);

            if (diffusePoint > 0.0) {
                vec3 Hp = normalize(Lp + V);

                float specularPoint = pow(
                    max(dot(N, Hp), 0.0),
                    max(modelData.material_specular.a, 1.0)
                );

                pointSpecular = specularMap
                              * modelData.material_specular.rgb
                              * light.color.rgb
                              * specularPoint;
            }

            float attenuation =
                1.0 / (distanceToLight * distanceToLight);

            vec3 pointDiffuse =
                baseColor * light.color.rgb * diffusePoint;

            pointLighting +=
                (pointDiffuse + pointSpecular)
                * attenuation
                * light.color.a;
        }
    }

    vec3 spotLighting = vec3(0.0);

    for (uint i = 0; i < spotLightBuffer.count; ++i) {
        SpotLight light = spotLightBuffer.lights[i];

        vec3 toLight = light.position.xyz - fragPosition;
        float distanceToLight = length(toLight);

        if (distanceToLight > 0.001) {
            vec3 Ls = toLight / distanceToLight;
            vec3 direction = normalize(light.direction.xyz);
            vec3 fromLightToFragment = -Ls;

            float theta = dot(fromLightToFragment, direction);

            float innerCos = cos(radians(light.cone_angles.x));
            float outerCos = cos(radians(light.cone_angles.y));

            float coneFactor = smoothstep(outerCos, innerCos, theta);

            float attenuation =
                1.0 / (distanceToLight * distanceToLight);

            float diffuseSpot = max(dot(N, Ls), 0.0);

            vec3 spotDiffuse =
                baseColor * light.color.rgb * diffuseSpot;

            vec3 spotSpecular = vec3(0.0);

            if (diffuseSpot > 0.0) {
                vec3 Hs = normalize(Ls + V);

                float specularSpot = pow(
                    max(dot(N, Hs), 0.0),
                    max(modelData.material_specular.a, 1.0)
                );

                spotSpecular = specularMap
                             * modelData.material_specular.rgb
                             * light.color.rgb
                             * specularSpot;
            }

            spotLighting +=
                (spotDiffuse + spotSpecular)
                * attenuation
                * coneFactor
                * light.color.a;
        }
    }

    vec3 result =
        ambient
        + diffuse
        + specular
        + pointLighting
        + spotLighting
        + emissiveMap;

    outColor = vec4(result, 1.0);
}
