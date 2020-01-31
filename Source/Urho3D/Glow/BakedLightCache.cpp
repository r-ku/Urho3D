//
// Copyright (c) 2019-2020 the rbfx project.
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

#include "../Glow/BakedLightCache.h"

namespace Urho3D
{

BakedLightCache::~BakedLightCache() = default;

void BakedLightMemoryCache::StoreBakedChunk(const IntVector3& chunk, BakedSceneChunk bakedChunk)
{
    bakedChunkCache_[chunk] = ea::make_shared<BakedSceneChunk>(ea::move(bakedChunk));
}

ea::shared_ptr<const BakedSceneChunk> BakedLightMemoryCache::LoadBakedChunk(const IntVector3& chunk)
{
    auto iter = bakedChunkCache_.find(chunk);
    return iter != bakedChunkCache_.end() ? iter->second : nullptr;
}

void BakedLightMemoryCache::StoreDirectLight(unsigned lightmapIndex, LightmapChartBakedDirect bakedDirect)
{
    directLightCache_[lightmapIndex] = ea::make_shared<LightmapChartBakedDirect>(ea::move(bakedDirect));
}

ea::shared_ptr<const LightmapChartBakedDirect> BakedLightMemoryCache::LoadDirectLight(unsigned lightmapIndex)
{
    auto iter = directLightCache_.find(lightmapIndex);
    return iter != directLightCache_.end() ? iter->second : nullptr;
}

void BakedLightMemoryCache::StoreLightmap(unsigned lightmapIndex, BakedLightmap bakedLightmap)
{
    lightmapCache_[lightmapIndex] = ea::make_shared<BakedLightmap>(ea::move(bakedLightmap));
}

ea::shared_ptr<const BakedLightmap> BakedLightMemoryCache::LoadLightmap(unsigned lightmapIndex)
{
    auto iter = lightmapCache_.find(lightmapIndex);
    return iter != lightmapCache_.end() ? iter->second : nullptr;
}

}
