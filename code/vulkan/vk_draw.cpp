#include "vk_common.h"
#include "vk_state.h"
#include "vk_driver.h"
#include "vk_state.h"
#include "vk_image.h"
#include "vk_drawdata.h"

//----------------------------------------------------------------------------
// Tessellation buffer upload helpers
//----------------------------------------------------------------------------

static void UploadTessBuffers( const shaderCommands_t* input, qboolean needDlights, qboolean needFog )
{
    vkCircularBufferUpload( &g_vkDrawState.tessBufs.indexes, input->indexes, sizeof(glIndex_t) * input->numIndexes );
    vkCircularBufferUpload( &g_vkDrawState.tessBufs.xyz, input->xyz, sizeof(vec4_t) * input->numVertexes );

    for ( int stage = 0; stage < MAX_SHADER_STAGES; stage++ )
    {
        const shaderStage_t* xstage = input->xstages[stage];
        const stageVars_t* cpuStage = &input->svars[stage];

        if ( !xstage || !cpuStage )
            break;

        vkTessStageBuffers_t* gpuStage = &g_vkDrawState.tessBufs.stages[stage];
        vkCircularBufferUpload( &gpuStage->colors, cpuStage->colors, sizeof(color4ub_t) * input->numVertexes );
        vkCircularBufferUpload( &gpuStage->texCoords[0], cpuStage->texcoords[0], sizeof(vec2_t) * input->numVertexes );

        if ( xstage->bundle[1].image[0] != 0 )
        {
            vkCircularBufferUpload( &gpuStage->texCoords[1], cpuStage->texcoords[1], sizeof(vec2_t) * input->numVertexes );
        }
    }

    if ( needDlights )
    {
        for ( int l = 0; l < input->dlightCount; l++ )
        {
            const dlightProjectionInfo_t* cpuLight = &input->dlightInfo[l];
            vkTessLightProjBuffers_t* gpuLight = &g_vkDrawState.tessBufs.dlights[l];

            if ( !cpuLight->numIndexes )
                continue;

            vkCircularBufferUpload( &gpuLight->indexes, cpuLight->hitIndexes, sizeof(glIndex_t) * cpuLight->numIndexes );
            vkCircularBufferUpload( &gpuLight->colors, cpuLight->colorArray, sizeof(byte) * 4 * input->numVertexes );
            vkCircularBufferUpload( &gpuLight->texCoords, cpuLight->texCoordsArray, sizeof(float) * 2 * input->numVertexes );
        }
    }

    if ( needFog )
    {
        vkCircularBufferUpload( &g_vkDrawState.tessBufs.fog.colors, input->fogVars.colors, sizeof(color4ub_t) * input->numVertexes );
        vkCircularBufferUpload( &g_vkDrawState.tessBufs.fog.texCoords, input->fogVars.texcoords, sizeof(vec2_t) * input->numVertexes );
    }
}

//----------------------------------------------------------------------------
// Image animation helper
//----------------------------------------------------------------------------

static const vkImage_t* GetAnimatedImage( textureBundle_t* bundle, float shaderTime )
{
    int index;

    if ( bundle->isVideoMap )
    {
        ri.CIN_RunCinematic( bundle->videoMapHandle );
        ri.CIN_UploadCinematic( bundle->videoMapHandle );
        return GetImageRenderInfo( tr.scratchImage[bundle->videoMapHandle] );
    }

    if ( bundle->numImageAnimations <= 1 )
        return GetImageRenderInfo( bundle->image[0] );

    index = myftol( shaderTime * bundle->imageAnimationSpeed * FUNCTABLE_SIZE );
    index >>= FUNCTABLE_SIZE2;

    if ( index < 0 )
        index = 0;
    index %= bundle->numImageAnimations;

    return GetImageRenderInfo( bundle->image[index] );
}

//----------------------------------------------------------------------------
// DrawQuad -- for 2D image rendering
//----------------------------------------------------------------------------

