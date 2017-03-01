// Vulkan Example base class
// 
// Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
// 
// This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
#pragma once

#ifdef _WIN32
#pragma comment(linker, "/subsystem:windows")
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#elif defined(__ANDROID__)
#include <android/native_activity.h>
#include <android/asset_manager.h>
#include <android_native_app_glue.h>
#include "vulkanandroid.h"
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
#include <wayland-client.h>
#elif defined(__linux__)
#include <xcb/xcb.h>
#endif

#include <iostream>
#include <chrono>
#include <sys/stat.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

#include <string>
#include <array>

#include "keycodes.hpp"
#include "VulkanDebug.h"

#include "VulkanDevice.hpp"
#include "vulkanswapchain.hpp"
#include "vulkantextoverlay.hpp"
#include "camera.hpp"

class VulkanExampleBase
{
private:	
	float								fpsTimer					= 0.0f;												// fps timer (one second interval)
	uint32_t							destWidth					= 0													// Destination dimensions for resizing the window
		,								destHeight					= 0
		;
	bool								viewUpdated					= false;											// Indicates that the view (position, rotation) has changed and... ?
	bool								resizing					= false;

	std::string							getWindowTitle				();													// Get window title with example name, device, et.
	void								windowResize				();													// Called if the window is resized and some resources have to be recreatesd.

protected:
	
	float								frameTimer					= 1.0f;												// Last frame time, measured using a high performance timer (if available).
	uint32_t							frameCounter				= 0;												// Frame counter to display fps.
	uint32_t							lastFPS						= 0;												// Vulkan instance, stores all per-application states.
	VkInstance							instance					= VK_NULL_HANDLE;									// Physical device (GPU) that Vulkan will ise.
	VkPhysicalDevice					physicalDevice				= VK_NULL_HANDLE;
	VkPhysicalDeviceProperties			deviceProperties;																// Stores physical device properties (for e.g. checking device limits).
	VkPhysicalDeviceFeatures			deviceFeatures;																	// Stores the features available on the selected physical device (for e.g. checking if a feature is available).
	VkPhysicalDeviceMemoryProperties	deviceMemoryProperties;															// Stores all available memory (type) properties for the physical device.
	VkPhysicalDeviceFeatures			enabledFeatures{};																// Set of physical device features to be enabled for this example (must be set in the derived constructor). @note By default no phyiscal device features are enabled.
	std::vector<const char*>			enabledExtensions;																// Set of device extensions to be enabled for this example (must be set in the derived constructor.
	VkDevice							device						= VK_NULL_HANDLE;									// Logical device, application's view of the physical device (GPU) todo: getter? should always point to VulkanDevice->device
	vks::VulkanDevice					* vulkanDevice				= nullptr;											// Encapsulated physical and logical vulkan device
	VkQueue								queue						= VK_NULL_HANDLE;									// Handle to the device graphics queue that command buffers are submitted to
	VkFormat							depthFormat;																	// Depth buffer format (selected during Vulkan initialization)
	VkCommandPool						cmdPool						= VK_NULL_HANDLE;									// Command buffer pool
	VkPipelineStageFlags				submitPipelineStages		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;	// Pipeline stages used to wait at for graphics queue submissions
	VkSubmitInfo						submitInfo;																		// Contains command buffers and semaphores to be presented to the queue
	std::vector<VkCommandBuffer>		drawCmdBuffers;																	// Command buffers used for rendering
	VkRenderPass						renderPass					= VK_NULL_HANDLE;									// Global render pass for frame buffer writes
	std::vector<VkFramebuffer>			frameBuffers;																	// List of available frame buffers (same as number of swap chain images)
	uint32_t							currentBuffer				= 0;												// Active frame buffer index
	VkDescriptorPool					descriptorPool				= VK_NULL_HANDLE;									// Descriptor set pool
	std::vector<VkShaderModule>			shaderModules;																	// List of shader modules created (stored for cleanup)
	VkPipelineCache						pipelineCache				= VK_NULL_HANDLE;									// Pipeline cache object
	VulkanSwapChain						swapChain					;													// Wraps the swap chain to present images (framebuffers) to the windowing system

	struct {
		VkSemaphore							presentComplete				= VK_NULL_HANDLE;								// Swap chain image presentation
		VkSemaphore							renderComplete				= VK_NULL_HANDLE;								// Command buffer submission and execution
		VkSemaphore							textOverlayComplete			= VK_NULL_HANDLE;								// Text overlay submission and execution
	}									semaphores;																		// Synchronization semaphores
	
	// Simple texture loader
	//vks::tools::VulkanTextureLoader	* textureLoader				= nullptr;

