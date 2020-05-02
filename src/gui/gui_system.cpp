﻿#include "gui_system.h"
#include "engine/crc32.h"
#include "engine/delegate.h"
#include "engine/engine.h"
#include "engine/allocator.h"
#include "engine/input_system.h"
#include "engine/lua_wrapper.h"
#include "engine/math.h"
#include "engine/path.h"
#include "engine/reflection.h"
#include "engine/resource_manager.h"
#include "engine/universe.h"
#include "gui/gui_scene.h"
#include "gui/sprite.h"
#include "renderer/font.h"
#include "renderer/material.h"
#include "renderer/pipeline.h"
#include "renderer/renderer.h"
#include "renderer/render_scene.h"
#include "renderer/shader.h"
#include "renderer/texture.h"


namespace Lumix
{


struct GUISystemImpl;


struct SpriteManager final : ResourceManager
{
	SpriteManager(IAllocator& allocator)
		: ResourceManager(allocator)
		, m_allocator(allocator)
	{
	}

	Resource* createResource(const Path& path) override {
		return LUMIX_NEW(m_allocator, Sprite)(path, *this, m_allocator);
	}

	void destroyResource(Resource& resource) override {
		LUMIX_DELETE(m_allocator, static_cast<Sprite*>(&resource));
	}

	IAllocator& m_allocator;
};


struct GUISystemImpl final : GUISystem
{
	static const char* getTextHAlignName(int index)
	{
		switch ((GUIScene::TextHAlign) index)
		{
		case GUIScene::TextHAlign::LEFT: return "left";
		case GUIScene::TextHAlign::RIGHT: return "right";
		case GUIScene::TextHAlign::CENTER: return "center";
		default: ASSERT(false); return "Unknown";
		}
	}


	explicit GUISystemImpl(Engine& engine)
		: m_engine(engine)
		, m_interface(nullptr)
		, m_sprite_manager(engine.getAllocator())
	{
		registerLuaAPI();

		using namespace Reflection;

		struct TextHAlignEnum : Reflection::EnumAttribute {
			u32 count(ComponentUID cmp) const override { return 3; }
			const char* name(ComponentUID cmp, u32 idx) const override {
				switch((GUIScene::TextHAlign)idx) {
					case GUIScene::TextHAlign::LEFT: return "Left";
					case GUIScene::TextHAlign::RIGHT: return "Right";
					case GUIScene::TextHAlign::CENTER: return "Center";
					default: ASSERT(false); return "N/A";
				}
			}
		};

		struct TextVAlignEnum : Reflection::EnumAttribute {
			u32 count(ComponentUID cmp) const override { return 3; }
			const char* name(ComponentUID cmp, u32 idx) const override {
				switch((GUIScene::TextVAlign)idx) {
					case GUIScene::TextVAlign::TOP: return "Top";
					case GUIScene::TextVAlign::MIDDLE: return "Middle";
					case GUIScene::TextVAlign::BOTTOM: return "Bottom";
					default: ASSERT(false); return "N/A";
				}
			}
		};
		
		struct CursorEnum : Reflection::EnumAttribute {
			u32 count(ComponentUID cmp) const override { return 7; }
			const char* name(ComponentUID cmp, u32 idx) const override {
				switch((OS::CursorType)idx) {
					case OS::CursorType::UNDEFINED: return "Ignore";
					case OS::CursorType::DEFAULT: return "Default";
					case OS::CursorType::LOAD: return "Load";
					case OS::CursorType::SIZE_NS: return "Size NS";
					case OS::CursorType::SIZE_NWSE: return "Size NWSE";
					case OS::CursorType::SIZE_WE: return "Size WE";
					case OS::CursorType::TEXT_INPUT: return "Text input";
					default: ASSERT(false); return "N/A";
				}
			}
		};

		static auto lua_scene = scene("gui",
			functions(
				LUMIX_FUNC(GUIScene::getRectAt),
				LUMIX_FUNC(GUIScene::isOver)
			),
			component("gui_text",
				property("Text", LUMIX_PROP(GUIScene, Text), MultilineAttribute()),
				property("Font", LUMIX_PROP(GUIScene, TextFontPath), ResourceAttribute("Font (*.ttf)", FontResource::TYPE)),
				property("Font Size", LUMIX_PROP(GUIScene, TextFontSize)),
				enum_property("Horizontal align", LUMIX_PROP(GUIScene, TextHAlign), TextHAlignEnum()),
				enum_property("Vertical align", LUMIX_PROP(GUIScene, TextVAlign), TextVAlignEnum()),
				property("Color", LUMIX_PROP(GUIScene, TextColorRGBA), ColorAttribute())
			),
			component("gui_input_field"),
			component("gui_canvas"),
			component("gui_button",
				property("Hovered color", LUMIX_PROP(GUIScene, ButtonHoveredColorRGBA), ColorAttribute()),
				enum_property("Cursor", LUMIX_PROP(GUIScene, ButtonHoveredCursor), CursorEnum())
			),
			component("gui_image",
				property("Enabled", &GUIScene::isImageEnabled, &GUIScene::enableImage),
				property("Color", LUMIX_PROP(GUIScene, ImageColorRGBA), ColorAttribute()),
				property("Sprite", LUMIX_PROP(GUIScene, ImageSprite), ResourceAttribute("Sprite (*.spr)", Sprite::TYPE))
			),
			component("gui_rect",
				property("Enabled", &GUIScene::isRectEnabled, &GUIScene::enableRect),
				property("Clip content", LUMIX_PROP(GUIScene, RectClip)),
				property("Top Points", LUMIX_PROP(GUIScene, RectTopPoints)),
				property("Top Relative", LUMIX_PROP(GUIScene, RectTopRelative)),
				property("Right Points", LUMIX_PROP(GUIScene, RectRightPoints)),
				property("Right Relative", LUMIX_PROP(GUIScene, RectRightRelative)),
				property("Bottom Points", LUMIX_PROP(GUIScene, RectBottomPoints)),
				property("Bottom Relative", LUMIX_PROP(GUIScene, RectBottomRelative)),
				property("Left Points", LUMIX_PROP(GUIScene, RectLeftPoints)),
				property("Left Relative", LUMIX_PROP(GUIScene, RectLeftRelative))
			)
		);
		registerScene(lua_scene);
		m_sprite_manager.create(Sprite::TYPE, m_engine.getResourceManager());
	}