static void DrawQuad(
    const vkQuadRenderData_t* qrd,
    const vkImage_t* image,
    const float* coords,
    const float* texcoords,
    const float* color )
{
    UpdateViewState();
    UpdateMaterialState();

    // Update color uniform
    if ( qrd->uniformData )
    {
        if ( color )
        {
            qrd->uniformData->color[0] = color[0];
            qrd->uniformData->color[1] = color[1];
            qrd->uniformData->color[2] = color[2];
            qrd->uniformData->color[3] = color[3];
        }
        else
        {
            qrd->uniformData->color[0] = 1.0f;
            qrd->uniformData->color[1] = 1.0f;
            qrd->uniformData->color[2] = 1.0f;
            qrd->uniformData->color[3] = 1.0f;
        }
    }

    VkCommandBuffer cmd = g_vkCommandBuffers[g_vkCurrentFrame];

    // Apply viewport/scissor from coords (x, y, x+w, y+h) so the quad renders at the
    // requested screen region instead of always fullscreen.
    if ( coords )
    {
        float width = coords[2] - coords[0];
        float height = coords[3] - coords[1];

        VkViewport vp = {};
        vp.x = coords[0];
        vp.y = coords[1];
        vp.width = width;
        vp.height = -height;  // Negative height flips Y axis for Vulkan (NDC +1 -> top)
        vp.minDepth = 0.0f;
        vp.maxDepth = 1.0f;
        vkCmdSetViewport( cmd, 0, 1, &vp );

        VkRect2D scissor = {};
        scissor.offset = { (int32_t)coords[0], (int32_t)coords[1] };
        scissor.extent = { (uint32_t)width, (uint32_t)height };
        vkCmdSetScissor( cmd, 0, 1, &scissor );
    }

    // Bind pipeline
    vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, qrd->pipeline );

    // Bind vertex buffers
    VkBuffer vertexBuffers[] = { qrd->vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers( cmd, 0, 1, vertexBuffers, offsets );

    // Bind index buffer
    vkCmdBindIndexBuffer( cmd, qrd->indexBuffer, 0, VK_INDEX_TYPE_UINT16 );

    // Bind descriptor set
    vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vkPipelineLayout, 0, 1, &g_vkDescriptorSets[g_vkCurrentFrame], 0, nullptr );

    // Update texture descriptor for the quad
    if ( image )
    {
        VKDRV_UpdateTextureDescriptors( image, nullptr );
    }

    // Update binding 1 to quad color buffer (initialized to skybox VS eye buffer)
    // The fsq_ps shader reads color from register b1 / binding 1
    {
        VkDescriptorBufferInfo colorBufInfo = {};
        colorBufInfo.buffer = qrd->uniformBuffer;
        colorBufInfo.offset = 0;
        colorBufInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = g_vkDescriptorSets[g_vkCurrentFrame];
        write.dstBinding = VK_BIND_SKYBOX_VSEYE;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &colorBufInfo;

        vkUpdateDescriptorSets( g_vkDevice, 1, &write, 0, nullptr );
    }

    // Draw
    vkCmdDrawIndexed( cmd, 6, 1, 0, 0, 0 );
}

//----------------------------------------------------------------------------
// DrawSkyBox
//----------------------------------------------------------------------------

