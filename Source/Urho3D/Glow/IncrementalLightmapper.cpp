//
// Copyright (c) 2008-2019 the Urho3D project.
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

/// \file

#include "../Glow/IncrementalLightmapper.h"

#include "../Glow/EmbreeScene.h"
#include "../Glow/LightmapCharter.h"
#include "../Glow/LightmapGeometryBaker.h"
#include "../Glow/LightmapStitcher.h"
#include "../Glow/LightmapTracer.h"
#include "../Graphics/LightProbeGroup.h"
#include "../Graphics/Model.h"
#include "../Resource/Image.h"

#include <EASTL/algorithm.h>
#include <EASTL/numeric.h>
#include <EASTL/sort.h>

namespace Urho3D
{

namespace
{

/// Per-component min for 3D integer vector.
IntVector3 MinIntVector3(const IntVector3& lhs, const IntVector3& rhs) { return VectorMin(lhs, rhs); }

/// Swizzle components of 3D integer vector.
unsigned long long Swizzle(const IntVector3& vec, const IntVector3& base = IntVector3::ZERO)
{
    static const unsigned numComponents = 3;
    static const unsigned resultBits = 8 * sizeof(unsigned long long);
    static const unsigned maxBitsPerComponent = resultBits / numComponents;

    const unsigned xyz[numComponents] = {
        static_cast<unsigned>(vec.x_ - base.x_),
        static_cast<unsigned>(vec.y_ - base.y_),
        static_cast<unsigned>(vec.z_ - base.z_),
    };

    unsigned result = 0;
    for (unsigned j = 0; j < numComponents; ++j)
    {
        for (unsigned i = 0; i < maxBitsPerComponent; ++i)
        {
            const unsigned long long bit = !!(xyz[j] & (1 << i));
            result |= bit << i * numComponents + j;
        }
    }

    return result;
}

/// Base context for incremental lightmap lightmapping.
struct BaseIncrementalContext
{
    /// Current chunk.
    unsigned currentChunkIndex_{};
};

/// Context used for incremental lightmap chunk processing (first pass).
struct LocalChunkProcessingContext : public BaseIncrementalContext
{
    /// Current lightmap chart base index.
    unsigned lightmapChartBaseIndex_{};
};

/// Context used for incremental lightmap chunk processing (second pass).
struct AdjacentChartProcessingContext : public BaseIncrementalContext
{
};

/// Context used for direct light baking.
struct DirectLightBakingContext : public BaseIncrementalContext
{
};

/// Context used for indirect light baking.
struct IndirectLightBakingFilterAndSaveContext : public BaseIncrementalContext
{
    /// Stitching context for 4-component textures.
    LightmapStitchingContext stitchingContext4_;
};

}

/// Incremental lightmapper implementation.
struct IncrementalLightmapper::Impl
{
    /// Construct.
    Impl(const LightmapSettings& lightmapSettings, const IncrementalLightmapperSettings& incrementalSettings,
        Scene* scene, LightmapSceneCollector* collector, LightmapCache* cache)
        : context_(scene->GetContext())
        , lightmapSettings_(lightmapSettings)
        , incrementalSettings_(incrementalSettings)
        , scene_(scene)
        , collector_(collector)
        , cache_(cache)
    {
        // Fix settings
        if (!incrementalSettings_.outputDirectory_.ends_with("/"))
            incrementalSettings_.outputDirectory_ += "/";

        // Collect chunks
        collector_->LockScene(scene_, incrementalSettings_.chunkSize_);
        chunks_ = collector_->GetChunks();

        // Sort chunks
        if (!chunks_.empty())
        {
            const IntVector3 baseChunkIndex = ea::accumulate(chunks_.begin(), chunks_.end(), chunks_.front(), MinIntVector3);
            const auto compareSwizzled = [&](const IntVector3& lhs, const IntVector3& rhs)
            {
                return Swizzle(lhs, baseChunkIndex) < Swizzle(rhs, baseChunkIndex);
            };
            ea::sort(chunks_.begin(), chunks_.end(), compareSwizzled);
        }
    }

