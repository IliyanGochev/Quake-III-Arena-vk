#include "vk_common.h"
#include "vk_state.h"
#include "vk_shaders.h"

//----------------------------------------------------------------------------
// Circular buffer helpers
//----------------------------------------------------------------------------

void vkCircularBufferInit( vkCircularBuffer_t* cb, VkDeviceSize size )
{
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VK_CHECK( vmaCreateBuffer( g_vkAllocator, &bufferInfo, &allocInfo,
                                &cb->buffer, &cb->allocation, &cb->mappedData ) );
    cb->currentOffset = 0;
    cb->nextOffset = 0;
    cb->size = (unsigned)size;
}

void vkCircularBufferDestroy( vkCircularBuffer_t* cb )
{
    if ( cb->buffer )
    {
        vmaDestroyBuffer( g_vkAllocator, cb->buffer, cb->allocation );
        cb->buffer = VK_NULL_HANDLE;
        cb->allocation = nullptr;
        cb->mappedData = nullptr;
    }
    cb->currentOffset = 0;
    cb->nextOffset = 0;
    cb->size = 0;
}

void vkCircularBufferUpload( vkCircularBuffer_t* cb, const void* data, VkDeviceSize size )
{
    if ( size == 0 )
        return;

    if ( cb->nextOffset + (unsigned)size > cb->size )
    {
        // Buffer overflow: wait for the current frame's GPU work to complete before wrapping.
        // Uses the in-flight fence instead of vkDeviceWaitIdle to preserve frame overlap
        // on other queues and avoid spec §9 violations.
        vkWaitForFences( g_vkDevice, 1, &g_vkInFlightFences[g_vkCurrentFrame], VK_TRUE, UINT64_MAX );
        cb->currentOffset = 0;
        cb->nextOffset = (unsigned)size;
    }
    else
    {
        cb->currentOffset = cb->nextOffset;
        cb->nextOffset += (unsigned)size;
    }

    memcpy( (byte*)cb->mappedData + cb->currentOffset, data, (size_t)size );
}

//----------------------------------------------------------------------------
// Quad render data -- 2D quad for DrawImage
//----------------------------------------------------------------------------

// Quad vertices: 2 corners expanded to full quad in vertex shader
// Format: xy(st) -- matches D3D11 d3dQuadRenderVertex_t
struct vkQuadVertex_t {
    float x, y, s, t;
};

static const vkQuadVertex_t g_quadVertices[4] = {
    { 0, 0, 0, 0 },
    { 1, 0, 1, 0 },
    { 1, 1, 1, 1 },
    { 0, 1, 0, 1 }
};

static const uint16_t g_quadIndices[6] = { 0, 1, 2, 0, 2, 3 };