static void DrawSkyBox(
    const vkSkyBoxRenderData_t* sbrd,
    const skyboxDrawInfo_t* skybox,
    const float* eye_origin,
    const float* colorTint )
{
    VKDrv_SetState( 0 );
    VKDRV_CommitRasterizerState( CT_TWO_SIDED, qfalse, qfalse );

    UpdateViewState();
    UpdateMaterialState();

    // Update skybox VS uniform (eye position)
    if ( sbrd->vsUniformData )
    {
        Com_Memcpy( sbrd->vsUniformData->eyePos, eye_origin, sizeof(float) * 3 );
        sbrd->vsUniformData->eyePos[3] = 0;
    }

    // Update skybox PS uniform (color tint)
    if ( sbrd->psUniformData )
    {
        if ( colorTint )
        {
            Com_Memcpy( sbrd->psUniformData->color, colorTint, sizeof(float) * 3 );
            sbrd->psUniformData->color[3] = 1;
        }
        else
        {
            sbrd->psUniformData->color[0] = 1;
            sbrd->psUniformData->color[1] = 1;
            sbrd->psUniformData->color[2] = 1;
            sbrd->psUniformData->color[3] = 1;
        }
    }

    // Update descriptor binding 2 to point to skybox PS uniform buffer
    // (it was initialized with quad color buffer for 2D rendering)
    {
        VkDescriptorBufferInfo skyboxPsBufInfo = {};
        skyboxPsBufInfo.buffer = sbrd->psUniformBuffer;
        skyboxPsBufInfo.offset = 0;
        skyboxPsBufInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = g_vkDescriptorSets[g_vkCurrentFrame];
        write.dstBinding = VK_BIND_SKYBOX_PSCOLOR;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &skyboxPsBufInfo;

        vkUpdateDescriptorSets( g_vkDevice, 1, &write, 0, nullptr );
    }

    VkCommandBuffer cmd = g_vkCommandBuffers[g_vkCurrentFrame];

    vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sbrd->pipeline );

    VkBuffer vertexBuffers[] = { sbrd->vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers( cmd, 0, 1, vertexBuffers, offsets );

    // Draw each skybox face
    for ( int i = 0; i < 6; i++ )
    {
        const skyboxSideDrawInfo_t* side = &skybox->sides[i];
        if ( !side->image )
            continue;

        const vkImage_t* image = GetImageRenderInfo( side->image );
        if ( !image )
            continue;

        // Update texture descriptor for this skybox face
        VKDRV_UpdateTextureDescriptors( image, nullptr );

        // Bind descriptor set for skybox rendering
        vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vkPipelineLayout, 0, 1, &g_vkDescriptorSets[g_vkCurrentFrame], 0, nullptr );

        vkCmdDraw( cmd, 6, 1, i * 6, 0 );
    }
}

//----------------------------------------------------------------------------
// TessDrawTextured -- single texture stage draw
//----------------------------------------------------------------------------

static void TessDrawTextured( const shaderCommands_t* input, int stage, VkPipeline overridePipeline )
{
    const vkTessBuffers_t* buffers = &g_vkDrawState.tessBufs;
    shaderStage_t* pStage = input->xstages[stage];

    const vkImage_t* tex = nullptr;
    if ( pStage->bundle[0].vertexLightmap &&
         ( (r_vertexLight->integer && !r_uiFullScreen->integer) || vdConfig.hardwareType == GLHW_PERMEDIA2 ) &&
         r_lightmap->integer )
    {
        tex = GetImageRenderInfo( tr.whiteImage );
    }
    else
    {
        tex = GetAnimatedImage( &pStage->bundle[0], input->shaderTime );
    }
    ASSERT( tex );

    VkCommandBuffer cmd = g_vkCommandBuffers[g_vkCurrentFrame];

    VkPipeline pipe = ( overridePipeline != VK_NULL_HANDLE )
        ? overridePipeline
        : VKDRV_SelectPipeline( g_vkRunState.depthStateMask,
                                g_vkRunState.cullMode,
                                g_vkRunState.wireframe,
                                g_vkRunState.blendState );
    vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe );

    // Bind all vertex buffers including position at slot 0 — makes the draw
    // self-contained instead of relying on the caller's implicit binding.
    VkBuffer vbufs[3] = {
        buffers->xyz.buffer,
        buffers->stages[stage].texCoords[0].buffer,
        buffers->stages[stage].colors.buffer
    };
    VkDeviceSize voffsets[3] = {
        buffers->xyz.currentOffset,
        buffers->stages[stage].texCoords[0].currentOffset,
        buffers->stages[stage].colors.currentOffset
    };
    vkCmdBindVertexBuffers( cmd, 0, 3, vbufs, voffsets );

    // Bind descriptor set
    vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vkPipelineLayout, 0, 1, &g_vkDescriptorSets[g_vkCurrentFrame], 0, nullptr );

    // Update texture descriptor for this draw
    VKDRV_UpdateTextureDescriptors( tex, nullptr );

    vkCmdDrawIndexed( cmd, input->numIndexes, 1, 0, 0, 0 );
}

//----------------------------------------------------------------------------
// TessDrawMultitextured -- two texture stage draw
//----------------------------------------------------------------------------

