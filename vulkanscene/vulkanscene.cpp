// Vulkan Demo Scene - Don't take this a an example, it's more of a personal playground
// 
// Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
// 
// Note : Different license than the other examples! 
// This code is licensed under the Mozilla Public License Version 2.0 (http://opensource.org/licenses/MPL-2.0)
#include "vulkanexamplebase.h"
#include "VulkanTexture.hpp"
#include "VulkanModel.hpp"

#include <glm/gtc/matrix_inverse.hpp>

#define VERTEX_BUFFER_BIND_ID	0
#define ENABLE_VALIDATION		false

class VulkanExample : public VulkanExampleBase
{
public:
	// Vertex layout for the models
	vks::VertexLayout											vertexLayout							= vks::VertexLayout(
		{	vks::VERTEX_COMPONENT_POSITION
		,	vks::VERTEX_COMPONENT_NORMAL
		,	vks::VERTEX_COMPONENT_UV
		,	vks::VERTEX_COMPONENT_COLOR
		});

	struct DemoModel {
		vks::Model													model;
		VkPipeline													* pipeline								= VK_NULL_HANDLE;

		void														draw									(VkCommandBuffer cmdBuffer)			{
			VkDeviceSize													offsets	[1]								= { 0 };
			vkCmdBindPipeline		(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
			vkCmdBindVertexBuffers	(cmdBuffer, VERTEX_BUFFER_BIND_ID, 1, &model.vertices.buffer, offsets);
			vkCmdBindIndexBuffer	(cmdBuffer, model.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed		(cmdBuffer, model.indexCount, 1, 0, 0, 0);
		}
	};

	std::vector<DemoModel>										demoModels;

	struct {
		vks::Buffer													meshVS;
	}															uniformData;

	typedef vks::Uniform_Proj_Model_Normal_View_LightPos		UBOVS;
	UBOVS														uboVS;

	struct {
		vks::TextureCubeMap											skybox;
	}															textures;

	struct {
		VkPipeline													logos									= VK_NULL_HANDLE;
		VkPipeline													models									= VK_NULL_HANDLE;
		VkPipeline													skybox									= VK_NULL_HANDLE;
	}															pipelines;

	VkPipelineLayout											pipelineLayout							= VK_NULL_HANDLE;
	VkDescriptorSet												descriptorSet							= VK_NULL_HANDLE;
	VkDescriptorSetLayout										descriptorSetLayout						= VK_NULL_HANDLE;

	glm::vec4													lightPos								= glm::vec4(1.0f, 2.0f, 0.0f, 0.0f);

																VulkanExample							()									: VulkanExampleBase(ENABLE_VALIDATION)	{
		zoom														= -3.75f;
		rotationSpeed												= 0.5f;
		rotation													= glm::vec3(15.0f, 0.f, 0.0f);
		enableTextOverlay											= true;
		title														= "Vulkan Demo Scene - (c) 2016 by Sascha Willems";
	}

																~VulkanExample							()									{
		// Clean up used Vulkan resources. Inherited destructor cleans up resources stored in base class
		vkDestroyPipeline				(device, pipelines.logos		, nullptr);
		vkDestroyPipeline				(device, pipelines.models		, nullptr);
		vkDestroyPipeline				(device, pipelines.skybox		, nullptr);
		vkDestroyPipelineLayout			(device, pipelineLayout			, nullptr);
		vkDestroyDescriptorSetLayout	(device, descriptorSetLayout	, nullptr);

		uniformData.meshVS.destroy();

		for (auto& model : demoModels)
			model.model.destroy();

		textures.skybox.destroy();
	}

	void														loadAssets								()									{
		// Models
		std::vector<std::string>										modelFiles								= { "vulkanscenelogos.dae", "vulkanscenebackground.dae", "vulkanscenemodels.dae", "cube.obj" };
		std::vector<VkPipeline*>										modelPipelines							= { &pipelines.logos, &pipelines.models, &pipelines.models, &pipelines.skybox };
		for (size_t i = 0; i < modelFiles.size(); i++) {
			DemoModel														model;
			model.pipeline												= modelPipelines[i];
			vks::ModelCreateInfo											modelCreateInfo(glm::vec3(1.0f), glm::vec3(1.0f), glm::vec3(0.0f));
			if (modelFiles[i] != "cube.obj")
				modelCreateInfo.center.y									+= 1.15f;

			model.model.loadFromFile(getAssetPath() + "models/" + modelFiles[i], vertexLayout, &modelCreateInfo, vulkanDevice, queue);
			demoModels.push_back(model);
		}
		// Textures
		textures.skybox.loadFromFile(getAssetPath() + "textures/cubemap_vulkan.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
	}

	void														buildCommandBuffers						()									{
		VkCommandBufferBeginInfo										cmdBufInfo								= vks::initializers::commandBufferBeginInfo();
		VkClearValue													clearValues	[2];
		clearValues[0].color										= defaultClearColor;
		clearValues[1].depthStencil									= { 1.0f, 0 };

		VkRenderPassBeginInfo											renderPassBeginInfo						= vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass								= renderPass;
		renderPassBeginInfo.renderArea.offset.x						= 0;
		renderPassBeginInfo.renderArea.offset.y						= 0;
		renderPassBeginInfo.renderArea.extent.width					= width;
		renderPassBeginInfo.renderArea.extent.height				= height;
		renderPassBeginInfo.clearValueCount							= 2;
		renderPassBeginInfo.pClearValues							= clearValues;

		for (size_t i = 0; i < drawCmdBuffers.size(); ++i) {
			renderPassBeginInfo.framebuffer								= frameBuffers[i];

			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));
			vkCmdBeginRenderPass	(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport														viewport					= vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
			vkCmdSetViewport		(drawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D														scissor						= vks::initializers::rect2D(width, height, 0, 0);
			vkCmdSetScissor			(drawCmdBuffers[i], 0, 1, &scissor);

			vkCmdBindDescriptorSets	(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);

			for (VulkanExample::DemoModel& model : demoModels)
				model.draw				(drawCmdBuffers[i]);

			vkCmdEndRenderPass		(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void														setupDescriptorPool						()									{
		// Example uses one ubo and one image sampler	
		std::vector<VkDescriptorPoolSize>								poolSizes								=
			{	vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER			, 2)
			,	vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER	, 1)
			};

		VkDescriptorPoolCreateInfo										descriptorPoolInfo						= vks::initializers::descriptorPoolCreateInfo(static_cast<uint32_t>(poolSizes.size()), poolSizes.data(), 2);
		VK_CHECK_RESULT(vkCreateDescriptorPool		(device, &descriptorPoolInfo, nullptr, &descriptorPool));
	}

	void														setupDescriptorSetLayout				()									{
		std::vector<VkDescriptorSetLayoutBinding>						setLayoutBindings						=
			{	vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER			, VK_SHADER_STAGE_VERTEX_BIT	, 0)	// Binding 0 : Vertex shader uniform buffer
			,	vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER	, VK_SHADER_STAGE_FRAGMENT_BIT	, 1)	// Binding 1 : Fragment shader color map image sampler
			};

		VkDescriptorSetLayoutCreateInfo									descriptorLayout						= vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout	(device, &descriptorLayout, nullptr, &descriptorSetLayout));

		VkPipelineLayoutCreateInfo										pPipelineLayoutCreateInfo				= vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout		(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));
	}