void InitQuadRenderData( vkQuadRenderData_t* qrd )
{
    // Vertex buffer
    VkDeviceSize vertexSize = sizeof( g_quadVertices );
    VkBufferCreateInfo vbufInfo = {};
    vbufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vbufInfo.size = vertexSize;
    vbufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vbufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vallocInfo = {};
    vallocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    vallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    void* vertexMapped = nullptr;
    VK_CHECK( vmaCreateBuffer( g_vkAllocator, &vbufInfo, &vallocInfo,
                                &qrd->vertexBuffer, &qrd->vertexAllocation,
                                &vertexMapped ) );
    memcpy( vertexMapped, g_quadVertices, vertexSize );

    // Index buffer
    VkDeviceSize indexSize = sizeof( g_quadIndices );
    VkBufferCreateInfo ibufInfo = {};
    ibufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ibufInfo.size = indexSize;
    ibufInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    ibufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo iallocInfo = {};
    iallocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    iallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    void* indexData = nullptr;
    VK_CHECK( vmaCreateBuffer( g_vkAllocator, &ibufInfo, &iallocInfo,
                                &qrd->indexBuffer, &qrd->indexAllocation, &indexData ) );
    memcpy( indexData, g_quadIndices, indexSize );

    // Uniform buffer (color)
    VkBufferCreateInfo ubufInfo = {};
    ubufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ubufInfo.size = sizeof( vkQuadUniformBuffer_t );
    ubufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    ubufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo uallocInfo = {};
    uallocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    uallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VK_CHECK( vmaCreateBuffer( g_vkAllocator, &ubufInfo, &uallocInfo,
                                &qrd->uniformBuffer, &qrd->uniformAllocation,
                                (void**)&qrd->uniformData ) );
    qrd->uniformData->color[0] = 1;
    qrd->uniformData->color[1] = 1;
    qrd->uniformData->color[2] = 1;
    qrd->uniformData->color[3] = 1;

    // Pipeline -- created with fsq (full-screen quad) shaders
    VkShaderModule vsModule = LoadShaderModule( "fsq_vs" );
    VkShaderModule psModule = LoadShaderModule( "fsq_ps" );

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0] = CreateVertexShaderStage( vsModule );
    stages[1] = CreateFragmentShaderStage( psModule );

    // Vertex input: binding 0 = { float x, y, s, t } = 16 bytes
    VkVertexInputBindingDescription viBindings[] = {
        { 0, sizeof(vkQuadVertex_t), VK_VERTEX_INPUT_RATE_VERTEX }
    };
    VkVertexInputAttributeDescription viAttrs[] = {
        { 0, 0, VK_FORMAT_R32G32_SFLOAT, 0 },   // location 0: position (xy)
        { 1, 0, VK_FORMAT_R32G32_SFLOAT, 8 }    // location 1: texCoord (st)
    };
    VkPipelineVertexInputStateCreateInfo vi = {};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = viBindings;
    vi.vertexAttributeDescriptionCount = 2;
    vi.pVertexAttributes = viAttrs;

    VkPipelineInputAssemblyStateCreateInfo ia = {};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport vpState = {};
    vpState.x = 0; vpState.y = 0; vpState.width = 1; vpState.height = 1;
    vpState.minDepth = 0; vpState.maxDepth = 1;
    VkRect2D scissorState = {};
    scissorState.offset = { 0, 0 };
    scissorState.extent = { 1, 1 };
    VkPipelineViewportStateCreateInfo vp = {};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.pViewports = &vpState;
    vp.scissorCount = 1;
    vp.pScissors = &scissorState;

    VkPipelineRasterizationStateCreateInfo rs = {};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo ms = {};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds = {};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_FALSE;
    ds.depthWriteEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState cb = {};
    cb.blendEnable = VK_TRUE;
    cb.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cb.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cb.colorBlendOp = VK_BLEND_OP_ADD;
    cb.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cb.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cb.alphaBlendOp = VK_BLEND_OP_ADD;
    cb.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cbState = {};
    cbState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbState.attachmentCount = 1;
    cbState.pAttachments = &cb;

    qrd->pipeline = VKDRV_CreatePipeline(
        g_vkPipelineLayout, g_vkRenderPass,
        stages, 2, &vi, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        &ia, &vp, &rs, &ms, &ds, &cbState );

    // Debug pipeline: single-position vertex input, wireframe, opaque
    // Used by DebugDrawAxis, DebugDrawTris, DebugDrawNormals, DebugDrawPolygon
    // to avoid binding mismatch (generic pipeline expects 3 bindings but
    // debug functions only upload position data).
    {
        VkShaderModule dbgVs = LoadShaderModule( "genericst_vs" );
        VkShaderModule dbgPs = LoadShaderModule( "genericst_ps" );

        VkPipelineShaderStageCreateInfo dbgStages[2] = {};
        dbgStages[0] = CreateVertexShaderStage( dbgVs );
        dbgStages[1] = CreateFragmentShaderStage( dbgPs );

        VkVertexInputBindingDescription dbgBindings[] = {
            { 0, sizeof(vec4_t), VK_VERTEX_INPUT_RATE_VERTEX }
        };
        VkVertexInputAttributeDescription dbgAttrs[] = {
            { 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0 }
        };
        VkPipelineVertexInputStateCreateInfo dbgVi = {};
        dbgVi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        dbgVi.vertexBindingDescriptionCount = 1;
        dbgVi.pVertexBindingDescriptions = dbgBindings;
        dbgVi.vertexAttributeDescriptionCount = 1;
        dbgVi.pVertexAttributes = dbgAttrs;

        VkPipelineRasterizationStateCreateInfo dbgRs = {};
        dbgRs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        dbgRs.polygonMode = VK_POLYGON_MODE_LINE;
        dbgRs.cullMode = VK_CULL_MODE_NONE;
        dbgRs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        VkPipelineColorBlendAttachmentState dbgCb = {};
        dbgCb.blendEnable = VK_FALSE;
        dbgCb.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo dbgCbState = {};
        dbgCbState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        dbgCbState.attachmentCount = 1;
        dbgCbState.pAttachments = &dbgCb;

        qrd->debugPipeline = VKDRV_CreatePipeline(
            g_vkPipelineLayout, g_vkRenderPass,
            dbgStages, 2, &dbgVi, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            &ia, &vp, &dbgRs, &ms, &ds, &dbgCbState );
    }

    // Shadow volume pipeline: stencil-based shadow volumes with depth bias.
    // Stencil: increment front, decrement back. Depth: LESS_OR_EQUAL, no write.
    // Polygon offset prevents shadow volume from z-fighting with the occluding geometry.
    {
        VkShaderModule shdVs = LoadShaderModule( "genericst_vs" );
        VkShaderModule shdPs = LoadShaderModule( "genericst_ps" );

        VkPipelineShaderStageCreateInfo shdStages[2] = {};
        shdStages[0] = CreateVertexShaderStage( shdVs );
        shdStages[1] = CreateFragmentShaderStage( shdPs );

        VkVertexInputBindingDescription shdBindings[] = {
            { 0, sizeof(vec4_t), VK_VERTEX_INPUT_RATE_VERTEX }
        };
        VkVertexInputAttributeDescription shdAttrs[] = {
            { 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0 }
        };
        VkPipelineVertexInputStateCreateInfo shdVi = {};
        shdVi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        shdVi.vertexBindingDescriptionCount = 1;
        shdVi.pVertexBindingDescriptions = shdBindings;
        shdVi.vertexAttributeDescriptionCount = 1;
        shdVi.pVertexAttributes = shdAttrs;

        VkPipelineRasterizationStateCreateInfo shdRs = {};
        shdRs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        shdRs.polygonMode = VK_POLYGON_MODE_FILL;
        shdRs.cullMode = VK_CULL_MODE_NONE;
        shdRs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        shdRs.depthBiasEnable = VK_TRUE;
        shdRs.depthBiasConstantFactor = 1.0f;
        shdRs.depthBiasClamp = 1.0f;
        shdRs.depthBiasSlopeFactor = 1.0f;

        VkPipelineDepthStencilStateCreateInfo shdDs = {};
        shdDs.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        shdDs.depthTestEnable = VK_TRUE;
        shdDs.depthWriteEnable = VK_FALSE;
        shdDs.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        shdDs.stencilTestEnable = VK_TRUE;
        shdDs.front.sFailOp = VK_STENCIL_OP_KEEP;
        shdDs.front.dpFailOp = VK_STENCIL_OP_KEEP;
        shdDs.front.passOp = VK_STENCIL_OP_INCREMENT_AND_WRAP;
        shdDs.front.compareOp = VK_COMPARE_OP_ALWAYS;
        shdDs.back.sFailOp = VK_STENCIL_OP_KEEP;
        shdDs.back.dpFailOp = VK_STENCIL_OP_KEEP;
        shdDs.back.passOp = VK_STENCIL_OP_DECREMENT_AND_WRAP;
        shdDs.back.compareOp = VK_COMPARE_OP_ALWAYS;
        shdDs.stencilCompareMask = 0xFF;
        shdDs.stencilWriteMask = 0xFF;
        shdDs.minDepthBounds = 0.0f;
        shdDs.maxDepthBounds = 1.0f;

        VkPipelineColorBlendAttachmentState shdCb = {};
        shdCb.blendEnable = VK_FALSE;
        shdCb.colorWriteMask = 0; // Shadow volumes only affect stencil, not color
        VkPipelineColorBlendStateCreateInfo shdCbState = {};
        shdCbState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        shdCbState.attachmentCount = 1;
        shdCbState.pAttachments = &shdCb;

        qrd->shadowPipeline = VKDRV_CreatePipeline(
            g_vkPipelineLayout, g_vkRenderPass,
            shdStages, 2, &shdVi, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            &ia, &vp, &shdRs, &ms, &shdDs, &shdCbState );
    }
}

