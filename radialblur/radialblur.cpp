// Vulkan Example - Fullscreen radial blur (Single pass offscreen effect)
// 
// Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
// 
// This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
#include "vulkanexamplebase.h"
#include "VulkanTexture.hpp"
#include "VulkanModel.hpp"

#define VERTEX_BUFFER_BIND_ID	0
#define ENABLE_VALIDATION		false

// Offscreen frame buffer properties
#define FB_DIM					512
#define FB_COLOR_FORMAT			VK_FORMAT_R8G8B8A8_UNORM

class VulkanExample : public VulkanExampleBase
{
public:
	bool														blur											= true;
	bool														displayTexture									= false;

	struct {
		vks::Texture2D												gradient;
	}															textures;

	// Vertex layout for the models
	vks::VertexLayout											vertexLayout									= vks::VertexLayout(
		{	vks::VERTEX_COMPONENT_POSITION
		,	vks::VERTEX_COMPONENT_UV
		,	vks::VERTEX_COMPONENT_COLOR
		,	vks::VERTEX_COMPONENT_NORMAL
		});

	struct {
		vks::Model													example;
	}															models;

	vks::VertexInputStateAndDescriptions						vertices;

	struct {
		vks::Buffer													scene;
		vks::Buffer													blurParams;
	}															uniformBuffers;

	struct UboVS {
		glm::mat4													projection;
		glm::mat4													model;
		float														gradientPos										= 0.0f;
	}															uboScene;

	struct UboBlurParams {
		float														radialBlurScale									= 0.35f;
		float														radialBlurStrength								= 0.75f;
		glm::vec2													radialOrigin									= glm::vec2(0.5f, 0.5f);
	}															uboBlurParams;

	struct {
		VkPipeline													radialBlur										= VK_NULL_HANDLE;
		VkPipeline													colorPass										= VK_NULL_HANDLE;
		VkPipeline													phongPass										= VK_NULL_HANDLE;
		VkPipeline													offscreenDisplay								= VK_NULL_HANDLE;
	}															pipelines;

	struct {
		VkPipelineLayout											radialBlur										= VK_NULL_HANDLE;
		VkPipelineLayout											scene											= VK_NULL_HANDLE;
	}															pipelineLayouts;

	struct {
		VkDescriptorSet												scene											= VK_NULL_HANDLE;
		VkDescriptorSet												radialBlur										= VK_NULL_HANDLE;
	}															descriptorSets;

	struct {
		VkDescriptorSetLayout										scene											= VK_NULL_HANDLE;
		VkDescriptorSetLayout										radialBlur										= VK_NULL_HANDLE;
	}															descriptorSetLayouts;

	vks::OffscreenPass											offscreenPass;

																VulkanExample									()										: VulkanExampleBase(ENABLE_VALIDATION) {
		zoom														= -10.0f;
		rotation													= { -16.25f, -28.75f, 0.0f };
		timerSpeed													*= 0.5f;
		enableTextOverlay											= true;
		title														= "Vulkan Example - Radial blur";
	}

																~VulkanExample									()										{
		// Clean up used Vulkan resources 
		// Note : Inherited destructor cleans up resources stored in base class

		// Frame buffer
		offscreenPass.color		.destroy(device);	// Color attachment
		offscreenPass.depth		.destroy(device);	// Depth attachment
		

		vkDestroyRenderPass				(device, offscreenPass.renderPass			, nullptr);
		vkDestroySampler				(device, offscreenPass.sampler				, nullptr);
		vkDestroyFramebuffer			(device, offscreenPass.frameBuffer			, nullptr);

		vkDestroyPipeline				(device, pipelines.radialBlur				, nullptr);
		vkDestroyPipeline				(device, pipelines.phongPass				, nullptr);
		vkDestroyPipeline				(device, pipelines.colorPass				, nullptr);
		vkDestroyPipeline				(device, pipelines.offscreenDisplay			, nullptr);

		vkDestroyPipelineLayout			(device, pipelineLayouts.radialBlur			, nullptr);
		vkDestroyPipelineLayout			(device, pipelineLayouts.scene				, nullptr);

		vkDestroyDescriptorSetLayout	(device, descriptorSetLayouts.scene			, nullptr);
		vkDestroyDescriptorSetLayout	(device, descriptorSetLayouts.radialBlur	, nullptr);

		models.example				.destroy();
		uniformBuffers.scene		.destroy();
		uniformBuffers.blurParams	.destroy();

		vkFreeCommandBuffers			(device, cmdPool, 1, &offscreenPass.commandBuffer);
		vkDestroySemaphore				(device, offscreenPass.semaphore			, nullptr);

		textures.gradient.destroy();
	}

