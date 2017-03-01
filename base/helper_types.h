#include "vulkan/vulkan.h"

namespace vks
{
#pragma once
	// Framebuffer for offscreen rendering
	struct FrameBufferAttachmentSmall {
		VkImage														image									= VK_NULL_HANDLE;
		VkImageView													view									= VK_NULL_HANDLE;
		VkDeviceMemory												memory									= VK_NULL_HANDLE;

		void														destroy												(VkDevice device_)				{
			vkDestroyImage			(device_, image	, nullptr);
			vkDestroyImageView		(device_, view	, nullptr);
			vkFreeMemory			(device_, memory, nullptr);
		}
	};

	// Framebuffer for offscreen rendering
	struct FrameBufferAttachmentWithFormat {
		VkImage														image									= VK_NULL_HANDLE;
		VkImageView													view									= VK_NULL_HANDLE;
		VkDeviceMemory												memory									= VK_NULL_HANDLE;
		VkFormat													format									= VK_FORMAT_UNDEFINED;

		void														destroy												(VkDevice device_)				{
			vkDestroyImage			(device_, image	, nullptr);
			vkDestroyImageView		(device_, view	, nullptr);
			vkFreeMemory			(device_, memory, nullptr);
		}
	};
}