void DestroyQuadRenderData( vkQuadRenderData_t* qrd )
{
    if ( qrd->pipeline )
    {
        vkDestroyPipeline( g_vkDevice, qrd->pipeline, nullptr );
        qrd->pipeline = VK_NULL_HANDLE;
    }
    if ( qrd->debugPipeline )
    {
        vkDestroyPipeline( g_vkDevice, qrd->debugPipeline, nullptr );
        qrd->debugPipeline = VK_NULL_HANDLE;
    }
    if ( qrd->shadowPipeline )
    {
        vkDestroyPipeline( g_vkDevice, qrd->shadowPipeline, nullptr );
        qrd->shadowPipeline = VK_NULL_HANDLE;
    }
    if ( qrd->vertexBuffer )
    {
        vmaDestroyBuffer( g_vkAllocator, qrd->vertexBuffer, qrd->vertexAllocation );
        qrd->vertexBuffer = VK_NULL_HANDLE;
        qrd->vertexAllocation = nullptr;
    }
    if ( qrd->indexBuffer )
    {
        vmaDestroyBuffer( g_vkAllocator, qrd->indexBuffer, qrd->indexAllocation );
        qrd->indexBuffer = VK_NULL_HANDLE;
        qrd->indexAllocation = nullptr;
    }
    if ( qrd->uniformBuffer )
    {
        vmaDestroyBuffer( g_vkAllocator, qrd->uniformBuffer, qrd->uniformAllocation );
        qrd->uniformBuffer = VK_NULL_HANDLE;
        qrd->uniformAllocation = nullptr;
        qrd->uniformData = nullptr;
    }
}