    /// Step chunk processing. Chunks are processed individually. Return true when completed.
    bool StepLocalChunkProcessing(LocalChunkProcessingContext& ctx)
    {
        if (ctx.currentChunkIndex_ >= chunks_.size())
        {
            numLightmapCharts_ = ctx.lightmapChartBaseIndex_;
            return true;
        }

        // Collect nodes for current chunks
        const IntVector3 chunk = chunks_[ctx.currentChunkIndex_];
        ea::vector<Node*> nodes = collector_->GetUniqueNodes(chunk);

        // Generate charts
        const LightmapChartVector charts = GenerateLightmapCharts(nodes, lightmapSettings_.charting_, ctx.lightmapChartBaseIndex_);

        // Apply charts to scene
        ApplyLightmapCharts(charts);

        // Generate scenes for geometry baking
        const ea::vector<LightmapGeometryBakingScene> geometryBakingScene =
            GenerateLightmapGeometryBakingScenes(context_, charts, lightmapSettings_.geometryBaking_);

        // Bake geometries
        LightmapChartGeometryBufferVector geometryBuffers = BakeLightmapGeometryBuffers(geometryBakingScene);

        // Store result in the cache
        ea::vector<unsigned> lightmapsInChunk;
        for (LightmapChartGeometryBuffer& geometryBuffer : geometryBuffers)
        {
            lightmapsInChunk.push_back(geometryBuffer.index_);
            cache_->StoreGeometryBuffer(geometryBuffer.index_, ea::move(geometryBuffer));
        }

        cache_->StoreLightmapsForChunk(chunk, ea::move(lightmapsInChunk));

        // Advance
        ctx.lightmapChartBaseIndex_ += charts.size();
        ++ctx.currentChunkIndex_;
        return false;
    }

    /// Reference lightmaps by the scene.
    void ReferenceLightmapsByScene()
    {
        for (unsigned i = 0; i < numLightmapCharts_; ++i)
            scene_->AddLightmap(GetLightmapFileName(i));
    }

    /// Step chunk processing. Chunks are processed with adjacent context. Return true when completed.
    bool StepAdjacentChunkProcessing(AdjacentChartProcessingContext& ctx)
    {
        if (ctx.currentChunkIndex_ >= chunks_.size())
            return true;

        // Collect nodes around current chunk
        // TODO: Use separate volumes for direct and indirect light
        const IntVector3 chunk = chunks_[ctx.currentChunkIndex_];
        BoundingBox boundingBox = collector_->GetChunkBoundingBox(chunk);
        boundingBox.min_ -= Vector3::ONE * incrementalSettings_.raytracingScenePadding_;
        boundingBox.min_ += Vector3::ONE * incrementalSettings_.raytracingScenePadding_;

        const ea::vector<Node*> nodesInVolume = collector_->GetNodesInBoundingBox(chunk, boundingBox);
        const unsigned uvChannel = lightmapSettings_.geometryBaking_.uvChannel_;
        const SharedPtr<EmbreeScene> embreeScene = CreateEmbreeScene(context_, nodesInVolume, uvChannel);

        // Collect lights
        ea::vector<BakedDirectLight> bakedLights;
        for (Node* node : nodesInVolume)
        {
            if (auto light = node->GetComponent<Light>())
            {
                BakedDirectLight bakedLight;
                bakedLight.lightType_ = light->GetLightType();
                bakedLight.lightMode_ = light->GetLightMode();
                bakedLight.lightColor_ = light->GetEffectiveColor();
                bakedLight.position_ = node->GetWorldPosition();
                bakedLight.rotation_ = node->GetWorldRotation();
                bakedLight.direction_ = node->GetWorldDirection();
                bakedLights.push_back(bakedLight);
            }
        }

        // Store result in the cache
        cache_->StoreChunkVicinity(chunk, { embreeScene, bakedLights });

        // Advance
        ++ctx.currentChunkIndex_;
        return false;
    }

