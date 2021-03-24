//
// Copyright (c) 2017-2020 the rbfx project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#pragma once

#include "../Container/Hash.h"
#include "../Core/Signal.h"
#include "../Graphics/GraphicsDefs.h"
#include "../Math/Vector2.h"
#include "../Scene/Serializable.h"

namespace Urho3D
{

class Light;
class PipelineState;
class RenderSurface;
class Texture2D;
class Viewport;
struct BatchStateCreateKey;
struct BatchStateCreateContext;
struct UIBatchStateKey;
struct UIBatchStateCreateContext;

/// Common parameters of rendered frame.
struct CommonFrameInfo
{
    unsigned frameNumber_{};
    float timeStep_{};

    IntVector2 viewportSize_;
    IntRect viewportRect_;

    Viewport* viewport_{};
    RenderSurface* renderTarget_{};
};

/// Traits of scene pass.
enum class DrawableProcessorPassFlag
{
    None = 0,
    HasAmbientLighting = 1 << 0,
    DisableInstancing = 1 << 1,
    DeferredLightMaskToStencil = 1 << 2,
};

URHO3D_FLAGSET(DrawableProcessorPassFlag, DrawableProcessorPassFlags);

/// Flags that control how exactly batches are rendered.
enum class BatchRenderFlag
{
    None = 0,
    EnableAmbientLighting = 1 << 0,
    EnableVertexLights = 1 << 1,
    EnablePixelLights = 1 << 2,
    EnableInstancingForStaticGeometry = 1 << 3,

    EnableAmbientAndVertexLighting = EnableAmbientLighting | EnableVertexLights,
};

URHO3D_FLAGSET(BatchRenderFlag, BatchRenderFlags);

/// Render buffer traits.
enum class RenderBufferFlag
{
    /// Texture content is preserved between frames.
    Persistent = 1 << 0,
    FixedTextureSize = 1 << 1,
    sRGB = 1 << 2,
    BilinearFiltering = 1 << 3,
    CubeMap = 1 << 4,
    NoMultiSampledAutoResolve = 1 << 5
};

URHO3D_FLAGSET(RenderBufferFlag, RenderBufferFlags);

/// Render buffer parameters. Actual render buffer size is controlled externally.
struct RenderBufferParams
{
    unsigned textureFormat_{};
    int multiSampleLevel_{ 1 };
    RenderBufferFlags flags_;

    bool operator==(const RenderBufferParams& rhs) const
    {
        return textureFormat_ == rhs.textureFormat_
            && multiSampleLevel_ == rhs.multiSampleLevel_
            && flags_ == rhs.flags_;
    }

    bool operator!=(const RenderBufferParams& rhs) const { return !(*this == rhs); }
};

/// Color space of primary color outputs of render pipeline.
/// Color buffer is guaranteed to have Red, Green and Blue channels regardless of this choice.
enum class RenderPipelineColorSpace
{
    /// Low dynamic range lighting in Gamma space, trimmed to [0, 1].
    GammaLDR,
    /// Low dynamic range lighting in Linear space, trimmed to [0, 1].
    LinearLDR,
    /// High dynamic range lighting in Linear space. Should be tone mapped before frame end.
    LinearHDR
};

/// Rarely-changing settings of render buffer manager.
struct RenderBufferManagerSettings
{
    /// Whether to inherit multisample level from output render texture.
    bool inheritMultiSampleLevel_{};
    /// Multisample level of both output color buffers and depth buffer.
    int multiSampleLevel_{ 1 };
    /// Preferred color space of both output color buffers.
    RenderPipelineColorSpace colorSpace_{};
    /// Whether output color buffers are required to have at least bilinear filtering.
    bool filteredColor_{};
    /// Whether the depth-stencil buffer is required to have stencil.
    bool stencilBuffer_{};
    /// Whether the both of output color buffers should be usable with other render targets.
    /// OpenGL backbuffer color cannot do that.
    bool colorUsableWithMultipleRenderTargets_{};

