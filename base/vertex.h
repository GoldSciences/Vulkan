#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

// Vertex layout used in the examples
// These must fit input locations of the vertex shader used to render the model
struct VertexPC		{	glm::vec3 position;	glm::vec3 color		;																												};
struct VertexPU		{	glm::vec3 position;	glm::vec2 uv		;																												};
struct VertexPNC	{	glm::vec3 position;	glm::vec3 normal	;	glm::vec3 color		;																						};
struct VertexPUN	{	glm::vec3 position;	glm::vec2 uv		;	glm::vec3 normal	; 																						};
struct VertexPNU	{	glm::vec3 position;	glm::vec3 normal	;	glm::vec2 uv		;																						};
struct VertexPNUC	{	glm::vec3 position;	glm::vec3 normal	;	glm::vec2 uv		;	glm::vec3 color		;																};
struct VertexPUCN	{	glm::vec3 position;	glm::vec2 uv		;	glm::vec3 color		;	glm::vec3 normal	; 																};
struct VertexPUCNT	{	glm::vec3 position;	glm::vec2 uv		;	glm::vec3 color		;	glm::vec3 normal	;	glm::vec3	tangent			; 								};
struct VertexPNUCWI	{	glm::vec3 position;	glm::vec3 normal	;	glm::vec2 uv		;	glm::vec3 color		;	float		boneWeights	[4]	;	uint32_t boneIDs	[4]	;	};