//----------------------------------------------------------------------------
// Skybox render data
//----------------------------------------------------------------------------

// Unit cube vertices for skybox: xyz + st
struct vkSkyboxVertex_t {
    float x, y, z, s, t;
};

// 6 faces, 2 triangles each = 12 triangles, 36 vertices
static const vkSkyboxVertex_t g_skyboxVertices[36] = {
    // Front face (+Z)
    { -1, -1,  1, 0, 0 }, {  1, -1,  1, 1, 0 }, {  1,  1,  1, 1, 1 },
    { -1, -1,  1, 0, 0 }, {  1,  1,  1, 1, 1 }, { -1,  1,  1, 0, 1 },
    // Back face (-Z)
    {  1, -1, -1, 0, 0 }, { -1, -1, -1, 1, 0 }, { -1,  1, -1, 1, 1 },
    {  1, -1, -1, 0, 0 }, { -1,  1, -1, 1, 1 }, {  1,  1, -1, 0, 1 },
    // Top face (+Y)
    { -1,  1,  1, 0, 0 }, {  1,  1,  1, 1, 0 }, {  1,  1, -1, 1, 1 },
    { -1,  1,  1, 0, 0 }, {  1,  1, -1, 1, 1 }, { -1,  1, -1, 0, 1 },
    // Bottom face (-Y)
    { -1, -1, -1, 0, 0 }, {  1, -1, -1, 1, 0 }, {  1, -1,  1, 1, 1 },
    { -1, -1, -1, 0, 0 }, {  1, -1,  1, 1, 1 }, { -1, -1,  1, 0, 1 },
    // Left face (-X)
    { -1, -1, -1, 0, 0 }, { -1, -1,  1, 1, 0 }, { -1,  1,  1, 1, 1 },
    { -1, -1, -1, 0, 0 }, { -1,  1,  1, 1, 1 }, { -1,  1, -1, 0, 1 },
    // Right face (+X)
    {  1, -1,  1, 0, 0 }, {  1, -1, -1, 1, 0 }, {  1,  1, -1, 1, 1 },
    {  1, -1,  1, 0, 0 }, {  1,  1, -1, 1, 1 }, {  1,  1,  1, 0, 1 },
};

