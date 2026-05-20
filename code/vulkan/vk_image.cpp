#include "vk_common.h"
#include "vk_state.h"
#include "vk_driver.h"

//----------------------------------------------------------------------------
// Apply gamma correction to pixel data (matches OpenGL R_LightScaleTexture)
//----------------------------------------------------------------------------

static void VKDRV_ApplyGamma( byte* data, int pixelCount, int bytesPerPixel )
{
    for ( int i = 0; i < pixelCount; i++ )
    {
        data[i * bytesPerPixel + 0] = g_vkGammaTable[data[i * bytesPerPixel + 0]];
        data[i * bytesPerPixel + 1] = g_vkGammaTable[data[i * bytesPerPixel + 1]];
        data[i * bytesPerPixel + 2] = g_vkGammaTable[data[i * bytesPerPixel + 2]];
    }
}

//----------------------------------------------------------------------------
// Image management -- mirrors the D3D11 image system
//----------------------------------------------------------------------------

#define VK_IMAGE_POOL_SIZE  4096

static vkImage_t  g_vkImagePool[VK_IMAGE_POOL_SIZE];
static int        g_vkImageCount = 0;

//----------------------------------------------------------------------------
// Format conversion: engine imageFormat_t → VkFormat
//----------------------------------------------------------------------------

static VkDeviceSize VKDRV_CalculateImageUploadSize( VkFormat format, int width, int height )
{
    // Compressed (block) formats
    switch ( format )
    {
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_BC4_SNORM_BLOCK:
        // 8 bytes per 4x4 block
    {
        int blocksX = (width + 3) / 4;
        int blocksY = (height + 3) / 4;
        return (VkDeviceSize)blocksX * blocksY * 8;
    }
    case VK_FORMAT_BC2_UNORM_BLOCK:
    case VK_FORMAT_BC3_UNORM_BLOCK:
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
        // 16 bytes per 4x4 block
    {
        int blocksX = (width + 3) / 4;
        int blocksY = (height + 3) / 4;
        return (VkDeviceSize)blocksX * blocksY * 16;
    }
    default:
        // Uncompressed formats
        break;
    }

    // Uncompressed: calculate bytes per pixel
    int bytesPerPixel = 4; // default RGBA
    switch ( format )
    {
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_SRGB:
        bytesPerPixel = 4; break;
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_B8G8R8_UNORM:
        bytesPerPixel = 3; break;
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SNORM:
        bytesPerPixel = 1; break;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        bytesPerPixel = 16; break;
    case VK_FORMAT_R32G32B32_SFLOAT:
        bytesPerPixel = 12; break;
    default:
        bytesPerPixel = 4; break;
    }

    return (VkDeviceSize)width * height * bytesPerPixel;
}

// Calculate the source data size based on the original engine format
static VkDeviceSize VKDRV_CalculateSourceDataSize( imageFormat_t fmt, int width, int height, qboolean isLightmap )
{
    // Lightmaps are always promoted to 4-byte format
    if ( isLightmap && fmt == IMAGEFORMAT_I )
        return (VkDeviceSize)width * height * 4;

    switch ( fmt )
    {
    case IMAGEFORMAT_RGB:
    case IMAGEFORMAT_RGB5:
        return (VkDeviceSize)width * height * 3;
    case IMAGEFORMAT_I:
        return (VkDeviceSize)width * height * 1;
    case IMAGEFORMAT_RGBA:
    case IMAGEFORMAT_RGBA8:
    case IMAGEFORMAT_RGBA4:
    case IMAGEFORMAT_IA:
    case IMAGEFORMAT_S3TC:
    default:
        return (VkDeviceSize)width * height * 4;
    }
}

static VkFormat VKDRV_GetVkFormat( imageFormat_t fmt, qboolean isLightmap )
{
    switch ( fmt )
    {
    case IMAGEFORMAT_RGBA:
    case IMAGEFORMAT_RGBA8:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case IMAGEFORMAT_RGB:
    case IMAGEFORMAT_RGB5:
        return VK_FORMAT_R8G8B8A8_UNORM; // promote to RGBA
    case IMAGEFORMAT_IA:
        return VK_FORMAT_R8G8B8A8_UNORM; // IA → RGBA expansion
    case IMAGEFORMAT_I:
        if ( isLightmap )
            return VK_FORMAT_R8G8B8A8_UNORM; // lightmaps need full color
        return VK_FORMAT_R8_UNORM;
    case IMAGEFORMAT_RGBA4:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case IMAGEFORMAT_S3TC:
        return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
    default:
        return VK_FORMAT_R8G8B8A8_UNORM;
    }
}

