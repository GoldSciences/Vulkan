/*
* Vulkan Example - Text overlay rendering on-top of an existing scene using a separate render pass
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <sstream>
#include <iomanip>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <vulkan/vulkan.h>

#include "vulkanexamplebase.h"
#include "VulkanDevice.hpp"
#include "VulkanBuffer.hpp"
#include "VulkanModel.hpp"
#include "VulkanTexture.hpp"

#include "../external/stb/stb_font_consolas_24_latin1.inl"

#define VERTEX_BUFFER_BIND_ID 0
#define ENABLE_VALIDATION false

// Defines for the STB font used
// STB font files can be found at http://nothings.org/stb/font/
#define STB_FONT_NAME stb_font_consolas_24_latin1
#define STB_FONT_WIDTH STB_FONT_consolas_24_latin1_BITMAP_WIDTH
#define STB_FONT_HEIGHT STB_FONT_consolas_24_latin1_BITMAP_HEIGHT 
#define STB_FIRST_CHAR STB_FONT_consolas_24_latin1_FIRST_CHAR
#define STB_NUM_CHARS STB_FONT_consolas_24_latin1_NUM_CHARS

// Max. number of chars the text overlay buffer can hold
#define TEXTOVERLAY_MAX_CHAR_COUNT 2048

class VulkanExample : public VulkanExampleBase
{
public:
	VulkanTextOverlay									* textOverlay				= nullptr;

	// Vertex layout for the models
	vks::VertexLayout									vertexLayout				= vks::VertexLayout({
		vks::VERTEX_COMPONENT_POSITION,
		vks::VERTEX_COMPONENT_NORMAL,
		vks::VERTEX_COMPONENT_UV,
		vks::VERTEX_COMPONENT_COLOR,
	});

	struct {
		vks::Texture2D										background;
		vks::Texture2D										cube;
	} textures;

	struct {
		vks::Model											cube;
	} models;

	struct {
		VkPipelineVertexInputStateCreateInfo				inputState;
		std::vector<VkVertexInputBindingDescription>		bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription>		attributeDescriptions;
	} vertices;

	vks::Buffer											uniformBuffer;

	struct UBOVS {
		glm::mat4											projection;
		glm::mat4											model;
		glm::vec4											lightPos					= glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	} uboVS;

	struct {
		VkPipeline											solid;
		VkPipeline											background;
	} pipelines;

	VkPipelineLayout									pipelineLayout;
	VkDescriptorSetLayout								descriptorSetLayout;

	struct {
		VkDescriptorSet										background;
		VkDescriptorSet										cube;
	} descriptorSets;


														VulkanExample				()	: VulkanExampleBase(ENABLE_VALIDATION)
	{
		zoom = -4.5f;
		zoomSpeed = 2.5f;
		rotation = { -25.0f, 0.0f, 0.0f };
		title = "Vulkan Example - Text overlay";
		// Disable text overlay of the example base class
		enableTextOverlay = false;
	}

														~VulkanExample				()								{
		vkDestroyPipeline(device, pipelines.solid, nullptr);
		vkDestroyPipeline(device, pipelines.background, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		models.cube.destroy();
		textures.background.destroy();
		textures.cube.destroy();
		uniformBuffer.destroy();
		delete(textOverlay);
	}

	void												buildCommandBuffers			()								{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VkClearValue clearValues[3];

		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

		for (size_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			// Set target frame buffer
			renderPassBeginInfo.framebuffer = frameBuffers[i];

			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets.background, 0, NULL);

			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &models.cube.vertices.buffer, offsets);
			vkCmdBindIndexBuffer(drawCmdBuffers[i], models.cube.indices.buffer, 0, VK_INDEX_TYPE_UINT32);

			// Background
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.background);
			vkCmdDraw(drawCmdBuffers[i], 4, 1, 0, 0);		// Vertices are generated by the vertex shader

			// Cube
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.solid);
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets.cube, 0, NULL);
			vkCmdDrawIndexed(drawCmdBuffers[i], models.cube.indexCount, 1, 0, 0, 0);

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}

		vkQueueWaitIdle(queue);
	}

	// Update the text buffer displayed by the text overlay
	void												updateTextOverlay			()								{
		textOverlay->beginTextUpdate();

		textOverlay->addText(title, 5.0f, 5.0f, VulkanTextOverlay::alignLeft);

		std::stringstream ss;
		ss << std::fixed << std::setprecision(2) << (frameTimer * 1000.0f) << "ms (" << lastFPS << " fps)";
		textOverlay->addText(ss.str(), 5.0f, 25.0f, VulkanTextOverlay::alignLeft);

		textOverlay->addText(deviceProperties.deviceName, 5.0f, 45.0f, VulkanTextOverlay::alignLeft);
		
		textOverlay->addText("Press \"space\" to toggle text overlay", 5.0f, 65.0f, VulkanTextOverlay::alignLeft);

		// Display projected cube vertices
		for (int32_t x = -1; x <= 1; x += 2)
		{
			for (int32_t y = -1; y <= 1; y += 2)
			{
				for (int32_t z = -1; z <= 1; z += 2)
				{
					std::stringstream vpos;
					vpos << std::showpos << x << "/" << y << "/" << z;
					glm::vec3 projected = glm::project(glm::vec3((float)x, (float)y, (float)z), uboVS.model, uboVS.projection, glm::vec4(0, 0, (float)width, (float)height));
					textOverlay->addText(vpos.str(), projected.x, projected.y + (y > -1 ? 5.0f : -20.0f), VulkanTextOverlay::alignCenter);
				}
			}
		}

		// Display current model view matrix
		textOverlay->addText("model view matrix", (float)width, 5.0f, VulkanTextOverlay::alignRight);

		for (uint32_t i = 0; i < 4; i++)
		{
			ss.str("");
			ss << std::fixed << std::setprecision(2) << std::showpos;
			ss << uboVS.model[0][i] << " " << uboVS.model[1][i] << " " << uboVS.model[2][i] << " " << uboVS.model[3][i];
			textOverlay->addText(ss.str(), (float)width, 25.0f + (float)i * 20.0f, VulkanTextOverlay::alignRight);
		}

		glm::vec3												projected					= glm::project(glm::vec3(0.0f), uboVS.model, uboVS.projection, glm::vec4(0, 0, (float)width, (float)height));
		textOverlay->addText("Uniform cube", projected.x, projected.y, VulkanTextOverlay::alignCenter);

#if defined(__ANDROID__)
		// toto
#else
		textOverlay->addText("Hold middle mouse button and drag to move", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
#endif
		textOverlay->endTextUpdate();
	}

	void												loadAssets					()								{
		textures.background	.loadFromFile(getAssetPath() + "textures/skysphere_bc3.ktx"		, VK_FORMAT_BC3_UNORM_BLOCK	, vulkanDevice, queue);
		textures.cube		.loadFromFile(getAssetPath() + "textures/round_window_bc3.ktx"	, VK_FORMAT_BC3_UNORM_BLOCK	, vulkanDevice, queue);
		models	.cube		.loadFromFile(getAssetPath() + "models/cube.dae"				, vertexLayout, 1.0f		, vulkanDevice, queue);
	}

	void												setupVertexDescriptions		()								{
		// Binding description
		vertices.bindingDescriptions.resize(1);
		vertices.bindingDescriptions	[0]					= vks::initializers::vertexInputBindingDescription(VERTEX_BUFFER_BIND_ID, vertexLayout.stride(), VK_VERTEX_INPUT_RATE_VERTEX);

		// Attribute descriptions
		vertices.attributeDescriptions.resize(4);
		vertices.attributeDescriptions	[0]					= vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 0, VK_FORMAT_R32G32B32_SFLOAT,0);						// Location 0 : Position
		vertices.attributeDescriptions	[1]					= vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 1, VK_FORMAT_R32G32B32_SFLOAT,sizeof(float) * 3);		// Location 1 : Normal
		vertices.attributeDescriptions	[2]					= vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 2, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 6);			// Location 2 : Texture coordinates
		vertices.attributeDescriptions	[3]					= vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 3, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 8);		// Location 3 : Color

		vertices.inputState									= vks::initializers::pipelineVertexInputStateCreateInfo();
		vertices.inputState.vertexBindingDescriptionCount	= static_cast<uint32_t>(vertices.bindingDescriptions.size());
		vertices.inputState.pVertexBindingDescriptions		= vertices.bindingDescriptions.data();
		vertices.inputState.vertexAttributeDescriptionCount	= static_cast<uint32_t>(vertices.attributeDescriptions.size());
		vertices.inputState.pVertexAttributeDescriptions	= vertices.attributeDescriptions.data();
	}

	void												setupDescriptorPool			()								{
		std::vector<VkDescriptorPoolSize>						poolSizes					=
		{	vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2)
		,	vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2)
		};

		VkDescriptorPoolCreateInfo								descriptorPoolInfo			= vks::initializers::descriptorPoolCreateInfo(static_cast<uint32_t>(poolSizes.size()), poolSizes.data(), 2);

		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
	}

	void												setupDescriptorSetLayout	()								{
		std::vector<VkDescriptorSetLayoutBinding>				setLayoutBindings			=
		{	vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER			, VK_SHADER_STAGE_VERTEX_BIT	, 0)	// Binding 0 : Vertex shader uniform buffer
		,	vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER	, VK_SHADER_STAGE_FRAGMENT_BIT	, 1)	// Binding 1 : Fragment shader combined sampler
		};

		VkDescriptorSetLayoutCreateInfo							descriptorLayout			= vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

		VkPipelineLayoutCreateInfo								pPipelineLayoutCreateInfo	= vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));
	}

	void												setupDescriptorSet			()								{
		VkDescriptorSetAllocateInfo								allocInfo					= vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);

		// Background
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets.background));
		VkDescriptorImageInfo									texDescriptor				= vks::initializers::descriptorImageInfo(textures.background.sampler, textures.background.view, VK_IMAGE_LAYOUT_GENERAL);

		std::vector<VkWriteDescriptorSet>						writeDescriptorSets;

		
		writeDescriptorSets.push_back(vks::initializers::writeDescriptorSet(descriptorSets.background, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER		, 0, &uniformBuffer.descriptor	));	// Binding 0 : Vertex shader uniform buffer
		writeDescriptorSets.push_back(vks::initializers::writeDescriptorSet(descriptorSets.background, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &texDescriptor				));	// Binding 1 : Color map 
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

		// Cube
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets.cube));
		texDescriptor.sampler								= textures.cube.sampler;
		texDescriptor.imageView								= textures.cube.view;
		writeDescriptorSets[0].dstSet						= descriptorSets.cube;
		writeDescriptorSets[1].dstSet						= descriptorSets.cube;
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
	}

	void												preparePipelines			()								{
		VkPipelineInputAssemblyStateCreateInfo					inputAssemblyState		= vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo					rasterizationState		= vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState						blendAttachmentState	= vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo						colorBlendState			= vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo					depthStencilState		= vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo						viewportState			= vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo					multisampleState		= vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState>								dynamicStateEnables		= {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
		VkPipelineDynamicStateCreateInfo						dynamicState			= vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);

		// Wire frame rendering pipeline
		std::array<VkPipelineShaderStageCreateInfo, 2>			shaderStages			= {};
		shaderStages[0] = loadShader(getAssetPath() + "shaders/textoverlay/mesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/textoverlay/mesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		VkGraphicsPipelineCreateInfo							pipelineCreateInfo		= vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass, 0);

		pipelineCreateInfo.pVertexInputState				= &vertices.inputState;
		pipelineCreateInfo.pInputAssemblyState				= &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState				= &rasterizationState;
		pipelineCreateInfo.pColorBlendState					= &colorBlendState;
		pipelineCreateInfo.pMultisampleState				= &multisampleState;
		pipelineCreateInfo.pViewportState					= &viewportState;
		pipelineCreateInfo.pDepthStencilState				= &depthStencilState;
		pipelineCreateInfo.pDynamicState					= &dynamicState;
		pipelineCreateInfo.stageCount						= static_cast<uint32_t>(shaderStages.size());
		pipelineCreateInfo.pStages							= shaderStages.data();

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.solid));

		// Background rendering pipeline
		depthStencilState.depthTestEnable					= VK_FALSE;
		depthStencilState.depthWriteEnable					= VK_FALSE;

		rasterizationState.polygonMode						= VK_POLYGON_MODE_FILL;

		shaderStages[0]										= loadShader(getAssetPath() + "shaders/textoverlay/background.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1]										= loadShader(getAssetPath() + "shaders/textoverlay/background.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.background));
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void												prepareUniformBuffers		()								{
		// Vertex shader uniform buffer block
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffer, sizeof(uboVS)));
		VK_CHECK_RESULT(uniformBuffer.map());		// Map persistent

		updateUniformBuffers();
	}

	void												updateUniformBuffers		()								{
		// Vertex shader
		uboVS.projection									= glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.1f, 256.0f);

		glm::mat4												viewMatrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, zoom));

		uboVS.model											= viewMatrix * glm::translate(glm::mat4(), cameraPos);
		uboVS.model											= glm::rotate(uboVS.model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		uboVS.model											= glm::rotate(uboVS.model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		uboVS.model											= glm::rotate(uboVS.model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		memcpy(uniformBuffer.mapped, &uboVS, sizeof(uboVS));
	}

	void												prepareTextOverlay			()								{
		// Load the text rendering shaders
		std::vector<VkPipelineShaderStageCreateInfo>			shaderStages;
		shaderStages.push_back(loadShader(getAssetPath() + "shaders/textoverlay/text.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
		shaderStages.push_back(loadShader(getAssetPath() + "shaders/textoverlay/text.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));

		textOverlay											= new VulkanTextOverlay(vulkanDevice, queue, frameBuffers, swapChain.colorFormat, depthFormat, &width, &height, shaderStages);
		updateTextOverlay();
	}

		void											draw						()								{
		VulkanExampleBase::prepareFrame();

		// Command buffer to be sumitted to the queue
		submitInfo.commandBufferCount						= 1;
		submitInfo.pCommandBuffers							= &drawCmdBuffers[currentBuffer];

		// Submit to queue
		//VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

		// Submit text overlay to queue
		textOverlay->submit(queue, currentBuffer, submitInfo);

		VulkanExampleBase::submitFrame();
	}

	void												prepare						()								{
		VulkanExampleBase::prepare();
		loadAssets();
		setupVertexDescriptions();
		prepareUniformBuffers();
		setupDescriptorSetLayout();
		preparePipelines();
		setupDescriptorPool();
		setupDescriptorSet();
		buildCommandBuffers();
		prepareTextOverlay();
		prepared = true;
	}

	virtual void										render						()								{
		if (!prepared)
			return;
		draw();
		if (frameCounter == 0)
		{
			vkDeviceWaitIdle(device);
			updateTextOverlay();
		}
	}

	virtual void										viewChanged					()								{
		vkDeviceWaitIdle(device);
		updateUniformBuffers();
		updateTextOverlay();
	}

	virtual void										windowResized				()								{ updateTextOverlay(); }
	virtual void										keyPressed					(uint32_t keyCode)				{
		switch (keyCode)
		{
		case KEY_KPADD:
		case KEY_SPACE:
			textOverlay->visible = !textOverlay->visible;
		}
	}
};

VULKAN_EXAMPLE_EXPORT_FUNCTIONS()