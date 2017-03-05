// Vulkan Example - Using different pipelines in one single renderpass
// 
// Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
// 
// This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
#include "vulkanexamplebase.h"
#include "VulkanModel.hpp"

#define VERTEX_BUFFER_BIND_ID	0
#define ENABLE_VALIDATION		false

class VulkanExample : public VulkanExampleBase 
{
public:
	vks::VertexInputStateAndDescriptions						vertices;

	// Vertex layout for the models
	vks::VertexLayout											vertexLayout									= vks::VertexLayout(
		{	vks::VERTEX_COMPONENT_POSITION
		,	vks::VERTEX_COMPONENT_NORMAL
		,	vks::VERTEX_COMPONENT_UV
		,	vks::VERTEX_COMPONENT_COLOR
		});

	struct {
		vks::Model													cube;
	}															models;

	vks::Buffer													uniformBuffer;

	// Same uniform buffer layout as shader
	typedef vks::Uniform_Proj_ModelView_LightPos				UBOVS;
	UBOVS														uboVS											= {{}, {}, glm::vec4(0.0f, 2.0f, 1.0f, 0.0f)};

	VkPipelineLayout											pipelineLayout									= VK_NULL_HANDLE;
	VkDescriptorSet												descriptorSet									= VK_NULL_HANDLE;
	VkDescriptorSetLayout										descriptorSetLayout								= VK_NULL_HANDLE;

	struct {
		VkPipeline													phong											= VK_NULL_HANDLE;
		VkPipeline													wireframe										= VK_NULL_HANDLE;
		VkPipeline													toon											= VK_NULL_HANDLE;
	}															pipelines;

																VulkanExample									()							: VulkanExampleBase(ENABLE_VALIDATION)	{
		zoom														= -10.5f;
		rotation													= glm::vec3(-25.0f, 15.0f, 0.0f);
		enableTextOverlay											= true;
		title														= "Vulkan Example - Pipeline state objects";
		// Enable features for wireframe rendering and line width setting
		enabledFeatures.fillModeNonSolid							= VK_TRUE;
		enabledFeatures.wideLines									= VK_TRUE;
	}

	// Clean up used Vulkan resources 
	// Note : Inherited destructor cleans up resources stored in base class
																~VulkanExample									()							{
		vkDestroyPipeline				(device, pipelines.phong		, nullptr);
		if (deviceFeatures.fillModeNonSolid)
			vkDestroyPipeline				(device, pipelines.wireframe	, nullptr);

		vkDestroyPipeline				(device, pipelines.toon			, nullptr);
		
		vkDestroyPipelineLayout			(device, pipelineLayout			, nullptr);
		vkDestroyDescriptorSetLayout	(device, descriptorSetLayout	, nullptr);

		models.cube		.destroy();
		uniformBuffer	.destroy();
	}

