#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

struct VertexPC		{	glm::vec3 pos;	glm::vec3 color		;																												};
struct VertexPU		{	glm::vec3 pos;	glm::vec2 uv		;																												};
struct VertexPNC	{	glm::vec3 pos;	glm::vec3 normal	;	glm::vec3 color		;																						};
struct VertexPUN	{	glm::vec3 pos;	glm::vec2 uv		;	glm::vec3 normal	; 																						};
struct VertexPNU	{	glm::vec3 pos;	glm::vec3 normal	;	glm::vec2 uv		;																						};
struct VertexPNUC	{	glm::vec3 pos;	glm::vec3 normal	;	glm::vec2 uv		;	glm::vec3 color		;																};
struct VertexPUCN	{	glm::vec3 pos;	glm::vec2 uv		;	glm::vec3 color		;	glm::vec3 normal	; 																};
struct VertexPUCNT	{	glm::vec3 pos;	glm::vec2 uv		;	glm::vec3 color		;	glm::vec3 normal	;	glm::vec3	tangent			; 								};
struct VertexPNUCWI	{	glm::vec3 pos;	glm::vec3 normal	;	glm::vec2 uv		;	glm::vec3 color		;	float		boneWeights	[4]	;	uint32_t boneIDs	[4]	;	};