void InitSkyBoxRenderData( vkSkyBoxRenderData_t* rd )
{
    // Vertex buffer (immutable)
    VkDeviceSize vertexSize = sizeof( g_skyboxVertices );
    VkBufferCreateInfo vbufInfo = {};
    vbufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vbufInfo.size = vertexSize;
    vbufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vbufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vallocInfo = {};
    vallocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    vallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    void* skyboxVertexMapped = nullptr;
    VK_CHECK( vmaCreateBuffer( g_vkAllocator, &vbufInfo, &vallocInfo,
                                &rd->vertexBuffer, &rd->vertexAllocation,
                                &skyboxVertexMapped ) );
    memcpy( skyboxVertexMapped, g_skyboxVertices, vertexSize );

    // VS uniform buffer
    VkBufferCreateInfo vsBufInfo = {};
    vsBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vsBufInfo.size = sizeof( vkSkyBoxVSUniformBuffer_t );
    vsBufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    vsBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vsAllocInfo = {};
    vsAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    vsAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VK_CHECK( vmaCreateBuffer( g_vkAllocator, &vsBufInfo, &vsAllocInfo,
                                &rd->vsUniformBuffer, &rd->vsUniformAllocation,
                                (void**)&rd->vsUniformData ) );

    // PS uniform buffer
    VkBufferCreateInfo psBufInfo = {};
    psBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    psBufInfo.size = sizeof( vkSkyBoxPSUniformBuffer_t );
    psBufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    psBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo psAllocInfo = {};
    psAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    psAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VK_CHECK( vmaCreateBuffer( g_vkAllocator, &psBufInfo, &psAllocInfo,
                                &rd->psUniformBuffer, &rd->psUniformAllocation,
                                (void**)&rd->psUniformData ) );

    // Pipeline
    VkShaderModule vsModule = LoadShaderModule( "skybox_vs" );
    VkShaderModule psModule = LoadShaderModule( "skybox_ps" );

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0] = CreateVertexShaderStage( vsModule );
    stages[1] = CreateFragmentShaderStage( psModule );

    // Vertex input: binding 0 = { float x, y, z, s, t } = 20 bytes
    VkVertexInputBindingDescription skyboxViBindings[] = {
        { 0, sizeof(vkSkyboxVertex_t), VK_VERTEX_INPUT_RATE_VERTEX }
    };
    VkVertexInputAttributeDescription skyboxViAttrs[] = {
        { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },   // location 0: position (xyz)
        { 1, 0, VK_FORMAT_R32G32_SFLOAT, 12 }      // location 1: texCoord (st)
    };
    VkPipelineVertexInputStateCreateInfo vi = {};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = skyboxViBindings;
    vi.vertexAttributeDescriptionCount = 2;
    vi.pVertexAttributes = skyboxViAttrs;

    VkPipelineInputAssemblyStateCreateInfo ia = {};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp = {};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs = {};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo ms = {};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds = {};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_FALSE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState skyboxCb = {};
    skyboxCb.blendEnable = VK_FALSE;
    skyboxCb.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cbState = {};
    cbState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbState.logicOpEnable = VK_FALSE;
    cbState.attachmentCount = 1;
    cbState.pAttachments = &skyboxCb;

    rd->pipeline = VKDRV_CreatePipeline(
        g_vkPipelineLayout, g_vkRenderPass,
        stages, 2, &vi, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        &ia, &vp, &rs, &ms, &ds, &cbState );

    // Do not destroy shader modules — cached by LoadShaderModule, cleaned up by DestroyShaders()
}

