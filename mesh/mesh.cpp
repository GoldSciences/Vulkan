// Vulkan Example - Model loading and rendering
// 
// Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
// 
// This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
#include "vulkanexamplebase.h"
#include "VulkanTexture.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <assimp/Importer.hpp> 
#include <assimp/scene.h>     
#include <assimp/postprocess.h>
#include <assimp/cimport.h>


#define VERTEX_BUFFER_BIND_ID	0
#define ENABLE_VALIDATION		false

class VulkanExample : public VulkanExampleBase
{
public:
	bool														wireframe										= false;

	struct {
		vks::Texture2D												colorMap;
	}															textures;

	struct {
		VkPipelineVertexInputStateCreateInfo						inputState;
		std::vector<VkVertexInputBindingDescription>				bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription>				attributeDescriptions;
	}															vertices;

	// Vertex layout used in this example
	// This must fit input locations of the vertex shader used to render the model
	typedef VertexPNUC											Vertex;		// Vertex layout used in this example

	// Contains all Vulkan resources required to represent vertex and index buffers for a model
	// This is for demonstration and learning purposes, the other examples use a model loader class for easy access
	struct Model {
		vks::VertexBuffer											vertices;
		vks::IndexBuffer											indices;

		// Destroys all Vulkan resources created for this model
		void														destroy											(VkDevice device_)					{
			vertices	.destroy(device_);
			indices		.destroy(device_);
		}
	}															model;

	struct {
		vks::Buffer													scene;
	}															uniformBuffers;

	struct {
		glm::mat4													projection;
		glm::mat4													model;
		glm::vec4													lightPos										= glm::vec4(25.0f, 5.0f, 5.0f, 1.0f);
	}															uboVS;

	struct {
		VkPipeline													solid											= VK_NULL_HANDLE;
		VkPipeline													wireframe										= VK_NULL_HANDLE;
	}															pipelines;

	VkPipelineLayout											pipelineLayout									= VK_NULL_HANDLE;
	VkDescriptorSet												descriptorSet									= VK_NULL_HANDLE;
	VkDescriptorSetLayout										descriptorSetLayout								= VK_NULL_HANDLE;

																VulkanExample									()									: VulkanExampleBase(ENABLE_VALIDATION)	{
		zoom														= -5.5f;
		zoomSpeed													= 2.5f;
		rotationSpeed												= 0.5f;
		rotation													= { -0.5f, -112.75f, 0.0f };
		cameraPos													= { 0.1f, 1.1f, 0.0f };
		enableTextOverlay											= true;
		title														= "Vulkan Example - Model rendering";
		enabledFeatures.fillModeNonSolid							= VK_TRUE;	// Enable physical device features required for this example				
	}

																~VulkanExample									()									{
		// Clean up used Vulkan resources 
		// Note : Inherited destructor cleans up resources stored in base class
		vkDestroyPipeline				(device, pipelines.solid		, nullptr);
		vkDestroyPipeline				(device, pipelines.wireframe	, nullptr);

		vkDestroyPipelineLayout			(device, pipelineLayout			, nullptr);
		vkDestroyDescriptorSetLayout	(device, descriptorSetLayout	, nullptr);

		model					.destroy(device);

		textures.colorMap		.destroy();
		uniformBuffers.scene	.destroy();
	}

	void														reBuildCommandBuffers							()									{
		if (!checkCommandBuffers()) {
			destroyCommandBuffers();
			createCommandBuffers();
		}
		buildCommandBuffers();
	}

