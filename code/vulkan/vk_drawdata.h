#pragma once

#include "vk_common.h"

//----------------------------------------------------------------------------
// Static geometry data
//----------------------------------------------------------------------------

// Quad vertex (for 2D rendering)
struct vkQuadVertex_t {
    float position[2];
    float texcoord[2];
};

// Skybox vertex
struct vkSkyboxVertex_t {
    float position[3];
    float texcoord[2];
};

//----------------------------------------------------------------------------
// Static render data
//----------------------------------------------------------------------------
struct vkQuadRenderData_t {
    VkBuffer vertexBuffer;
    VkBuffer indexBuffer;
    VmaAllocation vertexAllocation;
    VmaAllocation indexAllocation;
    uint32_t indexCount;
};

struct vkSkyboxRenderData_t {
    VkBuffer vertexBuffer;
    VmaAllocation vertexAllocation;
    uint32_t vertexCount;
};

//----------------------------------------------------------------------------
// Render data collection
//----------------------------------------------------------------------------
struct vkRenderData_t {
    vkQuadRenderData_t quad;
    vkSkyboxRenderData_t skybox;
};

//----------------------------------------------------------------------------
// Global render data
//----------------------------------------------------------------------------
extern vkRenderData_t g_vkRenderData;

//----------------------------------------------------------------------------
// Initialization functions
//----------------------------------------------------------------------------

// Initialize all draw data
void VK_InitDrawData();

// Destroy all draw data
void VK_DestroyDrawData();

// Create quad geometry (for 2D rendering)
void VK_CreateQuadGeometry();

// Create skybox geometry
void VK_CreateSkyboxGeometry();

// Destroy quad geometry
void VK_DestroyQuadGeometry();

// Destroy skybox geometry
void VK_DestroySkyboxGeometry();

