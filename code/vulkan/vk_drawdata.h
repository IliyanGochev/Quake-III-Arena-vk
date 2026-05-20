#pragma once

// @pjb: draw data buffer initialization and teardown

void InitQuadRenderData( vkQuadRenderData_t* qrd );
void DestroyQuadRenderData( vkQuadRenderData_t* qrd );

void InitSkyBoxRenderData( vkSkyBoxRenderData_t* rd );
void DestroySkyBoxRenderData( vkSkyBoxRenderData_t* rd );

void InitGenericStageRenderData( vkGenericStageRenderData_t* rd );
void DestroyGenericStageRenderData( vkGenericStageRenderData_t* rd );

void InitViewRenderData( vkViewRenderData_t* vrd );
void DestroyViewRenderData( vkViewRenderData_t* vrd );

void InitTessBuffers( vkTessBuffers_t* tess );
void DestroyTessBuffers( vkTessBuffers_t* tess );

// Circular buffer helpers
void vkCircularBufferInit( vkCircularBuffer_t* cb, VkDeviceSize size );
void vkCircularBufferDestroy( vkCircularBuffer_t* cb );
void vkCircularBufferUpload( vkCircularBuffer_t* cb, const void* data, VkDeviceSize size );