	~GUISystemImpl()
	{
		m_sprite_manager.destroy();
	}


	Engine& getEngine() override { return m_engine; }


	void createScenes(Universe& universe) override
	{
		IAllocator& allocator = m_engine.getAllocator();
		auto* scene = GUIScene::createInstance(*this, universe, allocator);
		universe.addScene(scene);
	}


	void destroyScene(IScene* scene) override
	{
		GUIScene::destroyInstance(static_cast<GUIScene*>(scene));
	}

	static int LUA_enableCursor(lua_State* L) {
		const bool enable = LuaWrapper::checkArg<bool>(L, 1);
		const int index = lua_upvalueindex(1);
		GUISystemImpl* system = LuaWrapper::toType<GUISystemImpl*>(L, index);
		system->enableCursor(enable);
		return 0;
	}

	static int LUA_GUIRect_getScreenRect(lua_State* L)
	{
		EntityRef e;
		Universe* universe;
		if (!LuaWrapper::toEntity(L, 1, Ref(universe), Ref(e))) return 0;
		GUIScene* scene = (GUIScene*)universe->getScene(crc32("gui"));
		GUIScene::Rect rect = scene->getRect(e);
		lua_newtable(L);
		LuaWrapper::push(L, rect.x);
		lua_setfield(L, -2, "x");
		LuaWrapper::push(L, rect.y);
		lua_setfield(L, -2, "y");
		LuaWrapper::push(L, rect.w);
		lua_setfield(L, -2, "w");
		LuaWrapper::push(L, rect.h);
		lua_setfield(L, -2, "h");
		return 1;
	}


	void registerLuaAPI()
	{
		lua_State* L = m_engine.getState();
		
		#define REGISTER_FUNCTION(name) \
			do {\
				auto f = &LuaWrapper::wrapMethod<&GUISystemImpl::name>; \
				LuaWrapper::createSystemFunction(L, "Gui", #name, f); \
			} while(false) \

		REGISTER_FUNCTION(enableCursor);

		LuaWrapper::createSystemFunction(L, "Gui", "getScreenRect", LUA_GUIRect_getScreenRect);
		LuaWrapper::createSystemClosure(L, "Gui", this, "enableCursor", LUA_enableCursor);

		LuaWrapper::createSystemVariable(L, "Gui", "instance", this);

		#undef REGISTER_FUNCTION
	}

	void setCursor(OS::CursorType type) override {
		if (m_interface) m_interface->setCursor(type);
	}

	void enableCursor(bool enable) override {
		if (m_interface) m_interface->enableCursor(enable);
	}


	void setInterface(Interface* interface) override
	{
		m_interface = interface;
		
		if (!m_interface) return;

		auto* pipeline = m_interface->getPipeline();
		pipeline->addCustomCommandHandler("renderIngameGUI")
			.callback.bind<&GUISystemImpl::pipelineCallback>(this);
	}


	void pipelineCallback()
	{
		if (!m_interface) return;

		Pipeline* pipeline = m_interface->getPipeline();
		auto* scene = (GUIScene*)pipeline->getScene()->getUniverse().getScene(crc32("gui"));
		Vec2 size = m_interface->getSize();
		scene->render(*pipeline, size, true);
	}


	void stopGame() override
	{
		Pipeline* pipeline = m_interface->getPipeline();
		pipeline->clearDraw2D();
	}


	const char* getName() const override { return "gui"; }

	u32 getVersion() const override { return 0; }
	void serialize(OutputMemoryStream& stream) const override {}
	bool deserialize(u32 version, InputMemoryStream& stream) override { return version == 0; }

	Engine& m_engine;
	SpriteManager m_sprite_manager;
	Interface* m_interface;
};


LUMIX_PLUGIN_ENTRY(gui)
{
	return LUMIX_NEW(engine.getAllocator(), GUISystemImpl)(engine);
}


} // namespace Lumix