	void														buildCommandBuffers								()							{
		VkCommandBufferBeginInfo										cmdBufInfo										= vks::initializers::commandBufferBeginInfo();

		VkClearValue													clearValues[2];
		clearValues[0].color										= defaultClearColor;
		clearValues[1].depthStencil									= { 1.0f, 0 };

		VkRenderPassBeginInfo											renderPassBeginInfo								= vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass								= renderPass;
		renderPassBeginInfo.renderArea.offset.x						= 0;
		renderPassBeginInfo.renderArea.offset.y						= 0;
		renderPassBeginInfo.renderArea.extent.width					= screenSize.Width;
		renderPassBeginInfo.renderArea.extent.height				= screenSize.Height;
		renderPassBeginInfo.clearValueCount							= 2;
		renderPassBeginInfo.pClearValues							= clearValues;

		for (size_t i = 0; i < drawCmdBuffers.size(); ++i) {
			// Set target frame buffer
			renderPassBeginInfo.framebuffer								= frameBuffers[i];
			VK_EVAL(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));
			vkCmdBeginRenderPass	(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport														viewport											= vks::initializers::viewport((float)screenSize.Width, (float)screenSize.Height, 0.0f, 1.0f);
			vkCmdSetViewport		(drawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D														scissor												= vks::initializers::rect2D(screenSize.Width, screenSize.Height,	0, 0);
			vkCmdSetScissor			(drawCmdBuffers[i], 0, 1, &scissor);

			vkCmdBindDescriptorSets	(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);

			VkDeviceSize													offsets[1]											= { 0 };
			vkCmdBindVertexBuffers	(drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &models.cube.vertices.buffer, offsets);
			vkCmdBindIndexBuffer	(drawCmdBuffers[i], models.cube.indices.buffer, 0, VK_INDEX_TYPE_UINT32);

			// Left : Solid colored 
			viewport.width												= (float)(screenSize.Width / 3.0);
			vkCmdSetViewport		(drawCmdBuffers[i], 0, 1, &viewport);
			vkCmdBindPipeline		(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.phong);
			
			vkCmdDrawIndexed		(drawCmdBuffers[i], models.cube.indexCount, 1, 0, 0, 0);

			// Center : Toon
			viewport.x													= (float)(screenSize.Width / 3.0);
			vkCmdSetViewport		(drawCmdBuffers[i], 0, 1, &viewport);
			vkCmdBindPipeline		(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.toon);
			vkCmdSetLineWidth		(drawCmdBuffers[i], 2.0f);
			vkCmdDrawIndexed		(drawCmdBuffers[i], models.cube.indexCount, 1, 0, 0, 0);

			if (deviceFeatures.fillModeNonSolid) {
				// Right : Wireframe 
				viewport.x														= (float)(screenSize.Width / 3.0 + screenSize.Width / 3.0);
				vkCmdSetViewport		(drawCmdBuffers[i], 0, 1, &viewport);
				vkCmdBindPipeline		(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.wireframe);
				vkCmdDrawIndexed		(drawCmdBuffers[i], models.cube.indexCount, 1, 0, 0, 0);
			}

			vkCmdEndRenderPass		(drawCmdBuffers[i]);

			VK_EVAL(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void														loadAssets										()									{
		models.cube.loadFromFile(getAssetPath() + "models/treasure_smooth.dae", vertexLayout, 1.0f, vulkanDevice, queue);
	}

	void														setupVertexDescriptions							()									{
		// Binding description
		vertices.bindingDescriptions.resize(1);
		vertices.bindingDescriptions	[0]							= vks::initializers::vertexInputBindingDescription(VERTEX_BUFFER_BIND_ID, vertexLayout.stride(), VK_VERTEX_INPUT_RATE_VERTEX);

		// Attribute descriptions. Describes memory layout and shader positions
		vertices.attributeDescriptions.resize(4);
		vertices.attributeDescriptions	[0]							= vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 0, VK_FORMAT_R32G32B32_SFLOAT	, 0);					// Location 0 : Position
		vertices.attributeDescriptions	[1]							= vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 1, VK_FORMAT_R32G32B32_SFLOAT	, sizeof(float) * 3);	// Location 1 : Color	
		vertices.attributeDescriptions	[2]							= vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 2, VK_FORMAT_R32G32_SFLOAT	, sizeof(float) * 6);		// Location 2 : Texture coordinates
		vertices.attributeDescriptions	[3]							= vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 3, VK_FORMAT_R32G32B32_SFLOAT	, sizeof(float) * 8);	// Location 3 : Normal

		vertices.inputState											= vks::initializers::pipelineVertexInputStateCreateInfo();
		vertices.inputState.vertexBindingDescriptionCount			= static_cast<uint32_t>(vertices.bindingDescriptions.size());
		vertices.inputState.pVertexBindingDescriptions				= vertices.bindingDescriptions.data();
		vertices.inputState.vertexAttributeDescriptionCount			= static_cast<uint32_t>(vertices.attributeDescriptions.size());
		vertices.inputState.pVertexAttributeDescriptions			= vertices.attributeDescriptions.data();
	}

	void														setupDescriptorPool								()									{
		std::vector<VkDescriptorPoolSize>								poolSizes										=
			{	vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
			};

		VkDescriptorPoolCreateInfo										descriptorPoolInfo			= vks::initializers::descriptorPoolCreateInfo(static_cast<uint32_t>(poolSizes.size()), poolSizes.data(), 2);
		VK_EVAL(vkCreateDescriptorPool		(device, &descriptorPoolInfo, nullptr, &descriptorPool));
	}

	void														setupDescriptorSetLayout						()									{
		std::vector<VkDescriptorSetLayoutBinding>						setLayoutBindings								=
			{	vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0)			// Binding 0 : Vertex shader uniform buffer
			};

		VkDescriptorSetLayoutCreateInfo									descriptorLayout								= vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
		VK_EVAL(vkCreateDescriptorSetLayout	(device, &descriptorLayout, nullptr, &descriptorSetLayout));