//----------------------------------------------------------------------------
// VKDrv_CreateImage -- uploads image data to GPU
//----------------------------------------------------------------------------

void VKDrv_CreateImage( const image_t* image, const byte* pic, qboolean isLightmap )
{
    if ( !image || !pic )
        return;

    VkFormat format = VKDRV_GetVkFormat( image->format, isLightmap );

    // Calculate total mip levels before using in image creation
    int maxDim = image->width > image->height ? image->width : image->height;
    int totalMipLevels = 1;
    while ( maxDim > 1 ) { totalMipLevels++; maxDim >>= 1; }

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = image->width;
    imageInfo.extent.height = image->height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = image->mipmap ? totalMipLevels : 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkImage vkImage;
    VmaAllocation allocation;
    VK_CHECK( vmaCreateImage( g_vkAllocator, &imageInfo, &allocInfo, &vkImage, &allocation, nullptr ) );

    // Create image view
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = vkImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = image->mipmap ? totalMipLevels : 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    VK_CHECK( vkCreateImageView( g_vkDevice, &viewInfo, nullptr, &imageView ) );

    // Transition image layout to TRANSFER_DST
    VkCommandBuffer cmd = VKDRV_BeginCommandBuffer();

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = vkImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           0, 0, nullptr, 0, nullptr, 1, &barrier );

    // Create staging buffer and copy
    VkDeviceSize imageSize = VKDRV_CalculateImageUploadSize( format, image->width, image->height );
    VkBuffer stagingBuffer;
    VmaAllocation stagingAllocation;

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = imageSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo stagingAllocInfo = {};
    stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    VK_CHECK( vmaCreateBuffer( g_vkAllocator, &bufferInfo, &stagingAllocInfo,
                                &stagingBuffer, &stagingAllocation, nullptr ) );

    void* data;
    vmaMapMemory( g_vkAllocator, stagingAllocation, &data );
    memcpy( data, pic, (size_t)imageSize );

    // Apply gamma correction matching OpenGL driver's table-based approach
    // (spec §11.6). Engine already applies s_intensitytable (overbright);
    // we apply s_gammatable here since Vulkan has no hardware gamma.
    int bpp = 4;
    switch ( format )
    {
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_B8G8R8_UNORM:  bpp = 3; break;
    case VK_FORMAT_R8_UNORM:      bpp = 1; break;
    default:                      bpp = 4; break;
    }
    VKDRV_ApplyGamma( (byte*)data, (int)(imageSize / bpp), bpp );

    vmaUnmapMemory( g_vkAllocator, stagingAllocation );

    VkBufferImageCopy copyRegion = {};
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent.width = image->width;
    copyRegion.imageExtent.height = image->height;
    copyRegion.imageExtent.depth = 1;

    vkCmdCopyBufferToImage( cmd, stagingBuffer, vkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             1, &copyRegion );

    // Transition to SHADER_READABLE
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                           0, 0, nullptr, 0, nullptr, 1, &barrier );

    VKDRV_EndCommandBuffer( cmd );
    VKDRV_SubmitCommandBuffer( cmd, qfalse, qtrue );

    // Generate mipmaps via vkCmdBlitImage (if mipmap > 1)
    if ( totalMipLevels > 1 )
    {
        VkCommandBuffer mipmapCmd = VKDRV_BeginCommandBuffer();

        // Transition entire mip chain to TRANSFER_DST_OPTIMAL.
        // Per Vulkan spec, any TRANSFER_*_OPTIMAL layout supports both
        // transfer reads and writes, so we can use TRANSFER_DST_OPTIMAL
        // for both source and destination of vkCmdBlitImage.
        VkImageMemoryBarrier toTransfer = {};
        toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toTransfer.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toTransfer.image = vkImage;
        toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toTransfer.subresourceRange.baseMipLevel = 0;
        toTransfer.subresourceRange.levelCount = (uint32_t)totalMipLevels;
        toTransfer.subresourceRange.baseArrayLayer = 0;
        toTransfer.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier( mipmapCmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                               0, nullptr, 0, nullptr, 1, &toTransfer );

        // Blit each mip level from the previous one
        int mipWidth = image->width;
        int mipHeight = image->height;

        for ( int level = 1; level < totalMipLevels; level++ )
        {
            int nextWidth = mipWidth > 2 ? mipWidth >> 1 : 1;
            int nextHeight = mipHeight > 2 ? mipHeight >> 1 : 1;

            VkImageBlit blitRegion = {};
            blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blitRegion.srcSubresource.mipLevel = level - 1;
            blitRegion.srcSubresource.baseArrayLayer = 0;
            blitRegion.srcSubresource.layerCount = 1;
            blitRegion.srcOffsets[0] = { 0, 0, 0 };
            blitRegion.srcOffsets[1] = { mipWidth, mipHeight, 1 };
            blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blitRegion.dstSubresource.mipLevel = level;
            blitRegion.dstSubresource.baseArrayLayer = 0;
            blitRegion.dstSubresource.layerCount = 1;
            blitRegion.dstOffsets[0] = { 0, 0, 0 };
            blitRegion.dstOffsets[1] = { nextWidth, nextHeight, 1 };

            vkCmdBlitImage( mipmapCmd, vkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             vkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             1, &blitRegion, VK_FILTER_LINEAR );

            mipWidth = nextWidth;
            mipHeight = nextHeight;
        }

        // Transition entire mip chain to SHADER_READ_ONLY_OPTIMAL
        VkImageMemoryBarrier finalBarrier = {};
        finalBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        finalBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        finalBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        finalBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        finalBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        finalBarrier.image = vkImage;
        finalBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        finalBarrier.subresourceRange.baseMipLevel = 0;
        finalBarrier.subresourceRange.levelCount = (uint32_t)totalMipLevels;
        finalBarrier.subresourceRange.baseArrayLayer = 0;
        finalBarrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier( mipmapCmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                               0, nullptr, 0, nullptr, 1, &finalBarrier );

        VKDRV_EndCommandBuffer( mipmapCmd );
        // Wait for mipmap generation to complete before returning, so a render
        // frame cannot sample a partially-mipmapped texture. The fence was
        // already reset by the initial upload submission, so waiting here
        // correctly gates on the mipmap blit (not the upload).
        VKDRV_SubmitCommandBuffer( mipmapCmd, qtrue, qtrue );
    }

    vmaDestroyBuffer( g_vkAllocator, stagingBuffer, stagingAllocation );

    // Store in pool -- use image index as direct lookup
    int slot = image->index;
    if ( slot < 0 || slot >= VK_IMAGE_POOL_SIZE )
    {
        ri.Printf( PRINT_WARNING, "WARNING: Vulkan image index %d out of pool range [0, %d)\n",
                    image->index, VK_IMAGE_POOL_SIZE );
        return;
    }
    g_vkImagePool[slot].image = vkImage;
    g_vkImagePool[slot].allocation = allocation;
    g_vkImagePool[slot].imageView = imageView;
    g_vkImagePool[slot].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    g_vkImagePool[slot].format = format;
    g_vkImagePool[slot].width = image->width;
    g_vkImagePool[slot].height = image->height;
    g_vkImagePool[slot].frameUsed = 0;
    g_vkImagePool[slot].dynamic = qfalse;

    // Select sampler based on wrap mode
    g_vkImagePool[slot].sampler = (image->wrapClampMode == WRAPMODE_REPEAT)
                                    ? g_vkSamplerRepeat : g_vkSamplerClamp;

    g_vkImageCount++;
}

