// Vulkan Example - Tessellation shader PN triangles
// 
// Based on http://alex.vlachos.com/graphics/CurvedPNTriangles.pdf
// Shaders based on http://onrendering.blogspot.de/2011/12/tessellation-on-gpu-curved-pn-triangles.html
// 
// Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
// 
// This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
#include "vulkanexamplebase.h"
#include "VulkanTexture.hpp"
#include "VulkanModel.hpp"
#include "VulkanBuffer.hpp"

#define VERTEX_BUFFER_BIND_ID	0
#define ENABLE_VALIDATION		false

class VulkanExample : public VulkanExampleBase
{
public:
	bool														splitScreen							= true;

	struct {
		vks::Texture2D												colorMap;
	}															textures;

	vks::VertexInputStateAndDescriptions						vertices;

	// Vertex layout for the models
	vks::VertexLayout											vertexLayout						= vks::VertexLayout(
	{	vks::VERTEX_COMPONENT_POSITION
	,	vks::VERTEX_COMPONENT_NORMAL
	,	vks::VERTEX_COMPONENT_UV
	});

	struct {
		vks::Model													object;
	}															models;

	struct {
		vks::Buffer													tessControl
			,														tessEval
			;
	}															uniformBuffers;
	
	struct UBOTessControl {
		float														tessLevel							= 3.0f;
	}															uboTessControl;

	struct UBOTessEval {
		glm::mat4													projection;
		glm::mat4													model;
		float														tessAlpha							= 1.0f;
	}															uboTessEval;

	struct {
		VkPipeline													solid								= VK_NULL_HANDLE;
		VkPipeline													wire								= VK_NULL_HANDLE;
		VkPipeline													solidPassThrough					= VK_NULL_HANDLE;
		VkPipeline													wirePassThrough						= VK_NULL_HANDLE;
	}															pipelines;
	VkPipeline													* pipelineLeft						= &pipelines.wirePassThrough;
	VkPipeline													* pipelineRight						= &pipelines.wire;
	
	VkPipelineLayout											pipelineLayout						= VK_NULL_HANDLE;
	VkDescriptorSet												descriptorSet						= VK_NULL_HANDLE;
	VkDescriptorSetLayout										descriptorSetLayout					= VK_NULL_HANDLE;				

																VulkanExample						() : VulkanExampleBase(ENABLE_VALIDATION)			{
		zoom														= -6.5f;
		rotation													= glm::vec3(-350.0f, 60.0f, 0.0f);
		cameraPos													= glm::vec3(-3.0f, 2.3f, 0.0f);
		title														= "Vulkan Example - Tessellation shader (PN Triangles)";
		enableTextOverlay											= true;
		// Enable physical device features required for this example				
		
		enabledFeatures.tessellationShader							= VK_TRUE;	// Tell the driver that we are going to use geometry shaders
		enabledFeatures.fillModeNonSolid							= VK_TRUE;	// Example also uses a wireframe pipeline, enable non-solid fill modes
	}

																~VulkanExample						()													{
		// Clean up used Vulkan resources 
		// Note : Inherited destructor cleans up resources stored in base class
		vkDestroyPipeline				(device, pipelines.solid			, nullptr);
		vkDestroyPipeline				(device, pipelines.wire				, nullptr);
		vkDestroyPipeline				(device, pipelines.solidPassThrough	, nullptr);
		vkDestroyPipeline				(device, pipelines.wirePassThrough	, nullptr);

		vkDestroyPipelineLayout			(device, pipelineLayout				, nullptr);
		vkDestroyDescriptorSetLayout	(device, descriptorSetLayout		, nullptr);

		models.object				.destroy();
		uniformBuffers.tessControl	.destroy();
		uniformBuffers.tessEval		.destroy();
		textures.colorMap			.destroy();
	}

	void														reBuildCommandBuffers				()													{
		if (!checkCommandBuffers ()) {
			destroyCommandBuffers();
			createCommandBuffers ();
		}
		buildCommandBuffers();
	}

