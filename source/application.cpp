#include "application.hpp"

#include <imgui.h>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <vector>
#include <iostream>
#include <windows.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <string>

struct Vertex {
    float x, y, z;
    float nx, ny, nz;
};

struct SceneUniformData {
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec4 light_direction;
    glm::vec4 light_color;
    glm::vec4 ambient;
    glm::vec4 camera_position;
};

struct ModelUniformData {
    glm::mat4 model;
    glm::vec4 color;
    glm::vec4 material_specular;
};

struct PointLight { // положение источника в пространстве 
    glm::vec4 position;
    glm::vec4 color;
};

struct SpotLight {
    glm::vec4 position;
    glm::vec4 direction;
    glm::vec4 color;
    glm::vec4 cone_angles;
};

std::vector<SpotLight> spot_lights = {
    {
        glm::vec4(0.0f, 3.0f, 2.0f, 1.0f),
        glm::vec4(0.0f, -1.0f, -1.0f, 0.0f),
        glm::vec4(1.0f, 0.8f, 0.5f, 1.0f),
        glm::vec4(20.0f, 30.0f, 0.0f, 0.0f)
    }
};

constexpr uint32_t MAX_POINT_LIGHTS = 16;

struct alignas(16) PointLightBufferData {
    uint32_t count;
    uint32_t padding[3];
    PointLight lights[MAX_POINT_LIGHTS];
};

constexpr uint32_t MAX_SPOT_LIGHTS = 16;

struct alignas(16) SpotLightBufferData {
    uint32_t count;
    uint32_t padding[3];
    SpotLight lights[MAX_SPOT_LIGHTS];
};

// геометрия вершин
const float phi = (1.0f + std::sqrt(5.0f)) / 2.0f;
const float inv_length = 1.0f / std::sqrt(1.0f + phi * phi);

const Vertex vertices[] = {
    { 0.0f,  1.0f,  phi,  0.0f,  inv_length,  phi * inv_length},
    { 0.0f, -1.0f,  phi,  0.0f, -inv_length,  phi * inv_length},
    { 0.0f,  1.0f, -phi,  0.0f,  inv_length, -phi * inv_length},
    { 0.0f, -1.0f, -phi,  0.0f, -inv_length, -phi * inv_length},

    { 1.0f,  phi, 0.0f,  inv_length,  phi * inv_length, 0.0f},
    {-1.0f,  phi, 0.0f, -inv_length,  phi * inv_length, 0.0f},
    { 1.0f, -phi, 0.0f,  inv_length, -phi * inv_length, 0.0f},
    {-1.0f, -phi, 0.0f, -inv_length, -phi * inv_length, 0.0f},

    { phi, 0.0f,  1.0f,  phi * inv_length, 0.0f,  inv_length},
    { phi, 0.0f, -1.0f,  phi * inv_length, 0.0f, -inv_length},
    {-phi, 0.0f,  1.0f, -phi * inv_length, 0.0f,  inv_length},
    {-phi, 0.0f, -1.0f, -phi * inv_length, 0.0f, -inv_length}
};

const uint32_t indices[] = {
    0, 1, 8,
    0, 8, 4,
    0, 4, 5,
    0, 5, 10,
    0, 10, 1,
    1, 10, 7,
    1, 7, 6,
    1, 6, 8,
    8, 6, 9,
    8, 9, 4,
    4, 9, 2,
    4, 2, 5,
    5, 2, 11,
    5, 11, 10,
    10, 11, 7,
    7, 11, 3,
    7, 3, 6,
    6, 3, 9,
    9, 3, 2,
    2, 3, 11
};

VkBuffer vertex_buffer = VK_NULL_HANDLE;
VmaAllocation vertex_buffer_allocation = VK_NULL_HANDLE;

VkBuffer index_buffer = VK_NULL_HANDLE;
VmaAllocation index_buffer_allocation = VK_NULL_HANDLE;

VkShaderModule vertex_shader = VK_NULL_HANDLE;
VkShaderModule fragment_shader = VK_NULL_HANDLE;
VkShaderModule light_visualization_fragment_shader = VK_NULL_HANDLE;

VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

VkPipeline graphics_pipeline = VK_NULL_HANDLE;
VkPipeline light_visualization_pipeline = VK_NULL_HANDLE;

VkShaderModule light_lines_vertex_shader = VK_NULL_HANDLE;
VkShaderModule light_lines_fragment_shader = VK_NULL_HANDLE;
VkPipeline light_lines_pipeline = VK_NULL_HANDLE;

VkBuffer light_lines_vertex_buffer = VK_NULL_HANDLE;
VmaAllocation light_lines_vertex_buffer_allocation = VK_NULL_HANDLE;

uint32_t light_lines_vertex_count = 0;


struct LightMarker {
    VkBuffer uniform_buffer = VK_NULL_HANDLE;
    VmaAllocation uniform_buffer_allocation = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
};

struct LightSphereGeometry {
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VmaAllocation vertex_buffer_allocation = VK_NULL_HANDLE;
    VkBuffer index_buffer = VK_NULL_HANDLE;
    VmaAllocation index_buffer_allocation = VK_NULL_HANDLE;
    uint32_t index_count = 0;
};

LightSphereGeometry light_sphere;

std::vector<LightMarker> light_markers;

VkBuffer scene_uniform_buffer = VK_NULL_HANDLE;
VmaAllocation scene_uniform_buffer_allocation = VK_NULL_HANDLE;

VkBuffer uniform_buffer = VK_NULL_HANDLE;
VmaAllocation uniform_buffer_allocation = VK_NULL_HANDLE;

VkBuffer uniform_buffer_2 = VK_NULL_HANDLE;
VmaAllocation uniform_buffer_allocation_2 = VK_NULL_HANDLE;

VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
VkDescriptorSetLayout model_descriptor_set_layout = VK_NULL_HANDLE;

VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;

VkDescriptorSet scene_descriptor_set = VK_NULL_HANDLE;
VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
VkDescriptorSet descriptor_set_2 = VK_NULL_HANDLE;

// буфер массива точечных источников света
VkBuffer point_light_buffer = VK_NULL_HANDLE;
VmaAllocation point_light_buffer_allocation = VK_NULL_HANDLE;

VkBuffer spot_light_buffer = VK_NULL_HANDLE;
VmaAllocation spot_light_buffer_allocation = VK_NULL_HANDLE;


glm::vec3 object_position = glm::vec3(0.0f);
glm::vec3 object_rotation = glm::vec3(0.0f);
glm::vec3 object_scale = glm::vec3(1.0f);
glm::vec3 object_color = glm::vec3(0.2f, 0.55f, 1.0f);

bool perspective_projection = true;

bool animation_playing = true;
float animation_speed = 1.0f;
float trajectory_radius = 2.0f;

// параметры камеры
glm::vec3 camera_position = glm::vec3(0.0f, 0.0f, 5.0f);

float camera_yaw = -90.0f;
float camera_pitch = 0.0f;
float camera_fov = 45.0f;

// направленный источник света
glm::vec3 light_direction =
    glm::normalize(glm::vec3(0.5f, 0.8f, 1.0f));

glm::vec3 light_color = glm::vec3(1.0f);
float light_intensity = 1.0f;

// фоновое освещение
glm::vec3 ambient_color = glm::vec3(1.0f);
float ambient_intensity = 0.15f;

// материал
glm::vec3 material_specular = glm::vec3(1.0f);
float material_shininess = 32.0f;

bool show_light_visualization = true;


std::vector<PointLight> point_lights = {
    {
        glm::vec4(3.0f, 2.0f, 3.0f, 1.0f),
        glm::vec4(1.0f, 0.4f, 0.2f, 1.0f)
    },
    {
        glm::vec4(-3.0f, 2.0f, 3.0f, 1.0f),
        glm::vec4(0.2f, 0.5f, 1.0f, 1.0f)
    }
};