	// Returns the base asset path (for shaders, models, textures) depending on the os
	const std::string					getAssetPath				();
public: 
	bool								prepared					= false;
	uint32_t							width						= 1280;
	uint32_t							height						= 720;

	// Example settings that can be changed e.g. by command line arguments
	struct Settings {
		bool								validation					= false;	// Activates validation layers (and message output) when set to true
		bool								fullscreen					= false;	// Set to true if fullscreen mode has been requested via command line
		bool								vsync						= false;	// Set to true if v-sync will be forced for the swapchain
	}									settings;

	VkClearColorValue					defaultClearColor			= { { 0.025f, 0.025f, 0.025f, 1.0f } };
	float								zoom						= 0;
	static std::vector<const char*>		args;

	
	float								timer						= 0.0f;			// Defines a frame rate independent timer value clamped from -1.0...1.0. For use in animations, rotations, etc.
	float								timerSpeed					= 0.25f;		// Multiplier for speeding up (or slowing down) the global timer
	bool								paused						= false;
	bool								enableTextOverlay			= false;
	VulkanTextOverlay					* textOverlay				= nullptr;

	float								rotationSpeed				= 1.0f;			// Use to adjust mouse rotation speed
	float								zoomSpeed					= 1.0f;			// Use to adjust mouse zoom speed
	Camera								camera;

	glm::vec3							rotation					= glm::vec3();
	glm::vec3							cameraPos					= glm::vec3();
	glm::vec2							mousePos;

	std::string							title						= "Vulkan Example";
	std::string							name						= "vulkanExample";

	struct 
	{
		VkImage								image						= VK_NULL_HANDLE;
		VkDeviceMemory						mem							= VK_NULL_HANDLE;
		VkImageView							view						= VK_NULL_HANDLE;
	}									depthStencil;

	// Gamepad state (only one pad supported)
	struct
	{
		glm::vec2							axisLeft					= glm::vec2(0.0f);
		glm::vec2							axisRight					= glm::vec2(0.0f);
	}									gamePadState;

	// OS specific 
#if defined(_WIN32)
	HWND								window						= NULL;
	HINSTANCE							windowInstance				= NULL;
#elif defined(__ANDROID__)
	
	bool								focused						= false;		// true if application has focused, false if moved to background

#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
	wl_display							* display					= nullptr;
	wl_registry							* registry					= nullptr;
	wl_compositor						* compositor				= nullptr;
	wl_shell							* shell						= nullptr;
	wl_seat								* seat						= nullptr;
	wl_pointer							* pointer					= nullptr;
	wl_keyboard							* keyboard					= nullptr;
	wl_surface							* surface					= nullptr;
	wl_shell_surface					* shell_surface				= nullptr;
	bool								quit						= false;
	struct {
		bool								left						= false;
		bool								right						= false;
		bool								middle						= false;
	}									mouseButtons;
#elif defined(__linux__)				
	struct {
		bool								left						= false;
		bool								right						= false;
		bool								middle						= false;
	}									mouseButtons;
	bool								quit						= false;
	xcb_connection_t					* connection				= nullptr;
	xcb_screen_t						* screen					= nullptr;
	xcb_window_t						window;
	xcb_intern_atom_reply_t				* atom_wm_delete_window		= nullptr;
#endif

										~VulkanExampleBase			();
										VulkanExampleBase			(bool enableValidation);

