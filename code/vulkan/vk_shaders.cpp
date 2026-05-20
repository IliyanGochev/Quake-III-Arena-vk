#include "vk_common.h"
#include "vk_state.h"

//----------------------------------------------------------------------------
// SPIR-V shader loading from compiled files
//----------------------------------------------------------------------------

// Shader module cache
#define VK_SHADER_CACHE_SIZE  32

struct vkShaderCacheEntry_t {
    const char* name;
    VkShaderModule module;
};

static vkShaderCacheEntry_t g_vkShaderCache[VK_SHADER_CACHE_SIZE];
static int g_vkShaderCount = 0;

//----------------------------------------------------------------------------
// LoadShaderModule -- reads a .spv file and creates a VkShaderModule
//----------------------------------------------------------------------------

VkShaderModule LoadShaderModule( const char* name )
{
    // Check cache first
    for ( int i = 0; i < g_vkShaderCount; i++ )
    {
        if ( strcmp( g_vkShaderCache[i].name, name ) == 0 )
            return g_vkShaderCache[i].module;
    }

    // Build path to shader file
    char path[MAX_QPATH];
    Q_strlcpy( path, "vulkan/shaders/compiled/", sizeof(path) );
    Q_strlcat( path, name, sizeof(path) );
    if ( Q_stricmp( path + strlen(path) - 4, ".spv" ) != 0 )
        Q_strlcat( path, ".spv", sizeof(path) );

    // Read file
    int fileSize;
    byte* fileData = nullptr;
    fileData = (byte*)ri.FS_ReadFile( path, &fileSize );
    if ( !fileData )
    {
        ri.Printf( PRINT_DEVELOPER, "WARNING: Could not load shader: %s\n", path );
        return VK_NULL_HANDLE;
    }

    VkShaderModule module;
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = fileSize;
    createInfo.pCode = (const uint32_t*)fileData;

    if ( vkCreateShaderModule( g_vkDevice, &createInfo, nullptr, &module ) != VK_SUCCESS )
    {
        ri.Printf( PRINT_DEVELOPER, "ERROR: Failed to create shader module: %s\n", path );
        ri.Free( fileData );
        return VK_NULL_HANDLE;
    }

    ri.Free( fileData );

    // Cache it
    if ( g_vkShaderCount < VK_SHADER_CACHE_SIZE )
    {
        g_vkShaderCache[g_vkShaderCount].name = name;
        g_vkShaderCache[g_vkShaderCount].module = module;
        g_vkShaderCount++;
    }

    return module;
}

//----------------------------------------------------------------------------
// Shader stage module creation helpers
//----------------------------------------------------------------------------

VkPipelineShaderStageCreateInfo CreateVertexShaderStage( VkShaderModule module )
{
    VkPipelineShaderStageCreateInfo stage = {};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage.module = module;
    stage.pName = "main";
    return stage;
}

VkPipelineShaderStageCreateInfo CreateFragmentShaderStage( VkShaderModule module )
{
    VkPipelineShaderStageCreateInfo stage = {};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stage.module = module;
    stage.pName = "main";
    return stage;
}

//----------------------------------------------------------------------------
// InitShaders -- preloads all shader modules
//----------------------------------------------------------------------------

void InitShaders()
{
    // Preload all known shaders
    // These correspond to the .spv files in vulkan/shaders/compiled/
    // - genericst_vs.spv / genericst_ps.spv  (single texture)
    // - genericmt_vs.spv / genericmt_ps.spv  (multi texture)
    // - skybox_vs.spv / skybox_ps.spv        (skybox)
    // - fsq_vs.spv / fsq_ps.spv              (2D quad)

    // Shaders are loaded on demand via LoadShaderModule
    // This function just initializes the cache
    g_vkShaderCount = 0;
    Com_Memset( g_vkShaderCache, 0, sizeof( g_vkShaderCache ) );
}

void DestroyShaders()
{
    for ( int i = 0; i < g_vkShaderCount; i++ )
    {
        if ( g_vkShaderCache[i].module != VK_NULL_HANDLE )
        {
            vkDestroyShaderModule( g_vkDevice, g_vkShaderCache[i].module, nullptr );
            g_vkShaderCache[i].module = VK_NULL_HANDLE;
        }
    }
    g_vkShaderCount = 0;
}