	void														setupDescriptorSet						()									{
		VkDescriptorSetAllocateInfo										allocInfo								= vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets	(device, &allocInfo, &descriptorSet));

		// Cube map image descriptor
		VkDescriptorImageInfo											texDescriptorCubeMap					= vks::initializers::descriptorImageInfo(textures.skybox.sampler, textures.skybox.view, VK_IMAGE_LAYOUT_GENERAL);
		std::vector<VkWriteDescriptorSet>								writeDescriptorSets						=
			{	vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformData.meshVS.descriptor)	// Binding 0 : Vertex shader uniform buffer
			,	vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &texDescriptorCubeMap)	// Binding 1 : Fragment shader image sampler
			};

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
	}

	void														preparePipelines						()									{
		VkPipelineInputAssemblyStateCreateInfo							stateInputAssembly						= vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo							stateRasterization						= vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState								stateBlendAttachment					= vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo								stateColorBlend							= vks::initializers::pipelineColorBlendStateCreateInfo(1, &stateBlendAttachment);
		VkPipelineDepthStencilStateCreateInfo							stateDepthStencil						= vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo								stateViewport							= vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo							stateMultisample						= vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);

		std::vector<VkDynamicState>										dynamicStateEnables						= {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
		VkPipelineDynamicStateCreateInfo								dynamicState							= vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);

		// Pipeline for the meshes (armadillo, bunny, etc.)
		// Load shaders
		std::array<VkPipelineShaderStageCreateInfo, 2>					shaderStages							= {};
		VkGraphicsPipelineCreateInfo									pipelineCreateInfo						= vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass, 0);
		pipelineCreateInfo.pInputAssemblyState						= &stateInputAssembly;
		pipelineCreateInfo.pRasterizationState						= &stateRasterization;
		pipelineCreateInfo.pColorBlendState							= &stateColorBlend;
		pipelineCreateInfo.pMultisampleState						= &stateMultisample;
		pipelineCreateInfo.pViewportState							= &stateViewport;
		pipelineCreateInfo.pDepthStencilState						= &stateDepthStencil;
		pipelineCreateInfo.pDynamicState							= &dynamicState;
		pipelineCreateInfo.stageCount								= static_cast<uint32_t>(shaderStages.size());
		pipelineCreateInfo.pStages									= shaderStages.data();

		VkVertexInputBindingDescription vertexInputBinding			= vks::initializers::vertexInputBindingDescription(VERTEX_BUFFER_BIND_ID, vertexLayout.stride(), VK_VERTEX_INPUT_RATE_VERTEX);

		// Attribute descriptions
		// Describes memory layout and shader positions
		std::vector<VkVertexInputAttributeDescription>					vertexInputAttributes					= 
			{	vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 0, VK_FORMAT_R32G32B32_SFLOAT	, 0)					// Location 0: Position		
			,	vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 1, VK_FORMAT_R32G32B32_SFLOAT	, sizeof(float) * 3)	// Location 1: Normal		
			,	vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 2, VK_FORMAT_R32G32_SFLOAT	, sizeof(float) * 5)	// Location 2: Texture coordinates		
			,	vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 3, VK_FORMAT_R32G32B32_SFLOAT	, sizeof(float) * 8)	// Location 3: Color		
			};

		VkPipelineVertexInputStateCreateInfo							stateVertexInput						= vks::initializers::pipelineVertexInputStateCreateInfo();
		stateVertexInput.vertexBindingDescriptionCount				= 1;
		stateVertexInput.pVertexBindingDescriptions					= &vertexInputBinding;
		stateVertexInput.vertexAttributeDescriptionCount			= static_cast<uint32_t>(vertexInputAttributes.size());
		stateVertexInput.pVertexAttributeDescriptions				= vertexInputAttributes.data();
		pipelineCreateInfo.pVertexInputState						= &stateVertexInput;

		// Default mesh rendering pipeline
		shaderStages[0]												= loadShader(getAssetPath() + "shaders/vulkanscene/mesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1]												= loadShader(getAssetPath() + "shaders/vulkanscene/mesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.models));

		// Pipeline for the logos
		shaderStages[0]												= loadShader(getAssetPath() + "shaders/vulkanscene/logo.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1]												= loadShader(getAssetPath() + "shaders/vulkanscene/logo.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.logos));

		// Pipeline for the sky sphere
		stateRasterization.cullMode									= VK_CULL_MODE_FRONT_BIT; // Inverted culling
		stateDepthStencil.depthWriteEnable							= VK_FALSE; // No depth writes
		shaderStages[0]												= loadShader(getAssetPath() + "shaders/vulkanscene/skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1]												= loadShader(getAssetPath() + "shaders/vulkanscene/skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.skybox));
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void														prepareUniformBuffers					()									{
		vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformData.meshVS, sizeof(uboVS));
		updateUniformBuffers();
	}

	void														updateUniformBuffers					()									{
		uboVS.projection											= glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.1f, 256.0f);
		uboVS.view													= glm::lookAt(glm::vec3(0, 0, -zoom), cameraPos, glm::vec3(0, 1, 0));
		uboVS.model													= glm::mat4();
		uboVS.model													= glm::rotate(uboVS.model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		uboVS.model													= glm::rotate(uboVS.model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		uboVS.model													= glm::rotate(uboVS.model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
		uboVS.normal												= glm::inverseTranspose(uboVS.view * uboVS.model);
		uboVS.lightPos												= lightPos;

		VK_CHECK_RESULT(uniformData.meshVS.map());
		memcpy(uniformData.meshVS.mapped, &uboVS, sizeof(uboVS));
		uniformData.meshVS.unmap();
	}

	void														draw									()									{
		VulkanExampleBase::prepareFrame();

		submitInfo.commandBufferCount								= 1;
		submitInfo.pCommandBuffers									= &drawCmdBuffers[currentBuffer];
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

		VulkanExampleBase::submitFrame();
	}

	void														prepare									()									{
		VulkanExampleBase::prepare	();
		loadAssets					();
		prepareUniformBuffers		();
		setupDescriptorSetLayout	();
		preparePipelines			();
		setupDescriptorPool			();
		setupDescriptorSet			();
		buildCommandBuffers			();
		prepared													= true;
	}

	virtual void												render									()									{ if (prepared) draw();		}
	virtual void												viewChanged								()									{ updateUniformBuffers();	}

};

VULKAN_EXAMPLE_EXPORT_FUNCTIONS()