	// Setup the vulkan instance, enable required extensions and connect to the physical device (GPU)
	void								initVulkan();

#if defined(_WIN32)
	void								setupConsole				(std::string title);
	HWND								setupWindow					(HINSTANCE hinstance, WNDPROC wndproc);
	void								handleMessages				(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
#elif defined(__ANDROID__)
	static int32_t						handleAppInput				(struct android_app* app, AInputEvent* event);
	static void							handleAppCommand			(android_app* app, int32_t cmd);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
	wl_shell_surface					* setupWindow				();
	void								initWaylandConnection		();
	static void							registryGlobalCb			(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
	void								registryGlobal				(struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
	static void							registryGlobalRemoveCb		(void *data, struct wl_registry *registry, uint32_t name);
	static void							seatCapabilitiesCb			(void *data, wl_seat *seat, uint32_t caps);
	void								seatCapabilities			(wl_seat *seat, uint32_t caps);
	static void							pointerEnterCb				(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t sx, wl_fixed_t sy);
	static void							pointerLeaveCb				(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface);
	static void							pointerMotionCb				(void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy);
	void								pointerMotion				(struct wl_pointer *pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy);
	static void							pointerButtonCb				(void *data, struct wl_pointer *wl_pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state);
	void								pointerButton				(struct wl_pointer *wl_pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state);
	static void							pointerAxisCb				(void *data, struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis, wl_fixed_t value);
	void								pointerAxis					(struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis, wl_fixed_t value);
	static void							keyboardKeymapCb			(void *data, struct wl_keyboard *keyboard, uint32_t format, int fd, uint32_t size);
	static void							keyboardEnterCb				(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys);
	static void							keyboardLeaveCb				(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface);
	static void							keyboardKeyCb				(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state);
	void								keyboardKey					(struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state);
	static void							keyboardModifiersCb			(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group);

#elif defined(__linux__)
	xcb_window_t						setupWindow					();
	void								initxcbConnection			();
	void								handleEvent					(const xcb_generic_event_t *event);
#endif
	
	virtual VkResult					createInstance				(bool enableValidation);											// Create the application wide Vulkan instance: note: Virtual, can be overriden by derived example class for custom instance creation
	virtual void						render						()															= 0;	// Pure virtual render function (override in derived class)
	virtual void						viewChanged					()															{}		// Called when view change occurs. Can be overriden in derived class to e.g. update uniform buffers. Containing view dependant matrices
	virtual void						keyPressed					(uint32_t)													{}		// Called if a key is pressed. Can be overriden in derived class to do custom key handling
	virtual void						windowResized				()															{}		// Called when the window has been resized. Can be overriden in derived class to recreate or rebuild resources attached to the frame buffer / swapchain
	virtual void						buildCommandBuffers			()															{}		// Pure virtual function to be overriden by the dervice class. Called in case of an event where e.g. the framebuffer has to be rebuild and thus all command buffers that may reference this

	void								createCommandPool			();																	// Creates a new (graphics) command pool object storing command buffers
	virtual void						setupDepthStencil			();																	// Setup default depth and stencil views
	virtual void						setupFrameBuffer			();																	// Create framebuffers for all requested swap chain images. Can be overriden in derived class to setup a custom framebuffer (e.g. for MSAA)
	virtual void						setupRenderPass				();																	// Setup a default render pass. Can be overriden in derived class to setup a custom render pass (e.g. for MSAA)
	void								initSwapchain				();																	// Connect and prepare the swap chain
	inline void							setupSwapChain				()															{ swapChain.create(&width, &height, settings.vsync); }	// Create swap chain images
	bool								checkCommandBuffers			();																	// Check if command buffers are valid (!= VK_NULL_HANDLE)
	void								createCommandBuffers		();																	// Create command buffers for drawing commands
	void								destroyCommandBuffers		();																	// Destroy all command buffers and set their handles to VK_NULL_HANDLE. May be necessary during runtime if options are toggled 
	VkCommandBuffer						createCommandBuffer			(VkCommandBufferLevel level, bool begin);							// Command buffer creation. Creates and returns a new command buffer
	void								flushCommandBuffer			(VkCommandBuffer commandBuffer, VkQueue queue, bool free);			// End the command buffer, submit it to the queue and free (if requested). Note: Waits for the queue to become idle
	void								createPipelineCache			();																	// Create a cache pool for rendering pipelines
	virtual void						prepare						();																	// Prepare commonly used Vulkan functions

	
	VkPipelineShaderStageCreateInfo		loadShader					(std::string fileName, VkShaderStageFlagBits stage);				// Load a SPIR-V shader
	void								renderLoop					();																	// Start the main render loop
	void								updateTextOverlay			();
	virtual void						getOverlayText				(VulkanTextOverlay *)										{}		// Called when the text overlay is updating. Can be overriden in derived class to add custom text to the overlay

	// Prepare the frame for workload submission
	// - Acquires the next image from the swap chain 
	// - Sets the default wait and signal semaphores
	void								prepareFrame				();

	void								submitFrame					();																	// Submit the frames' workload. Submits the text overlay (if enabled)

};

extern "C"
{
	int	createVulkanExample	(VulkanExampleBase** _vulkanExample);
	int	deleteVulkanExample	(VulkanExampleBase** _vulkanExample);
}

#define VULKAN_EXAMPLE_EXPORT_FUNCTIONS()	\
	int	createVulkanExample(VulkanExampleBase** _vulkanExampleBase)	{ if(0 == _vulkanExampleBase) return -1; VulkanExample* _vulkanExample = (VulkanExample*)*_vulkanExampleBase; *_vulkanExampleBase = new VulkanExample();	if(_vulkanExample) delete(_vulkanExample); return _vulkanExampleBase	? 0 : -1;	}	\
	int	deleteVulkanExample(VulkanExampleBase** _vulkanExampleBase)	{ if(0 == _vulkanExampleBase) return -1; VulkanExample* _vulkanExample = (VulkanExample*)*_vulkanExampleBase; *_vulkanExampleBase = nullptr;				if(_vulkanExample) { delete(_vulkanExample); return 0; } return -1;					}
