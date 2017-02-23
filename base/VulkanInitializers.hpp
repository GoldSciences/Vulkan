/*
* Initializers for Vulkan structures and objects used by the examples
* Saves lot of VK_STRUCTURE_TYPE assignments
* Some initializers are parameterized for convenience
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <vector>
#include "vulkan/vulkan.h"

namespace vks
{
	namespace initializers
	{

		inline VkMemoryAllocateInfo						memoryAllocateInfo					()																														{ return {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO			, };	}
		inline VkMappedMemoryRange						mappedMemoryRange					()																														{ return {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE				, };	}
		inline VkCommandBufferAllocateInfo				commandBufferAllocateInfo			(VkCommandPool commandPool, VkCommandBufferLevel level, uint32_t bufferCount)											{
			VkCommandBufferAllocateInfo										commandBufferAllocateInfo {};
			commandBufferAllocateInfo.sType								= VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			commandBufferAllocateInfo.commandPool						= commandPool;
			commandBufferAllocateInfo.level								= level;
			commandBufferAllocateInfo.commandBufferCount				= bufferCount;
			return commandBufferAllocateInfo;
		}

		inline VkCommandPoolCreateInfo					commandPoolCreateInfo				()																														{ return {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO		, };				}
		inline VkCommandBufferBeginInfo					commandBufferBeginInfo				()																														{ return {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO		, };				}
		inline VkCommandBufferInheritanceInfo			commandBufferInheritanceInfo		()																														{ return {VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO	, };				}
		inline VkRenderPassBeginInfo					renderPassBeginInfo					()																														{ return {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO			, };				}
		inline VkRenderPassCreateInfo					renderPassCreateInfo				()																														{ return {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO			, };				}

		// Initialize an image memory barrier with no image transfer ownership
		inline VkImageMemoryBarrier						imageMemoryBarrier					()																														{
			VkImageMemoryBarrier											imageMemoryBarrier {};
			imageMemoryBarrier.sType									= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrier.srcQueueFamilyIndex						= VK_QUEUE_FAMILY_IGNORED;
			imageMemoryBarrier.dstQueueFamilyIndex						= VK_QUEUE_FAMILY_IGNORED;
			return imageMemoryBarrier;
		}

		// Initialize a buffer memory barrier with no image transfer ownership
		inline VkBufferMemoryBarrier					bufferMemoryBarrier					()																														{
			VkBufferMemoryBarrier											bufferMemoryBarrier {};
			bufferMemoryBarrier.sType									= VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			bufferMemoryBarrier.srcQueueFamilyIndex						= VK_QUEUE_FAMILY_IGNORED;
			bufferMemoryBarrier.dstQueueFamilyIndex						= VK_QUEUE_FAMILY_IGNORED;
			return bufferMemoryBarrier;
		}

		inline VkMemoryBarrier							memoryBarrier							()																													{ return {VK_STRUCTURE_TYPE_MEMORY_BARRIER			, };						}
		inline VkImageCreateInfo						imageCreateInfo							()																													{ return {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO		, };						}
		inline VkSamplerCreateInfo						samplerCreateInfo						()																													{ return {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO		, };						}
		inline VkImageViewCreateInfo					imageViewCreateInfo						()																													{ return {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO	, };						}
		inline VkFramebufferCreateInfo					framebufferCreateInfo					()																													{ return {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO	, };						}
		inline VkSemaphoreCreateInfo					semaphoreCreateInfo						()																													{ return {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO	, };						}
		inline VkFenceCreateInfo						fenceCreateInfo							(VkFenceCreateFlags flags = 0)																						{ return {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, 0, flags};						}
		inline VkEventCreateInfo						eventCreateInfo							()																													{ return {VK_STRUCTURE_TYPE_EVENT_CREATE_INFO		, };						}
		inline VkSubmitInfo								submitInfo								()																													{ return {VK_STRUCTURE_TYPE_SUBMIT_INFO				, };						}
		inline VkViewport								viewport								(float width, float height, float minDepth, float maxDepth)															{ return {0, 0, width, height, minDepth, maxDepth};								}
		inline VkRect2D									rect2D									(int32_t width, int32_t height, int32_t offsetX, int32_t offsetY)													{ return { {offsetX, offsetY}, {(uint32_t)width, (uint32_t)height}};			}
		inline VkBufferCreateInfo						bufferCreateInfo						()																													{ return {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO		,};							}
		inline VkBufferCreateInfo						bufferCreateInfo						(VkBufferUsageFlags usage, VkDeviceSize size)																		{
			VkBufferCreateInfo												bufCreateInfo {};
			bufCreateInfo.sType											= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufCreateInfo.usage											= usage;
			bufCreateInfo.size											= size;
			return bufCreateInfo;
		}

		inline VkDescriptorPoolCreateInfo				descriptorPoolCreateInfo				(uint32_t poolSizeCount, VkDescriptorPoolSize* pPoolSizes, uint32_t maxSets)										{
			VkDescriptorPoolCreateInfo										descriptorPoolInfo {};
			descriptorPoolInfo.sType									= VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			descriptorPoolInfo.poolSizeCount							= poolSizeCount;
			descriptorPoolInfo.pPoolSizes								= pPoolSizes;
			descriptorPoolInfo.maxSets									= maxSets;
			return descriptorPoolInfo;
		}

		inline VkDescriptorPoolCreateInfo				descriptorPoolCreateInfo				(const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets)												{
			VkDescriptorPoolCreateInfo										descriptorPoolInfo{};
			descriptorPoolInfo.sType									= VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			descriptorPoolInfo.poolSizeCount							= static_cast<uint32_t>(poolSizes.size());
			descriptorPoolInfo.pPoolSizes								= poolSizes.data();
			descriptorPoolInfo.maxSets									= maxSets;
			return descriptorPoolInfo;
		}

		inline VkDescriptorPoolSize						descriptorPoolSize						(VkDescriptorType type, uint32_t descriptorCount)																	{ return {type, descriptorCount};												}
		inline VkDescriptorSetLayoutBinding				descriptorSetLayoutBinding				(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding, uint32_t descriptorCount = 1)				{ return {binding, type, descriptorCount, stageFlags, nullptr};					}
		inline VkDescriptorSetLayoutCreateInfo			descriptorSetLayoutCreateInfo			(const VkDescriptorSetLayoutBinding* pBindings, uint32_t bindingCount)												{
			VkDescriptorSetLayoutCreateInfo									descriptorSetLayoutCreateInfo {};
			descriptorSetLayoutCreateInfo.sType							= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			descriptorSetLayoutCreateInfo.pBindings						= pBindings;
			descriptorSetLayoutCreateInfo.bindingCount					= bindingCount;
			return descriptorSetLayoutCreateInfo;
		}

		inline VkDescriptorSetLayoutCreateInfo			descriptorSetLayoutCreateInfo			(const std::vector<VkDescriptorSetLayoutBinding>& bindings)															{
			VkDescriptorSetLayoutCreateInfo									descriptorSetLayoutCreateInfo{};
			descriptorSetLayoutCreateInfo.sType							= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			descriptorSetLayoutCreateInfo.pBindings						= bindings.data();
			descriptorSetLayoutCreateInfo.bindingCount					= static_cast<uint32_t>(bindings.size());
			return descriptorSetLayoutCreateInfo;
		}

		inline VkPipelineLayoutCreateInfo				pipelineLayoutCreateInfo				(const VkDescriptorSetLayout* pSetLayouts, uint32_t setLayoutCount = 1)												{
			VkPipelineLayoutCreateInfo										pipelineLayoutCreateInfo {};
			pipelineLayoutCreateInfo.sType								= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutCreateInfo.setLayoutCount						= setLayoutCount;
			pipelineLayoutCreateInfo.pSetLayouts						= pSetLayouts;
			return pipelineLayoutCreateInfo;
		}

		inline VkPipelineLayoutCreateInfo				pipelineLayoutCreateInfo				(uint32_t setLayoutCount = 1)																						{
			VkPipelineLayoutCreateInfo										pipelineLayoutCreateInfo{};
			pipelineLayoutCreateInfo.sType								= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutCreateInfo.setLayoutCount						= setLayoutCount;
			return pipelineLayoutCreateInfo;
		}

		inline VkDescriptorSetAllocateInfo				descriptorSetAllocateInfo				(VkDescriptorPool descriptorPool, const VkDescriptorSetLayout* pSetLayouts, uint32_t descriptorSetCount)			{
			VkDescriptorSetAllocateInfo										descriptorSetAllocateInfo {};
			descriptorSetAllocateInfo.sType								= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			descriptorSetAllocateInfo.descriptorPool					= descriptorPool;
			descriptorSetAllocateInfo.pSetLayouts						= pSetLayouts;
			descriptorSetAllocateInfo.descriptorSetCount				= descriptorSetCount;
			return descriptorSetAllocateInfo;
		}

		inline VkDescriptorImageInfo					descriptorImageInfo						(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout)												{ return {sampler, imageView, imageLayout};										}
		inline VkWriteDescriptorSet						writeDescriptorSet					
		(	VkDescriptorSet			dstSet
		,	VkDescriptorType		type
		,	uint32_t				binding
		,	VkDescriptorBufferInfo	* bufferInfo
		,	uint32_t				descriptorCount = 1
		)
		{
			VkWriteDescriptorSet											writeDescriptorSet {};
			writeDescriptorSet.sType									= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSet.dstSet									= dstSet;
			writeDescriptorSet.descriptorType							= type;
			writeDescriptorSet.dstBinding								= binding;
			writeDescriptorSet.pBufferInfo								= bufferInfo;
			writeDescriptorSet.descriptorCount							= descriptorCount;
			return writeDescriptorSet;
		}

		inline VkWriteDescriptorSet						writeDescriptorSet						
		(	VkDescriptorSet			dstSet
		,	VkDescriptorType		type
		,	uint32_t				binding
		,	VkDescriptorImageInfo	* imageInfo
		,	uint32_t				descriptorCount = 1
		)
		{
			VkWriteDescriptorSet											writeDescriptorSet {};
			writeDescriptorSet.sType									= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSet.dstSet									= dstSet;
			writeDescriptorSet.descriptorType							= type;
			writeDescriptorSet.dstBinding								= binding;
			writeDescriptorSet.pImageInfo								= imageInfo;
			writeDescriptorSet.descriptorCount							= descriptorCount;
			return writeDescriptorSet;
		}

		inline VkVertexInputBindingDescription			vertexInputBindingDescription			(uint32_t binding, uint32_t stride, VkVertexInputRate inputRate)													{ return {binding, stride, inputRate};											}
		inline VkVertexInputAttributeDescription		vertexInputAttributeDescription			(uint32_t binding, uint32_t location, VkFormat format, uint32_t offset)												{ return {location, binding, format, offset};									}
		inline VkPipelineVertexInputStateCreateInfo		pipelineVertexInputStateCreateInfo		()																													{ return {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO		, };	}
		inline VkPipelineInputAssemblyStateCreateInfo	pipelineInputAssemblyStateCreateInfo	(VkPrimitiveTopology topology, VkPipelineInputAssemblyStateCreateFlags flags, VkBool32 primitiveRestartEnable)		{
			VkPipelineInputAssemblyStateCreateInfo							pipelineInputAssemblyStateCreateInfo {};
			pipelineInputAssemblyStateCreateInfo.sType					= VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			pipelineInputAssemblyStateCreateInfo.topology				= topology;
			pipelineInputAssemblyStateCreateInfo.flags					= flags;
			pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable	= primitiveRestartEnable;
			return pipelineInputAssemblyStateCreateInfo;
		}

		inline VkPipelineRasterizationStateCreateInfo	pipelineRasterizationStateCreateInfo	
		(	VkPolygonMode							polygonMode
		,	VkCullModeFlags							cullMode
		,	VkFrontFace								frontFace
		,	VkPipelineRasterizationStateCreateFlags	flags = 0
		) 
		{
			VkPipelineRasterizationStateCreateInfo							pipelineRasterizationStateCreateInfo {};
			pipelineRasterizationStateCreateInfo.sType					= VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			pipelineRasterizationStateCreateInfo.polygonMode			= polygonMode;
			pipelineRasterizationStateCreateInfo.cullMode				= cullMode;
			pipelineRasterizationStateCreateInfo.frontFace				= frontFace;
			pipelineRasterizationStateCreateInfo.flags					= flags;
			pipelineRasterizationStateCreateInfo.depthClampEnable		= VK_FALSE;
			pipelineRasterizationStateCreateInfo.lineWidth				= 1.0f;
			return pipelineRasterizationStateCreateInfo;
		}

		inline VkPipelineColorBlendAttachmentState		pipelineColorBlendAttachmentState		(VkColorComponentFlags colorWriteMask, VkBool32 blendEnable)														{
			VkPipelineColorBlendAttachmentState								pipelineColorBlendAttachmentState {};
			pipelineColorBlendAttachmentState.colorWriteMask			= colorWriteMask;
			pipelineColorBlendAttachmentState.blendEnable				= blendEnable;
			return pipelineColorBlendAttachmentState;
		}

		inline VkPipelineColorBlendStateCreateInfo		pipelineColorBlendStateCreateInfo		(uint32_t attachmentCount, const VkPipelineColorBlendAttachmentState * pAttachments)								{
			VkPipelineColorBlendStateCreateInfo								pipelineColorBlendStateCreateInfo {};
			pipelineColorBlendStateCreateInfo.sType						= VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			pipelineColorBlendStateCreateInfo.attachmentCount			= attachmentCount;
			pipelineColorBlendStateCreateInfo.pAttachments				= pAttachments;
			return pipelineColorBlendStateCreateInfo;
		}

		inline VkPipelineDepthStencilStateCreateInfo	pipelineDepthStencilStateCreateInfo		(VkBool32 depthTestEnable, VkBool32 depthWriteEnable, VkCompareOp depthCompareOp)									{
			VkPipelineDepthStencilStateCreateInfo							pipelineDepthStencilStateCreateInfo {};
			pipelineDepthStencilStateCreateInfo.sType					= VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			pipelineDepthStencilStateCreateInfo.depthTestEnable			= depthTestEnable;
			pipelineDepthStencilStateCreateInfo.depthWriteEnable		= depthWriteEnable;
			pipelineDepthStencilStateCreateInfo.depthCompareOp			= depthCompareOp;
			pipelineDepthStencilStateCreateInfo.front					= pipelineDepthStencilStateCreateInfo.back;
			pipelineDepthStencilStateCreateInfo.back.compareOp			= VK_COMPARE_OP_ALWAYS;
			return pipelineDepthStencilStateCreateInfo;
		}

		inline VkPipelineViewportStateCreateInfo		pipelineViewportStateCreateInfo			(uint32_t viewportCount, uint32_t scissorCount, VkPipelineViewportStateCreateFlags flags = 0)						{
			VkPipelineViewportStateCreateInfo								pipelineViewportStateCreateInfo {};
			pipelineViewportStateCreateInfo.sType						= VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			pipelineViewportStateCreateInfo.viewportCount				= viewportCount;
			pipelineViewportStateCreateInfo.scissorCount				= scissorCount;
			pipelineViewportStateCreateInfo.flags						= flags;
			return pipelineViewportStateCreateInfo;
		}

		inline VkPipelineMultisampleStateCreateInfo		pipelineMultisampleStateCreateInfo		(VkSampleCountFlagBits rasterizationSamples, VkPipelineMultisampleStateCreateFlags flags = 0)						{
			VkPipelineMultisampleStateCreateInfo							pipelineMultisampleStateCreateInfo {};
			pipelineMultisampleStateCreateInfo.sType					= VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			pipelineMultisampleStateCreateInfo.rasterizationSamples		= rasterizationSamples;
			return pipelineMultisampleStateCreateInfo;
		}

		inline VkPipelineDynamicStateCreateInfo			pipelineDynamicStateCreateInfo			(const VkDynamicState * pDynamicStates, uint32_t dynamicStateCount, VkPipelineDynamicStateCreateFlags flags = 0)	{
			VkPipelineDynamicStateCreateInfo								pipelineDynamicStateCreateInfo {};
			pipelineDynamicStateCreateInfo.sType						= VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			pipelineDynamicStateCreateInfo.pDynamicStates				= pDynamicStates;
			pipelineDynamicStateCreateInfo.dynamicStateCount			= dynamicStateCount;
			return pipelineDynamicStateCreateInfo;
		}

		inline VkPipelineDynamicStateCreateInfo			pipelineDynamicStateCreateInfo			(const std::vector<VkDynamicState>& pDynamicStates, VkPipelineDynamicStateCreateFlags flags = 0)					{
			VkPipelineDynamicStateCreateInfo								pipelineDynamicStateCreateInfo{};
			pipelineDynamicStateCreateInfo.sType						= VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			pipelineDynamicStateCreateInfo.pDynamicStates				= pDynamicStates.data();
			pipelineDynamicStateCreateInfo.dynamicStateCount			= static_cast<uint32_t>(pDynamicStates.size());
			return pipelineDynamicStateCreateInfo;
		}

		inline VkPipelineTessellationStateCreateInfo	pipelineTessellationStateCreateInfo		(uint32_t patchControlPoints)																						{
			VkPipelineTessellationStateCreateInfo							pipelineTessellationStateCreateInfo {};
			pipelineTessellationStateCreateInfo.sType					= VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
			pipelineTessellationStateCreateInfo.patchControlPoints		= patchControlPoints;
			return pipelineTessellationStateCreateInfo;
		}
		inline VkGraphicsPipelineCreateInfo				pipelineCreateInfo						(VkPipelineLayout layout, VkRenderPass renderPass, VkPipelineCreateFlags flags = 0)									{
			VkGraphicsPipelineCreateInfo									pipelineCreateInfo {};
			pipelineCreateInfo.sType									= VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipelineCreateInfo.layout									= layout;
			pipelineCreateInfo.renderPass								= renderPass;
			pipelineCreateInfo.flags									= flags;
			pipelineCreateInfo.basePipelineIndex						= -1;
			pipelineCreateInfo.basePipelineHandle						= VK_NULL_HANDLE;
			return pipelineCreateInfo;
		}

		inline VkComputePipelineCreateInfo				computePipelineCreateInfo				(VkPipelineLayout layout, VkPipelineCreateFlags flags = 0)															{
			VkComputePipelineCreateInfo										computePipelineCreateInfo {};
			computePipelineCreateInfo.sType								= VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			computePipelineCreateInfo.layout							= layout;
			computePipelineCreateInfo.flags								= flags;
			return computePipelineCreateInfo;
		}

		inline VkPushConstantRange						pushConstantRange						(VkShaderStageFlags stageFlags, uint32_t size, uint32_t offset)														{ return {stageFlags, offset, size};						}
		inline VkBindSparseInfo							bindSparseInfo							()																													{ return {VK_STRUCTURE_TYPE_BIND_SPARSE_INFO		, };	}
		// Initialize a map entry for a shader specialization constant
		inline VkSpecializationMapEntry					specializationMapEntry					(uint32_t constantID, uint32_t offset, size_t size)																	{ return {constantID, offset, size};						}

		// Initialize a specialization constant info structure to pass to a shader stage
		inline VkSpecializationInfo						specializationInfo						(uint32_t mapEntryCount, const VkSpecializationMapEntry* mapEntries, size_t dataSize, const void* data)				{ return {mapEntryCount, mapEntries, dataSize, data};		}
	}
}