	void														buildCommandBuffers								()									{
		VkCommandBufferBeginInfo										cmdBufInfo										= vks::initializers::commandBufferBeginInfo();

		VkClearValue													clearValues[2];
		clearValues[0].color										= defaultClearColor;
		clearValues[1].depthStencil									= { 1.0f, 0 };

		VkRenderPassBeginInfo											renderPassBeginInfo								= vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass								= renderPass;
		renderPassBeginInfo.renderArea.offset.x						= 0;
		renderPassBeginInfo.renderArea.offset.y						= 0;
		renderPassBeginInfo.renderArea.extent.width					= width;
		renderPassBeginInfo.renderArea.extent.height				= height;
		renderPassBeginInfo.clearValueCount							= 2;
		renderPassBeginInfo.pClearValues							= clearValues;

		for (size_t i = 0; i < drawCmdBuffers.size(); ++i) {
			// Set target frame buffer
			renderPassBeginInfo.framebuffer								= frameBuffers[i];

			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

			vkCmdBeginRenderPass	(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport														viewport										= vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
			vkCmdSetViewport		(drawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D														scissor											= vks::initializers::rect2D(width, height, 0, 0);
			vkCmdSetScissor			(drawCmdBuffers[i], 0, 1, &scissor);

			vkCmdBindDescriptorSets	(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);
			vkCmdBindPipeline		(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, wireframe ? pipelines.wireframe : pipelines.solid);

			VkDeviceSize													offsets[1]										= { 0 };
			
			vkCmdBindVertexBuffers	(drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &model.vertices.buffer, offsets);	// Bind mesh vertex buffer
			vkCmdBindIndexBuffer	(drawCmdBuffers[i], model.indices.buffer, 0, VK_INDEX_TYPE_UINT32);				// Bind mesh index buffer
			vkCmdDrawIndexed		(drawCmdBuffers[i], model.indices.count, 1, 0, 0, 0);							// Render mesh vertex buffer using it's indices

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	// Load a model from file using the ASSIMP model loader and generate all resources required to render the model
	void														loadModel										(std::string filename)				{
		// Load the model from file using ASSIMP
		const aiScene													* scene;
		Assimp::Importer												Importer;		
		static const int												assimpFlags										= aiProcess_FlipWindingOrder | aiProcess_Triangulate | aiProcess_PreTransformVertices;	// Flags for loading the mesh

#if defined(__ANDROID__)
		// Meshes are stored inside the apk on Android (compressed)
		// So they need to be loaded via the asset manager

		AAsset															* asset											= AAssetManager_open(androidApp->activity->assetManager, filename.c_str(), AASSET_MODE_STREAMING);
		assert(asset);
		size_t															size											= AAsset_getLength(asset);

		assert(size > 0);

		void															* meshData										= malloc(size);
		AAsset_read(asset, meshData, size);
		AAsset_close(asset);

		scene														= Importer.ReadFileFromMemory(meshData, size, assimpFlags);

		free(meshData);
#else
		scene														= Importer.ReadFile(filename.c_str(), assimpFlags);
#endif

		// Generate vertex buffer from ASSIMP scene data
		float															scale											= 1.0f;
		std::vector<Vertex>												vertexBuffer;

		// Iterate through all meshes in the file and extract the vertex components
		for (uint32_t m = 0; m < scene->mNumMeshes; m++)
			for (uint32_t v = 0; v < scene->mMeshes[m]->mNumVertices; v++) {
				Vertex															vertex;

				// Use glm make_* functions to convert ASSIMP vectors to glm vectors
				vertex.position												= glm::make_vec3(&scene->mMeshes[m]->mVertices[v].x) * scale;
				vertex.normal												= glm::make_vec3(&scene->mMeshes[m]->mNormals[v].x);
				
				vertex.uv													= glm::make_vec2(&scene->mMeshes[m]->mTextureCoords[0][v].x);					// Texture coordinates and colors may have multiple channels, we only use the first [0] one
				vertex.color												= (scene->mMeshes[m]->HasVertexColors(0)) ? glm::make_vec3(&scene->mMeshes[m]->mColors[0][v].r) : glm::vec3(1.0f);	// Mesh may not have vertex colors

				vertex.position.y											*= -1.0f;	// Vulkan uses a right-handed NDC (contrary to OpenGL), so simply flip Y-Axis

				vertexBuffer.push_back(vertex);
			}

		size_t															vertexBufferSize								= vertexBuffer.size() * sizeof(Vertex);

		// Generate index buffer from ASSIMP scene data
		std::vector<uint32_t>											indexBuffer;
		for (uint32_t m = 0; m < scene->mNumMeshes; m++) {
			uint32_t														indexBase										= static_cast<uint32_t>(indexBuffer.size());
			for (uint32_t f = 0; f < scene->mMeshes[m]->mNumFaces; f++)
				for (uint32_t i = 0; i < 3; i++)	// We assume that all faces are triangulated
					indexBuffer.push_back(scene->mMeshes[m]->mFaces[f].mIndices[i] + indexBase);
		}
		size_t															indexBufferSize										= indexBuffer.size() * sizeof(uint32_t);
		model.indices.count											= static_cast<uint32_t>(indexBuffer.size());

		// Static mesh should always be device local

		bool															useStaging											= true;

		if (useStaging) {
			struct {
				VkBuffer														buffer;
				VkDeviceMemory													memory;
			}																vertexStaging, indexStaging;

			// Create staging buffers
			VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexBufferSize, &vertexStaging.buffer, &vertexStaging.memory,vertexBuffer.data()));	// Vertex data
			VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, indexBufferSize, &indexStaging.buffer, &indexStaging.memory, indexBuffer.data()));		// Index data

			// Create device local buffers
			VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBufferSize, &model.vertices.buffer, &model.vertices.memory));	// Vertex buffer
			VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBufferSize, &model.indices.buffer, &model.indices.memory));		// Index buffer

			// Copy from staging buffers
			VkCommandBuffer													copyCmd											= VulkanExampleBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

			VkBufferCopy													copyRegion										= {};
			copyRegion.size												= vertexBufferSize	; vkCmdCopyBuffer(copyCmd, vertexStaging.buffer	, model.vertices.buffer	, 1, &copyRegion);
			copyRegion.size												= indexBufferSize	; vkCmdCopyBuffer(copyCmd, indexStaging.buffer	, model.indices.buffer	, 1, &copyRegion);

			VulkanExampleBase::flushCommandBuffer(copyCmd, queue, true);

			vkDestroyBuffer	(device, vertexStaging.buffer, nullptr);
			vkFreeMemory	(device, vertexStaging.memory, nullptr);
			vkDestroyBuffer	(device, indexStaging.buffer, nullptr);
			vkFreeMemory	(device, indexStaging.memory, nullptr);
		}
		else {
			VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, vertexBufferSize, &model.vertices.buffer, &model.vertices.memory, vertexBuffer.data()));		// Vertex buffer
			VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, indexBufferSize, &model.indices.buffer, &model.indices.memory, indexBuffer.data()));			// Index buffer
		}
	}

	void														loadAssets										()									{
		loadModel						(getAssetPath() + "models/voyager/voyager.dae");
		textures.colorMap.loadFromFile	(getAssetPath() + "models/voyager/voyager.ktx", VK_FORMAT_BC3_UNORM_BLOCK, vulkanDevice, queue);
	}

	void														setupVertexDescriptions							()									{
		// Binding description
		vertices.bindingDescriptions.resize(1);
		vertices.bindingDescriptions[0]								= vks::initializers::vertexInputBindingDescription(VERTEX_BUFFER_BIND_ID, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX);
		
		// Attribute descriptions. Describes memory layout and shader positions
		vertices.attributeDescriptions.resize(4);
		vertices.attributeDescriptions[0]							= vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 0, VK_FORMAT_R32G32B32_SFLOAT	, offsetof(Vertex, position));	// Location 0 : Position
		vertices.attributeDescriptions[1]							= vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 1, VK_FORMAT_R32G32B32_SFLOAT	, offsetof(Vertex, normal));	// Location 1 : Normal
		vertices.attributeDescriptions[2]							= vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 2, VK_FORMAT_R32G32_SFLOAT		, offsetof(Vertex, uv));		// Location 2 : Texture coordinates
		vertices.attributeDescriptions[3]							= vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 3, VK_FORMAT_R32G32B32_SFLOAT	, offsetof(Vertex, color));		// Location 3 : Color

		vertices.inputState											= vks::initializers::pipelineVertexInputStateCreateInfo();
		vertices.inputState.vertexBindingDescriptionCount			= static_cast<uint32_t>(vertices.bindingDescriptions.size());
		vertices.inputState.pVertexBindingDescriptions				= vertices.bindingDescriptions.data();
		vertices.inputState.vertexAttributeDescriptionCount			= static_cast<uint32_t>(vertices.attributeDescriptions.size());
		vertices.inputState.pVertexAttributeDescriptions			= vertices.attributeDescriptions.data();
	}

	void														setupDescriptorPool								()									{
		// Example uses one ubo and one combined image sampler
		std::vector<VkDescriptorPoolSize>								poolSizes										=
			{	vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)
			,	vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
			};

		VkDescriptorPoolCreateInfo										descriptorPoolInfo								= vks::initializers::descriptorPoolCreateInfo(static_cast<uint32_t>(poolSizes.size()), poolSizes.data(), 1);
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
	}

	void														setupDescriptorSetLayout						()									{
		std::vector<VkDescriptorSetLayoutBinding>						setLayoutBindings								=
			{	vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER			, VK_SHADER_STAGE_VERTEX_BIT	, 0)	// Binding 0 : Vertex shader uniform buffer
			,	vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER	, VK_SHADER_STAGE_FRAGMENT_BIT	, 1)	// Binding 1 : Fragment shader combined sampler
			};

		VkDescriptorSetLayoutCreateInfo									descriptorLayout								= vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

		VkPipelineLayoutCreateInfo										pPipelineLayoutCreateInfo						= vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));
	}

	void														setupDescriptorSet								()									{
		VkDescriptorSetAllocateInfo										allocInfo										= vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

		VkDescriptorImageInfo											texDescriptor									= vks::initializers::descriptorImageInfo(textures.colorMap.sampler, textures.colorMap.view, VK_IMAGE_LAYOUT_GENERAL);
		std::vector<VkWriteDescriptorSet>								writeDescriptorSets								=
			{	vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.scene.descriptor)	// Binding 0 : Vertex shader uniform buffer
			,	vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &texDescriptor)				// Binding 1 : Color map 
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
		std::vector<VkDynamicState>										dynamicStateEnables								= {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
		VkPipelineDynamicStateCreateInfo								dynamicState									= vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);

		// Solid rendering pipeline
		// Load shaders
		std::array<VkPipelineShaderStageCreateInfo, 2>					shaderStages									= {};

		shaderStages[0]												= loadShader(getAssetPath() + "shaders/mesh/mesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1]												= loadShader(getAssetPath() + "shaders/mesh/mesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

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

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.solid));

		// Wire frame rendering pipeline
		rasterizationState.polygonMode								= VK_POLYGON_MODE_LINE;
		rasterizationState.lineWidth								= 1.0f;

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.wireframe));
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void														prepareUniformBuffers							()									{
		// Vertex shader uniform buffer block
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffers.scene, sizeof(uboVS)));
		VK_CHECK_RESULT(uniformBuffers.scene.map());		// Map persistent
		updateUniformBuffers();
	}

	void														updateUniformBuffers							()									{
		uboVS.projection											= glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.1f, 256.0f);
		glm::mat4														viewMatrix										= glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, zoom));

		uboVS.model													= viewMatrix * glm::translate(glm::mat4(), cameraPos);
		uboVS.model													= glm::rotate(uboVS.model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		uboVS.model													= glm::rotate(uboVS.model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		uboVS.model													= glm::rotate(uboVS.model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		memcpy(uniformBuffers.scene.mapped, &uboVS, sizeof(uboVS));
	}

	void														draw											()									{
		VulkanExampleBase::prepareFrame();

		// Command buffer to be sumitted to the queue
		submitInfo.commandBufferCount								= 1;
		submitInfo.pCommandBuffers									= &drawCmdBuffers[currentBuffer];

		// Submit to queue
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

		VulkanExampleBase::submitFrame();
	}

	void														prepare											()									{	
		VulkanExampleBase::prepare	();
		loadAssets					();
		setupVertexDescriptions		();
		prepareUniformBuffers		();
		setupDescriptorSetLayout	();
		preparePipelines			();
		setupDescriptorPool			();
		setupDescriptorSet			();
		buildCommandBuffers			();
		prepared													= true;
	}

	virtual void												render											()									{ if (prepared) draw();		}
	virtual void												viewChanged										()									{ updateUniformBuffers();	}
	virtual void												keyPressed										(uint32_t keyCode)					{
		switch (keyCode) {
		case KEY_W				:
		case GAMEPAD_BUTTON_A	:
			wireframe													= !wireframe;
			reBuildCommandBuffers();
			break;
		}
	}

	virtual void												getOverlayText									(VulkanTextOverlay *textOverlay_)	{
#if defined(__ANDROID__)
		textOverlay_->addText("Press \"Button A\" to toggle wireframe", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
#else
		textOverlay_->addText("Press \"w\" to toggle wireframe", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
#endif
	}
};

VULKAN_EXAMPLE_EXPORT_FUNCTIONS()