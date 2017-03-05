// Vulkan texture loader
// 
// Copyright(C) 2016-2017 by Sascha Willems - www.saschawillems.de
// 
// This code is licensed under the MIT license(MIT) (http://opensource.org/licenses/MIT)
#pragma once

#include "VulkanDevice.hpp"

#include <gli/gli.hpp>

#if defined(__ANDROID__)
#include <android/asset_manager.h>
#endif

namespace vks
{
	struct Texture {
		vks::VulkanDevice									* device			= nullptr;
		VkImage												image				= VK_NULL_HANDLE;
		VkImageLayout										imageLayout			= VK_IMAGE_LAYOUT_UNDEFINED;
		VkDeviceMemory										deviceMemory		= VK_NULL_HANDLE;
		VkImageView											view				= VK_NULL_HANDLE;
		uint32_t											width				= 0
			,												height				= 0
			;
		uint32_t											mipLevels			= 0;
		uint32_t											layerCount			= 0;
		VkDescriptorImageInfo								descriptor			= {};

		
		VkSampler											sampler				= VK_NULL_HANDLE;				// Optional sampler to use with this texture
	
		// Update image descriptor from current sampler, view and image layout
		void												updateDescriptor	()										{
			descriptor.sampler									= sampler;
			descriptor.imageView								= view;
			descriptor.imageLayout								= imageLayout;
		}

		// Release all Vulkan resources held by this texture
		void												destroy				()										{
			if(VK_NULL_HANDLE != view			) {	vkDestroyImageView	(device->logicalDevice, view			, nullptr);	view			= VK_NULL_HANDLE; }
			if(VK_NULL_HANDLE != image			) {	vkDestroyImage		(device->logicalDevice, image			, nullptr);	image			= VK_NULL_HANDLE; }
			if(VK_NULL_HANDLE != sampler		) {	vkDestroySampler	(device->logicalDevice, sampler			, nullptr);	sampler			= VK_NULL_HANDLE; }
			if(VK_NULL_HANDLE != deviceMemory	) {	vkFreeMemory		(device->logicalDevice, deviceMemory	, nullptr);	deviceMemory	= VK_NULL_HANDLE; }
		}	
	};	// struct