    /// Utility operators
    /// @{
    unsigned CalculatePipelineStateHash() const
    {
        unsigned hash = 0;
        return hash;
    }

    bool operator==(const RenderBufferManagerSettings& rhs) const
    {
        return inheritMultiSampleLevel_ == rhs.inheritMultiSampleLevel_
            && multiSampleLevel_ == rhs.multiSampleLevel_
            && colorSpace_ == rhs.colorSpace_
            && filteredColor_ == rhs.filteredColor_
            && stencilBuffer_ == rhs.stencilBuffer_
            && colorUsableWithMultipleRenderTargets_ == rhs.colorUsableWithMultipleRenderTargets_;
    }

    bool operator!=(const RenderBufferManagerSettings& rhs) const { return !(*this == rhs); }
    /// @}
};

/// Frequently-changing settings of render buffer manager.
struct RenderBufferManagerFrameSettings
{
    /// Whether the depth buffer should be readable.
    bool readableDepth_{};
    /// Whether the both of output color buffers should be readable.
    bool readableColor_{};
    /// Whether it's should be supported to read from and write to output color buffer simultaneously.
    bool supportColorReadWrite_{};
};

/// Traits of post-processing pass
enum class PostProcessPassFlag
{
    None = 0,
    NeedColorOutputReadAndWrite = 1 << 0,
    NeedColorOutputBilinear = 1 << 1,
};

URHO3D_FLAGSET(PostProcessPassFlag, PostProcessPassFlags);

/// Pipeline state cache callback used to create actual pipeline state.
class BatchStateCacheCallback
{
public:
    /// Create pipeline state for given context and key.
    /// Only attributes that constribute to pipeline state hashes are safe to use.
    virtual SharedPtr<PipelineState> CreateBatchPipelineState(
        const BatchStateCreateKey& key, const BatchStateCreateContext& ctx) = 0;
};

/// Pipeline state cache callback used to create actual pipeline state for UI batches.
class UIBatchStateCacheCallback
{
public:
    /// Create pipeline state for given key.
    /// Only attributes that constribute to pipeline state hashes are safe to use.
    virtual SharedPtr<PipelineState> CreateUIBatchPipelineState(const UIBatchStateKey& key, const UIBatchStateCreateContext& ctx) = 0;
};

/// Base interface of render pipeline required by Render Pipeline classes.
class URHO3D_API RenderPipelineInterface
    : public Serializable
{
    URHO3D_OBJECT(RenderPipelineInterface, Serializable);

public:
    using Serializable::Serializable;

    /// Callbacks
    /// @{
    Signal<void(const CommonFrameInfo& frameInfo)> OnUpdateBegin;
    Signal<void(const CommonFrameInfo& frameInfo)> OnUpdateEnd;
    Signal<void(const CommonFrameInfo& frameInfo)> OnRenderBegin;
    Signal<void(const CommonFrameInfo& frameInfo)> OnRenderEnd;
    Signal<void()> OnPipelineStatesInvalidated;
    /// @}
};

/// Region of shadow map that contains one or more shadow split.
struct ShadowMapRegion
{
    unsigned pageIndex_{};
    Texture2D* texture_;
    IntRect rect_;

    /// Return whether the shadow map region is not empty.
    operator bool() const { return !!texture_; }
    /// Return sub-region for split.
    /// Splits are indexed as elements in rectangle grid, from left to right, top to bottom, row-major.
    ShadowMapRegion GetSplit(unsigned split, const IntVector2& numSplits) const;
};

/// Light processor callback.
class LightProcessorCallback
{
public:
    /// Return whether light needs shadow.
    virtual bool IsLightShadowed(Light* light) = 0;
    /// Return best shadow map size for given light. Should be safe to call from multiple threads.
    virtual unsigned GetShadowMapSize(Light* light, unsigned numActiveSplits) const = 0;
    /// Allocate shadow map for one frame.
    virtual ShadowMapRegion AllocateTransientShadowMap(const IntVector2& size) = 0;
};

struct DrawableProcessorSettings
{
    MaterialQuality materialQuality_{ QUALITY_HIGH };
    unsigned maxVertexLights_{ 4 };
    unsigned maxPixelLights_{ 4 };
    unsigned pcfKernelSize_{ 1 };

