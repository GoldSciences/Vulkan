// Vulkan device class
// 
// Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
// 
// This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
#pragma once

#include <exception>
#include <algorithm>
#include "vulkan/vulkan.h"
#include "VulkanTools.h"
#include "VulkanBuffer.hpp"

namespace vks
{	
	struct VulkanDevice
	{
		
		VkPhysicalDevice						physicalDevice			= VK_NULL_HANDLE;	// Physical device representation
		VkDevice								logicalDevice			= VK_NULL_HANDLE;	// Logical device representation (application's view of the device)
		VkPhysicalDeviceProperties				properties				= {};				// Properties of the physical device including limits that the application can check against
		VkPhysicalDeviceFeatures				features				= {};				// Features of the physical device that an application can use to check if a feature is supported 
		VkPhysicalDeviceMemoryProperties		memoryProperties		= {};				// Memory types and heaps of the physical device
		std::vector<VkQueueFamilyProperties>	queueFamilyProperties	= {};				// Queue family properties of the physical device
		std::vector<std::string>				supportedExtensions		= {};				// List of extensions supported by the device
		VkCommandPool							commandPool				= VK_NULL_HANDLE;	// Default command pool for the graphics queue family index
		bool									enableDebugMarkers		= false;			// Set to true when the debug marker extension is detected

		// Contains queue family indices
		struct
		{
			uint32_t graphics;
			uint32_t compute;
			uint32_t transfer;
		} queueFamilyIndices;

		operator								VkDevice				()									{ return logicalDevice; }

		// The default constructor takes a physical device that is to be used
												VulkanDevice			(VkPhysicalDevice physicalDevice)
		{
			assert(physicalDevice);
			this->physicalDevice						= physicalDevice;

			// Store Properties features, limits and properties of the physical device for later use
			// Device properties also contain limits and sparse properties
			vkGetPhysicalDeviceProperties(physicalDevice, &properties);
			// Features should be checked by the examples before using them
			vkGetPhysicalDeviceFeatures(physicalDevice, &features);
			// Memory properties are used regularly for creating all kinds of buffers
			vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
			// Queue family properties, used for setting up requested queues upon device creation
			uint32_t										queueFamilyCount;
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
			assert(queueFamilyCount > 0);
			queueFamilyProperties.resize(queueFamilyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

			// Get list of supported extensions
			uint32_t										extCount			= 0;
			vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
			if (extCount > 0)
			{
				std::vector<VkExtensionProperties>				extensions			(extCount);
				if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, &extensions.front()) == VK_SUCCESS)
				{
					for (auto ext : extensions)
						supportedExtensions.push_back(ext.extensionName);
				}
			}
		}

												~VulkanDevice			()					{
			if (commandPool		) vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
			if (logicalDevice	) vkDestroyDevice(logicalDevice, nullptr);
		}

		// Get the index of a memory type that has all the requested property bits set.
		// 
		// typeBits					: Bitmask with bits set for each memory type supported by the resource to request for (from VkMemoryRequirements)
		// properties				: Bitmask of properties for the memory type to request
		// (Optional) memTypeFound	: Pointer to a bool that is set to true if a matching memory type has been found
		// 
		// Returns index of the requested memory type. Throws an exception if memTypeFound is null and no memory type could be found that supports the requested properties.
		uint32_t								getMemoryType			(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32 *memTypeFound = nullptr)
		{
			for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
			{
				if ((typeBits & 1) == 1)
				{
					if ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
					{
						if (memTypeFound)
							*memTypeFound = true;

						return i;
					}
				}
				typeBits >>= 1;
			}

#if defined(__ANDROID__)
			//todo : Exceptions are disabled by default on Android (need to add LOCAL_CPP_FEATURES += exceptions to Android.mk), so for now just return zero
			if (memTypeFound)
				*memTypeFound = false;

			return 0;
#else
			if (memTypeFound) {
				*memTypeFound = false;
				return 0;
			}
			else {
				throw std::runtime_error("Could not find a matching memory type");
			}
#endif
		}