static void TessDrawMultitextured( const shaderCommands_t* input, int stage, VkPipeline overridePipeline )
{
    const vkTessBuffers_t* buffers = &g_vkDrawState.tessBufs;
    shaderStage_t* pStage = input->xstages[stage];

    const vkImage_t* tex0 = GetAnimatedImage( &pStage->bundle[0], input->shaderTime );
    const vkImage_t* tex1 = GetAnimatedImage( &pStage->bundle[1], input->shaderTime );
    ASSERT( tex0 && tex1 );

    VkCommandBuffer cmd = g_vkCommandBuffers[g_vkCurrentFrame];

    VkPipeline pipe = ( overridePipeline != VK_NULL_HANDLE )
        ? overridePipeline
        : g_vkDrawState.genericStage.pipelineMT;
    vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe );

    // Bind all vertex buffers including position at slot 0
    VkBuffer vbufs[4] = {
        buffers->xyz.buffer,
        buffers->stages[stage].texCoords[0].buffer,
        buffers->stages[stage].texCoords[1].buffer,
        buffers->stages[stage].colors.buffer
    };
    VkDeviceSize voffsets[4] = {
        buffers->xyz.currentOffset,
        buffers->stages[stage].texCoords[0].currentOffset,
        buffers->stages[stage].texCoords[1].currentOffset,
        buffers->stages[stage].colors.currentOffset
    };
    vkCmdBindVertexBuffers( cmd, 0, 4, vbufs, voffsets );

    // Bind descriptor set
    vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vkPipelineLayout, 0, 1, &g_vkDescriptorSets[g_vkCurrentFrame], 0, nullptr );

    // Update texture descriptors for multi-texture draw
    VKDRV_UpdateTextureDescriptors( tex0, tex1 );

    vkCmdDrawIndexed( cmd, input->numIndexes, 1, 0, 0, 0 );
}

//----------------------------------------------------------------------------
// TessProjectDynamicLights
//----------------------------------------------------------------------------

static void TessProjectDynamicLights( const shaderCommands_t* input )
{
    const vkTessBuffers_t* buffers = &g_vkDrawState.tessBufs;
    const vkImage_t* tex = GetImageRenderInfo( tr.dlightImage );
    ASSERT( tex );

    VkCommandBuffer cmd = g_vkCommandBuffers[g_vkCurrentFrame];

    VkPipeline pipeDL = VKDRV_SelectPipeline( g_vkRunState.depthStateMask,
                                               g_vkRunState.cullMode,
                                               g_vkRunState.wireframe,
                                               g_vkRunState.blendState );
    vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeDL );

    for ( int l = 0; l < input->dlightCount; l++ )
    {
        const dlightProjectionInfo_t* dlInfo = &input->dlightInfo[l];
        if ( !dlInfo->numIndexes )
            continue;

        const vkTessLightProjBuffers_t* projBuf = &buffers->dlights[l];

        // Bind all vertex buffers including position at slot 0
        VkBuffer vbufs[3] = {
            buffers->xyz.buffer, projBuf->texCoords.buffer, projBuf->colors.buffer
        };
        VkDeviceSize voffsets[3] = {
            buffers->xyz.currentOffset, projBuf->texCoords.currentOffset, projBuf->colors.currentOffset
        };
        vkCmdBindVertexBuffers( cmd, 0, 3, vbufs, voffsets );

        vkCmdBindIndexBuffer( cmd, projBuf->indexes.buffer,
                              projBuf->indexes.currentOffset, VK_INDEX_TYPE_UINT16 );

        // Update texture descriptor for dlight projection
        VKDRV_UpdateTextureDescriptors( tex, nullptr );

        // Select blend mode
        if ( dlInfo->additive )
        {
            VKDrv_SetState( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
        }
        else
        {
            VKDrv_SetState( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
        }
        UpdateMaterialState();

        vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vkPipelineLayout, 0, 1, &g_vkDescriptorSets[g_vkCurrentFrame], 0, nullptr );

        vkCmdDrawIndexed( cmd, dlInfo->numIndexes, 1, 0, 0, 0 );
    }
}

//----------------------------------------------------------------------------
// TessDrawFog
//----------------------------------------------------------------------------