    /// Utility operators
    /// @{
    unsigned CalculatePipelineStateHash() const
    {
        unsigned hash = 0;
        CombineHash(hash, maxVertexLights_);
        CombineHash(hash, pcfKernelSize_);
        return hash;
    }

    bool operator==(const DrawableProcessorSettings& rhs) const
    {
        return materialQuality_ == rhs.materialQuality_
            && maxVertexLights_ == rhs.maxVertexLights_
            && maxPixelLights_ == rhs.maxPixelLights_
            && pcfKernelSize_ == rhs.pcfKernelSize_;
    }

    bool operator!=(const DrawableProcessorSettings& rhs) const { return !(*this == rhs); }
    /// @}
};

struct InstancingBufferSettings
{
    // TODO(renderer): Make true when implemented
    bool enableInstancing_{ false };
    unsigned firstInstancingTexCoord_{};
    unsigned numInstancingTexCoords_{};

    /// Utility operators
    /// @{
    unsigned CalculatePipelineStateHash() const
    {
        unsigned hash = 0;
        CombineHash(hash, enableInstancing_);
        CombineHash(hash, firstInstancingTexCoord_);
        CombineHash(hash, numInstancingTexCoords_);
        return hash;
    }

    bool operator==(const InstancingBufferSettings& rhs) const
    {
        return enableInstancing_ == rhs.enableInstancing_
            && firstInstancingTexCoord_ == rhs.firstInstancingTexCoord_
            && numInstancingTexCoords_ == rhs.numInstancingTexCoords_;
    }

    bool operator!=(const InstancingBufferSettings& rhs) const { return !(*this == rhs); }
    /// @}
};

enum class DrawableAmbientMode
{
    Constant,
    Flat,
    Directional,
};

struct BatchRendererSettings
{
    bool linearSpaceLighting_{};
    DrawableAmbientMode ambientMode_{ DrawableAmbientMode::Directional };
    Vector2 varianceShadowMapParams_{ 0.0000001f, 0.9f };
    bool specularAntiAliasing_{};

    /// Utility operators
    /// @{
    unsigned CalculatePipelineStateHash() const
    {
        unsigned hash = 0;
        CombineHash(hash, linearSpaceLighting_);
        CombineHash(hash, MakeHash(ambientMode_));
        CombineHash(hash, specularAntiAliasing_);
        return hash;
    }

    bool operator==(const BatchRendererSettings& rhs) const
    {
        return linearSpaceLighting_ == rhs.linearSpaceLighting_
            && ambientMode_ == rhs.ambientMode_
            && varianceShadowMapParams_ == rhs.varianceShadowMapParams_
            && specularAntiAliasing_ == rhs.specularAntiAliasing_;
    }

    bool operator!=(const BatchRendererSettings& rhs) const { return !(*this == rhs); }
    /// @}
};

struct ShadowMapAllocatorSettings
{
    bool enableVarianceShadowMaps_{};
    int varianceShadowMapMultiSample_{ 1 };
    bool use16bitShadowMaps_{};
    unsigned shadowAtlasPageSize_{ 2048 };

    /// Utility operators
    /// @{
    unsigned CalculatePipelineStateHash() const
    {
        unsigned hash = 0;
        CombineHash(hash, enableVarianceShadowMaps_);
        CombineHash(hash, use16bitShadowMaps_);
        return hash;
    }

    bool operator==(const ShadowMapAllocatorSettings& rhs) const
    {
        return enableVarianceShadowMaps_ == rhs.enableVarianceShadowMaps_
            && varianceShadowMapMultiSample_ == rhs.varianceShadowMapMultiSample_
            && use16bitShadowMaps_ == rhs.use16bitShadowMaps_
            && shadowAtlasPageSize_ == rhs.shadowAtlasPageSize_;
    }

