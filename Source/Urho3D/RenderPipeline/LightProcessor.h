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

#include "../Core/NonCopyable.h"
#include "../Graphics/Light.h"
#include "../Graphics/Camera.h"
#include "../Graphics/PipelineStateTracker.h"
#include "../RenderPipeline/ShadowMapAllocator.h"
#include "../RenderPipeline/ShadowSplitProcessor.h"
#include "../Scene/Node.h"

#include <EASTL/array.h>
#include <EASTL/vector.h>

namespace Urho3D
{

class DrawableProcessor;
class LightProcessor;

/// Light processor callback.
class LightProcessorCallback
{
public:
    /// Return whether light needs shadow.
    virtual bool IsLightShadowed(Light* light) = 0;
    /// Allocate shadow map for one frame.
    virtual ShadowMap AllocateTransientShadowMap(const IntVector2& size) = 0;
};

/// Cooked shadow parameters of light.
struct LightShaderParameters
{
    /// Light direction.
    Vector3 direction_;
    /// Light position.
    Vector3 position_;
    /// Inverse range.
    float invRange_{};

    /// Number of light matrices.
    unsigned numLightMatrices_{};
    /// Shadow matrices for each split (for directional light).
    /// Light matrix and shadow matrix (for spot and point lights).
    Matrix4 lightMatrices_[MAX_CASCADE_SPLITS];

    /// Light color in gamma space (faded).
    Vector3 colorGamma_;
    /// Light color in linear space (faded).
    Vector3 colorLinear_;
    /// Specular intensity (faded).
    float specularIntensity_{};

    /// Light radius for volumetric lights.
    float radius_{};
    /// Light length for volumetric lights.
    float length_{};

    /// Shadow cube adjustment.
    Vector4 shadowCubeAdjust_;
    /// Shadow depth fade parameters.
    Vector4 shadowDepthFade_;
    /// Shadow intensity parameters.
    Vector4 shadowIntensity_;
    /// Inverse size of shadowmap.
    Vector2 shadowMapInvSize_;
    /// Bias multiplier applied to UV to avoid seams.
    Vector2 shadowCubeUVBias_;
    /// Shadow splits distances.
    Vector4 shadowSplits_;

    /// Cutoff for vertex lighting.
    float cutoff_{};
    /// Inverse cutoff for vertex lighting.
    float invCutoff_{};

    ea::array<float, MAX_LIGHT_SPLITS> shadowNormalBias_;
    ea::array<float, MAX_LIGHT_SPLITS> shadowDepthBiasMultiplier_{};

    /// Shadow map texture.
    Texture2D* shadowMap_{};
    /// Light ramp texture.
    Texture* lightRamp_{};
    /// Light shape texture.
    Texture* lightShape_{};

    /// Return light color in appropriate color space.
    Vector3 GetColor(bool isLinear) const { return isLinear ? colorLinear_ : colorGamma_; }
};

/// Light and shadow processing utility.
class URHO3D_API LightProcessor : public NonCopyable
{
public:
    /// Number of frames for shadow splits expiration.
    static const unsigned NumSplitFramesToLive = 600;

    /// Construct.
    explicit LightProcessor(Light* light);
    /// Destruct.
    ~LightProcessor();

    /// Begin update from main thread.
    void BeginUpdate(DrawableProcessor* drawableProcessor, LightProcessorCallback* callback);
    /// Update light in worker thread.
    void Update(DrawableProcessor* drawableProcessor);
    /// End update from main thread.
    void EndUpdate(DrawableProcessor* drawableProcessor, LightProcessorCallback* callback);

    /// Return hash for forward light.
    unsigned GetForwardLitHash() const { return forwardHash_; }
    /// Return hash for shadow batches.
    unsigned GetShadowHash(unsigned splitIndex) const { return shadowBatchStateHashes_[splitIndex]; }
    /// Return hash for light volumes batches.
    unsigned GetLightVolumeHash() const { return lightVolumeHash_; }