	void														buildCommandBuffers					()													{
		VkCommandBufferBeginInfo										cmdBufInfo							= vks::initializers::commandBufferBeginInfo();

		VkClearValue													clearValues[2];
		clearValues[0].color										= { {0.5f, 0.5f, 0.5f, 0.0f} };
		clearValues[1].depthStencil									= { 1.0f, 0 };

		VkRenderPassBeginInfo											renderPassBeginInfo					= vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass								= renderPass;
		renderPassBeginInfo.renderArea.offset.x						= 0;
		renderPassBeginInfo.renderArea.offset.y						= 0;
		renderPassBeginInfo.renderArea.extent.width					= screenSize.Width;
		renderPassBeginInfo.renderArea.extent.height				= screenSize.Height;
		renderPassBeginInfo.clearValueCount							= 2;
		renderPassBeginInfo.pClearValues							= clearValues;

		for (size_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			// Set target frame buffer
			renderPassBeginInfo.framebuffer								= frameBuffers[i];
			VK_EVAL(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));
			vkCmdBeginRenderPass	(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport														viewport							= vks::initializers::viewport(splitScreen ? (float)screenSize.Width * 0.5f : (float)screenSize.Width, (float)screenSize.Height, 0.0f, 1.0f);
			vkCmdSetViewport		(drawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D														scissor								= vks::initializers::rect2D(screenSize.Width, screenSize.Height, 0, 0);
			vkCmdSetScissor			(drawCmdBuffers[i], 0, 1, &scissor);

			vkCmdSetLineWidth		(drawCmdBuffers[i], 1.0f);

			vkCmdBindDescriptorSets	(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);

			VkDeviceSize													offsets	[1]							= { 0 };
			vkCmdBindVertexBuffers	(drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &models.object.vertices.buffer, offsets);
			vkCmdBindIndexBuffer	(drawCmdBuffers[i], models.object.indices.buffer, 0, VK_INDEX_TYPE_UINT32);

			if (splitScreen) {
				vkCmdSetViewport		(drawCmdBuffers[i], 0, 1, &viewport);
				vkCmdBindPipeline		(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLeft);
				vkCmdDrawIndexed		(drawCmdBuffers[i], models.object.indexCount, 1, 0, 0, 0);
				viewport.x													= float(screenSize.Width) / 2.0f;
			}

			vkCmdSetViewport		(drawCmdBuffers[i], 0, 1, &viewport);
			vkCmdBindPipeline		(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineRight);
			vkCmdDrawIndexed		(drawCmdBuffers[i], models.object.indexCount, 1, 0, 0, 0);

			vkCmdEndRenderPass		(drawCmdBuffers[i]);

			VK_EVAL(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void														loadAssets							()													{
		models.object		.loadFromFile(getAssetPath() + "models/lowpoly/deer.dae", vertexLayout, 1.0f, vulkanDevice, queue);
		textures.colorMap	.loadFromFile(getAssetPath() + "textures/deer.ktx", VK_FORMAT_BC3_UNORM_BLOCK, vulkanDevice, queue);
	}

	void														setupVertexDescriptions				()													{
		// Binding description
		vertices.bindingDescriptions.resize(1);
		vertices.bindingDescriptions[0]								= vks::initializers::vertexInputBindingDescription(VERTEX_BUFFER_BIND_ID, vertexLayout.stride(), VK_VERTEX_INPUT_RATE_VERTEX);
		// Attribute descriptions. Describes memory layout and shader positions
		vertices.attributeDescriptions.resize(3);

		vertices.attributeDescriptions[0]							= vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);					// Location 0 : Position
		vertices.attributeDescriptions[1]							= vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 1, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3);	// Location 1 : Normals
		vertices.attributeDescriptions[2]							= vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 2, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 6);		// Location 2 : Texture coordinates

		vertices.inputState											= vks::initializers::pipelineVertexInputStateCreateInfo();
		vertices.inputState.vertexBindingDescriptionCount			= static_cast<uint32_t>(vertices.bindingDescriptions.size());
		vertices.inputState.pVertexBindingDescriptions				= vertices.bindingDescriptions.data();
		vertices.inputState.vertexAttributeDescriptionCount			= static_cast<uint32_t>(vertices.attributeDescriptions.size());
		vertices.inputState.pVertexAttributeDescriptions			= vertices.attributeDescriptions.data();
	}

	void														setupDescriptorPool					()													{
		// Example uses two ubos and one combined image sampler
		std::vector<VkDescriptorPoolSize>								poolSizes							=
		{	vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2)
		,	vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
		};

		VkDescriptorPoolCreateInfo										descriptorPoolInfo					= vks::initializers::descriptorPoolCreateInfo(static_cast<uint32_t>(poolSizes.size()), poolSizes.data(), 1);

		VK_EVAL(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
	}

	void														setupDescriptorSetLayout			()													{
		std::vector<VkDescriptorSetLayoutBinding>						setLayoutBindings					=
		{	vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 0)		// Binding 0 : Tessellation control shader ubo
		,	vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 1)	// Binding 1 : Tessellation evaluation shader ubo
		,	vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2)			// Binding 2 : Fragment shader combined sampler
		};

		VkDescriptorSetLayoutCreateInfo									descriptorLayout					= vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));

		VK_EVAL(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

		VkPipelineLayoutCreateInfo										pPipelineLayoutCreateInfo			= vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		VK_EVAL(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));
	}

	void														setupDescriptorSet					()													{
		VkDescriptorSetAllocateInfo										allocInfo							= vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
		VK_EVAL(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

		VkDescriptorImageInfo											texDescriptor						= vks::initializers::descriptorImageInfo(textures.colorMap.sampler, textures.colorMap.view, VK_IMAGE_LAYOUT_GENERAL);
		std::vector<VkWriteDescriptorSet>								writeDescriptorSets					=
		{	vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.tessControl.descriptor)		// Binding 0 : Tessellation control shader ubo
		,	vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &uniformBuffers.tessEval.descriptor)			// Binding 1 : Tessellation evaluation shader ubo
		,	vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &texDescriptor)						// Binding 2 : Color map 
		};

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
	}

	void														preparePipelines					()													{
		VkPipelineInputAssemblyStateCreateInfo							inputAssemblyState					= vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo							rasterizationState					= vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState								blendAttachmentState				= vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo								colorBlendState						= vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo							depthStencilState					= vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo								viewportState						= vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo							multisampleState					= vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState>										dynamicStateEnables					= 
		{	VK_DYNAMIC_STATE_VIEWPORT
		,	VK_DYNAMIC_STATE_SCISSOR
		,	VK_DYNAMIC_STATE_LINE_WIDTH
		};
		VkPipelineDynamicStateCreateInfo								dynamicState						= vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);
		VkPipelineTessellationStateCreateInfo							tessellationState					= vks::initializers::pipelineTessellationStateCreateInfo(3);

		// Tessellation pipelines
		// Load shaders
		std::array<VkPipelineShaderStageCreateInfo, 4>					shaderStages;
		shaderStages[0]												= loadShader(getAssetPath() + "shaders/tessellation/base.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1]												= loadShader(getAssetPath() + "shaders/tessellation/base.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		shaderStages[2]												= loadShader(getAssetPath() + "shaders/tessellation/pntriangles.tesc.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
		shaderStages[3]												= loadShader(getAssetPath() + "shaders/tessellation/pntriangles.tese.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

		VkGraphicsPipelineCreateInfo									pipelineCreateInfo					= vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass, 0);
		pipelineCreateInfo.pVertexInputState						= &vertices.inputState;
		pipelineCreateInfo.pInputAssemblyState						= &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState						= &rasterizationState;
		pipelineCreateInfo.pColorBlendState							= &colorBlendState;
		pipelineCreateInfo.pMultisampleState						= &multisampleState;
		pipelineCreateInfo.pViewportState							= &viewportState;
		pipelineCreateInfo.pDepthStencilState						= &depthStencilState;
		pipelineCreateInfo.pDynamicState							= &dynamicState;
		pipelineCreateInfo.pTessellationState						= &tessellationState;
		pipelineCreateInfo.stageCount								= static_cast<uint32_t>(shaderStages.size());
		pipelineCreateInfo.pStages									= shaderStages.data();
		pipelineCreateInfo.renderPass								= renderPass;

		// Tessellation pipelines
		// Solid
		VK_EVAL(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.solid));
		// Wireframe
		rasterizationState.polygonMode								= VK_POLYGON_MODE_LINE;
		VK_EVAL(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.wire));

		// Pass through pipelines
		// Load pass through tessellation shaders (Vert and frag are reused)
		shaderStages[2]												= loadShader(getAssetPath() + "shaders/tessellation/passthrough.tesc.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
		shaderStages[3]												= loadShader(getAssetPath() + "shaders/tessellation/passthrough.tese.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

		// Solid
		rasterizationState.polygonMode								= VK_POLYGON_MODE_FILL;
		VK_EVAL(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.solidPassThrough));
		// Wireframe
		rasterizationState.polygonMode								= VK_POLYGON_MODE_LINE;
		VK_EVAL(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.wirePassThrough));
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void														prepareUniformBuffers				()													{
		VK_EVAL(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffers.tessEval, sizeof(uboTessEval)));			// Tessellation evaluation shader uniform buffer
		VK_EVAL(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffers.tessControl, sizeof(uboTessControl)));	// Tessellation control shader uniform buffer

		// Map persistent
		VK_EVAL(uniformBuffers.tessControl.map());
		VK_EVAL(uniformBuffers.tessEval.map());

		updateUniformBuffers();
	}

	void														updateUniformBuffers				()													{
		// Tessellation eval
		glm::mat4 viewMatrix										= glm::mat4();
		uboTessEval.projection										= glm::perspective(glm::radians(45.0f), (float)(screenSize.Width * ((splitScreen) ? 0.5f : 1.0f)) / (float)screenSize.Height, 0.1f, 256.0f);
		viewMatrix													= glm::translate(viewMatrix, glm::vec3(0.0f, 0.0f, zoom));

		uboTessEval.model											= glm::mat4();
		uboTessEval.model											= viewMatrix * glm::translate(uboTessEval.model, cameraPos);
		uboTessEval.model											= glm::rotate(uboTessEval.model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		uboTessEval.model											= glm::rotate(uboTessEval.model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		uboTessEval.model											= glm::rotate(uboTessEval.model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		memcpy(uniformBuffers.tessEval.mapped, &uboTessEval, sizeof(uboTessEval));			// Tessellation evaulation uniform block
		memcpy(uniformBuffers.tessControl.mapped, &uboTessControl, sizeof(uboTessControl));	// Tessellation control uniform block
	}

	void														draw								()													{
		VulkanExampleBase::prepareFrame();

		submitInfo.commandBufferCount								= 1;
		submitInfo.pCommandBuffers									= &drawCmdBuffers[currentBuffer];
		VK_EVAL(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

		VulkanExampleBase::submitFrame();
	}

	void														prepare								()													{
		// Check if device supports tessellation shaders
		if (!deviceFeatures.tessellationShader)
			vks::tools::exitFatal("Selected GPU does not support tessellation shaders!", "Feature not supported");

		VulkanExampleBase::prepare	();
		loadAssets					();
		setupVertexDescriptions		();
		prepareUniformBuffers		();
		setupDescriptorSetLayout	();
		preparePipelines			();
		setupDescriptorPool			();
		setupDescriptorSet			();
		buildCommandBuffers 		();
		prepared													= true;
	}

	virtual void												render								()													{
		if (!prepared)
			return;
		vkDeviceWaitIdle(device);
		draw();
		vkDeviceWaitIdle(device);
	}

	virtual void												viewChanged							()													{ updateUniformBuffers(); }
	virtual void												keyPressed							(uint32_t keyCode)									{
		switch (keyCode)
		{
		case KEY_KPADD			:
		case GAMEPAD_BUTTON_R1	: changeTessellationLevel( 0.25);	break;
		case KEY_KPSUB			:
		case GAMEPAD_BUTTON_L1	: changeTessellationLevel(-0.25);	break;
		case KEY_W				:
		case GAMEPAD_BUTTON_A	: togglePipelines();				break;
		case KEY_S				:
		case GAMEPAD_BUTTON_X	: toggleSplitScreen();				break;
		}
	}

	virtual void												getOverlayText						(VulkanTextOverlay *textOverlay_)					{
		std::stringstream												ss;
		ss << std::setprecision(2) << std::fixed << uboTessControl.tessLevel;
#if defined(__ANDROID__)
		textOverlay_->addText("Tessellation level: " + ss.str() + " (Buttons L1/R1 to change)", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
		textOverlay_->addText("Press \"Button X\" to toggle splitscreen", 5.0f, 100.0f, VulkanTextOverlay::alignLeft);
#else
		textOverlay_->addText("Tessellation level: " + ss.str() + " (NUMPAD +/- to change)", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
		textOverlay_->addText("Press \"s\" to toggle splitscreen", 5.0f, 100.0f, VulkanTextOverlay::alignLeft);
#endif
	}

	void														changeTessellationLevel				(float delta)										{
		uboTessControl.tessLevel									+= delta;
		uboTessControl.tessLevel									= fmax(1.0f, fmin(uboTessControl.tessLevel, 32.0f));	// Clamp
		updateUniformBuffers();
		updateTextOverlay();
	}

	void														togglePipelines						()													{
		if (pipelineRight == &pipelines.solid) {
			pipelineRight												= &pipelines.wire;
			pipelineLeft												= &pipelines.wirePassThrough;
		}
		else {
			pipelineRight												= &pipelines.solid;
			pipelineLeft												= &pipelines.solidPassThrough;
		}
		reBuildCommandBuffers();
	}

	void														toggleSplitScreen					()													{
		splitScreen													= !splitScreen;
		updateUniformBuffers();
		reBuildCommandBuffers();
	}

};

VULKAN_EXAMPLE_EXPORT_FUNCTIONS()