static void TessDrawFog( const shaderCommands_t* input )
{
    VkCommandBuffer cmd = g_vkCommandBuffers[g_vkCurrentFrame];

    // Select fog pipeline based on fogPass type
    VkPipeline pipeFog = ( input->shader->fogPass == FP_EQUAL )
        ? g_vkDrawState.fogRenderData.additivePipeline
        : g_vkDrawState.fogRenderData.pipeline;
    vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeFog );

    // Bind vertex buffers: position + fog texcoords + fog colors
    VkBuffer vbufs[3] = {
        g_vkDrawState.tessBufs.xyz.buffer,
        g_vkDrawState.tessBufs.fog.texCoords.buffer,
        g_vkDrawState.tessBufs.fog.colors.buffer
    };
    VkDeviceSize voffsets[3] = {
        g_vkDrawState.tessBufs.xyz.currentOffset,
        g_vkDrawState.tessBufs.fog.texCoords.currentOffset,
        g_vkDrawState.tessBufs.fog.colors.currentOffset
    };
    vkCmdBindVertexBuffers( cmd, 0, 3, vbufs, voffsets );

    vkCmdBindIndexBuffer( cmd, g_vkDrawState.tessBufs.indexes.buffer,
                          g_vkDrawState.tessBufs.indexes.currentOffset, VK_INDEX_TYPE_UINT16 );

    vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vkPipelineLayout, 0, 1, &g_vkDescriptorSets[g_vkCurrentFrame], 0, nullptr );

    // Update texture descriptor for fog
    const vkImage_t* tex = GetImageRenderInfo( tr.fogImage );
    ASSERT( tex );
    VKDRV_UpdateTextureDescriptors( tex, nullptr );

    vkCmdDrawIndexed( cmd, input->numIndexes, 1, 0, 0, 0 );
}

//----------------------------------------------------------------------------
// IterateStagesGeneric -- iterates through shader stages
//----------------------------------------------------------------------------

static void IterateStagesGeneric( const shaderCommands_t* input )
{
    qboolean lightmapMode = (qboolean)( r_lightmap->integer != 0 );
    VkPipeline overridePipe = lightmapMode ? g_vkDrawState.lightmapRenderData.pipeline : VK_NULL_HANDLE;

    for ( int stage = 0; stage < MAX_SHADER_STAGES; stage++ )
    {
        shaderStage_t* pStage = input->xstages[stage];
        if ( !pStage )
            break;

        VKDrv_SetState( pStage->stateBits );
        UpdateMaterialState();

        if ( pStage->bundle[1].image[0] != 0 )
        {
            TessDrawMultitextured( input, stage, overridePipe );
        }
        else
        {
            TessDrawTextured( input, stage, overridePipe );
        }

        if ( lightmapMode &&
             ( pStage->bundle[0].isLightmap || pStage->bundle[1].isLightmap || pStage->bundle[0].vertexLightmap ) )
        {
            break;
        }
    }
}

//----------------------------------------------------------------------------
// Public driver entry points for drawing
//----------------------------------------------------------------------------

void VKDrv_DrawImage( const image_t* image, const float* coords, const float* texcoords, const float* color )
{
    const vkImage_t* vkImg = GetImageRenderInfo( image );
    DrawQuad( &g_vkDrawState.quadRenderData, vkImg, coords, texcoords, color );
}

void VKDrv_DrawSkyBox( const skyboxDrawInfo_t* skybox, const float* eye_origin, const float* colorTint )
{
    DrawSkyBox( &g_vkDrawState.skyBoxRenderData, skybox, eye_origin, colorTint );
}