//----------------------------------------------------------------------------
// VKDrv_DeleteImage
//----------------------------------------------------------------------------

void VKDrv_DeleteImage( const image_t* image )
{
    if ( !image )
        return;

    int slot = image->index;
    if ( slot < 0 || slot >= VK_IMAGE_POOL_SIZE )
        return;
    vkImage_t* img = &g_vkImagePool[slot];

    if ( img->imageView )
    {
        vkDestroyImageView( g_vkDevice, img->imageView, nullptr );
        img->imageView = VK_NULL_HANDLE;
    }
    if ( img->image )
    {
        vmaDestroyImage( g_vkAllocator, img->image, img->allocation );
        img->image = VK_NULL_HANDLE;
        img->allocation = nullptr;
    }
    Com_Memset( img, 0, sizeof( vkImage_t ) );
    if ( g_vkImageCount > 0 ) g_vkImageCount--;
}

//----------------------------------------------------------------------------
// VKDrv_UpdateCinematic -- for video texture updates
//----------------------------------------------------------------------------

void VKDrv_UpdateCinematic( const image_t* image, const byte* pic, int cols, int rows, qboolean dirty )
{
    if ( !image || !pic )
        return;

    int slot = image->index;
    if ( slot < 0 || slot >= VK_IMAGE_POOL_SIZE )
        return;
    vkImage_t* img = &g_vkImagePool[slot];
    if ( !img->image )
        return;

    // Mark as dynamic
    img->dynamic = qtrue;

    // Create staging buffer and copy
    VkDeviceSize imageSize = cols * rows * 4;
    VkBuffer stagingBuffer;
    VmaAllocation stagingAllocation;

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = imageSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo stagingAllocInfo = {};
    stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    VK_CHECK( vmaCreateBuffer( g_vkAllocator, &bufferInfo, &stagingAllocInfo,
                                &stagingBuffer, &stagingAllocation, nullptr ) );

    void* data;
    vmaMapMemory( g_vkAllocator, stagingAllocation, &data );
    memcpy( data, pic, (size_t)imageSize );

    // Apply gamma correction for cinematic updates
    VKDRV_ApplyGamma( (byte*)data, cols * rows, 4 );

    vmaUnmapMemory( g_vkAllocator, stagingAllocation );

    VkCommandBuffer cmd = VKDRV_BeginCommandBuffer();

    // Transition from SHADER_READ_ONLY to TRANSFER_DST
    VkImageMemoryBarrier toTransfer = {};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = img->image;
    toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toTransfer.subresourceRange.baseMipLevel = 0;
    toTransfer.subresourceRange.levelCount = 1;
    toTransfer.subresourceRange.baseArrayLayer = 0;
    toTransfer.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           0, 0, nullptr, 0, nullptr, 1, &toTransfer );

    VkBufferImageCopy copyRegion = {};
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent.width = cols;
    copyRegion.imageExtent.height = rows;
    copyRegion.imageExtent.depth = 1;

    vkCmdCopyBufferToImage( cmd, stagingBuffer, img->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             1, &copyRegion );

    // Transition back to SHADER_READ_ONLY
    VkImageMemoryBarrier toShader = {};
    toShader.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toShader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toShader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toShader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toShader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toShader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toShader.image = img->image;
    toShader.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toShader.subresourceRange.baseMipLevel = 0;
    toShader.subresourceRange.levelCount = 1;
    toShader.subresourceRange.baseArrayLayer = 0;
    toShader.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                           0, 0, nullptr, 0, nullptr, 1, &toShader );

    VKDRV_EndCommandBuffer( cmd );
    VKDRV_SubmitCommandBuffer( cmd, qfalse, qtrue );

    vmaDestroyBuffer( g_vkAllocator, stagingBuffer, stagingAllocation );
}

