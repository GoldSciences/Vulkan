// Vulkan Example - Animated gears using multiple uniform buffers
// 
// See readme.md for details
// 
// Copyright (C) 2015 by Sascha Willems - www.saschawillems.de
// 
// This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
#pragma once
#include "VulkanDevice.hpp"
#include "vertex.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

typedef vks::VertexPNC										Vertex;		// Vertex layout used in this example

struct GearInfo {
	float														innerRadius;
	float														outerRadius;
	float														width;
	int															numTeeth;
	float														toothDepth;
	glm::vec3													color;
	glm::vec3													position;
	float														rotSpeed;
	float														rotOffset;
};

class VulkanGear
{
private:
	struct UBO {
		glm::mat4													projection;
		glm::mat4													model;
		glm::mat4													normal;
		glm::mat4													view;
		glm::vec3													lightPos;
	};

	vks::VulkanDevice											* vulkanDevice				= nullptr;

	glm::vec3													color;
	glm::vec3													position;
	float														rotSpeed;
	float														rotOffset;

	vks::Buffer													vertexBuffer;
	vks::Buffer													indexBuffer;
	uint32_t													indexCount;

	UBO ubo;
	vks::Buffer													uniformBuffer;

	int32_t														newVertex					(std::vector<Vertex> *vBuffer, float x, float y, float z, const glm::vec3& normal);
	void														newFace						(std::vector<uint32_t> *iBuffer, int a, int b, int c);

	void														prepareUniformBuffer		();
public:
	VkDescriptorSet												descriptorSet				= VK_NULL_HANDLE;

																VulkanGear					(vks::VulkanDevice *vulkanDevice)														: vulkanDevice(vulkanDevice) {};
																~VulkanGear					();

	void														draw						(VkCommandBuffer cmdbuffer, VkPipelineLayout pipelineLayout);
	void														updateUniformBuffer			(glm::mat4 perspective, glm::vec3 rotation, float zoom, float timer);
	void														setupDescriptorSet			(VkDescriptorPool pool, VkDescriptorSetLayout descriptorSetLayout);
	void														generate					(GearInfo *gearinfo, VkQueue queue);

};

