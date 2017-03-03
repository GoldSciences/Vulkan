#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace vks
{
	// Vertex layout used in the examples
	// These must fit input locations of the vertex shader used to render the model
	struct VertexPC				{	glm::vec3 position		;	glm::vec3 color		;																													};
	struct VertexPU				{	glm::vec3 position		;	glm::vec2 uv		;																													};
	struct VertexPNC			{	glm::vec3 position		;	glm::vec3 normal	;	glm::vec3 color			;																						};
	struct VertexPUN			{	glm::vec3 position		;	glm::vec2 uv		;	glm::vec3 normal		; 																						};
	struct VertexPNU			{	glm::vec3 position		;	glm::vec3 normal	;	glm::vec2 uv			;																						};
	struct VertexPNUC			{	glm::vec3 position		;	glm::vec3 normal	;	glm::vec2 uv			;	glm::vec3 color		;																};
	struct VertexPUCN			{	glm::vec3 position		;	glm::vec2 uv		;	glm::vec3 color			;	glm::vec3 normal	; 																};
	struct VertexPUCNT			{	glm::vec3 position		;	glm::vec2 uv		;	glm::vec3 color			;	glm::vec3 normal	;	glm::vec3	tangent			; 								};
	struct VertexPNUCWI			{	glm::vec3 position		;	glm::vec3 normal	;	glm::vec2 uv			;	glm::vec3 color		;	float		boneWeights	[4]	;	uint32_t boneIDs	[4]	;	};
	
	struct Uniform_Proj_Model								{	glm::mat4 projection	;	glm::mat4 model			;																					};
	struct Uniform_Proj_View								{	glm::mat4 projection	;	glm::mat4 view			;																					};
	struct Uniform_Proj_ModelView							{	glm::mat4 projection	;	glm::mat4 modelView		;																					};
	struct Uniform_Proj_ModelView_LightPos					{	glm::mat4 projection	;	glm::mat4 modelView		;	glm::vec3 lightPos	;															};
	struct Uniform_Proj_Model_View							{	glm::mat4 projection	;	glm::mat4 model			;	glm::mat4 view		;															};
	struct Uniform_Proj_View_Model							{	glm::mat4 projection	;	glm::mat4 view			;	glm::mat4 model		;															};
	struct Uniform_Proj_Model_LightPos						{	glm::mat4 projection	;	glm::mat4 model			;	glm::vec3 lightPos	;															};
	struct Uniform_Proj_Model_View_CamPos					{	glm::mat4 projection	;	glm::mat4 model			;	glm::mat4 view		;	glm::vec3 camPos	;									};
	struct Uniform_Proj_Model_Normal_View_LightPos			{	glm::mat4 projection	;	glm::mat4 model			;	glm::mat4 normal	;	glm::mat4 view		;	glm::vec3 lightPos	;			};
	struct Uniform_Proj_Model_Normal_LightPos_CamPos		{	glm::mat4 projection	;	glm::mat4 model			;	glm::mat4 normal	;	glm::vec3 lightPos	;	glm::vec3 cameraPos	;			};

	struct VertexInputStateAndDescriptions	{
		VkPipelineVertexInputStateCreateInfo						inputState								= {};
		std::vector<VkVertexInputBindingDescription>				bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription>				attributeDescriptions;
	};

	struct Pipelines {
		// Pipelines (often called "pipeline state objects") are used to bake all states that affect a pipeline
		// While in OpenGL every state can be changed at (almost) any time, Vulkan requires to layout the graphics (and compute) pipeline states upfront
		// So for each combination of non-dynamic pipeline states you need a new pipeline (there are a few exceptions to this not discussed here)
		// Even though this adds a new dimension of planing ahead, it's a great opportunity for performance optimizations by the driver
		std::vector<VkPipeline>										Instances;

		// The pipeline layout is used by a pipline to access the descriptor sets 
		// It defines interface (without binding any actual data) between the shader stages used by the pipeline and the shader resources
		// A pipeline layout can be shared among multiple pipelines as long as their interfaces match
		VkPipelineLayout											Layout									= VK_NULL_HANDLE;

		void														Destroy									(VkDevice device)		{

			for(uint32_t iPipeline=0, pipelineCount = (uint32_t)Instances.size(); iPipeline < pipelineCount; ++iPipeline)	{
				VkPipeline&														currentPipeline							= Instances[iPipeline];
				if( currentPipeline ) { 
					vkDestroyPipeline(device, currentPipeline, nullptr); 
					currentPipeline												= VK_NULL_HANDLE;
				}
			}

			if(Layout) {
				vkDestroyPipelineLayout(device, Layout, nullptr);
				Layout														= VK_NULL_HANDLE;
			}
		}
	};

	struct DescriptorSets {

		// The descriptor set stores the resources bound to the binding points in a shader. 
		// It connects the binding points of the different shaders with the buffers and images used for those bindings
		std::vector<VkDescriptorSet>								Instances;

		// The descriptor set layout describes the shader binding layout (without actually referencing descriptor)
		// Like the pipeline layout it's pretty much a blueprint and can be used with different descriptor sets as long as their layout matches
		VkDescriptorSetLayout										Layout									= VK_NULL_HANDLE;

		void														Destroy									(VkDevice device)		{
			if(Layout) {
				vkDestroyDescriptorSetLayout(device, Layout, nullptr);
				Layout														= VK_NULL_HANDLE;
			}
		}
	};

	struct Pipeline { 
		VkPipeline													Instance								= VK_NULL_HANDLE; 
		VkPipelineLayout											Layout									= VK_NULL_HANDLE;

		inline void													Destroy									(VkDevice device) 																	{
			if(VK_NULL_HANDLE != Instance	) vkDestroyPipeline			(device, Instance	, nullptr); 
			if(VK_NULL_HANDLE != Layout		) vkDestroyPipelineLayout	(device, Layout		, nullptr); 
		}
	};

	struct DescriptorSet { 
		VkDescriptorSet												Instance								= VK_NULL_HANDLE; 
		VkDescriptorSetLayout										Layout									= VK_NULL_HANDLE;	

		inline void													Destroy									(VkDevice device) 																	{ if(VK_NULL_HANDLE != Layout) vkDestroyDescriptorSetLayout(device, Layout, nullptr); }
	};
}