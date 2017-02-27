// Vulkan buffer class
// 
// Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
// 
// This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
#pragma once

#include <vector>

#include "vulkan/vulkan.h"
#include "VulkanTools.h"

namespace vks
{	
	// Encapsulates access to a Vulkan buffer backed up by device memory. To be filled by an external source like the VulkanDevice.
	struct Buffer
	{
		VkBuffer												buffer					= VK_NULL_HANDLE;
		VkDevice												device					= VK_NULL_HANDLE;
		VkDeviceMemory											memory					= VK_NULL_HANDLE;
		VkDescriptorBufferInfo									descriptor				= {};
		VkDeviceSize											size					= 0;
		VkDeviceSize											alignment				= 0;
		void													* mapped				= nullptr;

		VkBufferUsageFlags										usageFlags				= 0;				// Usage flags to be filled by external source at buffer creation (to query at some later point)
		VkMemoryPropertyFlags									memoryPropertyFlags		= 0;				// Memory propertys flags to be filled by external source at buffer creation (to query at some later point)

		// Map a memory range of this buffer. Takes an optional byte offset. If successful, mapped points to the specified buffer range. Returns VkResult of the buffer mapping call. 
		// Pass VK_WHOLE_SIZE to map the complete buffer range. 
		VkResult												map						(VkDeviceSize size_ = VK_WHOLE_SIZE, VkDeviceSize offset = 0)	{ return vkMapMemory(device, memory, offset, size_, 0, &mapped);		}
		
		// Unmap a mapped memory range. Does not return a result as vkUnmapMemory can't fail
		void													unmap					()																{ if (mapped) { vkUnmapMemory(device, memory); mapped = nullptr; }	}	
		
		// Attach the allocated memory block to the buffer. Returns VkResult of the bindBufferMemory call. Optional byte offset parameter (from the beginning) for the memory region to bind
		VkResult												bind					(VkDeviceSize offset = 0)										{ return vkBindBufferMemory(device, buffer, memory, offset);		}

		//	Setup the default descriptor for this buffer
		//	
		//	size (Optional)		: Size of the memory range of the descriptor
		//	offset (Optional)	: Byte offset from beginning
		void													setupDescriptor			(VkDeviceSize size_ = VK_WHOLE_SIZE, VkDeviceSize offset = 0)	{
			descriptor.offset										= offset;
			descriptor.buffer										= buffer;
			descriptor.range										= size_;
		}

		// Copies the specified data to the mapped buffer. The size parameter is the size of the data to copy in machine units.
		void													copyTo					(void* data, VkDeviceSize size_)									{ assert(mapped); memcpy(mapped, data, (size_t)size_); }

		// Flush a memory range of the buffer to make it visible to the device. Only required for non-coherent memory. Returns VkResult of the flush call. Pass VK_WHOLE_SIZE to flush the complete buffer range.
		VkResult												flush					(VkDeviceSize size_ = VK_WHOLE_SIZE, VkDeviceSize offset = 0)	{
			VkMappedMemoryRange											mappedRange = {};
			mappedRange.sType										= VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
			mappedRange.memory										= memory;
			mappedRange.offset										= offset;
			mappedRange.size										= size_;
			return vkFlushMappedMemoryRanges(device, 1, &mappedRange);
		}

		// Invalidate a memory range of the buffer to make it visible to the host. Only required for non-coherent memory. Returns VkResult of the invalidate call. Pass VK_WHOLE_SIZE to invalidate the complete buffer range.
		VkResult												invalidate				(VkDeviceSize size_ = VK_WHOLE_SIZE, VkDeviceSize offset = 0)	{
			VkMappedMemoryRange											mappedRange = {};
			mappedRange.sType										= VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
			mappedRange.memory										= memory;
			mappedRange.offset										= offset;
			mappedRange.size										= size_;
			return vkInvalidateMappedMemoryRanges(device, 1, &mappedRange);
		}

		// Release all Vulkan resources held by this buffer
		void													destroy					()																{
			if(buffer)	vkDestroyBuffer	(device, buffer, nullptr);
			if(memory)	vkFreeMemory	(device, memory, nullptr);
		}
	};	// struct
}	// namespace