#pragma once

// @pjb: image loading functions

const d3dImage_t* GetImageRenderInfo( const image_t* image );

#ifdef Q3D3D11
void InitImages();
void DestroyImages();
#endif
