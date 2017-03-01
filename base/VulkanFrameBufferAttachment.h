#include "vulkan/vulkan.h"

namespace vks
{
#pragma once
	// Framebuffer for offscreen rendering
	struct FrameBufferAttachmentSmall {
		VkImage														image									= VK_NULL_HANDLE;
		VkImageView													view									= VK_NULL_HANDLE;
		VkDeviceMemory												memory									= VK_NULL_HANDLE;

		void														destroy												(VkDevice device_)	{
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

		void														destroy												(VkDevice device_)	{
			vkDestroyImage			(device_, image	, nullptr);
			vkDestroyImageView		(device_, view	, nullptr);
			vkFreeMemory			(device_, memory, nullptr);
		}
	};

	// Encapsulates a single frame buffer attachment 
	struct FramebufferAttachment {
		VkImage														image									= VK_NULL_HANDLE;
		VkDeviceMemory												memory									= VK_NULL_HANDLE;
		VkImageView													view									= VK_NULL_HANDLE;
		VkFormat													format									= VK_FORMAT_UNDEFINED;
		VkImageSubresourceRange										subresourceRange						= {};
		VkAttachmentDescription										description								= {};

		void														destroy									(VkDevice device_)				{
			vkDestroyImage			(device_, image	, nullptr);
			vkDestroyImageView		(device_, view	, nullptr);
			vkFreeMemory			(device_, memory, nullptr);
		}

		// Returns true if the attachment is a depth and/or stencil attachment
		bool														isDepthStencil		()																		{ return(hasDepth() || hasStencil()); }
		// Returns true if the attachment has a depth component
		bool														hasDepth			()																		{
			static const VkFormat											formats[]			= 
				{	VK_FORMAT_D16_UNORM
				,	VK_FORMAT_X8_D24_UNORM_PACK32
				,	VK_FORMAT_D32_SFLOAT
				,	VK_FORMAT_D16_UNORM_S8_UINT
				,	VK_FORMAT_D24_UNORM_S8_UINT
				,	VK_FORMAT_D32_SFLOAT_S8_UINT
				};
			return std::find(formats, std::end(formats), format) != std::end(formats);
		}

		// Returns true if the attachment has a stencil component
		bool														hasStencil			()																		{
			static const VkFormat											formats[]			= 
				{	VK_FORMAT_S8_UINT			
				,	VK_FORMAT_D16_UNORM_S8_UINT	
				,	VK_FORMAT_D24_UNORM_S8_UINT	
				,	VK_FORMAT_D32_SFLOAT_S8_UINT
				};
			return std::find(formats, std::end(formats), format) != std::end(formats);
		}

	};
}