void DestroySkyBoxRenderData( vkSkyBoxRenderData_t* rd )
{
    if ( rd->pipeline )
    {
        vkDestroyPipeline( g_vkDevice, rd->pipeline, nullptr );
        rd->pipeline = VK_NULL_HANDLE;
    }
    if ( rd->vertexBuffer )
    {
        vmaDestroyBuffer( g_vkAllocator, rd->vertexBuffer, rd->vertexAllocation );
        rd->vertexBuffer = VK_NULL_HANDLE;
        rd->vertexAllocation = nullptr;
    }
    if ( rd->vsUniformBuffer )
    {
        vmaDestroyBuffer( g_vkAllocator, rd->vsUniformBuffer, rd->vsUniformAllocation );
        rd->vsUniformBuffer = VK_NULL_HANDLE;
        rd->vsUniformAllocation = nullptr;
        rd->vsUniformData = nullptr;
    }
    if ( rd->psUniformBuffer )
    {
        vmaDestroyBuffer( g_vkAllocator, rd->psUniformBuffer, rd->psUniformAllocation );
        rd->psUniformBuffer = VK_NULL_HANDLE;
        rd->psUniformAllocation = nullptr;
        rd->psUniformData = nullptr;
    }
}

//----------------------------------------------------------------------------
// Generic stage render data -- tessellation draw pipelines
//----------------------------------------------------------------------------

void InitGenericStageRenderData( vkGenericStageRenderData_t* rd )
{
    // Single-texture pipeline
    VkShaderModule vsST = LoadShaderModule( "genericst_vs" );
    VkShaderModule psST = LoadShaderModule( "genericst_ps" );

    VkPipelineShaderStageCreateInfo stagesST[2] = {};
    stagesST[0] = CreateVertexShaderStage( vsST );
    stagesST[1] = CreateFragmentShaderStage( psST );

    // Vertex input for single-texture:
    // Binding 0: vec4 positions (stride 16)
    // Binding 1: vec2 texcoords (stride 8)
    // Binding 2: vec4 colors as ubyte (stride 4)
    VkVertexInputBindingDescription stViBindings[] = {
        { 0, sizeof(vec4_t), VK_VERTEX_INPUT_RATE_VERTEX },
        { 1, sizeof(vec2_t), VK_VERTEX_INPUT_RATE_VERTEX },
        { 2, 4, VK_VERTEX_INPUT_RATE_VERTEX }
    };
    VkVertexInputAttributeDescription stViAttrs[] = {
        { 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0 },    // location 0: position
        { 1, 1, VK_FORMAT_R32G32_SFLOAT, 0 },          // location 1: texCoord
        { 2, 2, VK_FORMAT_R8G8B8A8_UNORM, 0 }          // location 2: color
    };
    VkPipelineVertexInputStateCreateInfo vi = {};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = 3;
    vi.pVertexBindingDescriptions = stViBindings;
    vi.vertexAttributeDescriptionCount = 3;
    vi.pVertexAttributes = stViAttrs;

    VkPipelineInputAssemblyStateCreateInfo ia = {};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp = {};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs = {};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_BACK;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo ms = {};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds = {};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState cb = {};
    cb.blendEnable = VK_TRUE;
    cb.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cb.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cb.colorBlendOp = VK_BLEND_OP_ADD;
    cb.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cb.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cb.alphaBlendOp = VK_BLEND_OP_ADD;
    cb.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cbState = {};
    cbState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbState.attachmentCount = 1;
    cbState.pAttachments = &cb;

    rd->pipelineST = VKDRV_CreatePipeline(
        g_vkPipelineLayout, g_vkRenderPass,
        stagesST, 2, &vi, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        &ia, &vp, &rs, &ms, &ds, &cbState );

    // Do not destroy shader modules — cached by LoadShaderModule, cleaned up by DestroyShaders()

    // Multi-texture pipeline
    VkShaderModule vsMT = LoadShaderModule( "genericmt_vs" );
    VkShaderModule psMT = LoadShaderModule( "genericmt_ps" );

    VkPipelineShaderStageCreateInfo stagesMT[2] = {};
    stagesMT[0] = CreateVertexShaderStage( vsMT );
    stagesMT[1] = CreateFragmentShaderStage( psMT );

    // Vertex input for multi-texture:
    // Binding 0: vec4 positions (stride 16)
    // Binding 1: vec2 texcoord0 (stride 8)
    // Binding 2: vec2 texcoord1 (stride 8)
    // Binding 3: vec4 colors as ubyte (stride 4)
    VkVertexInputBindingDescription mtViBindings[] = {
        { 0, sizeof(vec4_t), VK_VERTEX_INPUT_RATE_VERTEX },
        { 1, sizeof(vec2_t), VK_VERTEX_INPUT_RATE_VERTEX },
        { 2, sizeof(vec2_t), VK_VERTEX_INPUT_RATE_VERTEX },
        { 3, 4, VK_VERTEX_INPUT_RATE_VERTEX }
    };
    VkVertexInputAttributeDescription mtViAttrs[] = {
        { 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0 },    // location 0: position
        { 1, 1, VK_FORMAT_R32G32_SFLOAT, 0 },          // location 1: texCoord0
        { 2, 2, VK_FORMAT_R32G32_SFLOAT, 0 },          // location 2: texCoord1
        { 3, 3, VK_FORMAT_R8G8B8A8_UNORM, 0 }          // location 3: color
    };
    VkPipelineVertexInputStateCreateInfo viMT = {};
    viMT.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    viMT.vertexBindingDescriptionCount = 4;
    viMT.pVertexBindingDescriptions = mtViBindings;
    viMT.vertexAttributeDescriptionCount = 4;
    viMT.pVertexAttributes = mtViAttrs;

    rd->pipelineMT = VKDRV_CreatePipeline(
        g_vkPipelineLayout, g_vkRenderPass,
        stagesMT, 2, &viMT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        &ia, &vp, &rs, &ms, &ds, &cbState );

    // Do not destroy shader modules — cached by LoadShaderModule, cleaned up by DestroyShaders()
}