    bool operator!=(const ShadowMapAllocatorSettings& rhs) const { return !(*this == rhs); }
    /// @}
};

struct OcclusionBufferSettings
{
    bool threadedOcclusion_{};
    unsigned maxOccluderTriangles_{ 5000 };
    unsigned occlusionBufferSize_{ 256 };
    float occluderSizeThreshold_{ 0.025f };

    /// Utility operators
    /// @{
    unsigned CalculatePipelineStateHash() const
    {
        return 0;
    }

    bool operator==(const OcclusionBufferSettings& rhs) const
    {
        return threadedOcclusion_ == rhs.threadedOcclusion_
            && maxOccluderTriangles_ == rhs.maxOccluderTriangles_
            && occlusionBufferSize_ == rhs.occlusionBufferSize_
            && occluderSizeThreshold_ == rhs.occluderSizeThreshold_;
    }

    bool operator!=(const OcclusionBufferSettings& rhs) const { return !(*this == rhs); }
    /// @}
};

enum class DirectLightingMode
{
    Forward,
    DeferredBlinnPhong,
    DeferredPBR
};

struct SceneProcessorSettings
    : public DrawableProcessorSettings
    , public OcclusionBufferSettings
    , public BatchRendererSettings
{
    bool enableShadows_{ true };
    DirectLightingMode lightingMode_{};
    unsigned directionalShadowSize_{ 1024 };
    unsigned spotShadowSize_{ 1024 };
    unsigned pointShadowSize_{ 256 };

    /// Utility operators
    /// @{
    bool IsDeferredLighting() const
    {
        switch (lightingMode_)
        {
        case DirectLightingMode::DeferredBlinnPhong:
        case DirectLightingMode::DeferredPBR:
            return true;
        default:
            return false;
        }
    }

    unsigned CalculatePipelineStateHash() const
    {
        unsigned hash = 0;
        CombineHash(hash, DrawableProcessorSettings::CalculatePipelineStateHash());
        CombineHash(hash, OcclusionBufferSettings::CalculatePipelineStateHash());
        CombineHash(hash, BatchRendererSettings::CalculatePipelineStateHash());
        CombineHash(hash, enableShadows_);
        CombineHash(hash, MakeHash(lightingMode_));
        return hash;
    }

    bool operator==(const SceneProcessorSettings& rhs) const
    {
        return DrawableProcessorSettings::operator==(rhs)
            && OcclusionBufferSettings::operator==(rhs)
            && BatchRendererSettings::operator==(rhs)
            && enableShadows_ == rhs.enableShadows_
            && lightingMode_ == rhs.lightingMode_
            && directionalShadowSize_ == rhs.directionalShadowSize_
            && spotShadowSize_ == rhs.spotShadowSize_
            && pointShadowSize_ == rhs.pointShadowSize_;
    }

    bool operator!=(const SceneProcessorSettings& rhs) const { return !(*this == rhs); }
    /// @}
};

enum class ToneMappingMode
{
    None,
    Reinhard,
    ReinhardWhite,
    Uncharted2,
};

struct ToneMappingPassSettings
{
    ToneMappingMode mode_{};
    bool autoExposure_{};
    float minExposure_{ 1.0f };
    float maxExposure_{ 3.0f };
    float adaptRate_{ 0.6f };

    /// Utility operators
    /// @{
    bool operator==(const ToneMappingPassSettings& rhs) const
    {
        return mode_ == rhs.mode_
            && autoExposure_ == rhs.autoExposure_
            && minExposure_ == rhs.minExposure_
            && maxExposure_ == rhs.maxExposure_
            && adaptRate_ == rhs.adaptRate_;
    }

    bool operator!=(const ToneMappingPassSettings& rhs) const { return !(*this == rhs); }
    /// @}
};

}