    /// Return light.
    Light* GetLight() const { return light_; }
    /// Return lit geometries.
    const ea::vector<Drawable*>& GetLitGeometries() const { return litGeometries_; }
    /// Return whether light has lit geometries (regardless of exact mode).
    bool HasLitGeometries() const { return hasLitGeometries_; }
    /// Return whether light has geometries lit with forward rendering.
    bool HasForwardLitGeometries() const { return hasForwardLitGeometries_; }

    /// Return whether overlaps camera.
    bool DoesOverlapCamera() const { return overlapsCamera_; }
    /// Return whether the light actually has shadow.
    bool HasShadow() const { return numActiveSplits_ != 0; }
    /// Return shadow map size.
    IntVector2 GetShadowMapSize() const { return numActiveSplits_ != 0 ? shadowMapSize_ : IntVector2::ZERO; }
    /// Return shadow map.
    ShadowMap GetShadowMap() const { return shadowMap_; }

    /// Return number of active splits.
    unsigned GetNumSplits() const { return numActiveSplits_; }
    /// Return shadow split.
    const ShadowSplitProcessor* GetSplit(unsigned splitIndex) const { return &splits_[splitIndex]; }
    /// Return mutable shadow split.
    ShadowSplitProcessor* GetMutableSplit(unsigned splitIndex) { return &splits_[splitIndex]; }
    /// Return active shadow splits.
    ea::span<const ShadowSplitProcessor> GetSplits() const { return { splits_.data(), numActiveSplits_ }; }

    /// Return shader parameters.
    const LightShaderParameters& GetShaderParams() const { return shaderParams_; }

private:
    /// Initialize shadow splits.
    void InitializeShadowSplits(DrawableProcessor* drawableProcessor);
    /// Update hashes.
    void UpdateHashes();
    /// Cook shader parameters for light.
    void CookShaderParameters(Camera* cullCamera, float subPixelOffset);
    /// Return dimensions of splits grid in shadow map.
    IntVector2 GetSplitsGridSize() const;

    /// Light.
    Light* light_{};
    /// Whether the camera is inside light volume.
    bool overlapsCamera_{};
    /// Light hash for forward rendering.
    unsigned forwardHash_{};
    /// Light hash for deferred light volume rendering.
    unsigned lightVolumeHash_{};
    ea::array<unsigned, MAX_LIGHT_SPLITS> shadowBatchStateHashes_{};

    /// Splits.
    ea::vector<ShadowSplitProcessor> splits_;

    /// Whether the shadow is requested.
    bool isShadowRequested_{};
    /// Number of shadow splits requested.
    unsigned numSplitsRequested_{};
    /// Split expiration timer.
    unsigned splitTimeToLive_{};

    /// Number of active splits.
    unsigned numActiveSplits_{};

    /// Shadow map split size.
    int shadowMapSplitSize_{};
    /// Shadow map size.
    IntVector2 shadowMapSize_{};

    /// Whether light has lit geometries.
    bool hasLitGeometries_{};
    /// Whether light has forward lit geometries.
    bool hasForwardLitGeometries_{};
    /// Lit geometries.
    /// Point and spot lights: only forward lit geometries.
    /// Directional lights: all lit geometries, for shadow focusing.
    ea::vector<Drawable*> litGeometries_;
    /// Shadow caster candidates.
    /// Point and spot lights: all possible shadow casters.
    /// Directional lights: temporary buffer for split queries.
    ea::vector<Drawable*> shadowCasterCandidates_;

    /// Shadow map allocated to this light.
    ShadowMap shadowMap_;
    /// Shader parameters.
    LightShaderParameters shaderParams_;
};

/// Cache of light processors.
// TODO(renderer): Add automatic expiration by time
class URHO3D_API LightProcessorCache : public NonCopyable
{
public:
    /// Construct.
    LightProcessorCache();
    /// Destruct.
    ~LightProcessorCache();
    /// Get existing or create new light processor. Lightweight. Not thread safe.
    LightProcessor* GetLightProcessor(Light* light);

private:
    /// Weak cache.
    ea::unordered_map<WeakPtr<Light>, ea::unique_ptr<LightProcessor>> cache_;
};

}