void DestroyGenericStageRenderData( vkGenericStageRenderData_t* rd )
{
    if ( rd->pipelineST )
    {
        vkDestroyPipeline( g_vkDevice, rd->pipelineST, nullptr );
        rd->pipelineST = VK_NULL_HANDLE;
    }
    if ( rd->pipelineMT )
    {
        vkDestroyPipeline( g_vkDevice, rd->pipelineMT, nullptr );
        rd->pipelineMT = VK_NULL_HANDLE;
    }

    // Destroy all lazily-created pipeline variants
    for ( int i = 0; i < VK_DEPTHSTATE_COUNT * VK_RASTERIZER_COUNT * VK_BLENDSTATE_COUNT * VK_ALPHATEST_COUNT; i++ )
    {
        if ( rd->pipelineCache[i] )
        {
            vkDestroyPipeline( g_vkDevice, rd->pipelineCache[i], nullptr );
            rd->pipelineCache[i] = VK_NULL_HANDLE;
        }
    }
}

//----------------------------------------------------------------------------
// View render data -- per-frame uniform buffers
//----------------------------------------------------------------------------

void InitViewRenderData( vkViewRenderData_t* vrd )
{
    for ( int i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; i++ )
    {
        // VS uniform buffer
        VkBufferCreateInfo vsBufInfo = {};
        vsBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        vsBufInfo.size = sizeof( vkViewVSUniformBuffer_t );
        vsBufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        vsBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo vsAllocInfo = {};
        vsAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        vsAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VK_CHECK( vmaCreateBuffer( g_vkAllocator, &vsBufInfo, &vsAllocInfo,
                                    &vrd->vsUniformBuffer[i], &vrd->vsUniformAllocation[i],
                                    (void**)&vrd->vsUniformData[i] ) );

        // PS uniform buffer
        VkBufferCreateInfo psBufInfo = {};
        psBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        psBufInfo.size = sizeof( vkViewPSUniformBuffer_t );
        psBufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        psBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo psAllocInfo = {};
        psAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        psAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VK_CHECK( vmaCreateBuffer( g_vkAllocator, &psBufInfo, &psAllocInfo,
                                    &vrd->psUniformBuffer[i], &vrd->psUniformAllocation[i],
                                    (void**)&vrd->psUniformData[i] ) );
    }
}

