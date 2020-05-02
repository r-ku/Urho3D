#include "engine/crc32.h"
#include "engine/crt.h"
#include "prefab.h"

namespace Lumix
{


const ResourceType PrefabResource::TYPE("prefab");


PrefabResource::PrefabResource(const Path& path, ResourceManager& resource_manager, IAllocator& allocator)
	: Resource(path, resource_manager, allocator)
	, data(allocator)
{
}


ResourceType PrefabResource::getType() const { return TYPE; }


void PrefabResource::unload() { data.clear(); }


bool PrefabResource::load(u64 size, const u8* mem)
{
	data.resize((int)size);
	memcpy(data.getMutableData(), mem, size);
	content_hash = crc32(mem, (u32)size);
	return true;
}


}