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

class DrawCommandQueue;
class PipelineState;
class RenderSurface;
class Texture2D;
class Viewport;
struct BatchStateCreateKey;
struct BatchStateCreateContext;

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

/// Pipeline state cache callback used to create actual pipeline state.
class BatchStateCacheCallback
{
public:
    /// Create pipeline state given context and key.
    /// Only attributes that constribute to pipeline state hashes are safe to use.
    virtual SharedPtr<PipelineState> CreateBatchPipelineState(
        const BatchStateCreateKey& key, const BatchStateCreateContext& ctx) = 0;
};

/// Base interface of render pipeline required by Render Pipeline classes.
class URHO3D_API RenderPipelineInterface
    : public Serializable
    , public BatchStateCacheCallback
{
    URHO3D_OBJECT(RenderPipelineInterface, Serializable);

public:
    using Serializable::Serializable;

    /// Return default draw queue that can be reused.
    virtual DrawCommandQueue* GetDefaultDrawQueue() = 0;

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

struct DrawableProcessorSettings
{
    MaterialQuality materialQuality_{ QUALITY_HIGH };
    // TODO(renderer): Make maxVertexLights_ contribute to pipeline state
    unsigned maxVertexLights_{ 4 };
    unsigned maxPixelLights_{ 4 };

    /// Utility operators
    /// @{
    unsigned CalculatePipelineStateHash() const
    {
        return 0;
    }

    bool operator==(const DrawableProcessorSettings& rhs) const
    {
        return materialQuality_ == rhs.materialQuality_
            && maxVertexLights_ == rhs.maxVertexLights_
            && maxPixelLights_ == rhs.maxPixelLights_;
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
    bool gammaCorrection_{};
    DrawableAmbientMode ambientMode_{ DrawableAmbientMode::Directional };
    Vector2 varianceShadowMapParams_{ 0.0000001f, 0.9f };

    /// Utility operators
    /// @{
    unsigned CalculatePipelineStateHash() const
    {
        unsigned hash = 0;
        CombineHash(hash, gammaCorrection_);
        CombineHash(hash, MakeHash(ambientMode_));
        return hash;
    }

    bool operator==(const BatchRendererSettings& rhs) const
    {
        return gammaCorrection_ == rhs.gammaCorrection_
            && ambientMode_ == rhs.ambientMode_
            && varianceShadowMapParams_ == rhs.varianceShadowMapParams_;
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

struct SceneProcessorSettings
    : public DrawableProcessorSettings
    , public OcclusionBufferSettings
    , public BatchRendererSettings
{
    bool enableShadows_{ true };
    bool deferredLighting_{ false };

    /// Utility operators
    /// @{
    unsigned CalculatePipelineStateHash() const
    {
        unsigned hash = 0;
        CombineHash(hash, DrawableProcessorSettings::CalculatePipelineStateHash());
        CombineHash(hash, OcclusionBufferSettings::CalculatePipelineStateHash());
        CombineHash(hash, BatchRendererSettings::CalculatePipelineStateHash());
        CombineHash(hash, enableShadows_);
        CombineHash(hash, deferredLighting_);
        return hash;
    }

    bool operator==(const SceneProcessorSettings& rhs) const
    {
        return DrawableProcessorSettings::operator==(rhs)
            && OcclusionBufferSettings::operator==(rhs)
            && BatchRendererSettings::operator==(rhs)
            && enableShadows_ == rhs.enableShadows_
            && deferredLighting_ == rhs.deferredLighting_;
    }

    bool operator!=(const SceneProcessorSettings& rhs) const { return !(*this == rhs); }
    /// @}
};

struct BaseRenderPipelineSettings
    : public SceneProcessorSettings
    , public ShadowMapAllocatorSettings
    , public InstancingBufferSettings
{
    /// Utility operators
    /// @{
    unsigned CalculatePipelineStateHash() const
    {
        unsigned hash = 0;
        CombineHash(hash, SceneProcessorSettings::CalculatePipelineStateHash());
        CombineHash(hash, ShadowMapAllocatorSettings::CalculatePipelineStateHash());
        CombineHash(hash, InstancingBufferSettings::CalculatePipelineStateHash());
        return hash;
    }

    bool operator==(const BaseRenderPipelineSettings& rhs) const
    {
        return SceneProcessorSettings::operator==(rhs)
            && ShadowMapAllocatorSettings::operator==(rhs)
            && InstancingBufferSettings::operator==(rhs);
    }

    bool operator!=(const BaseRenderPipelineSettings& rhs) const { return !(*this == rhs); }
    /// @}
};

}