	// Setup the offscreen framebuffer for rendering the blurred scene
	// The color attachment of this framebuffer will then be used to sample frame in the fragment shader of the final pass
	void														prepareOffscreen								()										{
		offscreenPass.width											= FB_DIM;
		offscreenPass.height										= FB_DIM;

		// Find a suitable depth format
		VkFormat														fbDepthFormat;
		VkBool32														validDepthFormat								= vks::tools::getSupportedDepthFormat(physicalDevice, &fbDepthFormat);
		assert(validDepthFormat);

		// Color attachment
		VkImageCreateInfo												image											= vks::initializers::imageCreateInfo();
		image.imageType												= VK_IMAGE_TYPE_2D;
		image.format												= FB_COLOR_FORMAT;
		image.extent.width											= offscreenPass.width;
		image.extent.height											= offscreenPass.height;
		image.extent.depth											= 1;
		image.mipLevels												= 1;
		image.arrayLayers											= 1;
		image.samples												= VK_SAMPLE_COUNT_1_BIT;
		image.tiling												= VK_IMAGE_TILING_OPTIMAL;
		// We will sample directly from the color attachment	
		image.usage													= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		VkMemoryAllocateInfo											memAlloc										= vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements											memReqs;

		VK_EVAL(vkCreateImage		(device, &image, nullptr, &offscreenPass.color.image));
		vkGetImageMemoryRequirements		(device, offscreenPass.color.image, &memReqs);
		memAlloc.allocationSize										= memReqs.size;
		memAlloc.memoryTypeIndex									= vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_EVAL(vkAllocateMemory	(device, &memAlloc, nullptr, &offscreenPass.color.memory));
		VK_EVAL(vkBindImageMemory	(device, offscreenPass.color.image, offscreenPass.color.memory, 0));

		VkImageViewCreateInfo											colorImageView									= vks::initializers::imageViewCreateInfo();
		colorImageView.viewType										= VK_IMAGE_VIEW_TYPE_2D;
		colorImageView.format										= FB_COLOR_FORMAT;
		colorImageView.subresourceRange								= {};
		colorImageView.subresourceRange.aspectMask					= VK_IMAGE_ASPECT_COLOR_BIT;
		colorImageView.subresourceRange.baseMipLevel				= 0;
		colorImageView.subresourceRange.levelCount					= 1;
		colorImageView.subresourceRange.baseArrayLayer				= 0;
		colorImageView.subresourceRange.layerCount					= 1;
		colorImageView.image										= offscreenPass.color.image;
		VK_EVAL(vkCreateImageView	(device, &colorImageView, nullptr, &offscreenPass.color.view));

		// Create sampler to sample from the attachment in the fragment shader
		VkSamplerCreateInfo												samplerInfo										= vks::initializers::samplerCreateInfo();
		samplerInfo.magFilter										= VK_FILTER_LINEAR;
		samplerInfo.minFilter										= VK_FILTER_LINEAR;
		samplerInfo.mipmapMode										= VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.addressModeU									= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV									= samplerInfo.addressModeU;
		samplerInfo.addressModeW									= samplerInfo.addressModeU;
		samplerInfo.mipLodBias										= 0.0f;
		samplerInfo.maxAnisotropy									= 0;
		samplerInfo.minLod											= 0.0f;
		samplerInfo.maxLod											= 1.0f;
		samplerInfo.borderColor										= VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_EVAL(vkCreateSampler		(device, &samplerInfo, nullptr, &offscreenPass.sampler));

		// Depth stencil attachment
		image.format												= fbDepthFormat;
		image.usage													= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		VK_EVAL(vkCreateImage		(device, &image, nullptr, &offscreenPass.depth.image));
		vkGetImageMemoryRequirements		(device, offscreenPass.depth.image, &memReqs);
		memAlloc.allocationSize										= memReqs.size;
		memAlloc.memoryTypeIndex									= vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_EVAL(vkAllocateMemory	(device, &memAlloc, nullptr, &offscreenPass.depth.memory));
		VK_EVAL(vkBindImageMemory	(device, offscreenPass.depth.image, offscreenPass.depth.memory, 0));

		VkImageViewCreateInfo											depthStencilView								= vks::initializers::imageViewCreateInfo();
		depthStencilView.viewType									= VK_IMAGE_VIEW_TYPE_2D;
		depthStencilView.format										= fbDepthFormat;
		depthStencilView.flags										= 0;
		depthStencilView.subresourceRange							= {};
		depthStencilView.subresourceRange.aspectMask				= VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		depthStencilView.subresourceRange.baseMipLevel				= 0;
		depthStencilView.subresourceRange.levelCount				= 1;
		depthStencilView.subresourceRange.baseArrayLayer			= 0;
		depthStencilView.subresourceRange.layerCount				= 1;
		depthStencilView.image										= offscreenPass.depth.image;
		VK_EVAL(vkCreateImageView	(device, &depthStencilView, nullptr, &offscreenPass.depth.view));

		// Create a separate render pass for the offscreen rendering as it may differ from the one used for scene rendering

		std::array<VkAttachmentDescription, 2>							attchmentDescriptions							= {};
		// Color attachment
		attchmentDescriptions[0].format								= FB_COLOR_FORMAT;
		attchmentDescriptions[0].samples							= VK_SAMPLE_COUNT_1_BIT;
		attchmentDescriptions[0].loadOp								= VK_ATTACHMENT_LOAD_OP_CLEAR;
		attchmentDescriptions[0].storeOp							= VK_ATTACHMENT_STORE_OP_STORE;
		attchmentDescriptions[0].stencilLoadOp						= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attchmentDescriptions[0].stencilStoreOp						= VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attchmentDescriptions[0].initialLayout						= VK_IMAGE_LAYOUT_UNDEFINED;
		attchmentDescriptions[0].finalLayout						= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		// Depth attachment
		attchmentDescriptions[1].format								= fbDepthFormat;
		attchmentDescriptions[1].samples							= VK_SAMPLE_COUNT_1_BIT;
		attchmentDescriptions[1].loadOp								= VK_ATTACHMENT_LOAD_OP_CLEAR;
		attchmentDescriptions[1].storeOp							= VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attchmentDescriptions[1].stencilLoadOp						= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attchmentDescriptions[1].stencilStoreOp						= VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attchmentDescriptions[1].initialLayout						= VK_IMAGE_LAYOUT_UNDEFINED;
		attchmentDescriptions[1].finalLayout						= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference											colorReference									= { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		VkAttachmentReference											depthReference									= { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

		VkSubpassDescription											subpassDescription								= {};
		subpassDescription.pipelineBindPoint						= VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount						= 1;
		subpassDescription.pColorAttachments						= &colorReference;
		subpassDescription.pDepthStencilAttachment					= &depthReference;

		// Use subpass dependencies for layout transitions
		std::array<VkSubpassDependency, 2>								dependencies;
		dependencies[0].srcSubpass									= VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass									= 0;
		dependencies[0].srcStageMask								= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask								= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask								= VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask								= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags								= VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[1].srcSubpass									= 0;
		dependencies[1].dstSubpass									= VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask								= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask								= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask								= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask								= VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags								= VK_DEPENDENCY_BY_REGION_BIT;

		// Create the actual renderpass
		VkRenderPassCreateInfo											renderPassInfo									= {};
		renderPassInfo.sType										= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount								= static_cast<uint32_t>(attchmentDescriptions.size());
		renderPassInfo.pAttachments									= attchmentDescriptions.data();
		renderPassInfo.subpassCount									= 1;
		renderPassInfo.pSubpasses									= &subpassDescription;
		renderPassInfo.dependencyCount								= static_cast<uint32_t>(dependencies.size());
		renderPassInfo.pDependencies								= dependencies.data();

		VK_EVAL(vkCreateRenderPass	(device, &renderPassInfo, nullptr, &offscreenPass.renderPass));

		VkImageView														attachments[2];
		attachments[0]												= offscreenPass.color.view;
		attachments[1]												= offscreenPass.depth.view;

		VkFramebufferCreateInfo											fbufCreateInfo									= vks::initializers::framebufferCreateInfo();
		fbufCreateInfo.renderPass									= offscreenPass.renderPass;
		fbufCreateInfo.attachmentCount								= 2;
		fbufCreateInfo.pAttachments									= attachments;
		fbufCreateInfo.width										= offscreenPass.width;
		fbufCreateInfo.height										= offscreenPass.height;
		fbufCreateInfo.layers										= 1;

		VK_EVAL(vkCreateFramebuffer	(device, &fbufCreateInfo, nullptr, &offscreenPass.frameBuffer));

		// Fill a descriptor for later use in a descriptor set 
		offscreenPass.descriptor.imageLayout						= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		offscreenPass.descriptor.imageView							= offscreenPass.color.view;
		offscreenPass.descriptor.sampler							= offscreenPass.sampler;
	}

	// Sets up the command buffer that renders the scene to the offscreen frame buffer
	void														buildOffscreenCommandBuffer						()										{
		if (offscreenPass.commandBuffer == VK_NULL_HANDLE)
			offscreenPass.commandBuffer									= VulkanExampleBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);

		if (offscreenPass.semaphore == VK_NULL_HANDLE) {
			VkSemaphoreCreateInfo											semaphoreCreateInfo								= vks::initializers::semaphoreCreateInfo();
			VK_EVAL(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &offscreenPass.semaphore));
		}

		VkCommandBufferBeginInfo										cmdBufInfo										= vks::initializers::commandBufferBeginInfo();

		VkClearValue													clearValues[2];
		clearValues[0].color										= { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[1].depthStencil									= { 1.0f, 0 };

		VkRenderPassBeginInfo											renderPassBeginInfo								= vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass								= offscreenPass.renderPass;
		renderPassBeginInfo.framebuffer								= offscreenPass.frameBuffer;
		renderPassBeginInfo.renderArea.extent.width					= offscreenPass.width;
		renderPassBeginInfo.renderArea.extent.height				= offscreenPass.height;
		renderPassBeginInfo.clearValueCount							= 2;
		renderPassBeginInfo.pClearValues							= clearValues;

		VK_EVAL(vkBeginCommandBuffer(offscreenPass.commandBuffer, &cmdBufInfo));

		VkViewport														viewport										= vks::initializers::viewport((float)offscreenPass.width, (float)offscreenPass.height, 0.0f, 1.0f);
		vkCmdSetViewport		(offscreenPass.commandBuffer, 0, 1, &viewport);

		VkRect2D														scissor											= vks::initializers::rect2D(offscreenPass.width, offscreenPass.height, 0, 0);
		vkCmdSetScissor			(offscreenPass.commandBuffer, 0, 1, &scissor);

		vkCmdBeginRenderPass	(offscreenPass.commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindDescriptorSets	(offscreenPass.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.scene, 0, 1, &descriptorSets.scene, 0, NULL);
		vkCmdBindPipeline		(offscreenPass.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.colorPass);

		VkDeviceSize													offsets	[1]										= { 0 };
		vkCmdBindVertexBuffers	(offscreenPass.commandBuffer, VERTEX_BUFFER_BIND_ID, 1, &models.example.vertices.buffer, offsets);
		vkCmdBindIndexBuffer	(offscreenPass.commandBuffer, models.example.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed		(offscreenPass.commandBuffer, models.example.indexCount, 1, 0, 0, 0);

		vkCmdEndRenderPass(offscreenPass.commandBuffer);

		VK_EVAL(vkEndCommandBuffer(offscreenPass.commandBuffer));
	}

	void														reBuildCommandBuffers							()										{
		if (!checkCommandBuffers()) {
			destroyCommandBuffers();
			createCommandBuffers();
		}
		buildCommandBuffers();
	}

	void														buildCommandBuffers								()										{
		VkCommandBufferBeginInfo										cmdBufInfo										= vks::initializers::commandBufferBeginInfo();

		VkClearValue													clearValues	[2];
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

		for (size_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			// Set target frame buffer
			renderPassBeginInfo.framebuffer								= frameBuffers[i];
			VK_EVAL(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));
			vkCmdBeginRenderPass	(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport														viewport										= vks::initializers::viewport((float)screenSize.Width, (float)screenSize.Height, 0.0f, 1.0f);
			vkCmdSetViewport		(drawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D														scissor											= vks::initializers::rect2D(screenSize.Width, screenSize.Height, 0, 0);
			vkCmdSetScissor			(drawCmdBuffers[i], 0, 1, &scissor);

			VkDeviceSize													offsets	[1]										= { 0 };

			// 3D scene
			vkCmdBindDescriptorSets	(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.scene, 0, 1, &descriptorSets.scene, 0, NULL);
			vkCmdBindPipeline		(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.phongPass);

			vkCmdBindVertexBuffers	(drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &models.example.vertices.buffer, offsets);
			vkCmdBindIndexBuffer	(drawCmdBuffers[i], models.example.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed		(drawCmdBuffers[i], models.example.indexCount, 1, 0, 0, 0);

			// Fullscreen triangle (clipped to a quad) with radial blur
			if (blur) {
				vkCmdBindDescriptorSets	(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.radialBlur, 0, 1, &descriptorSets.radialBlur, 0, NULL);
				vkCmdBindPipeline		(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, (displayTexture) ? pipelines.offscreenDisplay : pipelines.radialBlur);
				vkCmdDraw				(drawCmdBuffers[i], 3, 1, 0, 0);
			}

			vkCmdEndRenderPass		(drawCmdBuffers[i]);
			VK_EVAL(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void														loadAssets										()										{
		models.example		.loadFromFile(getAssetPath() + "models/glowsphere.dae", vertexLayout, 0.05f, vulkanDevice, queue);
		textures.gradient	.loadFromFile(getAssetPath() + "textures/particle_gradient_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
	}

	void														setupVertexDescriptions							()										{
		// Binding description
		vertices.bindingDescriptions.resize(1);
		vertices.bindingDescriptions[0]								= vks::initializers::vertexInputBindingDescription(VERTEX_BUFFER_BIND_ID, vertexLayout.stride(), VK_VERTEX_INPUT_RATE_VERTEX);

		// Attribute descriptions
		vertices.attributeDescriptions.resize(4);
		vertices.attributeDescriptions[0]							= vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);					// Location 0 : Position
		vertices.attributeDescriptions[1]							= vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 1, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 3);		// Location 1 : Texture coordinates
		vertices.attributeDescriptions[2]							= vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 2, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 5);	// Location 2 : Color
		vertices.attributeDescriptions[3]							= vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 3, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 8);	// Location 3 : Normal

		vertices.inputState											= vks::initializers::pipelineVertexInputStateCreateInfo();
		vertices.inputState.vertexBindingDescriptionCount			= static_cast<uint32_t>(vertices.bindingDescriptions.size());
		vertices.inputState.pVertexBindingDescriptions				= vertices.bindingDescriptions.data();
		vertices.inputState.vertexAttributeDescriptionCount			= static_cast<uint32_t>(vertices.attributeDescriptions.size());
		vertices.inputState.pVertexAttributeDescriptions			= vertices.attributeDescriptions.data();
	}

	void														setupDescriptorPool								()										{
		// Example uses three ubos and one image sampler
		std::vector<VkDescriptorPoolSize>								poolSizes										=
			{	vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4)
			,	vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6)
			};

		VkDescriptorPoolCreateInfo										descriptorPoolInfo								= vks::initializers::descriptorPoolCreateInfo(static_cast<uint32_t>(poolSizes.size()), poolSizes.data(), 2);
		VK_EVAL(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
	}

	void														setupDescriptorSetLayout						()										{
		std::vector<VkDescriptorSetLayoutBinding>						setLayoutBindings;
		VkDescriptorSetLayoutCreateInfo									descriptorLayout;
		VkPipelineLayoutCreateInfo										pPipelineLayoutCreateInfo;

		// Scene rendering
		setLayoutBindings											=
			{	vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0)				// Binding 0: Vertex shader uniform buffer
			,	vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)	// Binding 1: Fragment shader image sampler
			,	vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 2)			// Binding 2: Fragment shader uniform buffer
			};
		descriptorLayout											= vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
		VK_EVAL(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayouts.scene));
		pPipelineLayoutCreateInfo									= vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayouts.scene, 1);
		VK_EVAL(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayouts.scene));

		// Fullscreen radial blur
		setLayoutBindings											=
			{	vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 0)			// Binding 0 : Vertex shader uniform buffer
			,	vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)	// Binding 0: Fragment shader image sampler
			};
		descriptorLayout											= vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
		VK_EVAL(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayouts.radialBlur));
		pPipelineLayoutCreateInfo									= vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayouts.radialBlur, 1);
		VK_EVAL(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayouts.radialBlur));
	}

	void														setupDescriptorSet								()										{
		VkDescriptorSetAllocateInfo										descriptorSetAllocInfo;
		descriptorSetAllocInfo										= vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.scene, 1);	// Scene rendering
		VK_EVAL(vkAllocateDescriptorSets(device, &descriptorSetAllocInfo, &descriptorSets.scene));

		std::vector<VkWriteDescriptorSet>								offScreenWriteDescriptorSets					=
			{	vks::initializers::writeDescriptorSet(descriptorSets.scene, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.scene.descriptor)			// Binding 0: Vertex shader uniform buffer
			,	vks::initializers::writeDescriptorSet(descriptorSets.scene, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &textures.gradient.descriptor)	// Binding 1: Color gradient sampler
			};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(offScreenWriteDescriptorSets.size()), offScreenWriteDescriptorSets.data(), 0, NULL);

		// Fullscreen radial blur
		descriptorSetAllocInfo										= vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.radialBlur, 1);
		VK_EVAL(vkAllocateDescriptorSets(device, &descriptorSetAllocInfo, &descriptorSets.radialBlur));

		std::vector<VkWriteDescriptorSet>								writeDescriptorSets								=
			{	vks::initializers::writeDescriptorSet(descriptorSets.radialBlur, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.blurParams.descriptor)	// Binding 0: Vertex shader uniform buffer
			,	vks::initializers::writeDescriptorSet(descriptorSets.radialBlur, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &offscreenPass.descriptor)		// Binding 0: Fragment shader texture sampler
			};

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
	}


	void														preparePipelines								()										{
		VkPipelineInputAssemblyStateCreateInfo							inputAssemblyState								= vks::initializers::pipelineInputAssemblyStateCreateInfo	(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo							rasterizationState								= vks::initializers::pipelineRasterizationStateCreateInfo	(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState								blendAttachmentState							= vks::initializers::pipelineColorBlendAttachmentState		(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo								colorBlendState									= vks::initializers::pipelineColorBlendStateCreateInfo		(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo							depthStencilState								= vks::initializers::pipelineDepthStencilStateCreateInfo	(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo								viewportState									= vks::initializers::pipelineViewportStateCreateInfo		(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo							multisampleState								= vks::initializers::pipelineMultisampleStateCreateInfo		(VK_SAMPLE_COUNT_1_BIT, 0);
		
		std::vector<VkDynamicState>										dynamicStateEnables								= {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
		VkPipelineDynamicStateCreateInfo								dynamicState									= vks::initializers::pipelineDynamicStateCreateInfo			(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);

		std::array<VkPipelineShaderStageCreateInfo, 2>					shaderStages									= {};
		VkGraphicsPipelineCreateInfo									pipelineCreateInfo								= vks::initializers::pipelineCreateInfo						(pipelineLayouts.radialBlur, renderPass, 0);
		pipelineCreateInfo.pInputAssemblyState						= &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState						= &rasterizationState;
		pipelineCreateInfo.pColorBlendState							= &colorBlendState;
		pipelineCreateInfo.pMultisampleState						= &multisampleState;
		pipelineCreateInfo.pViewportState							= &viewportState;
		pipelineCreateInfo.pDepthStencilState						= &depthStencilState;
		pipelineCreateInfo.pDynamicState							= &dynamicState;
		pipelineCreateInfo.stageCount								= static_cast<uint32_t>(shaderStages.size());
		pipelineCreateInfo.pStages									= shaderStages.data();

		// Radial blur pipeline
		shaderStages[0]												= loadShader(getAssetPath() + "shaders/radialblur/radialblur.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1]												= loadShader(getAssetPath() + "shaders/radialblur/radialblur.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		// Empty vertex input state
		VkPipelineVertexInputStateCreateInfo							emptyInputState									= vks::initializers::pipelineVertexInputStateCreateInfo();
		pipelineCreateInfo.pVertexInputState						= &emptyInputState;
		pipelineCreateInfo.layout									= pipelineLayouts.radialBlur;
		// Additive blending
		blendAttachmentState.colorWriteMask							= 0xF;
		blendAttachmentState.blendEnable							= VK_TRUE;
		blendAttachmentState.colorBlendOp							= VK_BLEND_OP_ADD;
		blendAttachmentState.srcColorBlendFactor					= VK_BLEND_FACTOR_ONE;
		blendAttachmentState.dstColorBlendFactor					= VK_BLEND_FACTOR_ONE;
		blendAttachmentState.alphaBlendOp							= VK_BLEND_OP_ADD;
		blendAttachmentState.srcAlphaBlendFactor					= VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttachmentState.dstAlphaBlendFactor					= VK_BLEND_FACTOR_DST_ALPHA;
		VK_EVAL(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.radialBlur));

		// No blending (for debug display)
		blendAttachmentState.blendEnable							= VK_FALSE;
		VK_EVAL(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.offscreenDisplay));

		// Phong pass
		pipelineCreateInfo.layout									= pipelineLayouts.scene;
		shaderStages[0]												= loadShader(getAssetPath() + "shaders/radialblur/phongpass.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1]												= loadShader(getAssetPath() + "shaders/radialblur/phongpass.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		pipelineCreateInfo.pVertexInputState						= &vertices.inputState;
		blendAttachmentState.blendEnable							= VK_FALSE;
		depthStencilState.depthWriteEnable							= VK_TRUE;
		VK_EVAL(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.phongPass));

		// Color only pass (offscreen blur base)
		shaderStages[0]												= loadShader(getAssetPath() + "shaders/radialblur/colorpass.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1]												= loadShader(getAssetPath() + "shaders/radialblur/colorpass.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		pipelineCreateInfo.renderPass								= offscreenPass.renderPass;
		VK_EVAL(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.colorPass));
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void														prepareUniformBuffers							()										{
		VK_EVAL(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffers.scene, sizeof(uboScene)));							// Phong and color pass vertex shader uniform buffer
		VK_EVAL(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffers.blurParams, sizeof(uboBlurParams), &uboBlurParams));	// Fullscreen radial blur parameters

		// Map persistent
		VK_EVAL(uniformBuffers.scene.map());
		VK_EVAL(uniformBuffers.blurParams.map());

		updateUniformBuffersScene();
	}

	// Update uniform buffers for rendering the 3D scene
	void														updateUniformBuffersScene						()										{
		uboScene.projection											= glm::perspective(glm::radians(45.0f), (float)screenSize.Width / (float)screenSize.Height, 1.0f, 256.0f);
		glm::mat4														viewMatrix										= glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, zoom));

		uboScene.model												= glm::mat4();
		uboScene.model												= viewMatrix * glm::translate(uboScene.model, cameraPos);
		uboScene.model												= glm::rotate(uboScene.model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		uboScene.model												= glm::rotate(uboScene.model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		uboScene.model												= glm::rotate(uboScene.model, glm::radians(timer * 360.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		uboScene.model												= glm::rotate(uboScene.model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		if (!paused)
			uboScene.gradientPos										+= frameTimer * 0.1f;

		memcpy(uniformBuffers.scene.mapped, &uboScene, sizeof(uboScene));
	}

	void														draw											()										{
		VulkanExampleBase::prepareFrame();

		// Offscreen rendering
		submitInfo.pWaitSemaphores									= &semaphores.presentComplete;		// Wait for swap chain presentation to finish
		submitInfo.pSignalSemaphores								= &offscreenPass.semaphore;			// Signal ready with offscreen semaphore

		// Submit work
		submitInfo.commandBufferCount								= 1;
		submitInfo.pCommandBuffers									= &offscreenPass.commandBuffer;
		VK_EVAL(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

		// Scene rendering
		submitInfo.pWaitSemaphores									= &offscreenPass.semaphore;			// Wait for offscreen semaphore
		submitInfo.pSignalSemaphores								= &semaphores.renderComplete;		// Signal ready with render complete semaphpre

		submitInfo.pCommandBuffers									= &drawCmdBuffers[currentBuffer];	// Submit work
		VK_EVAL(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

		VulkanExampleBase::submitFrame();
	}

	void														prepare											()										{
		VulkanExampleBase::prepare	();
		loadAssets					();
		prepareOffscreen			();
		setupVertexDescriptions		();
		prepareUniformBuffers		();
		setupDescriptorSetLayout	();
		preparePipelines			();
		setupDescriptorPool			();
		setupDescriptorSet			();
		buildCommandBuffers			();
		buildOffscreenCommandBuffer	();
		prepared													= true;
	}

	virtual void												render											()										{
		if (!prepared)
			return;
		draw();
		if (!paused)
			updateUniformBuffersScene();
	}

	virtual void												viewChanged										()										{ updateUniformBuffersScene(); }
	void														toggleBlur										()										{
		blur														= !blur;
		updateUniformBuffersScene();
		reBuildCommandBuffers();
	}

	void														toggleTextureDisplay							()										{
		displayTexture												= !displayTexture;
		reBuildCommandBuffers();
	}

	virtual void												keyPressed										(uint32_t keyCode)						{
		switch (keyCode) {
		case KEY_B					:
		case GAMEPAD_BUTTON_A		: toggleBlur			(); break;
		case KEY_T					:
		case GAMEPAD_BUTTON_X		: toggleTextureDisplay	(); break;
		}
	}

	virtual void												getOverlayText									(VulkanTextOverlay *textOverlay_)		{
#if defined(__ANDROID__)
		textOverlay_->addText("Press \"Button A\" to toggle blur", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
		textOverlay_->addText("Press \"Button X\" to display offscreen texture", 5.0f, 105.0f, VulkanTextOverlay::alignLeft);
#else
		textOverlay_->addText("Press \"B\" to toggle blur", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
		textOverlay_->addText("Press \"T\" to display offscreen texture", 5.0f, 105.0f, VulkanTextOverlay::alignLeft);
#endif
	}
};

VULKAN_EXAMPLE_EXPORT_FUNCTIONS()
