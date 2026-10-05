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

struct Vertex {
	float x;
	float y;
	float z;
};

struct UniformData {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec4 color;
};
// геометрия вершин
const float phi = (1.0f + std::sqrt(5.0f)) / 2.0f;

const Vertex vertices[] = {
	{ 0.0f,  1.0f,  phi},
	{ 0.0f, -1.0f,  phi},
	{ 0.0f,  1.0f, -phi},
	{ 0.0f, -1.0f, -phi},

	{ 1.0f,  phi, 0.0f},
	{-1.0f,  phi, 0.0f},
	{ 1.0f, -phi, 0.0f},
	{-1.0f, -phi, 0.0f},

	{ phi, 0.0f,  1.0f},
	{ phi, 0.0f, -1.0f},
	{-phi, 0.0f,  1.0f},
	{-phi, 0.0f, -1.0f}
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

VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

VkPipeline graphics_pipeline = VK_NULL_HANDLE;

VkBuffer uniform_buffer = VK_NULL_HANDLE;
VmaAllocation uniform_buffer_allocation = VK_NULL_HANDLE;

VkBuffer uniform_buffer_2 = VK_NULL_HANDLE;
VmaAllocation uniform_buffer_allocation_2 = VK_NULL_HANDLE;

VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
VkDescriptorSet descriptor_set = VK_NULL_HANDLE;

VkDescriptorSet descriptor_set_2 = VK_NULL_HANDLE;

glm::vec3 object_position = glm::vec3(0.0f);
glm::vec3 object_rotation = glm::vec3(0.0f);
glm::vec3 object_scale = glm::vec3(1.0f);
glm::vec3 object_color = glm::vec3(0.2f, 0.55f, 1.0f);

bool perspective_projection = true;

bool animation_playing = true;
float animation_speed = 1.0f;
float trajectory_radius = 2.0f;

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

UniformData createUniformData()
{
    UniformData data{};

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

    data.view = glm::lookAt(
        glm::vec3(0.0f, 0.0f, 5.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
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
			glm::radians(45.0f),
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
			0.1f,
			100.0f
		);
	}

	data.color = glm::vec4(object_color, 1.0f);

	data.projection[1][1] *= -1.0f;

    return data;
}

void updateUniformBuffer()
{
    UniformData data = createUniformData();

    void* mapped_data = nullptr;

    if (vmaMapMemory(
            graphics::internal::context.allocator,
            uniform_buffer_allocation,
            &mapped_data) != VK_SUCCESS) {
        return;
    }

    std::memcpy(
        mapped_data,
        &data,
        sizeof(UniformData));

    vmaUnmapMemory(
        graphics::internal::context.allocator,
        uniform_buffer_allocation);
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

	UniformData uniform_data = createUniformData();
	uniform_data.model =
		glm::translate(
			uniform_data.model,
			glm::vec3(-2.0f, 0.0f, -2.0f));

	if (!createBuffer(
			sizeof(UniformData),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			&uniform_data,
			uniform_buffer,
			uniform_buffer_allocation)) {

		std::cerr << "FAILED: uniform buffer\n";
		cleanup();
		return false;
	}

	UniformData uniform_data_2 = createUniformData();

	uniform_data_2.model =
		glm::translate(
			uniform_data_2.model,
			glm::vec3(2.0f, 0.0f, -2.0f));

	if (!createBuffer(
			sizeof(UniformData),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			&uniform_data_2,
			uniform_buffer_2,
			uniform_buffer_allocation_2)) {
		std::cerr << "FAILED: uniform buffer 2\n";
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

	VkDescriptorSetLayoutBinding uniform_binding{};
	uniform_binding.binding = 0;
	uniform_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uniform_binding.descriptorCount = 1;
	uniform_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo descriptor_layout_info{};
	descriptor_layout_info.sType =
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptor_layout_info.bindingCount = 1;
	descriptor_layout_info.pBindings = &uniform_binding;

	if (vkCreateDescriptorSetLayout(
			context.device,
			&descriptor_layout_info,
			nullptr,
			&descriptor_set_layout) != VK_SUCCESS) {

		std::cerr << "FAILED: descriptor set layout\n";
		cleanup();
		return false;
	}

	VkPipelineLayoutCreateInfo pipeline_layout_info{};
	pipeline_layout_info.sType =
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	pipeline_layout_info.setLayoutCount = 1;
	pipeline_layout_info.pSetLayouts = &descriptor_set_layout;

	if (vkCreatePipelineLayout(
			context.device,
			&pipeline_layout_info,
			nullptr,
			&pipeline_layout) != VK_SUCCESS) {

		std::cerr << "FAILED: pipeline layout\n";
		cleanup();
		return false;
	}

	VkDescriptorPoolSize pool_size{};
	pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	pool_size.descriptorCount = 2;

	VkDescriptorPoolCreateInfo pool_info{};
	pool_info.sType =
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.maxSets = 2;
	pool_info.poolSizeCount = 1;
	pool_info.pPoolSizes = &pool_size;

	if (vkCreateDescriptorPool(
			context.device,
			&pool_info,
			nullptr,
			&descriptor_pool) != VK_SUCCESS) {

		std::cerr << "FAILED: descriptor pool\n";
		cleanup();
		return false;
	}

	VkDescriptorSet descriptor_sets[2] = {
		descriptor_set,
		descriptor_set_2
	};

	VkDescriptorSetLayout descriptor_set_layouts[2] = {
		descriptor_set_layout,
		descriptor_set_layout
	};

	VkDescriptorSetAllocateInfo descriptor_set_allocate_info{};
	descriptor_set_allocate_info.sType =
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptor_set_allocate_info.descriptorPool = descriptor_pool;
	descriptor_set_allocate_info.descriptorSetCount = 2;
	descriptor_set_allocate_info.pSetLayouts = descriptor_set_layouts;

	if (vkAllocateDescriptorSets(
			context.device,
			&descriptor_set_allocate_info,
			descriptor_sets) != VK_SUCCESS) {
		std::cerr << "FAILED: descriptor sets\n";
		cleanup();
		return false;
	}

	descriptor_set = descriptor_sets[0];
	descriptor_set_2 = descriptor_sets[1];

	VkDescriptorBufferInfo buffer_info{};
	buffer_info.buffer = uniform_buffer;
	buffer_info.offset = 0;
	buffer_info.range = sizeof(UniformData);

	VkWriteDescriptorSet descriptor_write{};
	descriptor_write.sType =
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptor_write.dstSet = descriptor_set;
	descriptor_write.dstBinding = 0;
	descriptor_write.dstArrayElement = 0;
	descriptor_write.descriptorType =
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptor_write.descriptorCount = 1;
	descriptor_write.pBufferInfo = &buffer_info;

	vkUpdateDescriptorSets(
		context.device,
		1,
		&descriptor_write,
		0,
		nullptr);


	VkDescriptorBufferInfo buffer_info_2{};
	buffer_info_2.buffer = uniform_buffer_2;
	buffer_info_2.offset = 0;
	buffer_info_2.range = sizeof(UniformData);

	VkWriteDescriptorSet descriptor_write_2{};
	descriptor_write_2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptor_write_2.dstSet = descriptor_set_2;
	descriptor_write_2.dstBinding = 0;
	descriptor_write_2.dstArrayElement = 0;
	descriptor_write_2.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptor_write_2.descriptorCount = 1;
	descriptor_write_2.pBufferInfo = &buffer_info_2;

	vkUpdateDescriptorSets(
		context.device,
		1,
		&descriptor_write_2,
		0,
		nullptr);

	VkVertexInputBindingDescription binding_description{};
	binding_description.binding = 0;
	binding_description.stride = sizeof(Vertex);
	binding_description.inputRate =
		VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription attribute_description{};
	attribute_description.binding = 0;
	attribute_description.location = 0;
	attribute_description.format =
		VK_FORMAT_R32G32B32_SFLOAT;
	attribute_description.offset = offsetof(Vertex, x);

	VkPipelineVertexInputStateCreateInfo vertex_input_info{};
	vertex_input_info.sType =
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	vertex_input_info.vertexBindingDescriptionCount = 1;
	vertex_input_info.pVertexBindingDescriptions =
		&binding_description;

	vertex_input_info.vertexAttributeDescriptionCount = 1;
	vertex_input_info.pVertexAttributeDescriptions =
		&attribute_description;

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

	return true;
}

void shutdown() {

    auto& context = graphics::internal::context;

    vkQueueWaitIdle(context.graphics_queue);

    vkDestroyPipeline(
        context.device,
        graphics_pipeline,
        nullptr);

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

    vmaDestroyBuffer(
        context.allocator,
        uniform_buffer,
        uniform_buffer_allocation);

    vmaDestroyBuffer(
        context.allocator,
        uniform_buffer_2,
        uniform_buffer_allocation_2);

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

    vkDestroyShaderModule(
        context.device,
        vertex_shader,
        nullptr);

    vkDestroyShaderModule(
        context.device,
        fragment_shader,
        nullptr);
}

void update(double time)
{
    static double previous_time = time;
    static float animation_time = 0.0f;

    float delta_time =
        static_cast<float>(time - previous_time);

    previous_time = time;

    ImGui::Begin("Icosahedron");

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

	vkCmdBindDescriptorSets(
		fd.command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipeline_layout,
		0,
		1,
		&descriptor_set,
		0,
		nullptr);

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

	vkCmdBindDescriptorSets(
		fd.command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipeline_layout,
		0,
		1,
		&descriptor_set_2,
		0,
		nullptr);

	vkCmdDrawIndexed(
		fd.command_buffer,
		60,
		1,
		0,
		0,
		0);

	vkCmdEndRenderPass(fd.command_buffer);

	vkEndCommandBuffer(fd.command_buffer);
}

} // namespace application