bool createBuffer(
	VkDeviceSize size,
	VkBufferUsageFlags usage,
	const void* data,
	VkBuffer& buffer,
	VmaAllocation& allocation)
{
	const VkBufferCreateInfo buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VmaAllocationCreateInfo allocation_info{};
	allocation_info.usage = VMA_MEMORY_USAGE_AUTO;
	allocation_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

	if (vmaCreateBuffer(
			graphics::internal::context.allocator,
			&buffer_info,
			&allocation_info,
			&buffer,
			&allocation,
			nullptr) != VK_SUCCESS) {
		return false;
	}

	void* mapped_data = nullptr;

	if (vmaMapMemory(
			graphics::internal::context.allocator,
			allocation,
			&mapped_data) != VK_SUCCESS) {

		vmaDestroyBuffer(
			graphics::internal::context.allocator,
			buffer,
			allocation);

		buffer = VK_NULL_HANDLE;
		allocation = VK_NULL_HANDLE;

		return false;
	}

	std::memcpy(mapped_data, data, static_cast<size_t>(size));

	vmaUnmapMemory(
		graphics::internal::context.allocator,
		allocation);

	return true;
}

bool createLightSphereGeometry()
{
    constexpr uint32_t segments = 16;
    constexpr uint32_t rings = 12;

    std::vector<Vertex> sphere_vertices;
    std::vector<uint32_t> sphere_indices;

    for (uint32_t ring = 0; ring <= rings; ++ring) {
        float theta = glm::pi<float>() *
            static_cast<float>(ring) / static_cast<float>(rings);

        for (uint32_t segment = 0; segment <= segments; ++segment) {
            float phi = 2.0f * glm::pi<float>() *
                static_cast<float>(segment) /
                static_cast<float>(segments);

            float x = std::sin(theta) * std::cos(phi);
            float y = std::cos(theta);
            float z = std::sin(theta) * std::sin(phi);

            sphere_vertices.push_back({
                x, y, z,
                x, y, z
            });
        }
    }

    for (uint32_t ring = 0; ring < rings; ++ring) {
        for (uint32_t segment = 0; segment < segments; ++segment) {
            uint32_t first = ring * (segments + 1) + segment;
            uint32_t second = first + segments + 1;

            sphere_indices.push_back(first);
            sphere_indices.push_back(second);
            sphere_indices.push_back(first + 1);

            sphere_indices.push_back(second);
            sphere_indices.push_back(second + 1);
            sphere_indices.push_back(first + 1);
        }
    }

    if (!createBuffer(
            sphere_vertices.size() * sizeof(Vertex),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            sphere_vertices.data(),
            light_sphere.vertex_buffer,
            light_sphere.vertex_buffer_allocation)) {
        return false;
    }

    if (!createBuffer(
            sphere_indices.size() * sizeof(uint32_t),
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            sphere_indices.data(),
            light_sphere.index_buffer,
            light_sphere.index_buffer_allocation)) {
        return false;
    }

    light_sphere.index_count =
        static_cast<uint32_t>(sphere_indices.size());

    return true;
}


void updateLightLinesGeometry()
{
    constexpr uint32_t segments = 32;
    constexpr float cone_length = 2.5f;
    constexpr uint32_t vertices_per_spot = 2 + segments * 2 + 16;
    constexpr uint32_t max_vertices =
        MAX_SPOT_LIGHTS * vertices_per_spot;

    std::vector<glm::vec3> vertices;
    vertices.reserve(max_vertices);

    for (size_t i = 0;
         i < spot_lights.size() && i < MAX_SPOT_LIGHTS;
         ++i) {
        const SpotLight& spot = spot_lights[i];

        glm::vec3 origin = glm::vec3(spot.position);
        glm::vec3 direction = glm::vec3(spot.direction);

        if (glm::length(direction) < 0.001f) {
            direction = glm::vec3(0.0f, -1.0f, 0.0f);
        }

        direction = glm::normalize(direction);

        glm::vec3 reference =
            std::abs(direction.y) < 0.99f
                ? glm::vec3(0.0f, 1.0f, 0.0f)
                : glm::vec3(1.0f, 0.0f, 0.0f);

        glm::vec3 right =
            glm::normalize(glm::cross(direction, reference));
        glm::vec3 up =
            glm::normalize(glm::cross(right, direction));

        glm::vec3 tip = origin + direction * cone_length;

        vertices.push_back(origin);
        vertices.push_back(tip);

        float radius = cone_length *
            std::tan(glm::radians(spot.cone_angles.y));

        std::vector<glm::vec3> ring;
        ring.reserve(segments);

        for (uint32_t j = 0; j < segments; ++j) {
            float angle = 2.0f * glm::pi<float>() *
                static_cast<float>(j) / segments;

            glm::vec3 point = tip +
                radius * (
                    std::cos(angle) * right +
                    std::sin(angle) * up
                );

            ring.push_back(point);
        }

        for (uint32_t j = 0; j < segments; ++j) {
            vertices.push_back(ring[j]);
            vertices.push_back(ring[(j + 1) % segments]);
        }

        for (uint32_t j = 0; j < 8; ++j) {
            uint32_t index = j * segments / 8;
            vertices.push_back(origin);
            vertices.push_back(ring[index]);
        }
    }

    if (light_lines_vertex_buffer == VK_NULL_HANDLE) {
        std::vector<glm::vec3> initial_data(max_vertices);

        if (!createBuffer(
                sizeof(glm::vec3) * max_vertices,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                initial_data.data(),
                light_lines_vertex_buffer,
                light_lines_vertex_buffer_allocation)) {
            std::cerr << "FAILED: light lines vertex buffer\n";
            return;
        }
    }

    void* mapped_data = nullptr;

    if (vmaMapMemory(
            graphics::internal::context.allocator,
            light_lines_vertex_buffer_allocation,
            &mapped_data) != VK_SUCCESS) {
        return;
    }

    if (!vertices.empty()) {
        std::memcpy(
            mapped_data,
            vertices.data(),
            sizeof(glm::vec3) * vertices.size());
    }

    vmaUnmapMemory(
        graphics::internal::context.allocator,
        light_lines_vertex_buffer_allocation);

    light_lines_vertex_count =
        static_cast<uint32_t>(vertices.size());
}


std::vector<char> readFile(const char* filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		return {};
	}

	const size_t file_size = static_cast<size_t>(file.tellg());

	std::vector<char> buffer(file_size);

	file.seekg(0);
	file.read(buffer.data(), file_size);

	return buffer;
}

bool createShaderModule(
	const std::vector<char>& code,
	VkShaderModule& shader_module)
{
	if (code.empty()) {
		return false;
	}

	const VkShaderModuleCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = code.size(),
		.pCode = reinterpret_cast<const uint32_t*>(code.data()),
	};

	return vkCreateShaderModule(
		graphics::internal::context.device,
		&create_info,
		nullptr,
		&shader_module) == VK_SUCCESS;
}