void DestroyViewRenderData( vkViewRenderData_t* vrd )
{
    for ( int i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; i++ )
    {
        if ( vrd->vsUniformBuffer[i] )
        {
            vmaDestroyBuffer( g_vkAllocator, vrd->vsUniformBuffer[i], vrd->vsUniformAllocation[i] );
            vrd->vsUniformBuffer[i] = VK_NULL_HANDLE;
            vrd->vsUniformAllocation[i] = nullptr;
            vrd->vsUniformData[i] = nullptr;
        }
        if ( vrd->psUniformBuffer[i] )
        {
            vmaDestroyBuffer( g_vkAllocator, vrd->psUniformBuffer[i], vrd->psUniformAllocation[i] );
            vrd->psUniformBuffer[i] = VK_NULL_HANDLE;
            vrd->psUniformAllocation[i] = nullptr;
            vrd->psUniformData[i] = nullptr;
        }
    }
}

//----------------------------------------------------------------------------
// Tessellation buffers
//----------------------------------------------------------------------------

void InitTessBuffers( vkTessBuffers_t* tess )
{
    vkCircularBufferInit( &tess->indexes, VK_TESS_INDEX_BUFFER_SIZE );
    vkCircularBufferInit( &tess->xyz, VK_TESS_VERTEX_BUFFER_SIZE );

    for ( int s = 0; s < MAX_SHADER_STAGES; s++ )
    {
        for ( int b = 0; b < NUM_TEXTURE_BUNDLES; b++ )
            vkCircularBufferInit( &tess->stages[s].texCoords[b], VK_TESS_STAGE_BUFFER_SIZE );
        vkCircularBufferInit( &tess->stages[s].colors, VK_TESS_STAGE_BUFFER_SIZE );
    }

    for ( int d = 0; d < MAX_DLIGHTS; d++ )
    {
        vkCircularBufferInit( &tess->dlights[d].indexes, VK_TESS_INDEX_BUFFER_SIZE / MAX_DLIGHTS );
        vkCircularBufferInit( &tess->dlights[d].texCoords, VK_TESS_STAGE_BUFFER_SIZE / MAX_DLIGHTS );
        vkCircularBufferInit( &tess->dlights[d].colors, VK_TESS_STAGE_BUFFER_SIZE / MAX_DLIGHTS );
    }

    vkCircularBufferInit( &tess->fog.texCoords, VK_TESS_STAGE_BUFFER_SIZE );
    vkCircularBufferInit( &tess->fog.colors, VK_TESS_STAGE_BUFFER_SIZE );
}

void DestroyTessBuffers( vkTessBuffers_t* tess )
{
    vkCircularBufferDestroy( &tess->indexes );
    vkCircularBufferDestroy( &tess->xyz );

    for ( int s = 0; s < MAX_SHADER_STAGES; s++ )
    {
        for ( int b = 0; b < NUM_TEXTURE_BUNDLES; b++ )
            vkCircularBufferDestroy( &tess->stages[s].texCoords[b] );
        vkCircularBufferDestroy( &tess->stages[s].colors );
    }

    for ( int d = 0; d < MAX_DLIGHTS; d++ )
    {
        vkCircularBufferDestroy( &tess->dlights[d].indexes );
        vkCircularBufferDestroy( &tess->dlights[d].texCoords );
        vkCircularBufferDestroy( &tess->dlights[d].colors );
    }

    vkCircularBufferDestroy( &tess->fog.texCoords );
    vkCircularBufferDestroy( &tess->fog.colors );
}