	struct Texture2D : public Texture {
		// Load a 2D texture including all mip levels
		void												loadFromFile
			(	std::string				filename																// File to load (supports .ktx and .dds)
			,	VkFormat				format																	// Vulkan format of the image data stored in the file
			,	vks::VulkanDevice		* device_																// Vulkan device to create the texture on
			,	VkQueue					copyQueue																// Queue used for the texture staging copy commands (must support transfer)
			,	VkImageUsageFlags		imageUsageFlags		= VK_IMAGE_USAGE_SAMPLED_BIT						// imageUsageFlags Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
			,	VkImageLayout			imageLayout_		= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL			// imageLayout Usage layout for the texture (defaults VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
			,	bool					forceLinear			= false												// forceLinear Force linear tiling (not advised, defaults to false)
			)
		{
#if defined(__ANDROID__)
			// Textures are stored inside the apk on Android (compressed)
			// So they need to be loaded via the asset manager
			AAsset*													asset							= AAssetManager_open(androidApp->activity->assetManager, filename.c_str(), AASSET_MODE_STREAMING);
			assert(asset);
			size_t													size							= AAsset_getLength(asset);
			assert(size > 0);

			void													* textureData					= malloc(size);
			AAsset_read(asset, textureData, size);
			AAsset_close(asset);

			gli::texture2d											tex2D(gli::load((const char*)textureData, size));

			free(textureData);
#else
			gli::texture2d											tex2D(gli::load(filename.c_str()));
#endif		
			assert(!tex2D.empty());

			this->device										= device_;
			width												= static_cast<uint32_t>(tex2D[0].extent().x);
			height												= static_cast<uint32_t>(tex2D[0].extent().y);
			mipLevels											= static_cast<uint32_t>(tex2D.levels());

			// Get device properites for the requested texture format
			VkFormatProperties										formatProperties;
			vkGetPhysicalDeviceFormatProperties(device_->physicalDevice, format, &formatProperties);

			// Only use linear tiling if requested (and supported by the device)
			// Support for linear tiling is mostly limited, so prefer to use optimal tiling instead
			// On most implementations linear tiling will only support a very limited amount of formats and features (mip maps, cubemaps, arrays, etc.)
			VkBool32												useStaging						= !forceLinear;

			VkMemoryAllocateInfo									memAllocInfo					= vks::initializers::memoryAllocateInfo();
			VkMemoryRequirements									memReqs;

			// Use a separate command buffer for texture loading
			VkCommandBuffer											copyCmd							= device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

			if (useStaging)
			{
				// Create a host-visible staging buffer that contains the raw image data
				VkBuffer												stagingBuffer;
				VkDeviceMemory											stagingMemory;

				VkBufferCreateInfo										bufferCreateInfo				= vks::initializers::bufferCreateInfo();
				bufferCreateInfo.size								= tex2D.size();
				
				bufferCreateInfo.usage								= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;		// This buffer is used as a transfer source for the buffer copy
				bufferCreateInfo.sharingMode						= VK_SHARING_MODE_EXCLUSIVE;

				VK_EVAL(vkCreateBuffer		(device_->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

				
				vkGetBufferMemoryRequirements(device_->logicalDevice, stagingBuffer, &memReqs);	// Get memory requirements for the staging buffer (alignment, memory type bits)

				memAllocInfo.allocationSize								= memReqs.size;
				memAllocInfo.memoryTypeIndex							= device_->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);	// Get memory type index for a host visible buffer

				VK_EVAL(vkAllocateMemory	(device_->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
				VK_EVAL(vkBindBufferMemory	(device_->logicalDevice, stagingBuffer, stagingMemory, 0));

				// Copy texture data into staging buffer
				uint8_t													* data							= nullptr;
				VK_EVAL(vkMapMemory			(device_->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));
				memcpy(data, tex2D.data(), tex2D.size());
				vkUnmapMemory							(device_->logicalDevice, stagingMemory);

				// Setup buffer copy regions for each mip level
				std::vector<VkBufferImageCopy>							bufferCopyRegions;
				uint32_t												offset							= 0;

				for (uint32_t i = 0; i < mipLevels; i++) {
					VkBufferImageCopy										bufferCopyRegion				= {};
					bufferCopyRegion.imageSubresource.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
					bufferCopyRegion.imageSubresource.mipLevel			= i;
					bufferCopyRegion.imageSubresource.baseArrayLayer	= 0;
					bufferCopyRegion.imageSubresource.layerCount		= 1;
					bufferCopyRegion.imageExtent.width					= static_cast<uint32_t>(tex2D[i].extent().x);
					bufferCopyRegion.imageExtent.height					= static_cast<uint32_t>(tex2D[i].extent().y);
					bufferCopyRegion.imageExtent.depth					= 1;
					bufferCopyRegion.bufferOffset						= offset;

					bufferCopyRegions.push_back(bufferCopyRegion);

					offset												+= static_cast<uint32_t>(tex2D[i].size());
				}

				// Create optimal tiled target image
				VkImageCreateInfo										imageCreateInfo					= vks::initializers::imageCreateInfo();
				imageCreateInfo.imageType							= VK_IMAGE_TYPE_2D;
				imageCreateInfo.format								= format;
				imageCreateInfo.mipLevels							= mipLevels;
				imageCreateInfo.arrayLayers							= 1;
				imageCreateInfo.samples								= VK_SAMPLE_COUNT_1_BIT;
				imageCreateInfo.tiling								= VK_IMAGE_TILING_OPTIMAL;
				imageCreateInfo.sharingMode							= VK_SHARING_MODE_EXCLUSIVE;
				imageCreateInfo.initialLayout						= VK_IMAGE_LAYOUT_UNDEFINED;
				imageCreateInfo.extent								= { width, height, 1 };
				imageCreateInfo.usage								= imageUsageFlags;
				// Ensure that the TRANSFER_DST bit is set for staging
				if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
					imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

				VK_EVAL(vkCreateImage		(device_->logicalDevice, &imageCreateInfo, nullptr, &image));

				vkGetImageMemoryRequirements(device_->logicalDevice, image, &memReqs);

				memAllocInfo.allocationSize							= memReqs.size;

				memAllocInfo.memoryTypeIndex						= device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
				VK_EVAL(vkAllocateMemory	(device_->logicalDevice, &memAllocInfo, nullptr, &deviceMemory));
				VK_EVAL(vkBindImageMemory	(device_->logicalDevice, image, deviceMemory, 0));

				VkImageSubresourceRange									subresourceRange				= {};
				subresourceRange.aspectMask							= VK_IMAGE_ASPECT_COLOR_BIT;
				subresourceRange.baseMipLevel						= 0;
				subresourceRange.levelCount							= mipLevels;
				subresourceRange.layerCount							= 1;

				vks::tools::setImageLayout	(copyCmd, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);				// Image barrier for optimal image (target). Optimal image will be used as destination for the copy.

				vkCmdCopyBufferToImage		(copyCmd, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(bufferCopyRegions.size()), bufferCopyRegions.data());	// Copy mip levels from staging buffer.

				this->imageLayout									= imageLayout_;
				vks::tools::setImageLayout	(copyCmd, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageLayout_, subresourceRange);								// Change texture image layout to shader read after all mip levels have been copied.

				device->flushCommandBuffer	(copyCmd, copyQueue);

				// Clean up staging resources
				vkFreeMemory				(device_->logicalDevice, stagingMemory, nullptr);
				vkDestroyBuffer				(device_->logicalDevice, stagingBuffer, nullptr);
			}
			else
			{
				// Prefer using optimal tiling, as linear tiling may support only a small set of features depending on implementation (e.g. no mip maps, only one layer, etc.)
				assert(formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);	// Check if this support is supported for linear tiling

				VkImage													mappableImage;
				VkDeviceMemory											mappableMemory;

				VkImageCreateInfo										imageCreateInfo					= vks::initializers::imageCreateInfo();
				imageCreateInfo.imageType							= VK_IMAGE_TYPE_2D;
				imageCreateInfo.format								= format;
				imageCreateInfo.extent								= { width, height, 1 };
				imageCreateInfo.mipLevels							= 1;
				imageCreateInfo.arrayLayers							= 1;
				imageCreateInfo.samples								= VK_SAMPLE_COUNT_1_BIT;
				imageCreateInfo.tiling								= VK_IMAGE_TILING_LINEAR;
				imageCreateInfo.usage								= imageUsageFlags;
				imageCreateInfo.sharingMode							= VK_SHARING_MODE_EXCLUSIVE;
				imageCreateInfo.initialLayout						= VK_IMAGE_LAYOUT_UNDEFINED;

				VK_EVAL(vkCreateImage		(device_->logicalDevice, &imageCreateInfo, nullptr, &mappableImage));		// Load mip map level 0 to linear tiling image

				vkGetImageMemoryRequirements(device_->logicalDevice, mappableImage, &memReqs);							// Get memory requirements for this image, like size and alignment.
				memAllocInfo.allocationSize							= memReqs.size;											// Set memory allocation size to required memory size

				// Get memory type that can be mapped to host memory
				memAllocInfo.memoryTypeIndex						= device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

				VK_EVAL(vkAllocateMemory	(device_->logicalDevice, &memAllocInfo, nullptr, &mappableMemory));		// Allocate host memory
				VK_EVAL(vkBindImageMemory	(device->logicalDevice, mappableImage, mappableMemory, 0));			// Bind allocated image for use

				// Get sub resource layout
				// Mip map count, array layer, etc.
				VkImageSubresource										subRes							= {};
				subRes.aspectMask									= VK_IMAGE_ASPECT_COLOR_BIT;
				subRes.mipLevel										= 0;

				VkSubresourceLayout										subResLayout;
				void													* data							= nullptr;

				vkGetImageSubresourceLayout	(device_->logicalDevice, mappableImage, &subRes, &subResLayout);				// Get sub resources layout. Includes row pitch, size offsets, etc.
				VK_EVAL(vkMapMemory			(device_->logicalDevice, mappableMemory, 0, memReqs.size, 0, &data));			// Map image memory

				memcpy(data, tex2D[subRes.mipLevel].data(), tex2D[subRes.mipLevel].size());								// Copy image data into memory
				vkUnmapMemory				(device_->logicalDevice, mappableMemory);

				// Linear tiled images don't need to be staged and can be directly used as textures
				image												= mappableImage;
				deviceMemory										= mappableMemory;
				imageLayout											= imageLayout_;

				// Setup image memory barrier
				vks::tools::setImageLayout(copyCmd, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, imageLayout_);

				device->flushCommandBuffer(copyCmd, copyQueue);
			}

			// Create a defaultsampler
			VkSamplerCreateInfo										samplerCreateInfo				= {};
			samplerCreateInfo.sType								= VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerCreateInfo.magFilter							= VK_FILTER_LINEAR;
			samplerCreateInfo.minFilter							= VK_FILTER_LINEAR;
			samplerCreateInfo.mipmapMode						= VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerCreateInfo.addressModeU						= VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCreateInfo.addressModeV						= VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCreateInfo.addressModeW						= VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCreateInfo.mipLodBias						= 0.0f;
			samplerCreateInfo.compareOp							= VK_COMPARE_OP_NEVER;
			samplerCreateInfo.minLod							= 0.0f;
			// Max level-of-detail should match mip level count
			samplerCreateInfo.maxLod							= (useStaging) ? (float)mipLevels : 0.0f;
			// Enable anisotropic filtering
			samplerCreateInfo.maxAnisotropy						= 8;
			samplerCreateInfo.anisotropyEnable					= VK_TRUE;
			samplerCreateInfo.borderColor						= VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
			VK_EVAL(vkCreateSampler			(device_->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

			// Create image view
			// Textures are not directly accessed by the shaders and are abstracted by image views containing additional information and sub resource ranges
			VkImageViewCreateInfo									viewCreateInfo					= {};
			viewCreateInfo.sType								= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewCreateInfo.viewType								= VK_IMAGE_VIEW_TYPE_2D;
			viewCreateInfo.format								= format;
			viewCreateInfo.components							= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
			viewCreateInfo.subresourceRange						= { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			viewCreateInfo.subresourceRange.levelCount			= (useStaging) ? mipLevels : 1;			// Linear tiling usually won't support mip maps. Only set mip map count if optimal tiling is used
			viewCreateInfo.image								= image;
			VK_EVAL(vkCreateImageView		(device_->logicalDevice, &viewCreateInfo, nullptr, &view));

			// Update descriptor image info member that can be used for setting up descriptor sets
			updateDescriptor();
		}

		// Creates a 2D texture from a buffer
		void												fromBuffer
			(	void					* buffer													// Buffer containing texture data to upload
			,	VkDeviceSize			bufferSize													// Size of the buffer in machine units
			,	VkFormat				format														// Vulkan format of the image data stored in the file
			,	uint32_t				width_														// Width of the texture to create
			,	uint32_t				height_														// Height of the texture to create
			,	vks::VulkanDevice		* device_													// Vulkan device to create the texture on
			,	VkQueue					copyQueue													// Queue used for the texture staging copy commands (must support transfer)
			,	VkFilter				filter			= VK_FILTER_LINEAR							// Texture filtering for the sampler (defaults to VK_FILTER_LINEAR)
			,	VkImageUsageFlags		imageUsageFlags	= VK_IMAGE_USAGE_SAMPLED_BIT				// Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
			,	VkImageLayout			imageLayout_		= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL	// Usage layout for the texture (defaults VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
			)
		{
			assert(buffer);

			this->device										= device_;
			width												= width_;
			height												= height_;
			mipLevels											= 1;

			VkMemoryAllocateInfo									memAllocInfo					= vks::initializers::memoryAllocateInfo();
			VkMemoryRequirements									memReqs;

			VkCommandBuffer											copyCmd							= device_->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);	// Use a separate command buffer for texture loading

			// Create a host-visible staging buffer that contains the raw image data
			VkBuffer												stagingBuffer;
			VkDeviceMemory											stagingMemory;

			VkBufferCreateInfo										bufferCreateInfo				= vks::initializers::bufferCreateInfo();
			bufferCreateInfo.size								= bufferSize;
			// This buffer is used as a transfer source for the buffer copy
			bufferCreateInfo.usage								= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			bufferCreateInfo.sharingMode						= VK_SHARING_MODE_EXCLUSIVE;

			VK_EVAL(vkCreateBuffer			(device_->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

			// Get memory requirements for the staging buffer (alignment, memory type bits)
			vkGetBufferMemoryRequirements	(device_->logicalDevice, stagingBuffer, &memReqs);

			memAllocInfo.allocationSize							= memReqs.size;
			// Get memory type index for a host visible buffer
			memAllocInfo.memoryTypeIndex						= device_->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			VK_EVAL(vkAllocateMemory		(device_->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
			VK_EVAL(vkBindBufferMemory		(device_->logicalDevice, stagingBuffer, stagingMemory, 0));

			// Copy texture data into staging buffer
			uint8_t												* data								= nullptr;
			VK_EVAL(vkMapMemory				(device_->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));
			memcpy(data, buffer, (size_t)bufferSize);
			vkUnmapMemory					(device_->logicalDevice, stagingMemory);

			VkBufferImageCopy										bufferCopyRegion				= {};
			bufferCopyRegion.imageSubresource.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
			bufferCopyRegion.imageSubresource.mipLevel			= 0;
			bufferCopyRegion.imageSubresource.baseArrayLayer	= 0;
			bufferCopyRegion.imageSubresource.layerCount		= 1;
			bufferCopyRegion.imageExtent.width					= width_;
			bufferCopyRegion.imageExtent.height					= height_;
			bufferCopyRegion.imageExtent.depth					= 1;
			bufferCopyRegion.bufferOffset						= 0;

			// Create optimal tiled target image
			VkImageCreateInfo										imageCreateInfo					= vks::initializers::imageCreateInfo();
			imageCreateInfo.imageType							= VK_IMAGE_TYPE_2D;
			imageCreateInfo.format								= format;
			imageCreateInfo.mipLevels							= mipLevels;
			imageCreateInfo.arrayLayers							= 1;
			imageCreateInfo.samples								= VK_SAMPLE_COUNT_1_BIT;
			imageCreateInfo.tiling								= VK_IMAGE_TILING_OPTIMAL;
			imageCreateInfo.sharingMode							= VK_SHARING_MODE_EXCLUSIVE;
			imageCreateInfo.initialLayout						= VK_IMAGE_LAYOUT_UNDEFINED;
			imageCreateInfo.extent								= { width_, height_, 1 };
			imageCreateInfo.usage								= imageUsageFlags;
			// Ensure that the TRANSFER_DST bit is set for staging
			if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
				imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

			VK_EVAL(vkCreateImage			(device_->logicalDevice, &imageCreateInfo, nullptr, &image));

			vkGetImageMemoryRequirements	(device_->logicalDevice, image, &memReqs);

			memAllocInfo.allocationSize							= memReqs.size;

			memAllocInfo.memoryTypeIndex						= device_->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_EVAL(vkAllocateMemory		(device_->logicalDevice, &memAllocInfo, nullptr, &deviceMemory));
			VK_EVAL(vkBindImageMemory		(device_->logicalDevice, image, deviceMemory, 0));

			VkImageSubresourceRange									subresourceRange				= {};
			subresourceRange.aspectMask							= VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.baseMipLevel						= 0;
			subresourceRange.levelCount							= mipLevels;
			subresourceRange.layerCount							= 1;

			vks::tools::setImageLayout	(copyCmd, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);	// Image barrier for optimal image (target). Optimal image will be used as destination for the copy.
			vkCmdCopyBufferToImage		(copyCmd, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);											// Copy mip levels from staging buffer

			this->imageLayout									= imageLayout_;
			vks::tools::setImageLayout	(copyCmd, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageLayout_, subresourceRange);					// Change texture image layout to shader read after all mip levels have been copied

			device_->flushCommandBuffer	(copyCmd, copyQueue);

			// Clean up staging resources
			vkFreeMemory					(device_->logicalDevice, stagingMemory, nullptr);
			vkDestroyBuffer					(device_->logicalDevice, stagingBuffer, nullptr);

			// Create sampler
			VkSamplerCreateInfo										samplerCreateInfo				= {};
			samplerCreateInfo.sType								= VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerCreateInfo.magFilter							= filter;
			samplerCreateInfo.minFilter							= filter;
			samplerCreateInfo.mipmapMode						= VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerCreateInfo.addressModeU						= VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCreateInfo.addressModeV						= VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCreateInfo.addressModeW						= VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCreateInfo.mipLodBias						= 0.0f;
			samplerCreateInfo.compareOp							= VK_COMPARE_OP_NEVER;
			samplerCreateInfo.minLod							= 0.0f;
			samplerCreateInfo.maxLod							= 0.0f;
			VK_EVAL(vkCreateSampler			(device_->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

			// Create image view
			VkImageViewCreateInfo									viewCreateInfo					= {};
			viewCreateInfo.sType								= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewCreateInfo.pNext								= NULL;
			viewCreateInfo.viewType								= VK_IMAGE_VIEW_TYPE_2D;
			viewCreateInfo.format								= format;
			viewCreateInfo.components							= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
			viewCreateInfo.subresourceRange						= { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			viewCreateInfo.subresourceRange.levelCount			= 1;
			viewCreateInfo.image								= image;
			VK_EVAL(vkCreateImageView		(device_->logicalDevice, &viewCreateInfo, nullptr, &view));

			// Update descriptor image info member that can be used for setting up descriptor sets
			updateDescriptor();
		}

	};	// struct

	struct Texture2DArray : public Texture {
		// Load a 2D texture array including all mip levels
		void												loadFromFile
			(	std::string				filename															// File to load (supports .ktx and .dds)
			,	VkFormat				format																// Vulkan format of the image data stored in the file
			,	vks::VulkanDevice		* device_															// Vulkan device to create the texture on
			,	VkQueue					copyQueue															// Queue used for the texture staging copy commands (must support transfer)
			,	VkImageUsageFlags		imageUsageFlags			= VK_IMAGE_USAGE_SAMPLED_BIT				// Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
			,	VkImageLayout			imageLayout_			= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL	// Usage layout for the texture (defaults VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
			)
		{
#if defined(__ANDROID__)
			// Textures are stored inside the apk on Android (compressed)
			// So they need to be loaded via the asset manager
			AAsset* asset										= AAssetManager_open(androidApp->activity->assetManager, filename.c_str(), AASSET_MODE_STREAMING);
			assert(asset);
			size_t													size							= AAsset_getLength(asset);
			assert(size > 0);

			void													* textureData					= malloc(size);
			AAsset_read(asset, textureData, size);
			AAsset_close(asset);

			gli::texture2d_array									tex2DArray(gli::load((const char*)textureData, size));

			free(textureData);
#else
			gli::texture2d_array									tex2DArray(gli::load(filename));
#endif	

			assert(!tex2DArray.empty());

			this->device										= device_;
			width												= static_cast<uint32_t>(tex2DArray.extent().x);
			height												= static_cast<uint32_t>(tex2DArray.extent().y);
			layerCount											= static_cast<uint32_t>(tex2DArray.layers());
			mipLevels											= static_cast<uint32_t>(tex2DArray.levels());

			VkMemoryAllocateInfo									memAllocInfo					= vks::initializers::memoryAllocateInfo();
			VkMemoryRequirements									memReqs;

			// Create a host-visible staging buffer that contains the raw image data
			VkBuffer												stagingBuffer;
			VkDeviceMemory											stagingMemory;
			VkBufferCreateInfo										bufferCreateInfo				= vks::initializers::bufferCreateInfo();
			bufferCreateInfo.size								= tex2DArray.size();
			bufferCreateInfo.usage								= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;		// This buffer is used as a transfer source for the buffer copy
			bufferCreateInfo.sharingMode						= VK_SHARING_MODE_EXCLUSIVE;

			VK_EVAL(vkCreateBuffer			(device_->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

			// Get memory requirements for the staging buffer (alignment, memory type bits)
			vkGetBufferMemoryRequirements	(device_->logicalDevice, stagingBuffer, &memReqs);

			memAllocInfo.allocationSize							= memReqs.size;
			memAllocInfo.memoryTypeIndex						= device_->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);	// Get memory type index for a host visible buffer

			VK_EVAL(vkAllocateMemory		(device_->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
			VK_EVAL(vkBindBufferMemory		(device_->logicalDevice, stagingBuffer, stagingMemory, 0));

			// Copy texture data into staging buffer
			uint8_t													* data							= nullptr;
			VK_EVAL(vkMapMemory				(device_->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));
			memcpy(data, tex2DArray.data(), static_cast<size_t>(tex2DArray.size()));
			vkUnmapMemory					(device_->logicalDevice, stagingMemory);

			// Setup buffer copy regions for each layer including all of it's miplevels
			std::vector<VkBufferImageCopy>							bufferCopyRegions;
			size_t													offset							= 0;

			for (uint32_t layer = 0; layer < layerCount; layer++)
				for (uint32_t level = 0; level < mipLevels; level++) {
					VkBufferImageCopy										bufferCopyRegion				= {};
					bufferCopyRegion.imageSubresource.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
					bufferCopyRegion.imageSubresource.mipLevel			= level;
					bufferCopyRegion.imageSubresource.baseArrayLayer	= layer;
					bufferCopyRegion.imageSubresource.layerCount		= 1;
					bufferCopyRegion.imageExtent.width					= static_cast<uint32_t>(tex2DArray[layer][level].extent().x);
					bufferCopyRegion.imageExtent.height					= static_cast<uint32_t>(tex2DArray[layer][level].extent().y);
					bufferCopyRegion.imageExtent.depth					= 1;
					bufferCopyRegion.bufferOffset						= offset;

					bufferCopyRegions.push_back(bufferCopyRegion);

					// Increase offset into staging buffer for next level / face
					offset												+= tex2DArray[layer][level].size();
				}

			// Create optimal tiled target image
			VkImageCreateInfo										imageCreateInfo					= vks::initializers::imageCreateInfo();
			imageCreateInfo.imageType							= VK_IMAGE_TYPE_2D;
			imageCreateInfo.format								= format;
			imageCreateInfo.samples								= VK_SAMPLE_COUNT_1_BIT;
			imageCreateInfo.tiling								= VK_IMAGE_TILING_OPTIMAL;
			imageCreateInfo.sharingMode							= VK_SHARING_MODE_EXCLUSIVE;
			imageCreateInfo.initialLayout						= VK_IMAGE_LAYOUT_UNDEFINED;
			imageCreateInfo.extent								= { width, height, 1 };
			imageCreateInfo.usage								= imageUsageFlags;
			// Ensure that the TRANSFER_DST bit is set for staging
			if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
				imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

			imageCreateInfo.arrayLayers							= layerCount;
			imageCreateInfo.mipLevels							= mipLevels;

			VK_EVAL(vkCreateImage			(device_->logicalDevice, &imageCreateInfo, nullptr, &image));

			vkGetImageMemoryRequirements	(device_->logicalDevice, image, &memReqs);

			memAllocInfo.allocationSize							= memReqs.size;
			memAllocInfo.memoryTypeIndex						= device_->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			VK_EVAL(vkAllocateMemory		(device_->logicalDevice, &memAllocInfo, nullptr, &deviceMemory));
			VK_EVAL(vkBindImageMemory		(device_->logicalDevice, image, deviceMemory, 0));

			// Use a separate command buffer for texture loading
			VkCommandBuffer											copyCmd							= device_->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

			// Image barrier for optimal image (target)
			// Set initial layout for all array layers (faces) of the optimal (target) tiled texture
			VkImageSubresourceRange									subresourceRange				= {};
			subresourceRange.aspectMask							= VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.baseMipLevel						= 0;
			subresourceRange.levelCount							= mipLevels;
			subresourceRange.layerCount							= layerCount;

			vks::tools::setImageLayout	(copyCmd, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

			// Copy the layers and mip levels from the staging buffer to the optimal tiled image
			vkCmdCopyBufferToImage		(copyCmd, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(bufferCopyRegions.size()), bufferCopyRegions.data());

			// Change texture image layout to shader read after all faces have been copied
			this->imageLayout									= imageLayout_;
			vks::tools::setImageLayout	(copyCmd, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageLayout_, subresourceRange);

			device_->flushCommandBuffer	(copyCmd, copyQueue);

			// Create sampler
			VkSamplerCreateInfo										samplerCreateInfo				= vks::initializers::samplerCreateInfo();
			samplerCreateInfo.magFilter							= VK_FILTER_LINEAR;
			samplerCreateInfo.minFilter							= VK_FILTER_LINEAR;
			samplerCreateInfo.mipmapMode						= VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerCreateInfo.addressModeU						= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCreateInfo.addressModeV						= samplerCreateInfo.addressModeU;
			samplerCreateInfo.addressModeW						= samplerCreateInfo.addressModeU;
			samplerCreateInfo.mipLodBias						= 0.0f;
			samplerCreateInfo.maxAnisotropy						= 8;
			samplerCreateInfo.compareOp							= VK_COMPARE_OP_NEVER;
			samplerCreateInfo.minLod							= 0.0f;
			samplerCreateInfo.maxLod							= (float)mipLevels;
			samplerCreateInfo.borderColor						= VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
			VK_EVAL(vkCreateSampler			(device_->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

			// Create image view
			VkImageViewCreateInfo									viewCreateInfo					= vks::initializers::imageViewCreateInfo();
			viewCreateInfo.viewType								= VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			viewCreateInfo.format								= format;
			viewCreateInfo.components							= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
			viewCreateInfo.subresourceRange						= { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			viewCreateInfo.subresourceRange.layerCount			= layerCount;
			viewCreateInfo.subresourceRange.levelCount			= mipLevels;
			viewCreateInfo.image								= image;
			VK_EVAL(vkCreateImageView		(device_->logicalDevice, &viewCreateInfo, nullptr, &view));

			// Clean up staging resources
			vkFreeMemory					(device_->logicalDevice, stagingMemory, nullptr);
			vkDestroyBuffer					(device_->logicalDevice, stagingBuffer, nullptr);

			// Update descriptor image info member that can be used for setting up descriptor sets
			updateDescriptor();
		}
	};

	struct TextureCubeMap : public Texture {
	public:
		// Load a cubemap texture including all mip levels from a single file
		void												loadFromFile
			(	std::string			filename													// File to load (supports .ktx and .dds)
			,	VkFormat			format														// Vulkan format of the image data stored in the file
			,	vks::VulkanDevice	* device_													// Vulkan device to create the texture on
			,	VkQueue				copyQueue													// Queue used for the texture staging copy commands (must support transfer)
			,	VkImageUsageFlags	imageUsageFlags	= VK_IMAGE_USAGE_SAMPLED_BIT				// Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
			,	VkImageLayout		imageLayout_	= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL	// Usage layout for the texture (defaults VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
			)
		{
#if defined(__ANDROID__)
			// Textures are stored inside the apk on Android (compressed)
			// So they need to be loaded via the asset manager
			AAsset*													asset							= AAssetManager_open(androidApp->activity->assetManager, filename.c_str(), AASSET_MODE_STREAMING);
			assert(asset);
			size_t													size							= AAsset_getLength(asset);
			assert(size > 0);

			void													* textureData					= malloc(size);
			AAsset_read(asset, textureData, size);
			AAsset_close(asset);

			gli::texture_cube										texCube(gli::load((const char*)textureData, size));

			free(textureData);
#else
			gli::texture_cube										texCube(gli::load(filename));
#endif	
			assert(!texCube.empty());

			this->device										= device_;
			width												= static_cast<uint32_t>(texCube.extent().x);
			height												= static_cast<uint32_t>(texCube.extent().y);
			mipLevels											= static_cast<uint32_t>(texCube.levels());

			VkMemoryAllocateInfo									memAllocInfo					= vks::initializers::memoryAllocateInfo();
			VkMemoryRequirements									memReqs;

			// Create a host-visible staging buffer that contains the raw image data
			VkBuffer												stagingBuffer;
			VkDeviceMemory											stagingMemory;
			VkBufferCreateInfo										bufferCreateInfo				= vks::initializers::bufferCreateInfo();
			bufferCreateInfo.size								= texCube.size();
			bufferCreateInfo.usage								= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;																								// This buffer is used as a transfer source for the buffer copy
			bufferCreateInfo.sharingMode						= VK_SHARING_MODE_EXCLUSIVE;

			VK_EVAL(vkCreateBuffer			(device_->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));
			vkGetBufferMemoryRequirements	(device_->logicalDevice, stagingBuffer, &memReqs);																										// Get memory requirements for the staging buffer (alignment, memory type bits)

			memAllocInfo.allocationSize							= memReqs.size;
			memAllocInfo.memoryTypeIndex						= device_->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);	// Get memory type index for a host visible buffer

			VK_EVAL(vkAllocateMemory		(device_->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
			VK_EVAL(vkBindBufferMemory		(device_->logicalDevice, stagingBuffer, stagingMemory, 0));

			// Copy texture data into staging buffer
			uint8_t													* data							= nullptr;
			VK_EVAL(vkMapMemory				(device_->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));
			memcpy(data, texCube.data(), texCube.size());
			vkUnmapMemory					(device_->logicalDevice, stagingMemory);

			// Setup buffer copy regions for each face including all of it's miplevels
			std::vector<VkBufferImageCopy>							bufferCopyRegions;
			size_t													offset							= 0;

			for (uint32_t face = 0; face < 6; face++)
				for (uint32_t level = 0; level < mipLevels; level++) {
					VkBufferImageCopy										bufferCopyRegion				= {};
					bufferCopyRegion.imageSubresource.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
					bufferCopyRegion.imageSubresource.mipLevel			= level;
					bufferCopyRegion.imageSubresource.baseArrayLayer	= face;
					bufferCopyRegion.imageSubresource.layerCount		= 1;
					bufferCopyRegion.imageExtent.width					= static_cast<uint32_t>(texCube[face][level].extent().x);
					bufferCopyRegion.imageExtent.height					= static_cast<uint32_t>(texCube[face][level].extent().y);
					bufferCopyRegion.imageExtent.depth					= 1;
					bufferCopyRegion.bufferOffset						= offset;

					bufferCopyRegions.push_back(bufferCopyRegion);

					offset												+= texCube[face][level].size();		// Increase offset into staging buffer for next level / face
				}

			// Create optimal tiled target image
			VkImageCreateInfo										imageCreateInfo					= vks::initializers::imageCreateInfo();
			imageCreateInfo.imageType							= VK_IMAGE_TYPE_2D;
			imageCreateInfo.format								= format;
			imageCreateInfo.mipLevels							= mipLevels;
			imageCreateInfo.samples								= VK_SAMPLE_COUNT_1_BIT;
			imageCreateInfo.tiling								= VK_IMAGE_TILING_OPTIMAL;
			imageCreateInfo.sharingMode							= VK_SHARING_MODE_EXCLUSIVE;
			imageCreateInfo.initialLayout						= VK_IMAGE_LAYOUT_UNDEFINED;
			imageCreateInfo.extent								= { width, height, 1 };
			imageCreateInfo.usage								= imageUsageFlags;
			// Ensure that the TRANSFER_DST bit is set for staging
			if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
				imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

			imageCreateInfo.arrayLayers							= 6;										// Cube faces count as array layers in Vulkan
			imageCreateInfo.flags								= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;		// This flag is required for cube map images


			VK_EVAL(vkCreateImage		(device_->logicalDevice, &imageCreateInfo, nullptr, &image));

			vkGetImageMemoryRequirements(device_->logicalDevice, image, &memReqs);

			memAllocInfo.allocationSize							= memReqs.size;
			memAllocInfo.memoryTypeIndex						= device_->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			VK_EVAL(vkAllocateMemory	(device_->logicalDevice, &memAllocInfo, nullptr, &deviceMemory));
			VK_EVAL(vkBindImageMemory	(device_->logicalDevice, image, deviceMemory, 0));

			VkCommandBuffer											copyCmd							= device_->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);				// Use a separate command buffer for texture loading.

			// Image barrier for optimal image (target)
			// Set initial layout for all array layers (faces) of the optimal (target) tiled texture
			VkImageSubresourceRange									subresourceRange				= {};
			subresourceRange.aspectMask							= VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.baseMipLevel						= 0;
			subresourceRange.levelCount							= mipLevels;
			subresourceRange.layerCount							= 6;

			vks::tools::setImageLayout	(copyCmd, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
			vkCmdCopyBufferToImage		(copyCmd, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(bufferCopyRegions.size()), bufferCopyRegions.data());		// Copy the cube map faces from the staging buffer to the optimal tiled image.

			this->imageLayout									= imageLayout_;
			vks::tools::setImageLayout	(copyCmd, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageLayout_, subresourceRange);								// Change texture image layout to shader read after all faces have been copied.

			device_->flushCommandBuffer	(copyCmd, copyQueue);

			// Create sampler
			VkSamplerCreateInfo										samplerCreateInfo				= vks::initializers::samplerCreateInfo();
			samplerCreateInfo.magFilter							= VK_FILTER_LINEAR;
			samplerCreateInfo.minFilter							= VK_FILTER_LINEAR;
			samplerCreateInfo.mipmapMode						= VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerCreateInfo.addressModeU						= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCreateInfo.addressModeV						= samplerCreateInfo.addressModeU;
			samplerCreateInfo.addressModeW						= samplerCreateInfo.addressModeU;
			samplerCreateInfo.mipLodBias						= 0.0f;
			samplerCreateInfo.maxAnisotropy						= 8;
			samplerCreateInfo.compareOp							= VK_COMPARE_OP_NEVER;
			samplerCreateInfo.minLod							= 0.0f;
			samplerCreateInfo.maxLod							= (float)mipLevels;
			samplerCreateInfo.borderColor						= VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
			VK_EVAL(vkCreateSampler		(device_->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

			// Create image view
			VkImageViewCreateInfo									viewCreateInfo					= vks::initializers::imageViewCreateInfo();
			viewCreateInfo.viewType								= VK_IMAGE_VIEW_TYPE_CUBE;
			viewCreateInfo.format								= format;
			viewCreateInfo.components							= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
			viewCreateInfo.subresourceRange						= { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			viewCreateInfo.subresourceRange.layerCount			= 6;
			viewCreateInfo.subresourceRange.levelCount			= mipLevels;
			viewCreateInfo.image								= image;
			VK_EVAL(vkCreateImageView	(device_->logicalDevice, &viewCreateInfo, nullptr, &view));

			// Clean up staging resources
			vkFreeMemory				(device_->logicalDevice, stagingMemory, nullptr);
			vkDestroyBuffer				(device_->logicalDevice, stagingBuffer, nullptr);

			// Update descriptor image info member that can be used for setting up descriptor sets
			updateDescriptor();
		}	// loadFromFile()
	};	// struct 

}