SceneUniformData createSceneUniformData()
{
    SceneUniformData data{};

    const float yaw = glm::radians(camera_yaw);
    const float pitch = glm::radians(camera_pitch);

    glm::vec3 camera_direction(
        std::cos(yaw) * std::cos(pitch),
        std::sin(pitch),
        std::sin(yaw) * std::cos(pitch)
    );

    camera_direction = glm::normalize(camera_direction);

    data.view = glm::lookAt(
        camera_position,
        camera_position + camera_direction,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    float aspect =
        static_cast<float>(
            graphics::internal::context.swapchain_extent.width
        ) /
        static_cast<float>(
            graphics::internal::context.swapchain_extent.height
        );

    if (perspective_projection) {
        data.projection = glm::perspective(
            glm::radians(camera_fov),
            aspect,
            0.1f,
            100.0f
        );
    } else {
        float size = 3.0f;

        data.projection = glm::ortho(
			-size * aspect,
			size * aspect,
			-size,
			size,
			-100.0f,
			100.0f
		);
    }

    data.projection[1][1] *= -1.0f;

    data.light_direction = glm::vec4(
        glm::normalize(light_direction), 0.0f
    );

    data.light_color = glm::vec4(
        light_color, light_intensity
    );

    data.ambient = glm::vec4(
        ambient_color, ambient_intensity
    );

    data.camera_position = glm::vec4(
        camera_position, 1.0f
    );

    return data;
}

ModelUniformData createModelUniformData()
{
    ModelUniformData data{};

    glm::mat4 model = glm::mat4(1.0f);

    model = glm::translate(
        model,
        object_position
    );

    model = glm::rotate(
        model,
        glm::radians(object_rotation.x),
        glm::vec3(1.0f, 0.0f, 0.0f)
    );

    model = glm::rotate(
        model,
        glm::radians(object_rotation.y),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    model = glm::rotate(
        model,
        glm::radians(object_rotation.z),
        glm::vec3(0.0f, 0.0f, 1.0f)
    );

    model = glm::scale(
        model,
        object_scale
    );

    data.model = model;
    data.color = glm::vec4(object_color, 1.0f);

    data.material_specular = glm::vec4(
        material_specular, material_shininess
    );

    return data;
}

PointLightBufferData createPointLightBufferData()
{
    PointLightBufferData data{};

    data.count = static_cast<uint32_t>(
        std::min(point_lights.size(),
                 static_cast<size_t>(MAX_POINT_LIGHTS))
    );

    for (uint32_t i = 0; i < data.count; ++i) {
        data.lights[i] = point_lights[i];
    }

    return data;
}

SpotLightBufferData createSpotLightBufferData()
{
    SpotLightBufferData data{};

    data.count = static_cast<uint32_t>(
        std::min(
            spot_lights.size(),
            static_cast<size_t>(MAX_SPOT_LIGHTS)
        )
    );

    for (uint32_t i = 0; i < data.count; ++i) {
        data.lights[i] = spot_lights[i];
        data.lights[i].direction =
            glm::vec4(
                glm::normalize(glm::vec3(spot_lights[i].direction)),
                0.0f
            );
    }

    return data;
}

void updatePointLightBuffer()
{
    auto& context = graphics::internal::context;

    PointLightBufferData data = createPointLightBufferData();

    void* mapped_data = nullptr;

    if (vmaMapMemory(
            context.allocator,
            point_light_buffer_allocation,
            &mapped_data) != VK_SUCCESS) {
        return;
    }

    std::memcpy(
        mapped_data,
        &data,
        sizeof(PointLightBufferData)
    );

    vmaUnmapMemory(
        context.allocator,
        point_light_buffer_allocation
    );
}

void updateSpotLightBuffer()
{
    auto& context = graphics::internal::context;

    SpotLightBufferData data = createSpotLightBufferData();

    void* mapped_data = nullptr;

    if (vmaMapMemory(
            context.allocator,
            spot_light_buffer_allocation,
            &mapped_data) != VK_SUCCESS) {
        return;
    }

    std::memcpy(
        mapped_data,
        &data,
        sizeof(SpotLightBufferData)
    );

    vmaUnmapMemory(
        context.allocator,
        spot_light_buffer_allocation
    );
}

bool syncLightMarkers();
void updateLightMarkerBuffers();
void updateUniformBuffer()
{
    auto& context = graphics::internal::context;

    SceneUniformData scene_data = createSceneUniformData();

    void* mapped_data = nullptr;

    if (vmaMapMemory(
            context.allocator,
            scene_uniform_buffer_allocation,
            &mapped_data) != VK_SUCCESS) {
        return;
    }

    std::memcpy(
        mapped_data,
        &scene_data,
        sizeof(SceneUniformData)
    );

    vmaUnmapMemory(
        context.allocator,
        scene_uniform_buffer_allocation
    );

    ModelUniformData model_data = createModelUniformData();

    if (vmaMapMemory(
            context.allocator,
            uniform_buffer_allocation,
            &mapped_data) != VK_SUCCESS) {
        return;
    }

    std::memcpy(
        mapped_data,
        &model_data,
        sizeof(ModelUniformData)
    );

    vmaUnmapMemory(
        context.allocator,
        uniform_buffer_allocation
    );

    ModelUniformData model_data_2 = createModelUniformData();

    model_data_2.model = glm::translate(
        glm::mat4(1.0f),
        glm::vec3(2.0f, 0.0f, -2.0f)
    );

    if (vmaMapMemory(
            context.allocator,
            uniform_buffer_allocation_2,
            &mapped_data) != VK_SUCCESS) {
        return;
    }

    std::memcpy(
        mapped_data,
        &model_data_2,
        sizeof(ModelUniformData)
    );

    vmaUnmapMemory(
        context.allocator,
        uniform_buffer_allocation_2
    );

	updatePointLightBuffer();
	updateSpotLightBuffer();

	if (syncLightMarkers()) {
		updateLightMarkerBuffers();
	}
	updateLightLinesGeometry();
}

bool syncLightMarkers()
{
    auto& context = graphics::internal::context;

    size_t point_count = std::min(
        point_lights.size(),
        static_cast<size_t>(MAX_POINT_LIGHTS)
    );

    size_t spot_count = std::min(
        spot_lights.size(),
        static_cast<size_t>(MAX_SPOT_LIGHTS)
    );

    size_t required_count = point_count + spot_count;

    while (light_markers.size() < required_count) {
        LightMarker marker{};

        ModelUniformData data{};
        data.model = glm::mat4(1.0f);
        data.color = glm::vec4(1.0f);
        data.material_specular = glm::vec4(1.0f);

        if (!createBuffer(
                sizeof(ModelUniformData),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                &data,
                marker.uniform_buffer,
                marker.uniform_buffer_allocation)) {
            return false;
        }

        VkDescriptorSetAllocateInfo allocate_info{};
        allocate_info.sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocate_info.descriptorPool = descriptor_pool;
        allocate_info.descriptorSetCount = 1;
        allocate_info.pSetLayouts = &model_descriptor_set_layout;

        if (vkAllocateDescriptorSets(
                context.device,
                &allocate_info,
                &marker.descriptor_set) != VK_SUCCESS) {
            vmaDestroyBuffer(
                context.allocator,
                marker.uniform_buffer,
                marker.uniform_buffer_allocation
            );

            return false;
        }

        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = marker.uniform_buffer;
        buffer_info.offset = 0;
        buffer_info.range = sizeof(ModelUniformData);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = marker.descriptor_set;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo = &buffer_info;

        vkUpdateDescriptorSets(
            context.device,
            1,
            &write,
            0,
            nullptr
        );

        light_markers.push_back(marker);
    }

    return true;
}

void updateLightMarkerBuffers()
{
    auto& context = graphics::internal::context;

    size_t marker_index = 0;

    auto update_marker = [&](const glm::vec3& position,
                             const glm::vec4& color) {
        if (marker_index >= light_markers.size()) {
            return;
        }

        ModelUniformData data{};
        data.model = glm::translate(
            glm::mat4(1.0f),
            position
        );

        data.model = glm::scale(
            data.model,
            glm::vec3(0.15f)
        );

        data.color = glm::vec4(
            glm::clamp(glm::vec3(color), glm::vec3(0.0f), glm::vec3(1.0f)),
            1.0f
        );

        data.material_specular = glm::vec4(1.0f);

        LightMarker& marker = light_markers[marker_index];

        void* mapped_data = nullptr;

        if (vmaMapMemory(
                context.allocator,
                marker.uniform_buffer_allocation,
                &mapped_data) == VK_SUCCESS) {
            std::memcpy(
                mapped_data,
                &data,
                sizeof(ModelUniformData)
            );

            vmaUnmapMemory(
                context.allocator,
                marker.uniform_buffer_allocation
            );
        }

        ++marker_index;
    };

    size_t point_count = std::min(
        point_lights.size(),
        static_cast<size_t>(MAX_POINT_LIGHTS)
    );

    for (size_t i = 0; i < point_count; ++i) {
        update_marker(
            glm::vec3(point_lights[i].position),
            point_lights[i].color
        );
    }

    size_t spot_count = std::min(
        spot_lights.size(),
        static_cast<size_t>(MAX_SPOT_LIGHTS)
    );

    for (size_t i = 0; i < spot_count; ++i) {
        update_marker(
            glm::vec3(spot_lights[i].position),
            spot_lights[i].color
        );
    }
}


namespace application {
bool initialize() {

	auto& context = graphics::internal::context;

	auto cleanup = [&]() {

		if (graphics_pipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(
				context.device,
				graphics_pipeline,
				nullptr);

			graphics_pipeline = VK_NULL_HANDLE;
		}

		if (light_visualization_pipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(
				context.device,
				light_visualization_pipeline,
				nullptr);

			light_visualization_pipeline = VK_NULL_HANDLE;
		}

		if (pipeline_layout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(
				context.device,
				pipeline_layout,
				nullptr);

			pipeline_layout = VK_NULL_HANDLE;
		}

		if (vertex_shader != VK_NULL_HANDLE) {
			vkDestroyShaderModule(
				context.device,
				vertex_shader,
				nullptr);

			vertex_shader = VK_NULL_HANDLE;
		}

		if (fragment_shader != VK_NULL_HANDLE) {
			vkDestroyShaderModule(
				context.device,
				fragment_shader,
				nullptr);

			fragment_shader = VK_NULL_HANDLE;
		}

		if (light_visualization_fragment_shader != VK_NULL_HANDLE) {
			vkDestroyShaderModule(
				context.device,
				light_visualization_fragment_shader,
				nullptr
			);

			light_visualization_fragment_shader = VK_NULL_HANDLE;
		}

		
		if (light_lines_vertex_shader != VK_NULL_HANDLE) {
			vkDestroyShaderModule(
				context.device,
				light_lines_vertex_shader,
				nullptr);
			light_lines_vertex_shader = VK_NULL_HANDLE;
		}

		if (light_lines_fragment_shader != VK_NULL_HANDLE) {
			vkDestroyShaderModule(
				context.device,
				light_lines_fragment_shader,
				nullptr);
			light_lines_fragment_shader = VK_NULL_HANDLE;
		}

		if (light_lines_vertex_shader != VK_NULL_HANDLE) {
			vkDestroyShaderModule(
				context.device,
				light_lines_vertex_shader,
				nullptr);
			light_lines_vertex_shader = VK_NULL_HANDLE;
		}

		if (light_lines_fragment_shader != VK_NULL_HANDLE) {
			vkDestroyShaderModule(
				context.device,
				light_lines_fragment_shader,
				nullptr);
			light_lines_fragment_shader = VK_NULL_HANDLE;
		}


		if (vertex_buffer != VK_NULL_HANDLE) {
			vmaDestroyBuffer(
				context.allocator,
				vertex_buffer,
				vertex_buffer_allocation);

			vertex_buffer = VK_NULL_HANDLE;
			vertex_buffer_allocation = VK_NULL_HANDLE;
		}

		if (index_buffer != VK_NULL_HANDLE) {
			vmaDestroyBuffer(
				context.allocator,
				index_buffer,
				index_buffer_allocation);

			index_buffer = VK_NULL_HANDLE;
			index_buffer_allocation = VK_NULL_HANDLE;
		}

		if (light_sphere.vertex_buffer != VK_NULL_HANDLE) {
			vmaDestroyBuffer(
				context.allocator,
				light_sphere.vertex_buffer,
				light_sphere.vertex_buffer_allocation
			);

			light_sphere.vertex_buffer = VK_NULL_HANDLE;
			light_sphere.vertex_buffer_allocation = VK_NULL_HANDLE;
		}

		if (light_sphere.index_buffer != VK_NULL_HANDLE) {
			vmaDestroyBuffer(
				context.allocator,
				light_sphere.index_buffer,
				light_sphere.index_buffer_allocation
			);

			light_sphere.index_buffer = VK_NULL_HANDLE;
			light_sphere.index_buffer_allocation = VK_NULL_HANDLE;
		}

		if (scene_uniform_buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(
			context.allocator,
			scene_uniform_buffer,
			scene_uniform_buffer_allocation);

		scene_uniform_buffer = VK_NULL_HANDLE;
		scene_uniform_buffer_allocation = VK_NULL_HANDLE;
		}

		if (uniform_buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(
			context.allocator,
			uniform_buffer,
			uniform_buffer_allocation);

		uniform_buffer = VK_NULL_HANDLE;
		uniform_buffer_allocation = VK_NULL_HANDLE;
		}

		if (uniform_buffer_2 != VK_NULL_HANDLE) {
		vmaDestroyBuffer(
			context.allocator,
			uniform_buffer_2,
			uniform_buffer_allocation_2);

		uniform_buffer_2 = VK_NULL_HANDLE;
		uniform_buffer_allocation_2 = VK_NULL_HANDLE;
		}

		if (point_light_buffer != VK_NULL_HANDLE) {
			vmaDestroyBuffer(
				context.allocator,
				point_light_buffer,
				point_light_buffer_allocation);

			point_light_buffer = VK_NULL_HANDLE;
			point_light_buffer_allocation = VK_NULL_HANDLE;
		}

		if (spot_light_buffer != VK_NULL_HANDLE) {
			vmaDestroyBuffer(
				context.allocator,
				spot_light_buffer,
				spot_light_buffer_allocation
			);

			spot_light_buffer = VK_NULL_HANDLE;
			spot_light_buffer_allocation = VK_NULL_HANDLE;
		}
	};

	if (!createBuffer(
			sizeof(vertices),
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			vertices,
			vertex_buffer,
			vertex_buffer_allocation)) {

		std::cerr << "FAILED: vertex buffer\n";
		cleanup();
		return false;
	}

	if (!createBuffer(
			sizeof(indices),
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			indices,
			index_buffer,
			index_buffer_allocation)) {
		std::cerr << "FAILED: index buffer\n";
		cleanup();
		return false;
	}

	if (!createLightSphereGeometry()) {
		std::cerr << "FAILED: light sphere geometry\n";
		cleanup();
		return false;
	}
	
	SceneUniformData scene_data = createSceneUniformData();

	if (!createBuffer(
			sizeof(SceneUniformData),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			&scene_data,
			scene_uniform_buffer,
			scene_uniform_buffer_allocation)) {

		std::cerr << "FAILED: scene uniform buffer\n";
		cleanup();
		return false;
	}

	ModelUniformData model_data = createModelUniformData();

	model_data.model = glm::translate(
		model_data.model,
		glm::vec3(-2.0f, 0.0f, -2.0f)
	);

	if (!createBuffer(
			sizeof(ModelUniformData),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			&model_data,
			uniform_buffer,
			uniform_buffer_allocation)) {

		std::cerr << "FAILED: model uniform buffer\n";
		cleanup();
		return false;
	}

	ModelUniformData model_data_2 = createModelUniformData();

	model_data_2.model = glm::translate(
		model_data_2.model,
		glm::vec3(2.0f, 0.0f, -2.0f)
	);

	if (!createBuffer(
			sizeof(ModelUniformData),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			&model_data_2,
			uniform_buffer_2,
			uniform_buffer_allocation_2)) {

		std::cerr << "FAILED: model uniform buffer 2\n";
		cleanup();
		return false;
	}

	// создание SSBO для точечных источников света
	PointLightBufferData point_light_data =
		createPointLightBufferData();

	if (!createBuffer(
			sizeof(PointLightBufferData),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			&point_light_data,
			point_light_buffer,
			point_light_buffer_allocation)) {

		std::cerr << "FAILED: point light buffer\n";
		cleanup();
		return false;
	}

	SpotLightBufferData spot_light_data =
		createSpotLightBufferData();

	if (!createBuffer(
			sizeof(SpotLightBufferData),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			&spot_light_data,
			spot_light_buffer,
			spot_light_buffer_allocation)) {

		std::cerr << "FAILED: spot light buffer\n";
		cleanup();
		return false;
	}

	const auto vertex_shader_code =
    	readFile("../shaders/icosahedron.vert.spv");

	if (!createShaderModule(
			vertex_shader_code,
			vertex_shader)) {

		std::cerr << "FAILED: vertex shader\n";
		cleanup();
		return false;
	}

	const auto fragment_shader_code =
    	readFile("../shaders/icosahedron.frag.spv");

	if (!createShaderModule(
			fragment_shader_code,
			fragment_shader)) {

		std::cerr << "FAILED: fragment shader\n";
		cleanup();
		return false;
	}

	const auto light_visualization_fragment_shader_code =
		readFile("../shaders/light_visualization.frag.spv");

	if (!createShaderModule(
			light_visualization_fragment_shader_code,
			light_visualization_fragment_shader)) {
		std::cerr << "FAILED: light visualization fragment shader\n";
		cleanup();
		return false;
	}

	
	const auto light_lines_vertex_shader_code =
		readFile("../shaders/light_lines.vert.spv");

	if (!createShaderModule(
			light_lines_vertex_shader_code,
			light_lines_vertex_shader)) {
		std::cerr << "FAILED: light lines vertex shader\n";
		cleanup();
		return false;
	}

	const auto light_lines_fragment_shader_code =
		readFile("../shaders/light_lines.frag.spv");

	if (!createShaderModule(
			light_lines_fragment_shader_code,
			light_lines_fragment_shader)) {
		std::cerr << "FAILED: light lines fragment shader\n";
		cleanup();
		return false;
	}


	VkDescriptorSetLayoutBinding scene_bindings[3]{};

	scene_bindings[0].binding = 0;
	scene_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	scene_bindings[0].descriptorCount = 1;
	scene_bindings[0].stageFlags =
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	scene_bindings[1].binding = 1;
	scene_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	scene_bindings[1].descriptorCount = 1;
	scene_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	scene_bindings[2].binding = 2;
	scene_bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	scene_bindings[2].descriptorCount = 1;
	scene_bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo scene_layout_info{};
	scene_layout_info.sType =
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	scene_layout_info.bindingCount = 3;
	scene_layout_info.pBindings = scene_bindings;

	if (vkCreateDescriptorSetLayout(
			context.device,
			&scene_layout_info,
			nullptr,
			&descriptor_set_layout) != VK_SUCCESS) {

		std::cerr << "FAILED: scene descriptor set layout\n";
		cleanup();
		return false;
	}

	VkDescriptorSetLayoutBinding model_binding{};

	model_binding.binding = 0;
	model_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	model_binding.descriptorCount = 1;
	model_binding.stageFlags =
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo model_layout_info{};
	model_layout_info.sType =
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	model_layout_info.bindingCount = 1;
	model_layout_info.pBindings = &model_binding;

	if (vkCreateDescriptorSetLayout(
			context.device,
			&model_layout_info,
			nullptr,
			&model_descriptor_set_layout) != VK_SUCCESS) {

		std::cerr << "FAILED: model descriptor set layout\n";
		cleanup();
		return false;
	}

	VkDescriptorSetLayout pipeline_layouts[] = {
		descriptor_set_layout,
		model_descriptor_set_layout
	};

	VkPipelineLayoutCreateInfo pipeline_layout_info{};
	pipeline_layout_info.sType =
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = 2;
	pipeline_layout_info.pSetLayouts = pipeline_layouts;

	if (vkCreatePipelineLayout(
			context.device,
			&pipeline_layout_info,
			nullptr,
			&pipeline_layout) != VK_SUCCESS) {

		std::cerr << "FAILED: pipeline layout\n";
		cleanup();
		return false;
	}
	
	VkDescriptorPoolSize pool_sizes[2]{};

	pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	pool_sizes[0].descriptorCount = 64;

	pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	pool_sizes[1].descriptorCount = 2;

	VkDescriptorPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.maxSets = 64;
	pool_info.poolSizeCount = 2;
	pool_info.pPoolSizes = pool_sizes;

	if (vkCreateDescriptorPool(
			context.device,
			&pool_info,
			nullptr,
			&descriptor_pool) != VK_SUCCESS) {

		std::cerr << "FAILED: descriptor pool\n";
		cleanup();
		return false;
	}

	VkDescriptorSetLayout descriptor_set_layouts[3] = {
		descriptor_set_layout,
		model_descriptor_set_layout,
		model_descriptor_set_layout
	};

	VkDescriptorSet descriptor_sets[3]{};

	VkDescriptorSetAllocateInfo descriptor_set_allocate_info{};
	descriptor_set_allocate_info.sType =
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptor_set_allocate_info.descriptorPool = descriptor_pool;
	descriptor_set_allocate_info.descriptorSetCount = 3;
	descriptor_set_allocate_info.pSetLayouts = descriptor_set_layouts;

	if (vkAllocateDescriptorSets(
			context.device,
			&descriptor_set_allocate_info,
			descriptor_sets) != VK_SUCCESS) {

		std::cerr << "FAILED: descriptor sets\n";
		cleanup();
		return false;
	}

	scene_descriptor_set = descriptor_sets[0];
	descriptor_set = descriptor_sets[1];
	descriptor_set_2 = descriptor_sets[2];

	VkDescriptorBufferInfo scene_buffer_info{};
	scene_buffer_info.buffer = scene_uniform_buffer;
	scene_buffer_info.offset = 0;
	scene_buffer_info.range = sizeof(SceneUniformData);

	VkDescriptorBufferInfo model_buffer_info{};
	model_buffer_info.buffer = uniform_buffer;
	model_buffer_info.offset = 0;
	model_buffer_info.range = sizeof(ModelUniformData);

	VkDescriptorBufferInfo model_buffer_info_2{};
	model_buffer_info_2.buffer = uniform_buffer_2;
	model_buffer_info_2.offset = 0;
	model_buffer_info_2.range = sizeof(ModelUniformData);

	VkDescriptorBufferInfo point_light_buffer_info{};
	point_light_buffer_info.buffer = point_light_buffer;
	point_light_buffer_info.offset = 0;
	point_light_buffer_info.range = sizeof(PointLightBufferData);

	VkDescriptorBufferInfo spot_light_buffer_info{};
	spot_light_buffer_info.buffer = spot_light_buffer;
	spot_light_buffer_info.offset = 0;
	spot_light_buffer_info.range = sizeof(SpotLightBufferData);

	VkWriteDescriptorSet descriptor_writes[5]{};

	descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptor_writes[0].dstSet = scene_descriptor_set;
	descriptor_writes[0].dstBinding = 0;
	descriptor_writes[0].descriptorCount = 1;
	descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptor_writes[0].pBufferInfo = &scene_buffer_info;

	descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptor_writes[1].dstSet = scene_descriptor_set;
	descriptor_writes[1].dstBinding = 1;
	descriptor_writes[1].descriptorCount = 1;
	descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptor_writes[1].pBufferInfo = &point_light_buffer_info;

	descriptor_writes[2].sType =
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptor_writes[2].dstSet = scene_descriptor_set;
	descriptor_writes[2].dstBinding = 2;
	descriptor_writes[2].descriptorCount = 1;
	descriptor_writes[2].descriptorType =
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptor_writes[2].pBufferInfo =
		&spot_light_buffer_info;

	descriptor_writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptor_writes[3].dstSet = descriptor_set;
	descriptor_writes[3].dstBinding = 0;
	descriptor_writes[3].descriptorCount = 1;
	descriptor_writes[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptor_writes[3].pBufferInfo = &model_buffer_info;

	descriptor_writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptor_writes[4].dstSet = descriptor_set_2;
	descriptor_writes[4].dstBinding = 0;
	descriptor_writes[4].descriptorCount = 1;
	descriptor_writes[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptor_writes[4].pBufferInfo = &model_buffer_info_2;

	vkUpdateDescriptorSets(
		context.device,
		5,
		descriptor_writes,
		0,
		nullptr
	);

	if (!syncLightMarkers()) {
		std::cerr << "FAILED: light markers initialization\n";
		cleanup();
		return false;
	}

	updateLightMarkerBuffers();

	VkVertexInputBindingDescription binding_description{};
	binding_description.binding = 0;
	binding_description.stride = sizeof(Vertex);
	binding_description.inputRate =
		VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription attribute_descriptions[2]{};

	// атрибут 0: позиция вершины
	attribute_descriptions[0].binding = 0;
	attribute_descriptions[0].location = 0;
	attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attribute_descriptions[0].offset = offsetof(Vertex, x);

	// атрибут 1: нормаль вершины
	attribute_descriptions[1].binding = 0;
	attribute_descriptions[1].location = 1;
	attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attribute_descriptions[1].offset = offsetof(Vertex, nx);

	VkPipelineVertexInputStateCreateInfo vertex_input_info{};
	vertex_input_info.sType =
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	vertex_input_info.vertexBindingDescriptionCount = 1;
	vertex_input_info.pVertexBindingDescriptions =
		&binding_description;

	vertex_input_info.vertexAttributeDescriptionCount = 2;
	vertex_input_info.pVertexAttributeDescriptions =
		attribute_descriptions;

	VkPipelineInputAssemblyStateCreateInfo input_assembly_info{};
	input_assembly_info.sType =
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;

	input_assembly_info.topology =
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	input_assembly_info.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width =
		static_cast<float>(context.swapchain_extent.width);
	viewport.height =
		static_cast<float>(context.swapchain_extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = context.swapchain_extent;

	VkPipelineRasterizationStateCreateInfo rasterization_info{};
	rasterization_info.sType =
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

	rasterization_info.depthClampEnable = VK_FALSE;
	rasterization_info.rasterizerDiscardEnable = VK_FALSE;
	rasterization_info.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization_info.lineWidth = 1.0f;
	rasterization_info.cullMode = VK_CULL_MODE_NONE;
	rasterization_info.frontFace =
		VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterization_info.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampling_info{};
	multisampling_info.sType =
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;

	multisampling_info.sampleShadingEnable = VK_FALSE;
	multisampling_info.rasterizationSamples =
		VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo depth_stencil_info{};
	depth_stencil_info.sType =
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

	depth_stencil_info.depthTestEnable = VK_TRUE;
	depth_stencil_info.depthWriteEnable = VK_TRUE;
	depth_stencil_info.depthCompareOp = VK_COMPARE_OP_LESS;
	depth_stencil_info.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_info.stencilTestEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState color_blend_attachment{};
	color_blend_attachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;

	color_blend_attachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo color_blend_info{};
	color_blend_info.sType =
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

	color_blend_info.logicOpEnable = VK_FALSE;
	color_blend_info.attachmentCount = 1;
	color_blend_info.pAttachments =
		&color_blend_attachment;

	VkPipelineShaderStageCreateInfo shader_stages[] = {
		{
			.sType =
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vertex_shader,
			.pName = "main",
		},
		{
			.sType =
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = fragment_shader,
			.pName = "main",
		}
	};

	VkPipelineViewportStateCreateInfo viewport_info{};
	viewport_info.sType =
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;

	viewport_info.viewportCount = 1;
	viewport_info.pViewports = &viewport;
	viewport_info.scissorCount = 1;
	viewport_info.pScissors = &scissor;

	VkGraphicsPipelineCreateInfo pipeline_info{};
	pipeline_info.sType =
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

	pipeline_info.stageCount = 2;
	pipeline_info.pStages = shader_stages;

	pipeline_info.pVertexInputState =
		&vertex_input_info;

	pipeline_info.pInputAssemblyState =
		&input_assembly_info;

	pipeline_info.pViewportState =
		&viewport_info;

	pipeline_info.pRasterizationState =
		&rasterization_info;

	pipeline_info.pMultisampleState =
		&multisampling_info;

	pipeline_info.pDepthStencilState =
		&depth_stencil_info;

	pipeline_info.pColorBlendState =
		&color_blend_info;

	pipeline_info.layout = pipeline_layout;
	pipeline_info.renderPass = context.render_pass;
	pipeline_info.subpass = 0;

	if (vkCreateGraphicsPipelines(
			context.device,
			VK_NULL_HANDLE,
			1,
			&pipeline_info,
			nullptr,
			&graphics_pipeline) != VK_SUCCESS) {

		std::cerr << "FAILED: graphics pipeline\n";
		cleanup();
		return false;
	}

	VkPipelineShaderStageCreateInfo visualization_shader_stages[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vertex_shader,
			.pName = "main",
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = light_visualization_fragment_shader,
			.pName = "main",
		}
	};

	VkGraphicsPipelineCreateInfo visualization_pipeline_info = pipeline_info;
	visualization_pipeline_info.pStages = visualization_shader_stages;

	if (vkCreateGraphicsPipelines(
			context.device,
			VK_NULL_HANDLE,
			1,
			&visualization_pipeline_info,
			nullptr,
			&light_visualization_pipeline) != VK_SUCCESS) {

		std::cerr << "FAILED: light visualization pipeline\n";
		cleanup();
		return false;
	}

	VkPipelineShaderStageCreateInfo line_shader_stages[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = light_lines_vertex_shader,
			.pName = "main",
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = light_lines_fragment_shader,
			.pName = "main",
		}
	};

	VkVertexInputBindingDescription line_binding_description{};
	line_binding_description.binding = 0;
	line_binding_description.stride = sizeof(glm::vec3);
	line_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription line_attribute_description{};
	line_attribute_description.binding = 0;
	line_attribute_description.location = 0;
	line_attribute_description.format = VK_FORMAT_R32G32B32_SFLOAT;
	line_attribute_description.offset = 0;

	VkPipelineVertexInputStateCreateInfo line_vertex_input_info{};
	line_vertex_input_info.sType =
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	line_vertex_input_info.vertexBindingDescriptionCount = 1;
	line_vertex_input_info.pVertexBindingDescriptions =
		&line_binding_description;
	line_vertex_input_info.vertexAttributeDescriptionCount = 1;
	line_vertex_input_info.pVertexAttributeDescriptions =
		&line_attribute_description;

	VkPipelineInputAssemblyStateCreateInfo line_input_assembly_info{};
	line_input_assembly_info.sType =
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	line_input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	line_input_assembly_info.primitiveRestartEnable = VK_FALSE;

	VkGraphicsPipelineCreateInfo line_pipeline_info = pipeline_info;
	line_pipeline_info.pStages = line_shader_stages;
	line_pipeline_info.pVertexInputState = &line_vertex_input_info;
	line_pipeline_info.pInputAssemblyState = &line_input_assembly_info;

	if (vkCreateGraphicsPipelines(
			context.device,
			VK_NULL_HANDLE,
			1,
			&line_pipeline_info,
			nullptr,
			&light_lines_pipeline) != VK_SUCCESS) {
		std::cerr << "FAILED: light lines pipeline\n";
		cleanup();
		return false;
	}


	return true;
}

void shutdown() {

    auto& context = graphics::internal::context;

    vkQueueWaitIdle(context.graphics_queue);

    vkDestroyPipeline(
        context.device,
        graphics_pipeline,
        nullptr);

	if (light_visualization_pipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(
			context.device,
			light_visualization_pipeline,
			nullptr);

		light_visualization_pipeline = VK_NULL_HANDLE;
	}

    vkDestroyPipelineLayout(
        context.device,
        pipeline_layout,
        nullptr);

    vmaDestroyBuffer(
        context.allocator,
        vertex_buffer,
        vertex_buffer_allocation);

    vmaDestroyBuffer(
        context.allocator,
        index_buffer,
        index_buffer_allocation);
	
	if (light_sphere.vertex_buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(
			context.allocator,
			light_sphere.vertex_buffer,
			light_sphere.vertex_buffer_allocation
		);

		light_sphere.vertex_buffer = VK_NULL_HANDLE;
		light_sphere.vertex_buffer_allocation = VK_NULL_HANDLE;
	}

	if (light_sphere.index_buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(
			context.allocator,
			light_sphere.index_buffer,
			light_sphere.index_buffer_allocation
		);

		light_sphere.index_buffer = VK_NULL_HANDLE;
		light_sphere.index_buffer_allocation = VK_NULL_HANDLE;
	}

    vmaDestroyBuffer(
        context.allocator,
        uniform_buffer,
        uniform_buffer_allocation);

    vmaDestroyBuffer(
        context.allocator,
        uniform_buffer_2,
        uniform_buffer_allocation_2);

	if (point_light_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(
            context.allocator,
            point_light_buffer,
            point_light_buffer_allocation
        );

        point_light_buffer = VK_NULL_HANDLE;
        point_light_buffer_allocation = VK_NULL_HANDLE;
    }

	if (spot_light_buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(
			context.allocator,
			spot_light_buffer,
			spot_light_buffer_allocation
		);

		spot_light_buffer = VK_NULL_HANDLE;
		spot_light_buffer_allocation = VK_NULL_HANDLE;
	}

    if (descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(
            context.device,
            descriptor_pool,
            nullptr);
        descriptor_pool = VK_NULL_HANDLE;
    }

    if (descriptor_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(
            context.device,
            descriptor_set_layout,
            nullptr);
        descriptor_set_layout = VK_NULL_HANDLE;
    }

	if (model_descriptor_set_layout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(
			context.device,
			model_descriptor_set_layout,
			nullptr);

		model_descriptor_set_layout = VK_NULL_HANDLE;
	}

    vkDestroyShaderModule(
        context.device,
        vertex_shader,
        nullptr);

    vkDestroyShaderModule(
        context.device,
        fragment_shader,
        nullptr);

	if (light_visualization_fragment_shader != VK_NULL_HANDLE) {
		vkDestroyShaderModule(
			context.device,
			light_visualization_fragment_shader,
			nullptr
		);

		light_visualization_fragment_shader = VK_NULL_HANDLE;
	}

	
	if (light_lines_vertex_buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(
			context.allocator,
			light_lines_vertex_buffer,
			light_lines_vertex_buffer_allocation);

		light_lines_vertex_buffer = VK_NULL_HANDLE;
		light_lines_vertex_buffer_allocation = VK_NULL_HANDLE;
	}

	if (light_lines_pipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(
			context.device,
			light_lines_pipeline,
			nullptr);

		light_lines_pipeline = VK_NULL_HANDLE;
	}

	if (light_lines_vertex_shader != VK_NULL_HANDLE) {
		vkDestroyShaderModule(
			context.device,
			light_lines_vertex_shader,
			nullptr);

		light_lines_vertex_shader = VK_NULL_HANDLE;
	}

	if (light_lines_fragment_shader != VK_NULL_HANDLE) {
		vkDestroyShaderModule(
			context.device,
			light_lines_fragment_shader,
			nullptr);

		light_lines_fragment_shader = VK_NULL_HANDLE;
	}

}

void update(double time)
{
    static double previous_time = time;
    static float animation_time = 0.0f;

    float delta_time =
        static_cast<float>(time - previous_time);

    previous_time = time;

    ImGui::Begin("Icosahedron");

	if (ImGui::CollapsingHeader(
			"Camera",
			ImGuiTreeNodeFlags_DefaultOpen)) {

		ImGui::Text("Position");

		ImGui::SliderFloat3(
			"Camera position",
			&camera_position.x,
			-10.0f,
			10.0f
		);

		ImGui::Text("Rotation");

		ImGui::SliderFloat(
			"Yaw",
			&camera_yaw,
			-180.0f,
			180.0f
		);

		ImGui::SliderFloat(
			"Pitch",
			&camera_pitch,
			-89.0f,
			89.0f
		);

		ImGui::SliderFloat(
			"Field of view",
			&camera_fov,
			20.0f,
			100.0f
		);

		ImGui::Separator();
    }

    // направленный источник света
    if (ImGui::CollapsingHeader("Directional Light")) {
        ImGui::SliderFloat3(
            "Light direction",
            &light_direction.x,
            -1.0f,
            1.0f
        );

        ImGui::ColorEdit3(
            "Light color",
            &light_color.x
        );

        ImGui::SliderFloat(
            "Light intensity",
            &light_intensity,
            0.0f,
            5.0f
        );
    }

    // фоновое освещение
    if (ImGui::CollapsingHeader("Ambient Light")) {
        ImGui::ColorEdit3(
            "Ambient color",
            &ambient_color.x
        );

        ImGui::SliderFloat(
            "Ambient intensity",
            &ambient_intensity,
            0.0f,
            1.0f
        );
    }

    // материал объекта
    if (ImGui::CollapsingHeader("Material")) {
        ImGui::ColorEdit3(
            "Specular color",
            &material_specular.x
        );

        ImGui::SliderFloat(
            "Shininess",
            &material_shininess,
            1.0f,
            256.0f
        );
    }

	if (ImGui::CollapsingHeader("Point Lights")) {
		ImGui::Text(
			"Active point lights: %d / %d",
			static_cast<int>(point_lights.size()),
			static_cast<int>(MAX_POINT_LIGHTS)
		);

		if (point_lights.size() < MAX_POINT_LIGHTS) {
			if (ImGui::Button("Add point light")) {
				point_lights.push_back({
					glm::vec4(0.0f, 2.0f, 0.0f, 1.0f),
					glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
				});
			}
		}

		for (size_t i = 0; i < point_lights.size(); ++i) {
			ImGui::PushID(static_cast<int>(i));

			std::string label =
				"Point light " + std::to_string(i + 1);

			if (ImGui::TreeNode(label.c_str())) {
				ImGui::SliderFloat3(
					"Position",
					&point_lights[i].position.x,
					-10.0f,
					10.0f
				);

				ImGui::ColorEdit3(
					"Color",
					&point_lights[i].color.x
				);

				ImGui::SliderFloat(
					"Intensity",
					&point_lights[i].color.w,
					0.0f,
					10.0f
				);

				if (ImGui::Button("Remove point light")) {
					point_lights.erase(point_lights.begin() + i);
					ImGui::TreePop();
					ImGui::PopID();
					break;
				}

				ImGui::TreePop();
			}

			ImGui::PopID();
		}
	}

	if (ImGui::CollapsingHeader("Spot Lights")) {
		ImGui::Text(
			"Active spot lights: %d / %d",
			static_cast<int>(spot_lights.size()),
			static_cast<int>(MAX_SPOT_LIGHTS)
		);

		if (spot_lights.size() < MAX_SPOT_LIGHTS) {
			if (ImGui::Button("Add spot light")) {
				spot_lights.push_back({
					glm::vec4(0.0f, 3.0f, 2.0f, 1.0f),
					glm::vec4(0.0f, -1.0f, -1.0f, 0.0f),
					glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
					glm::vec4(20.0f, 30.0f, 0.0f, 0.0f)
				});
			}
		}

		for (size_t i = 0; i < spot_lights.size(); ++i) {
			ImGui::PushID(static_cast<int>(i));

			std::string label =
				"Spot light " + std::to_string(i + 1);

			if (ImGui::TreeNode(label.c_str())) {
				ImGui::SliderFloat3(
					"Position",
					&spot_lights[i].position.x,
					-10.0f,
					10.0f
				);

				ImGui::SliderFloat3(
					"Direction",
					&spot_lights[i].direction.x,
					-1.0f,
					1.0f
				);

				ImGui::ColorEdit3(
					"Color",
					&spot_lights[i].color.x
				);

				ImGui::SliderFloat(
					"Intensity",
					&spot_lights[i].color.w,
					0.0f,
					10.0f
				);

				ImGui::SliderFloat(
					"Inner angle",
					&spot_lights[i].cone_angles.x,
					1.0f,
					88.0f
				);

				ImGui::SliderFloat(
					"Outer angle",
					&spot_lights[i].cone_angles.y,
					2.0f,
					89.0f
				);

				if (spot_lights[i].cone_angles.y <=
					spot_lights[i].cone_angles.x) {
					spot_lights[i].cone_angles.y =
						spot_lights[i].cone_angles.x + 1.0f;
				}

				if (glm::length(
						glm::vec3(spot_lights[i].direction)) < 0.001f) {
					spot_lights[i].direction =
						glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);
				}

				if (ImGui::Button("Remove spot light")) {
					spot_lights.erase(spot_lights.begin() + i);
					ImGui::TreePop();
					ImGui::PopID();
					break;
				}

				ImGui::TreePop();
			}

			ImGui::PopID();
		}
	}

	if (ImGui::CollapsingHeader("Light Visualization")) {
		ImGui::Checkbox(
			"Show light sources",
			&show_light_visualization
		);
	}


    ImGui::Separator();

    ImGui::Text("Projection");

    if (ImGui::RadioButton(
            "Perspective",
            perspective_projection)) {
        perspective_projection = true;
    }

    ImGui::SameLine();

    if (ImGui::RadioButton(
            "Orthographic",
            !perspective_projection)) {
        perspective_projection = false;
    }

    ImGui::Separator();

    ImGui::Text("Position");

    ImGui::SliderFloat3(
        "##position",
        &object_position.x,
        -5.0f,
        5.0f);

    ImGui::Text("Rotation");

    ImGui::SliderFloat3(
        "##rotation",
        &object_rotation.x,
        -180.0f,
        180.0f);

    ImGui::Text("Scale");

    ImGui::SliderFloat3(
        "##scale",
        &object_scale.x,
        0.1f,
        3.0f);

	ImGui::Text("Color");

	ImGui::ColorEdit3(
		"##color",
		&object_color.x);

    ImGui::Separator();

    ImGui::Text("Animation");

    if (ImGui::Button(
            animation_playing ? "Pause" : "Play")) {
        animation_playing = !animation_playing;
    }

    ImGui::SliderFloat(
        "Speed",
        &animation_speed,
        0.1f,
        5.0f);

    ImGui::SliderFloat(
        "Radius",
        &trajectory_radius,
        0.5f,
        5.0f);

    if (animation_playing) {
        animation_time +=
            delta_time * animation_speed;

        object_position.x =
            trajectory_radius *
            std::cos(animation_time);

        object_position.z =
            trajectory_radius *
            std::sin(animation_time);

        object_position.y =
            0.5f *
            std::sin(animation_time * 2.0f);

        object_rotation.y =
            animation_time * 60.0f;
    }

    updateUniformBuffer();

    ImGui::End();

    ImGui::ShowDemoWindow();
}

void render(const graphics::internal::FrameData& fd)
{
    const VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };

    if (vkBeginCommandBuffer(
            fd.command_buffer,
            &begin_info) != VK_SUCCESS) {
        return;
    }

	VkClearValue clear_values[2]{};

	clear_values[0].color = {
		0.0f, 0.0f, 0.0f, 1.0f
	};

	clear_values[1].depthStencil = {
		1.0f, 0
	};

	const VkRenderPassBeginInfo render_pass_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = graphics::internal::context.render_pass,
		.framebuffer = fd.framebuffer,
		.renderArea = {
			{0, 0},
			graphics::internal::context.swapchain_extent
		},
		.clearValueCount = 2,
		.pClearValues = clear_values,
	};

	vkCmdBeginRenderPass(
		fd.command_buffer,
		&render_pass_info,
		VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(
		fd.command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		graphics_pipeline);

	
	VkDescriptorSet descriptor_sets_first[] = {
		scene_descriptor_set,
		descriptor_set
	};

	vkCmdBindDescriptorSets(
		fd.command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipeline_layout,
		0,
		2,
		descriptor_sets_first,
		0,
		nullptr
	);

	const VkDeviceSize offset = 0;

	vkCmdBindVertexBuffers(
		fd.command_buffer,
		0,
		1,
		&vertex_buffer,
		&offset);

	vkCmdBindIndexBuffer(
		fd.command_buffer,
		index_buffer,
		0,
		VK_INDEX_TYPE_UINT32);

	vkCmdDrawIndexed(
		fd.command_buffer,
		60,
		1,
		0,
		0,
		0);

	
	VkDescriptorSet descriptor_sets_second[] = {
		scene_descriptor_set,
		descriptor_set_2
	};

	vkCmdBindDescriptorSets(
		fd.command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipeline_layout,
		0,
		2,
		descriptor_sets_second,
		0,
		nullptr);


	vkCmdDrawIndexed(
		fd.command_buffer,
		60,
		1,
		0,
		0,
		0);

	if (show_light_visualization) {
		vkCmdBindPipeline(
			fd.command_buffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			light_visualization_pipeline
		);

		VkDeviceSize sphere_offset = 0;

		vkCmdBindVertexBuffers(
			fd.command_buffer,
			0,
			1,
			&light_sphere.vertex_buffer,
			&sphere_offset
		);

		vkCmdBindIndexBuffer(
			fd.command_buffer,
			light_sphere.index_buffer,
			0,
			VK_INDEX_TYPE_UINT32
		);

		size_t point_count = std::min(
			point_lights.size(),
			static_cast<size_t>(MAX_POINT_LIGHTS)
		);

		size_t spot_count = std::min(
			spot_lights.size(),
			static_cast<size_t>(MAX_SPOT_LIGHTS)
		);

		size_t marker_count = point_count + spot_count;

		for (size_t i = 0;
			i < marker_count && i < light_markers.size();
			++i) {
			VkDescriptorSet marker_descriptor_sets[] = {
				scene_descriptor_set,
				light_markers[i].descriptor_set
			};

			vkCmdBindDescriptorSets(
				fd.command_buffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipeline_layout,
				0,
				2,
				marker_descriptor_sets,
				0,
				nullptr
			);

			vkCmdDrawIndexed(
				fd.command_buffer,
				light_sphere.index_count,
				1,
				0,
				0,
				0
			);
		}
	}

		
	if (show_light_visualization && light_lines_vertex_count > 0) {
		vkCmdBindPipeline(
			fd.command_buffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			light_lines_pipeline);

		VkDeviceSize line_offset = 0;

		vkCmdBindVertexBuffers(
			fd.command_buffer,
			0,
			1,
			&light_lines_vertex_buffer,
			&line_offset);

		vkCmdDraw(
			fd.command_buffer,
			light_lines_vertex_count,
			1,
			0,
			0);
	}


	vkCmdEndRenderPass(fd.command_buffer);

	vkEndCommandBuffer(fd.command_buffer);
}

} // namespace application