void VKDrv_DrawBeam( const image_t* image, const float* color, const vec3_t startPoints[], const vec3_t endPoints[], int segs )
{
    if ( !startPoints || !endPoints || segs <= 0 || !image )
        return;

    // Generate beam ribbon geometry: quad strip between start and end points
    float width = 1.0f;
    if ( segs > 0 )
    {
        float dx = endPoints[0][0] - startPoints[0][0];
        float dy = endPoints[0][1] - startPoints[0][1];
        float dz = endPoints[0][2] - startPoints[0][2];
        float dist = sqrtf( dx * dx + dy * dy + dz * dz );
        if ( dist > 0.001f )
            width = fminf( dist * 0.02f, 8.0f );
    }

    struct BeamVert { float x, y, z, s, t; };
    int vertCount = segs * 4;
    BeamVert* verts = (BeamVert*)ri.Malloc( vertCount * sizeof(BeamVert) );
    if ( !verts )
    {
        ri.Printf( PRINT_WARNING, "Vulkan: Out of memory in VKDrv_DrawBeam\n" );
        return;
    }

    for ( int i = 0; i < segs; i++ )
    {
        float sx = startPoints[i][0], sy = startPoints[i][1], sz = startPoints[i][2];
        float ex = endPoints[i][0], ey = endPoints[i][1], ez = endPoints[i][2];

        float dx = ex - sx, dy = ey - sy, dz = ez - sz;
        float len = sqrtf( dx * dx + dy * dy + dz * dz );
        if ( len < 0.001f ) len = 1.0f;
        dx /= len; dy /= len; dz /= len;

        float upx = 0, upy = 1, upz = 0;
        if ( fabsf(dy) > 0.9f ) { upx = 1; upy = 0; upz = 0; }

        float px = dy * upz - dz * upy;
        float py = dz * upx - dx * upz;
        float pz = dx * upy - dy * upx;
        float plen = sqrtf( px * px + py * py + pz * pz );
        if ( plen > 0.001f ) { px /= plen; py /= plen; pz /= plen; }

        float t0 = (float)i / segs, t1 = (float)(i + 1) / segs;
        verts[i * 4 + 0] = { sx - px * width, sy - py * width, sz - pz * width, 0, t0 };
        verts[i * 4 + 1] = { sx + px * width, sy + py * width, sz + pz * width, 1, t0 };
        verts[i * 4 + 2] = { ex - px * width, ey - py * width, ez - pz * width, 0, t1 };
        verts[i * 4 + 3] = { ex + px * width, ey + py * width, ez + pz * width, 1, t1 };
    }

    vkCircularBufferUpload( &g_vkDrawState.tessBufs.xyz, verts, vertCount * sizeof(BeamVert) );
    ri.Free( verts );

    const vkImage_t* vkTex = GetImageRenderInfo( image );
    if ( vkTex )
    {
        VKDRV_UpdateTextureDescriptors( vkTex, nullptr );
    }

    VKDrv_SetState( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHTEST_DISABLE );
    UpdateMaterialState();

    VkCommandBuffer cmd = g_vkCommandBuffers[g_vkCurrentFrame];
    VkPipeline pipe = VKDRV_SelectPipeline( g_vkRunState.depthStateMask, CT_BACK_SIDED,
                                            qfalse, g_vkRunState.blendState );
    vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe );

    VkBuffer vbuf[] = { g_vkDrawState.tessBufs.xyz.buffer };
    VkDeviceSize voff[] = { g_vkDrawState.tessBufs.xyz.currentOffset };
    vkCmdBindVertexBuffers( cmd, 0, 1, vbuf, voff );

    vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vkPipelineLayout, 0, 1, &g_vkDescriptorSets[g_vkCurrentFrame], 0, nullptr );
    vkCmdDraw( cmd, vertCount, 1, 0, 0 );
}

void VKDrv_DrawStageGeneric( const shaderCommands_t* input )
{
    UpdateViewState();

    qboolean needDLights = (qboolean)(input->dlightBits && input->shader->sort <= SS_OPAQUE
        && !(input->shader->surfaceFlags & (SURF_NODLIGHT | SURF_SKY)));
    qboolean needFog = (qboolean)(input->fogNum && input->shader->fogPass);

    UploadTessBuffers( input, needDLights, needFog );

    VKDRV_CommitRasterizerState( input->shader->cullType, input->shader->polygonOffset, qfalse );

    const vkCircularBuffer_t* indexes = &g_vkDrawState.tessBufs.indexes;

    VkCommandBuffer cmd = g_vkCommandBuffers[g_vkCurrentFrame];

    vkCmdBindIndexBuffer( cmd, indexes->buffer, indexes->currentOffset, VK_INDEX_TYPE_UINT16 );

    // Bind position vertex buffer
    VkBuffer vbuf[] = { g_vkDrawState.tessBufs.xyz.buffer };
    VkDeviceSize voff[] = { g_vkDrawState.tessBufs.xyz.currentOffset };
    vkCmdBindVertexBuffers( cmd, 0, 1, vbuf, voff );

    IterateStagesGeneric( input );

    if ( needDLights )
        TessProjectDynamicLights( input );

    if ( needFog )
        TessDrawFog( input );
}

