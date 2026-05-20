#pragma once

// @pjb: image loading and management functions

const vkImage_t* GetImageRenderInfo( const image_t* image );
void InitImages();
void DestroyImages();