		// Get the index of a queue family that supports the requested queue flags. Returns index of the queue family index that matches the flags.  Throws an exception if no queue family index could be found that supports the requested flags
		uint32_t								getQueueFamilyIndex		(VkQueueFlagBits queueFlags)
		{
			// Dedicated queue for compute
			// Try to find a queue family index that supports compute but not graphics
			if (queueFlags & VK_QUEUE_COMPUTE_BIT)
			{
				for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
				{
					if ((queueFamilyProperties[i].queueFlags & queueFlags) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
						return i;
				}
			}

			// Dedicated queue for transfer.  Try to find a queue family index that supports transfer but not graphics and compute
			if (queueFlags & VK_QUEUE_TRANSFER_BIT)
			{
				for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
				{
					if ((queueFamilyProperties[i].queueFlags & queueFlags) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
						return i;
				}
			}

			// For other queue types or if no separate compute queue is present, return the first one to support the requested flags
			for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
			{
				if (queueFamilyProperties[i].queueFlags & queueFlags)
					return i;
			}

#if defined(__ANDROID__)
			//todo : Exceptions are disabled by default on Android (need to add LOCAL_CPP_FEATURES += exceptions to Android.mk), so for now just return zero
			return 0;
#else
			throw std::runtime_error("Could not find a matching queue family index");
#endif
		}

		// Create the logical device based on the assigned physical device, also gets default queue family indices. Returns VkResult of the device creation call.
		// 
		// enabledFeatures		: Can be used to enable certain features upon device creation
		// useSwapChain			: Set to false for headless rendering to omit the swapchain device extensions
		// requestedQueueTypes	: Bit flags specifying the queue types to be requested from the device  
		VkResult								createLogicalDevice		(VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char*> enabledExtensions, bool useSwapChain = true, VkQueueFlags requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)
		{			
			// Desired queues need to be requested upon logical device creation
			// Due to differing queue family configurations of Vulkan implementations this can be a bit tricky, especially if the application
			// requests different queue types
			std::vector<VkDeviceQueueCreateInfo>			queueCreateInfos{};

			// Get queue family indices for the requested queue family types
			// Note that the indices may overlap depending on the implementation
			const float										defaultQueuePriority(0.0f);

			// Graphics queue
			if (requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT)
			{
				queueFamilyIndices.graphics					= getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
				VkDeviceQueueCreateInfo							queueInfo{};
				queueInfo.sType								= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queueInfo.queueFamilyIndex					= queueFamilyIndices.graphics;
				queueInfo.queueCount						= 1;
				queueInfo.pQueuePriorities					= &defaultQueuePriority;
				queueCreateInfos.push_back(queueInfo);
			}
			else
			{
				queueFamilyIndices.graphics = VK_NULL_HANDLE;
			}

			// Dedicated compute queue
			if (requestedQueueTypes & VK_QUEUE_COMPUTE_BIT)
			{
				queueFamilyIndices.compute					= getQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT);
				if (queueFamilyIndices.compute != queueFamilyIndices.graphics)
				{
					// If compute family index differs, we need an additional queue create info for the compute queue
					VkDeviceQueueCreateInfo							queueInfo{};
					queueInfo.sType								= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
					queueInfo.queueFamilyIndex					= queueFamilyIndices.compute;
					queueInfo.queueCount						= 1;
					queueInfo.pQueuePriorities					= &defaultQueuePriority;
					queueCreateInfos.push_back(queueInfo);
				}
			}
			else
			{
				// Else we use the same queue
				queueFamilyIndices.compute					= queueFamilyIndices.graphics;
			}

			// Dedicated transfer queue
			if (requestedQueueTypes & VK_QUEUE_TRANSFER_BIT)
			{
				queueFamilyIndices.transfer					= getQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT);
				if ((queueFamilyIndices.transfer != queueFamilyIndices.graphics) && (queueFamilyIndices.transfer != queueFamilyIndices.compute))
				{
					// If compute family index differs, we need an additional queue create info for the compute queue
					VkDeviceQueueCreateInfo							queueInfo{};
					queueInfo.sType								= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
					queueInfo.queueFamilyIndex					= queueFamilyIndices.transfer;
					queueInfo.queueCount						= 1;
					queueInfo.pQueuePriorities					= &defaultQueuePriority;
					queueCreateInfos.push_back(queueInfo);
				}
			}
			else { // Else we use the same queue
				queueFamilyIndices.transfer					= queueFamilyIndices.graphics;	
			}

			// Create the logical device representation
			std::vector<const char*>						deviceExtensions(enabledExtensions);
			if (useSwapChain)	{	// If the device will be used for presenting to a display via a swapchain we need to request the swapchain extension
				deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
			}

			VkDeviceCreateInfo								deviceCreateInfo		= {};
			deviceCreateInfo.sType						= VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			deviceCreateInfo.queueCreateInfoCount		= static_cast<uint32_t>(queueCreateInfos.size());;
			deviceCreateInfo.pQueueCreateInfos			= queueCreateInfos.data();
			deviceCreateInfo.pEnabledFeatures			= &enabledFeatures;

			// Enable the debug marker extension if it is present (likely meaning a debugging tool is present)
			if (extensionSupported(VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
			{
				deviceExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
				enableDebugMarkers							= true;
			}

			if (deviceExtensions.size() > 0)
			{
				deviceCreateInfo.enabledExtensionCount		= (uint32_t)deviceExtensions.size();
				deviceCreateInfo.ppEnabledExtensionNames	= deviceExtensions.data();
			}

			VkResult										result					= vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &logicalDevice);

			if (result == VK_SUCCESS)
				commandPool = createCommandPool(queueFamilyIndices.graphics);	// Create a default command pool for graphics command buffers

			return result;
		}

		
		// Create a buffer on the device. Returns VK_SUCCESS if buffer handle and memory have been created and (optionally passed) data has been copied
		// 
		// usageFlags			: Usage flag bitmask for the buffer (i.e. index, vertex, uniform buffer)
		// memoryPropertyFlags	: Memory properties for this buffer (i.e. device local, host visible, coherent)
		// size					: Size of the buffer in byes
		// buffer				: Pointer to the buffer handle acquired by the function
		// memory				: Pointer to the memory handle acquired by the function
		// data					: Pointer to the data that should be copied to the buffer after creation (optional, if not set, no data is copied over)
		VkResult								createBuffer			(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, VkBuffer *buffer, VkDeviceMemory *memory, void *data = nullptr)
		{
			// Create the buffer handle
			VkBufferCreateInfo								bufferCreateInfo		= vks::initializers::bufferCreateInfo(usageFlags, size);
			bufferCreateInfo.sharingMode				= VK_SHARING_MODE_EXCLUSIVE;
			VK_CHECK_RESULT(vkCreateBuffer(logicalDevice, &bufferCreateInfo, nullptr, buffer));

			// Create the memory backing up the buffer handle
			VkMemoryRequirements							memReqs;
			VkMemoryAllocateInfo							memAlloc				= vks::initializers::memoryAllocateInfo();
			vkGetBufferMemoryRequirements(logicalDevice, *buffer, &memReqs);
			memAlloc.allocationSize						= memReqs.size;
			// Find a memory type index that fits the properties of the buffer
			memAlloc.memoryTypeIndex					= getMemoryType(memReqs.memoryTypeBits, memoryPropertyFlags);
			VK_CHECK_RESULT(vkAllocateMemory(logicalDevice, &memAlloc, nullptr, memory));
			
			// If a pointer to the buffer data has been passed, map the buffer and copy over the data
			if (data != nullptr)
			{
				void											* mapped				= nullptr;
				VK_CHECK_RESULT(vkMapMemory(logicalDevice, *memory, 0, size, 0, &mapped));
				memcpy(mapped, data, (size_t)size);
				vkUnmapMemory(logicalDevice, *memory);
			}

			// Attach the memory to the buffer object
			VK_CHECK_RESULT(vkBindBufferMemory(logicalDevice, *buffer, *memory, 0));

			return VK_SUCCESS;
		}

		// Create a buffer on the device. Returns VK_SUCCESS if buffer handle and memory have been created and (optionally passed) data has been copied.
		// 
		// usageFlags			: Usage flag bitmask for the buffer (i.e. index, vertex, uniform buffer)
		// memoryPropertyFlags	: Memory properties for this buffer (i.e. device local, host visible, coherent)
		// buffer				: Pointer to a vk::Vulkan buffer object
		// size					: Size of the buffer in byes
		// data					: Pointer to the data that should be copied to the buffer after creation (optional, if not set, no data is copied over)
		VkResult								createBuffer			(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, vks::Buffer *buffer, VkDeviceSize size, void *data = nullptr)
		{
			buffer->device								= logicalDevice;

			// Create the buffer handle
			VkBufferCreateInfo								bufferCreateInfo		= vks::initializers::bufferCreateInfo(usageFlags, size);
			VK_CHECK_RESULT(vkCreateBuffer(logicalDevice, &bufferCreateInfo, nullptr, &buffer->buffer));

			// Create the memory backing up the buffer handle
			VkMemoryRequirements							memReqs;
			VkMemoryAllocateInfo							memAlloc				= vks::initializers::memoryAllocateInfo();
			vkGetBufferMemoryRequirements(logicalDevice, buffer->buffer, &memReqs);
			memAlloc.allocationSize						= memReqs.size;
			
			memAlloc.memoryTypeIndex					= getMemoryType(memReqs.memoryTypeBits, memoryPropertyFlags);				// Find a memory type index that fits the properties of the buffer
			VK_CHECK_RESULT(vkAllocateMemory(logicalDevice, &memAlloc, nullptr, &buffer->memory));

			buffer->alignment							= memReqs.alignment;
			buffer->size								= memAlloc.allocationSize;
			buffer->usageFlags							= usageFlags;
			buffer->memoryPropertyFlags					= memoryPropertyFlags;

			// If a pointer to the buffer data has been passed, map the buffer and copy over the data
			if (data != nullptr) {
				VK_CHECK_RESULT(buffer->map());
				memcpy(buffer->mapped, data, (size_t)size);
				buffer->unmap();
			}

			
			buffer->setupDescriptor();		// Initialize a default descriptor that covers the whole buffer size
			return buffer->bind();			// Attach the memory to the buffer object
		}

		// Copy buffer data from src to dst using VkCmdCopyBuffer. Source and destionation pointers must have the approriate transfer usage flags set (TRANSFER_SRC / TRANSFER_DST).
		// 
		// src			: Pointer to the source buffer to copy from
		// dst			: Pointer to the destination buffer to copy tp
		// queue		: Pointer
		// copyRegion	: Pointer to a copy region, if NULL, the whole buffer is copied
		void									copyBuffer				(vks::Buffer *src, vks::Buffer *dst, VkQueue queue, VkBufferCopy *copyRegion = nullptr)
		{
			assert(dst->size	<= src->size);
			assert(dst->buffer	&& src->buffer);
			VkCommandBuffer									copyCmd					= createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
			VkBufferCopy									bufferCopy				= {};
			if (copyRegion == nullptr)
				bufferCopy.size								= src->size;
			else
				bufferCopy									= *copyRegion;

			vkCmdCopyBuffer(copyCmd, src->buffer, dst->buffer, 1, &bufferCopy);

			flushCommandBuffer(copyCmd, queue);
		}

		// Create a command pool for allocation command buffers from. Returns a handle to the created command buffer. Command buffers allocated from the created pool can only be submitted to a queue with the same family index.
		// 
		// queueFamilyIndex Family index of the queue to create the command pool for
		// createFlags (Optional) Command pool creation flags (Defaults to VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
		VkCommandPool							createCommandPool		(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
		{
			VkCommandPoolCreateInfo							cmdPoolInfo				= {};
			cmdPoolInfo.sType							= VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			cmdPoolInfo.queueFamilyIndex				= queueFamilyIndex;
			cmdPoolInfo.flags							= createFlags;
			VkCommandPool									cmdPool;
			VK_CHECK_RESULT(vkCreateCommandPool(logicalDevice, &cmdPoolInfo, nullptr, &cmdPool));
			return cmdPool;
		}

		// Allocate a command buffer from the command pool. Returns a handle to the allocated command buffer
		// 
		// level		: Level of the new command buffer (primary or secondary)
		// begin		: If true, recording on the new command buffer will be started (vkBeginCommandBuffer) (Defaults to false)
		VkCommandBuffer							createCommandBuffer		(VkCommandBufferLevel level, bool begin = false)
		{
			VkCommandBufferAllocateInfo						cmdBufAllocateInfo		= vks::initializers::commandBufferAllocateInfo(commandPool, level, 1);
			VkCommandBuffer									cmdBuffer;
			VK_CHECK_RESULT(vkAllocateCommandBuffers(logicalDevice, &cmdBufAllocateInfo, &cmdBuffer));

			// If requested, also start recording for the new command buffer
			if (begin) {
				VkCommandBufferBeginInfo						cmdBufInfo			= vks::initializers::commandBufferBeginInfo();
				VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
			}

			return cmdBuffer;
		}

		// Finish command buffer recording and submit it to a queue
		// 
		// commandBuffer	: Command buffer to flush
		// queue			: Queue to submit the command buffer to 
		// free				: (Optional) Free the command buffer once it has been submitted (Defaults to true)
		// 
		// note				: The queue that the command buffer is submitted to must be from the same family index as the pool it was allocated from
		// note				: Uses a fence to ensure command buffer has finished executing
		void									flushCommandBuffer		(VkCommandBuffer commandBuffer, VkQueue queue, bool free = true)
		{
			if (commandBuffer == VK_NULL_HANDLE)
				return;

			VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

			VkSubmitInfo									submitInfo				= vks::initializers::submitInfo();
			submitInfo.commandBufferCount				= 1;
			submitInfo.pCommandBuffers					= &commandBuffer;

			// Create fence to ensure that the command buffer has finished executing
			VkFenceCreateInfo								fenceInfo				= vks::initializers::fenceCreateInfo(VK_FLAGS_NONE);
			VkFence											fence;

			VK_CHECK_RESULT(vkCreateFence(logicalDevice, &fenceInfo, nullptr, &fence));
			VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, fence));									// Submit to the queue
			VK_CHECK_RESULT(vkWaitForFences(logicalDevice, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));		// Wait for the fence to signal that command buffer has finished executing

			vkDestroyFence(logicalDevice, fence, nullptr);

			if (free)
				vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
		}

		// Check if an extension is supported by the (physical device). Returns true if the extension is supported (present in the list read at device creation time)
		bool									extensionSupported		(std::string extension) {
			return (std::find(supportedExtensions.begin(), supportedExtensions.end(), extension) != supportedExtensions.end());
		}
	};
}
