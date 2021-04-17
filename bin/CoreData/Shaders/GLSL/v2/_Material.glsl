/// _Material.glsl
/// [No depth-only shaders]
/// Helpers used to create material shaders
#ifndef _MATERIAL_GLSL_
#define _MATERIAL_GLSL_

#include "_Config.glsl"
#include "_GammaCorrection.glsl"
#include "_SurfaceData.glsl"
#include "_BRDF.glsl"

#include "_Uniforms.glsl"
#include "_Samplers.glsl"
#include "_VertexLayout.glsl"

#include "_VertexTransform.glsl"
#ifdef URHO3D_PIXEL_NEED_SCREEN_POSITION
#include "_VertexScreenPos.glsl"
#endif
#ifdef URHO3D_IS_LIT
#include "_IndirectLighting.glsl"
#include "_DirectLighting.glsl"
#include "_Shadow.glsl"
#endif
#include "_Fog.glsl"

#include "_Material_Common.glsl"
#ifdef URHO3D_VERTEX_SHADER
#include "_Material_Vertex.glsl"
#endif
#ifdef URHO3D_PIXEL_SHADER
#include "_Material_Pixel.glsl"
#endif

#endif // _MATERIAL_GLSL_