void VKDrv_DrawStageVertexLitTexture( const shaderCommands_t* input )
{
    // Delegates to generic path
    VKDrv_DrawStageGeneric( input );
}

void VKDrv_DrawStageLightmappedMultitexture( const shaderCommands_t* input )
{
    // Delegates to generic path
    VKDrv_DrawStageGeneric( input );
}

//----------------------------------------------------------------------------
// Debug drawing
//----------------------------------------------------------------------------

static qboolean g_vkDebugOverdrawEnabled = qfalse;
static int      g_vkDebugTextureMode = 0;

void VKDrv_DebugDrawAxis( void )
{
    // Draw a 3-axis debug indicator (X=red, Y=green, Z=blue)
    // Uses degenerate triangle pairs to render lines
    float axisVerts[48] = {
        0, 0, 0, 1,    100, 0, 0, 1,     // X axis
        0, 0, 0, 1,    100, 0, 0, 1,
        0, 0, 0, 1,    0, 100, 0, 1,     // Y axis
        0, 0, 0, 1,    0, 100, 0, 1,
        0, 0, 0, 1,    0, 0, 100, 1,     // Z axis
        0, 0, 0, 1,    0, 0, 100, 1
    };

    vkCircularBufferUpload( &g_vkDrawState.tessBufs.xyz, axisVerts, sizeof(axisVerts) );

    VkCommandBuffer cmd = g_vkCommandBuffers[g_vkCurrentFrame];
    vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vkDrawState.quadRenderData.debugPipeline );

    VkBuffer vbuf[] = { g_vkDrawState.tessBufs.xyz.buffer };
    VkDeviceSize voff[] = { g_vkDrawState.tessBufs.xyz.currentOffset };
    vkCmdBindVertexBuffers( cmd, 0, 1, vbuf, voff );

    vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vkPipelineLayout, 0, 1, &g_vkDescriptorSets[g_vkCurrentFrame], 0, nullptr );
    vkCmdDraw( cmd, 6, 1, 0, 0 );
}

void VKDrv_DebugDrawTris( const shaderCommands_t* input )
{
    if ( !input )
        return;

    UploadTessBuffers( input, qfalse, qfalse );

    VkCommandBuffer cmd = g_vkCommandBuffers[g_vkCurrentFrame];
    vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vkDrawState.quadRenderData.debugPipeline );

    const vkCircularBuffer_t* indexes = &g_vkDrawState.tessBufs.indexes;
    vkCmdBindIndexBuffer( cmd, indexes->buffer, indexes->currentOffset, VK_INDEX_TYPE_UINT16 );

    VkBuffer vbuf[] = { g_vkDrawState.tessBufs.xyz.buffer };
    VkDeviceSize voff[] = { g_vkDrawState.tessBufs.xyz.currentOffset };
    vkCmdBindVertexBuffers( cmd, 0, 1, vbuf, voff );

    vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vkPipelineLayout, 0, 1, &g_vkDescriptorSets[g_vkCurrentFrame], 0, nullptr );
    vkCmdDrawIndexed( cmd, input->numIndexes, 1, 0, 0, 0 );
}

