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

#include "../Core/Object.h"
#include "../Graphics/PipelineState.h"
#include "../RenderPipeline/RenderPipelineDefs.h"

namespace Urho3D
{

class BatchCompositorPass;
class CameraProcessor;
class InstancingBuffer;
class Light;
class PipelineState;
class SceneProcessor;
class ShadowMapAllocator;

/// Utility to build pipeline states for RenderPipeline.
class URHO3D_API PipelineStateBuilder : public Object, public BatchStateCacheCallback
{
    URHO3D_OBJECT(PipelineStateBuilder, Object);

public:
    PipelineStateBuilder(Context* context, const SceneProcessor* sceneProcessor, const CameraProcessor* cameraProcessor,
        const ShadowMapAllocator* shadowMapAllocator, const InstancingBuffer* instancingBuffer);

    /// Implement BatchStateCacheCallback
    /// @{
    SharedPtr<PipelineState> CreateBatchPipelineState(
        const BatchStateCreateKey& key, const BatchStateCreateContext& ctx) override;
    /// @}
private:
    /// State builder
    /// @{
    void ClearState();

    void ApplyCommonDefines(const Pass* materialPass);
    void ApplyGeometry(Geometry* geometry, GeometryType geometryType, bool useInstancingBuffer);
    void ApplyVertexLayout(bool isShadowPass);

    void ApplyShadowPass(unsigned splitIndex, const LightProcessor* lightProcessor,
        const Material* material, const Pass* materialPass);
    void ApplyLightVolumePass(const LightProcessor* lightProcessor);
    void ApplyUserPass(const BatchCompositorPass* compositorPass, unsigned subpassIndex,
        const Material* material, const Pass* materialPass, const Drawable* drawable);

    void ApplyPixelLight(const LightProcessor* lightProcessor, const Material* material);

    void FinalizeDescription(const Pass* materialPass);
    /// @}

    /// Objects whose settings contribute to pipeline states.
    /// Pipeline states should be invalidated if any of those changes.
    /// @{
    const SceneProcessor* const sceneProcessor_{};
    const CameraProcessor* cameraProcessor_{};
    const ShadowMapAllocator* const shadowMapAllocator_{};
    const InstancingBuffer* const instancingBuffer_{};
    /// @}

    Graphics* graphics_{};
    Renderer* renderer_{};

    /// Re-used objects
    /// TODO(renderer): Consider replacing define string with tokenized define lists
    /// @{
    PipelineStateDesc desc_;
    ea::string commonDefines_;
    ea::string vertexDefines_;
    ea::string pixelDefines_;
    /// @}
};

}