    /// Step baking direct lighting.
    bool StepBakeDirect(DirectLightBakingContext& ctx)
    {
        if (ctx.currentChunkIndex_ >= chunks_.size())
            return true;

        // Load chunk
        const IntVector3 chunk = chunks_[ctx.currentChunkIndex_];
        const LightmapChunkVicinity* chunkVicinity = cache_->LoadChunkVicinity(chunk);
        const ea::vector<unsigned> lightmapsInChunks = cache_->LoadLightmapsForChunk(chunk);

        // Bake direct lighting
        for (unsigned lightmapIndex : lightmapsInChunks)
        {
            const LightmapChartGeometryBuffer* geometryBuffer = cache_->LoadGeometryBuffer(lightmapIndex);
            LightmapChartBakedDirect bakedDirect{ geometryBuffer->width_, geometryBuffer->height_ };

            // Bake direct lights
            for (const BakedDirectLight& bakedLight : chunkVicinity->bakedLights_)
            {
                const bool bakeDirect = bakedLight.lightMode_ == LM_BAKED;
                const bool bakeIndirect = true;
                if (bakedLight.lightType_ == LIGHT_DIRECTIONAL)
                {
                    DirectionalLightParameters light;
                    light.direction_ = bakedLight.direction_;
                    light.color_ = bakedLight.lightColor_;
                    light.bakeDirect_ = bakeDirect;
                    light.bakeIndirect_ = bakeIndirect;

                    BakeDirectionalLight(bakedDirect, *geometryBuffer, *chunkVicinity->embreeScene_,
                        light, lightmapSettings_.tracing_);
                }
            }

            // Store direct light
            cache_->StoreDirectLight(lightmapIndex, ea::move(bakedDirect));
            cache_->ReleaseGeometryBuffer(lightmapIndex);
        }

        // Release cache
        cache_->ReleaseChunkVicinity(chunk);

        // Advance
        ++ctx.currentChunkIndex_;
        return false;
    }

    /// Step baking indirect lighting, filter and save images.
    bool StepBakeIndirectFilterAndSave(IndirectLightBakingFilterAndSaveContext& ctx)
    {
        if (ctx.currentChunkIndex_ >= chunks_.size())
            return true;

        // Initialize context
        if (ctx.currentChunkIndex_ == 0)
        {
            ctx.stitchingContext4_ = InitializeStitchingContext(context_, lightmapSettings_.charting_.chartSize_, 4);
        }

        // Load chunk
        const IntVector3 chunk = chunks_[ctx.currentChunkIndex_];
        const LightmapChunkVicinity* chunkVicinity = cache_->LoadChunkVicinity(chunk);
        const ea::vector<unsigned> lightmapsInChunks = cache_->LoadLightmapsForChunk(chunk);

        // Collect required direct lightmaps
        ea::hash_set<unsigned> requiredDirectLightmaps;
        for (const EmbreeGeometry& embreeGeometry : chunkVicinity->embreeScene_->GetEmbreeGeometryIndex())
        {
            if (embreeGeometry.node_)
                requiredDirectLightmaps.insert(embreeGeometry.lightmapIndex_);
        }

        ea::vector<const LightmapChartBakedDirect*> bakedDirectLightmaps(numLightmapCharts_);
        for (unsigned lightmapIndex : requiredDirectLightmaps)
            bakedDirectLightmaps[lightmapIndex] = cache_->LoadDirectLight(lightmapIndex);

        // Bake direct & indirect light probes
        // TODO(glow): Use chunks here
        LightProbeCollection lightProbes;
        LightProbeGroup::CollectLightProbes(scene_, lightProbes);
        lightProbes.ResetBakedData();
        BakeIndirectLightForLightProbes(lightProbes, bakedDirectLightmaps, *chunkVicinity->embreeScene_, lightmapSettings_.tracing_);
        LightProbeGroup::CommitLightProbes(lightProbes);

        // Bake indirect lighting for charts
        for (unsigned lightmapIndex : lightmapsInChunks)
        {
            const LightmapChartGeometryBuffer* geometryBuffer = cache_->LoadGeometryBuffer(lightmapIndex);
            LightmapChartBakedDirect* bakedDirect = cache_->LoadDirectLight(lightmapIndex);
            LightmapChartBakedIndirect bakedIndirect{ geometryBuffer->width_, geometryBuffer->height_ };

            // Bake indirect lights
            BakeIndirectLightForCharts(bakedIndirect, bakedDirectLightmaps,
                *geometryBuffer, *chunkVicinity->embreeScene_, lightmapSettings_.tracing_);

            // Filter indirect
            bakedIndirect.NormalizeLight();
            FilterIndirectLight(bakedIndirect, *geometryBuffer, { 5, 1, 10.0f, 4.0f, 1.0f }, lightmapSettings_.tracing_.numThreads_);

            // Stitch seams
            if (lightmapSettings_.stitching_.numIterations_ > 0 && !geometryBuffer->seams_.empty())
            {
                SharedPtr<Model> seamsModel = CreateSeamsModel(context_, geometryBuffer->seams_);
                StitchLightmapSeams(ctx.stitchingContext4_, bakedIndirect.light_,
                    lightmapSettings_.stitching_, seamsModel);
            }

            // Generate image
            auto lightmapImage = MakeShared<Image>(context_);
            lightmapImage->SetSize(geometryBuffer->width_, geometryBuffer->height_, 4);
            for (int y = 0; y < geometryBuffer->height_; ++y)
            {
                for (int x = 0; x < geometryBuffer->width_; ++x)
                {
                    const unsigned i = y * geometryBuffer->width_ + x;
                    const Vector3 directLight = static_cast<Vector3>(bakedDirect->directLight_[i]);
                    const Vector3 indirectLight = static_cast<Vector3>(bakedIndirect.light_[i]);
                    const Vector3 totalLight = directLight + indirectLight;

                    Color color;
                    color.r_ = Pow(totalLight.x_, 1 / 2.2f);
                    color.g_ = Pow(totalLight.y_, 1 / 2.2f);
                    color.b_ = Pow(totalLight.z_, 1 / 2.2f);
                    color.a_ = 1.0f;
                    lightmapImage->SetPixel(x, y, color);
                }
            }

            // Save image to destination folder
            lightmapImage->SaveFile(incrementalSettings_.outputDirectory_ + GetLightmapFileName(lightmapIndex));

            // Store direct light
            cache_->ReleaseGeometryBuffer(lightmapIndex);
            cache_->ReleaseDirectLight(lightmapIndex);
        }

        // Release cache
        cache_->ReleaseChunkVicinity(chunk);
        for (unsigned lightmapIndex : requiredDirectLightmaps)
            cache_->ReleaseDirectLight(lightmapIndex);

        // Advance
        ++ctx.currentChunkIndex_;
        return false;
    }

private:
    /// Return lightmap file name.
    ea::string GetLightmapFileName(unsigned lightmapIndex)
    {
        ea::string lightmapName;
        lightmapName += incrementalSettings_.lightmapNamePrefix_;
        lightmapName += ea::to_string(lightmapIndex);
        lightmapName += incrementalSettings_.lightmapNameSuffix_;
        return lightmapName;
    }

