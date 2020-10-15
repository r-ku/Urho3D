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

/// \file

#pragma once


#include "../Core/Object.h"
#include "../Core/Signal.h"
#include "../Graphics/Texture2D.h"
#include "../Math/Vector2.h"

#include <EASTL/vector.h>
#include <EASTL/string.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Context.h>


namespace Urho3D
{

namespace Detail { class RmlContext; class RmlPlugin; }

/// %UI subsystem. Manages the graphical user interface.
class URHO3D_API RmlUI : public Object
{
    URHO3D_OBJECT(RmlUI, Object);

public:
    /// Construct.
    explicit RmlUI(Context* context, const char* name="master");
    /// Destruct.
    ~RmlUI() override;
    /// Load a specified rml document.
    Rml::ElementDocument* LoadDocument(const ea::string& path);
    /// Show or hide RmlUi debugger.
    void SetDebuggerVisible(bool visible);
    /// Load a font resource for RmlUi to use. Set fallback parameter to true if font should be used as a fallback font for unsupported characters.
    bool LoadFont(const ea::string& resourceName, bool fallback=false);
    /// Returns RmlUi context object.
    Rml::Context* GetRmlContext() const;
    /// Set render target where this instance will render into.
    void SetRenderTarget(RenderSurface* target, const Color& clearColor=Color::TRANSPARENT_BLACK);
    /// Set render target where this instance will render into.
    void SetRenderTarget(Texture2D* target, const Color& clearColor=Color::TRANSPARENT_BLACK);
    /// Set render target where this instance will render into. A convenience overload to resolve ambiguity when clearing rendertarget.
    void SetRenderTarget(std::nullptr_t target, const Color& clearColor=Color::TRANSPARENT_BLACK);
    /// Enable or disable this UI subsystem. When disabled, no inputs will be processed and nothing will be rendered.
    void SetEnabled(bool enabled);
    /// Return true if this subsystem is rendering and accepting input.
    bool IsEnabled() const { return isEnabled_; }
    /// Return true if input is captured by UI.
    bool IsInputCaptured() const;

    /// Emitted when mouse input is detected. Should be used for translating mouse coordinates when UI is rendered on 3D objects. Takes 2D screen coordinates as input, they may be modified by subscribers.
    Signal<IntVector2> mouseMoveEvent_;

private:
    /// Returns a size that this UI screen will cover.
    IntVector2 GetDesiredCanvasSize() const;
    /// Returns true if any window of this UI context is hovered by mouse.
    bool IsHovered() const;
    /// Return true if input is captured by this instance of RmlUI.
    bool IsInputCapturedInternal() const;
    /// Signal that document belonging to this subsystem was closed.
    void OnDocumentUnload(Rml::ElementDocument* document);

    /// Handle screen mode event.
    void HandleScreenMode(StringHash eventType, VariantMap& eventData);
    /// Handle mouse button down event.
    void HandleMouseButtonDown(StringHash eventType, VariantMap& eventData);
    /// Handle mouse button up event.
    void HandleMouseButtonUp(StringHash eventType, VariantMap& eventData);
    /// Handle mouse move event.
    void HandleMouseMove(StringHash eventType, VariantMap& eventData);
    /// Handle mouse wheel event.
    void HandleMouseWheel(StringHash eventType, VariantMap& eventData);
    /// Handle touch begin event.
    void HandleTouchBegin(StringHash eventType, VariantMap& eventData);
    /// Handle touch end event.
    void HandleTouchEnd(StringHash eventType, VariantMap& eventData);
    /// Handle touch move event.
    void HandleTouchMove(StringHash eventType, VariantMap& eventData);
    /// Handle press event.
    void HandleKeyDown(StringHash eventType, VariantMap& eventData);
    /// Handle release event.
    void HandleKeyUp(StringHash eventType, VariantMap& eventData);
    /// Handle text input event.
    void HandleTextInput(StringHash eventType, VariantMap& eventData);
    /// Handle logic post-update event.
    void HandlePostUpdate(StringHash eventType, VariantMap& eventData);
    /// Handle a file being drag-dropped into the application window.
    void HandleDropFile(StringHash eventType, VariantMap& eventData);
    /// Handle rendering to a texture.
    void HandleEndAllViewsRender(StringHash eventType, VariantMap& eventData);

    /// UI context name.
    ea::string name_;
    /// RmlUi context.
    Detail::RmlContext* rmlContext_ = nullptr;
    /// Surface where UI will be rendered into.
    WeakPtr<RenderSurface> renderSurface_;
    /// Color used to clear render surface if not rendering into backbuffer.
    Color clearColor_ = Color::TRANSPARENT_BLACK;
    /// Flag indicating RmlUi debugger is already initialized.
    bool debuggerInitialized_ = false;
    /// Whether current subsystem is rendering or not.
    bool isEnabled_ = false;
    /// Other instances of RmlUI.
    ea::vector<WeakPtr<RmlUI>> siblingSubsystems_;

    friend class Detail::RmlPlugin;
};

namespace Detail
{

/// For internal use only! Helper class used to associate instance of RmlUI with Rml::Context.
class RmlContext : public Rml::Context
{
public:
    /// Construct.
    explicit RmlContext(const ea::string& name) : Rml::Context(name) { }
    /// Set owner subsystem pointer.
    void SetOwnerSubsystem(RmlUI* ui) { ownerSubsystem_ = ui; }
    /// Get owner subsystem pointer.
    RmlUI* GetOwnerSubsystem() const { return ownerSubsystem_; }

private:
    /// Subsystem instance which instantiated this context.
    WeakPtr<RmlUI> ownerSubsystem_;
};

}

/// Register UI library objects.
void URHO3D_API RegisterRmlUILibrary(Context* context);

}