		VkPipelineLayoutCreateInfo										pPipelineLayoutCreateInfo						= vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		VK_EVAL(vkCreatePipelineLayout		(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));
	}

	void														setupDescriptorSet								()									{
		VkDescriptorSetAllocateInfo										allocInfo										= vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
		VK_EVAL(vkAllocateDescriptorSets	(device, &allocInfo, &descriptorSet));
		std::vector<VkWriteDescriptorSet>								writeDescriptorSets								=
			{	vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffer.descriptor)	// Binding 0 : Vertex shader uniform buffer
			};

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
	}

	void														preparePipelines								()									{
		VkPipelineInputAssemblyStateCreateInfo							inputAssemblyState								= vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo							rasterizationState								= vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState								blendAttachmentState							= vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo								colorBlendState									= vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo							depthStencilState								= vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo								viewportState									= vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo							multisampleState								= vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);

		std::vector<VkDynamicState>										dynamicStateEnables								= 
			{	VK_DYNAMIC_STATE_VIEWPORT
			,	VK_DYNAMIC_STATE_SCISSOR
			,	VK_DYNAMIC_STATE_LINE_WIDTH
			};
		VkPipelineDynamicStateCreateInfo								dynamicState									= vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);

		std::array<VkPipelineShaderStageCreateInfo, 2>					shaderStages									= {};
		// Phong shading pipeline
		shaderStages[0]												= loadShader(getAssetPath() + "shaders/pipelines/phong.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1]												= loadShader(getAssetPath() + "shaders/pipelines/phong.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		VkGraphicsPipelineCreateInfo									pipelineCreateInfo								= vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass, 0);
		pipelineCreateInfo.pVertexInputState						= &vertices.inputState;
		pipelineCreateInfo.pInputAssemblyState						= &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState						= &rasterizationState;
		pipelineCreateInfo.pColorBlendState							= &colorBlendState;
		pipelineCreateInfo.pMultisampleState						= &multisampleState;
		pipelineCreateInfo.pViewportState							= &viewportState;
		pipelineCreateInfo.pDepthStencilState						= &depthStencilState;
		pipelineCreateInfo.pDynamicState							= &dynamicState;
		pipelineCreateInfo.stageCount								= static_cast<uint32_t>(shaderStages.size());
		pipelineCreateInfo.pStages									= shaderStages.data();

		// We are using this pipeline as the base for the other pipelines (derivatives)
		// Pipeline derivatives can be used for pipelines that share most of their state
		// Depending on the implementation this may result in better performance for pipeline 
		// switchting and faster creation time
		pipelineCreateInfo.flags									= VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;

		// Textured pipeline
		VK_EVAL(vkCreateGraphicsPipelines	(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.phong));
		pipelineCreateInfo.flags									= VK_PIPELINE_CREATE_DERIVATIVE_BIT;		// All pipelines created after the base pipeline will be derivatives
		pipelineCreateInfo.basePipelineHandle						= pipelines.phong;							// Base pipeline will be our first created pipeline
		// It's only allowed to either use a handle or index for the base pipeline
		// As we use the handle, we must set the index to -1 (see section 9.5 of the specification)
		pipelineCreateInfo.basePipelineIndex						= -1;

		// Toon shading pipeline
		shaderStages[0]												= loadShader(getAssetPath() + "shaders/pipelines/toon.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1]												= loadShader(getAssetPath() + "shaders/pipelines/toon.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		VK_EVAL(vkCreateGraphicsPipelines	(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.toon));

		// Non solid rendering is not a mandatory Vulkan feature
		if (deviceFeatures.fillModeNonSolid) {
			// Pipeline for wire frame rendering
			rasterizationState.polygonMode								= VK_POLYGON_MODE_LINE;
			shaderStages[0]												= loadShader(getAssetPath() + "shaders/pipelines/wireframe.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
			shaderStages[1]												= loadShader(getAssetPath() + "shaders/pipelines/wireframe.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
			VK_EVAL(vkCreateGraphicsPipelines	(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.wireframe));
		}
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void														prepareUniformBuffers							()									{
		// Create the vertex shader uniform buffer block
		VK_EVAL(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffer, sizeof(uboVS)));
		VK_EVAL(uniformBuffer.map());		// Map persistent

		updateUniformBuffers();
	}

	void														updateUniformBuffers							()									{
		uboVS.projection											= glm::perspective(glm::radians(60.0f), (float)(screenSize.Width / 3.0f) / (float)screenSize.Height, 0.1f, 256.0f);

		glm::mat4														viewMatrix										= glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, zoom));

		uboVS.modelView												= viewMatrix * glm::translate(glm::mat4(), cameraPos);
		uboVS.modelView												= glm::rotate(uboVS.modelView, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		uboVS.modelView												= glm::rotate(uboVS.modelView, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		uboVS.modelView												= glm::rotate(uboVS.modelView, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		memcpy(uniformBuffer.mapped, &uboVS, sizeof(uboVS));
	}

	void														draw											()									{
		VulkanExampleBase::prepareFrame();

		submitInfo.commandBufferCount								= 1;
		submitInfo.pCommandBuffers									= &drawCmdBuffers[currentBuffer];
		VK_EVAL(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

		VulkanExampleBase::submitFrame();
	}

	void														prepare											()									{
		VulkanExampleBase::prepare();
		loadAssets();
		setupVertexDescriptions();
		prepareUniformBuffers();
		setupDescriptorSetLayout();
		preparePipelines();
		setupDescriptorPool();
		setupDescriptorSet();
		buildCommandBuffers();
		prepared													= true;
	}

	virtual void												render											()									{ if (prepared) draw();		}
	virtual void												viewChanged										()									{ updateUniformBuffers();	}
	virtual void												getOverlayText									(VulkanTextOverlay *textOverlay_)	{
		textOverlay_->addText("Phong shading pipeline"	, (float)screenSize.Width / 6.0f			, screenSize.Height - 35.0f, VulkanTextOverlay::alignCenter);
		textOverlay_->addText("Toon shading pipeline"	, (float)screenSize.Width / 2.0f			, screenSize.Height - 35.0f, VulkanTextOverlay::alignCenter);
		textOverlay_->addText("Wireframe pipeline"		, screenSize.Width - (float)screenSize.Width / 6.5f	, screenSize.Height - 35.0f, VulkanTextOverlay::alignCenter);
	}
};

VULKAN_EXAMPLE_EXPORT_FUNCTIONS()