    /// Settings for lightmap generation.
    LightmapSettings lightmapSettings_;
    /// Settings for incremental lightmapper itself.
    IncrementalLightmapperSettings incrementalSettings_;

    /// Context.
    Context* context_{};
    /// Scene.
    Scene* scene_{};
    /// Scene collector.
    LightmapSceneCollector* collector_{};
    /// Lightmap cache.
    LightmapCache* cache_{};
    /// List of all chunks.
    ea::vector<IntVector3> chunks_;
    /// Number of lightmap charts.
    unsigned numLightmapCharts_{};
};

IncrementalLightmapper::~IncrementalLightmapper()
{
}

void IncrementalLightmapper::Initialize(const LightmapSettings& lightmapSettings,
    const IncrementalLightmapperSettings& incrementalSettings,
    Scene* scene, LightmapSceneCollector* collector, LightmapCache* cache)
{
    impl_ = ea::make_unique<Impl>(lightmapSettings, incrementalSettings, scene, collector, cache);
}

void IncrementalLightmapper::ProcessScene()
{
    // Generate charts
    LocalChunkProcessingContext chartingContext;
    while (!impl_->StepLocalChunkProcessing(chartingContext))
        ;

    // Reference generated charts by the scene
    impl_->ReferenceLightmapsByScene();

    // Generate baking geometry
    AdjacentChartProcessingContext geometryBakingContext;
    while (!impl_->StepAdjacentChunkProcessing(geometryBakingContext))
        ;
}

void IncrementalLightmapper::Bake()
{
    // Bake direct lighting
    DirectLightBakingContext directContext;
    while (!impl_->StepBakeDirect(directContext))
        ;

    // Bake indirect lighting, filter and save images
    IndirectLightBakingFilterAndSaveContext indirectContext;
    while (!impl_->StepBakeIndirectFilterAndSave(indirectContext))
        ;
}

}