void VKDrv_DebugDrawNormals( const shaderCommands_t* input )
{
    if ( !input || input->numVertexes == 0 )
        return;

    // Build line-segment pairs from vertex positions and their normals
    float* normVerts = (float*)ri.Malloc( input->numVertexes * 2 * sizeof(vec4_t) );
    if ( !normVerts )
    {
        ri.Printf( PRINT_WARNING, "Vulkan: Out of memory in VKDrv_DebugDrawNormals\n" );
        return;
    }
    int vertCount = 0;

    for ( int i = 0; i < input->numVertexes; i++ )
    {
        float scale = 10.0f;

        normVerts[vertCount * 4] = input->xyz[i][0];
        normVerts[vertCount * 4 + 1] = input->xyz[i][1];
        normVerts[vertCount * 4 + 2] = input->xyz[i][2];
        normVerts[vertCount * 4 + 3] = input->xyz[i][3];
        vertCount++;

        {
            float x = input->xyz[i][0];
            float y = input->xyz[i][1];
            float z = input->xyz[i][2];
            float nx = input->normal[i][0];
            float ny = input->normal[i][1];
            float nz = input->normal[i][2];
            normVerts[vertCount * 4]     = x + nx * scale;
            normVerts[vertCount * 4 + 1] = y + ny * scale;
            normVerts[vertCount * 4 + 2] = z + nz * scale;
        }
        normVerts[vertCount * 4 + 3] = 1.0f;
        vertCount++;
    }

    vkCircularBufferUpload( &g_vkDrawState.tessBufs.xyz, normVerts, vertCount * sizeof(vec4_t) );
    ri.Free( normVerts );

    VkCommandBuffer cmd = g_vkCommandBuffers[g_vkCurrentFrame];
    vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vkDrawState.quadRenderData.debugPipeline );

    VkBuffer vbuf[] = { g_vkDrawState.tessBufs.xyz.buffer };
    VkDeviceSize voff[] = { g_vkDrawState.tessBufs.xyz.currentOffset };
    vkCmdBindVertexBuffers( cmd, 0, 1, vbuf, voff );

    vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vkPipelineLayout, 0, 1, &g_vkDescriptorSets[g_vkCurrentFrame], 0, nullptr );
    vkCmdDraw( cmd, vertCount, 1, 0, 0 );
}

void VKDrv_DebugSetOverdrawMeasureEnabled( qboolean enabled )
{
    g_vkDebugOverdrawEnabled = enabled;
}

void VKDrv_DebugSetTextureMode( const char* mode )
{
    if ( mode )
    {
        if ( Q_stricmp( mode, "lightmap" ) == 0 )
            g_vkDebugTextureMode = 1;
        else if ( Q_stricmp( mode, "normalmap" ) == 0 )
            g_vkDebugTextureMode = 2;
        else
            g_vkDebugTextureMode = 0;
    }
    else
    {
        g_vkDebugTextureMode = 0;
    }
}

void VKDrv_DebugDrawPolygon( int color, int numPoints, const float* points )
{
    if ( !points || numPoints < 2 )
        return;

    (void)color;

    // Build closed polygon as degenerate triangle pairs (line rendering)
    float* polyVerts = (float*)ri.Malloc( (numPoints + 1) * sizeof(vec4_t) );
    if ( !polyVerts )
    {
        ri.Printf( PRINT_WARNING, "Vulkan: Out of memory in VKDrv_DebugDrawPolygon\n" );
        return;
    }
    for ( int i = 0; i < numPoints; i++ )
    {
        polyVerts[i * 4 + 0] = points[i * 3 + 0];
        polyVerts[i * 4 + 1] = points[i * 3 + 1];
        polyVerts[i * 4 + 2] = points[i * 3 + 2];
        polyVerts[i * 4 + 3] = 1.0f;
    }
    // Close the polygon
    polyVerts[numPoints * 4 + 0] = points[0];
    polyVerts[numPoints * 4 + 1] = points[1];
    polyVerts[numPoints * 4 + 2] = points[2];
    polyVerts[numPoints * 4 + 3] = 1.0f;

    vkCircularBufferUpload( &g_vkDrawState.tessBufs.xyz, polyVerts, (numPoints + 1) * sizeof(vec4_t) );
    ri.Free( polyVerts );

    VkCommandBuffer cmd = g_vkCommandBuffers[g_vkCurrentFrame];
    vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vkDrawState.quadRenderData.debugPipeline );

    VkBuffer vbuf[] = { g_vkDrawState.tessBufs.xyz.buffer };
    VkDeviceSize voff[] = { g_vkDrawState.tessBufs.xyz.currentOffset };
    vkCmdBindVertexBuffers( cmd, 0, 1, vbuf, voff );

    vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vkPipelineLayout, 0, 1, &g_vkDescriptorSets[g_vkCurrentFrame], 0, nullptr );
    vkCmdDraw( cmd, numPoints + 1, 1, 0, 0 );
}

void VKDrv_BeginTessellate( const shaderCommands_t* input )
{
    // No pre-tessellation setup needed for Vulkan
}

void VKDrv_EndTessellate( const shaderCommands_t* input )
{
    // No post-tessellation cleanup needed for Vulkan
}