//----------------------------------------------------------------------------
// VKDrv_GetImageFormat
//----------------------------------------------------------------------------

imageFormat_t VKDrv_GetImageFormat( const image_t* image )
{
    if ( !image )
        return IMAGEFORMAT_UNKNOWN;

    int slot = image->index;
    if ( slot < 0 || slot >= VK_IMAGE_POOL_SIZE )
        return IMAGEFORMAT_UNKNOWN;
    vkImage_t* img = &g_vkImagePool[slot];

    switch ( img->format )
    {
    case VK_FORMAT_R8G8B8A8_UNORM:  return IMAGEFORMAT_RGBA8;
    case VK_FORMAT_R8_UNORM:        return IMAGEFORMAT_I;
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK: return IMAGEFORMAT_S3TC;
    default:                        return IMAGEFORMAT_UNKNOWN;
    }
}

//----------------------------------------------------------------------------
// GetImageRenderInfo -- lookup helper
//----------------------------------------------------------------------------

const vkImage_t* GetImageRenderInfo( const image_t* image )
{
    if ( !image )
        return nullptr;

    int slot = image->index;
    if ( slot < 0 || slot >= VK_IMAGE_POOL_SIZE )
    {
        ri.Printf( PRINT_WARNING, "WARNING: Vulkan image index %d out of pool range [0, %d)\n",
                    image->index, VK_IMAGE_POOL_SIZE );
        return nullptr;
    }
    return &g_vkImagePool[slot];
}

//----------------------------------------------------------------------------
// Init/Destroy
//----------------------------------------------------------------------------

void InitImages()
{
    Com_Memset( g_vkImagePool, 0, sizeof( g_vkImagePool ) );
    g_vkImageCount = 0;
}

void DestroyImages()
{
    for ( int i = 0; i < VK_IMAGE_POOL_SIZE; i++ )
    {
        vkImage_t* img = &g_vkImagePool[i];
        if ( img->imageView )
        {
            vkDestroyImageView( g_vkDevice, img->imageView, nullptr );
            img->imageView = VK_NULL_HANDLE;
        }
        if ( img->image )
        {
            vmaDestroyImage( g_vkAllocator, img->image, img->allocation );
            img->image = VK_NULL_HANDLE;
            img->allocation = nullptr;
        }
    }
    Com_Memset( g_vkImagePool, 0, sizeof( g_vkImagePool ) );
    g_vkImageCount = 0;
}
