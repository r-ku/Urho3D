#include <imgui.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unordered_map>
#include <memory>
#include <cassert>
#include "../nfd/nfd.h"
#include <sys/stat.h>
#include <client/tracy_rpmalloc.hpp>

#ifdef _WIN32
#  include <windows.h>
#  include <shellapi.h>
#endif

#define STBI_ONLY_PNG
#include "stb_image.h"

#include "../../common/TracyProtocol.hpp"
#include "../../server/tracy_flat_hash_map.hpp"
#include "../../server/tracy_pdqsort.h"
#include "../../server/TracyBadVersion.hpp"
#include "../../server/TracyFileRead.hpp"
#include "../../server/TracyImGui.hpp"
#include "../../server/TracyPrint.hpp"
#include "../../server/TracyStorage.hpp"
#include "../../server/TracyView.hpp"
#include "../../server/TracyWorker.hpp"
#include "../../server/TracyVersion.hpp"
#include "../../server/IconsFontAwesome5.h"
#include "../../client/tracy_rpmalloc.hpp"

#include "imgui_freetype.h"
#include "Arimo.hpp"
#include "Cousine.hpp"
#include "FontAwesomeSolid.hpp"
#include "icon.hpp"
#include "ResolvService.hpp"

#include <Urho3D/Core/CommandLine.h>
#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Core/StringUtils.h>
#include <Urho3D/Engine/Application.h>
#include <Urho3D/Engine/EngineDefs.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/Input/Input.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/Resource/JSONFile.h>
#include <Urho3D/SystemUI/SystemUI.h>

using namespace Urho3D;

static void OpenWebpage( const char* url )
{
#ifdef _WIN32
    ShellExecuteA( nullptr, nullptr, url, nullptr, nullptr, 0 );
#else
    char buf[1024];
    sprintf( buf, "xdg-open %s", url );
    system( buf );
#endif
}

Urho3D::Context* gContext = nullptr;

static void SetWindowTitleCallback(const char* title)
{
    assert(gContext != nullptr);
    gContext->GetGraphics()->SetWindowTitle(title);
}

std::vector<std::unordered_map<std::string, uint64_t>::const_iterator> RebuildConnectionHistory( const std::unordered_map<std::string, uint64_t>& connHistMap )
{
    std::vector<std::unordered_map<std::string, uint64_t>::const_iterator> ret{};
    ret.reserve( connHistMap.size() );
    for( auto it = connHistMap.begin(); it != connHistMap.end(); ++it )
    {
        ret.emplace_back( it );
    }
    tracy::pdqsort_branchless( ret.begin(), ret.end(), []( const auto& lhs, const auto& rhs ) { return lhs->second > rhs->second; } );
    return ret;
}

struct ClientData
{
    int64_t time;
    uint32_t protocolVersion;
    uint32_t activeTime;
    std::string procName;
    std::string address;
};

#ifdef _WIN32
namespace tracy
{
    bool DiscoveryAVX()
    {
#ifdef __AVX__
        return true;
#else
        return false;
#endif
    }

    bool DiscoveryAVX2()
    {
#ifdef __AVX2__
        return true;
#else
        return false;
#endif
    }
}
#endif

class ProfilerApplication : public Application
{
    URHO3D_OBJECT(ProfilerApplication, Application);
public:
    explicit ProfilerApplication(Context* context)
        : Application(context)
    {
        ::gContext = context;
    }

    void Setup() override
    {
#if __APPLE__
        // Static initialization appears to be broken on apple platforms and allocator has to be manually initialized.
        tracy::rpmalloc_initialize();
#endif
        engineParameters_[EP_RESOURCE_PATHS] = "CoreData";
        engineParameters_[EP_RESOURCE_PREFIX_PATHS] = ";..;../..";
        engineParameters_[EP_FULL_SCREEN] = false;

        // Engine starts listening for profiler application connections automatically. Since we link to the engine we
        // would take over profiler port and profile ourselves. Just terminate the profiler.
        tracy::GetProfiler().RequestShutdown();

        GetCommandLineParser().add_option("capture", readCapture);

#if _WIN32
        uint32_t regs[4]{};
        __cpuidex((int*) regs, 0, 0);
        const uint32_t maxLeaf = regs[0];
        bool cpuHasAVX = false;
        bool cpuHasAVX2 = false;
        if (maxLeaf >= 1)
        {
            __cpuidex((int*) regs, 1, 0);
            cpuHasAVX = (regs[2] & 0x10000000) != 0;
        }
        if (maxLeaf >= 7)
        {
            __cpuidex((int*) regs, 7, 0);
            cpuHasAVX2 = (regs[1] & 0x00000020) != 0;
        }

        if (tracy::DiscoveryAVX2() && !cpuHasAVX2)
        {
            ErrorExit(
                "This program is compiled with AVX2 instruction set, but your CPU doesn't support it. You must recompile with lower instruction set.\n\n"
                "In Visual Studio go to Project properties -> C/C++ -> Code Generation -> Enable Enhanced Instruction Set and select appropriate value for your CPU.");
        }
        if (tracy::DiscoveryAVX() && !cpuHasAVX)
        {
            ErrorExit(
                "This program is compiled with AVX instruction set, but your CPU doesn't support it. You must recompile with lower instruction set.\n\n"
                "In Visual Studio go to Project properties -> C/C++ -> Code Generation -> Enable Enhanced Instruction Set and select appropriate value for your CPU.");
        }
#endif

        engineParameters_[EP_FULL_SCREEN] = false;
        engineParameters_[EP_WINDOW_RESIZABLE] = true;

        JSONFile config(context_);
        ea::string preferencesDir = context_->GetFileSystem()->GetAppPreferencesDir("rbfx", "Profiler");
        if (!context_->GetFileSystem()->DirExists(preferencesDir))
            context_->GetFileSystem()->CreateDir(preferencesDir);
        if (config.LoadFile(preferencesDir + "Settings.json"))
        {
            const auto& root = config.GetRoot();
            if (root.IsObject())
            {
                engineParameters_[EP_WINDOW_POSITION_X] = root["x"].GetInt();
                engineParameters_[EP_WINDOW_POSITION_Y] = root["y"].GetInt();
                engineParameters_[EP_WINDOW_WIDTH] = root["width"].GetUInt();
                engineParameters_[EP_WINDOW_HEIGHT] = root["height"].GetUInt();
            }
        }

        strncpy(addr, "127.0.0.1", sizeof(addr));
    }

    void Start() override
    {
        context_->GetGraphics()->SetWindowTitle(
            Format("Urho3D Profiler {}.{}.{}", tracy::Version::Major, tracy::Version::Minor, tracy::Version::Patch));
        context_->GetInput()->SetMouseVisible(true);
        context_->GetInput()->SetMouseMode(MM_FREE);

        ImGui::StyleColorsDark();
        float dpiScale = GetDPIScale();
        auto& style = ImGui::GetStyle();
        style.WindowBorderSize = 1.f * dpiScale;
        style.FrameBorderSize = 1.f * dpiScale;
        style.FrameRounding = 5.f * dpiScale;
        style.ScrollbarSize *= dpiScale;
        style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(1, 1, 1, 0.03f);
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.129f, 0.137f, 0.11f, 1.f);

        static const ImWchar rangesBasic[] = {
            0x0020, 0x00FF, // Basic Latin + Latin Supplement
            0x03BC, 0x03BC, // micro
            0x03C3, 0x03C3, // small sigma
            0,
        };
    static const ImWchar rangesIcons[] = {
        ICON_MIN_FA, ICON_MAX_FA,
        0
    };

        context_->GetSystemUI()->AddFontCompressed(tracy::Arimo_compressed_data, tracy::Arimo_compressed_size, rangesBasic, 15.0f);
        context_->GetSystemUI()->AddFontCompressed(tracy::FontAwesomeSolid_compressed_data, tracy::FontAwesomeSolid_compressed_size, rangesIcons, 14.0f, true);
        fixedWidth = context_->GetSystemUI()->AddFontCompressed(tracy::Cousine_compressed_data, tracy::Cousine_compressed_size, nullptr, 15.0f);
        bigFont = context_->GetSystemUI()->AddFontCompressed(tracy::Arimo_compressed_data, tracy::Cousine_compressed_size, nullptr, 20.0f);
        smallFont = context_->GetSystemUI()->AddFontCompressed(tracy::Arimo_compressed_data, tracy::Cousine_compressed_size, nullptr, 10.0f);

        if (!readCapture.empty())
        {
            std::unique_ptr<tracy::FileRead> file(tracy::FileRead::Open(readCapture.c_str()));
            if (file)
                view = std::make_unique<tracy::View>(*file);
        }

        SubscribeToEvent(E_UPDATE, [this](StringHash, VariantMap&) { Update(); });
    }

    void Stop() override
    {
        JSONValue root{JSON_OBJECT};
        root["x"] = context_->GetGraphics()->GetWindowPosition().x_;
        root["y"] = context_->GetGraphics()->GetWindowPosition().y_;
        root["width"] = context_->GetGraphics()->GetWidth();
        root["height"] = context_->GetGraphics()->GetHeight();

        JSONFile config(context_);
        config.GetRoot() = root;
        config.SaveFile(Format("{}/Settings.json", context_->GetFileSystem()->GetAppPreferencesDir("rbfx", "Profiler")));
    }

    float GetDPIScale()
    {
        return context_->GetGraphics()->GetDisplayDPI(context_->GetGraphics()->GetCurrentMonitor()).z_ / 96.f;
    }

    void Update()
    {
        auto& style = ImGui::GetStyle();
        auto& io = ImGui::GetIO();
        float dpiScale = GetDPIScale();

        if( !view )
        {
            const auto time = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::system_clock::now().time_since_epoch() ).count();
            if( !broadcastListen )
            {
                broadcastListen = std::make_unique<tracy::UdpListen>();
                if( !broadcastListen->Listen( port ) )
                {
                    broadcastListen.reset();
                }
            }
            else
            {
                tracy::IpAddress addr;
                size_t len;
                auto msg = broadcastListen->Read( len, addr );
                if( msg )
                {
                    assert( len <= sizeof( tracy::BroadcastMessage ) );
                    tracy::BroadcastMessage bm;
                    memcpy( &bm, msg, len );

                    if( bm.broadcastVersion == tracy::BroadcastVersion )
                    {
                        const uint32_t protoVer = bm.protocolVersion;
                        const auto procname = bm.programName;
                        const auto activeTime = bm.activeTime;
                        auto address = addr.GetText();

                        const auto ipNumerical = addr.GetNumber();
                        auto it = clients.find( ipNumerical );
                        if( it == clients.end() )
                        {
                            std::string ip( address );
                            resolvLock.lock();
                            if( resolvMap.find( ip ) == resolvMap.end() )
                            {
                                resolvMap.emplace( ip, ip );
                                resolv.Query( ipNumerical, [this, ip] ( std::string&& name ) {
                                    std::lock_guard<std::mutex> lock( resolvLock );
                                    auto it = resolvMap.find( ip );
                                    assert( it != resolvMap.end() );
                                    std::swap( it->second, name );
                                } );
                            }
                            resolvLock.unlock();
                            clients.emplace( addr.GetNumber(), ClientData { time, protoVer, activeTime, procname, std::move( ip ) } );
                        }
                        else
                        {
                            it->second.time = time;
                            it->second.activeTime = activeTime;
                            if( it->second.protocolVersion != protoVer ) it->second.protocolVersion = protoVer;
                            if( strcmp( it->second.procName.c_str(), procname ) != 0 ) it->second.procName = procname;
                        }
                    }
                }
                auto it = clients.begin();
                while( it != clients.end() )
                {
                    const auto diff = time - it->second.time;
                    if( diff > 4000 )  // 4s
                    {
                        it = clients.erase( it );
                    }
                    else
                    {
                        ++it;
                    }
                }
            }

            setlocale( LC_NUMERIC, "C" );
            style.Colors[ImGuiCol_WindowBg] = ImVec4( 0.129f, 0.137f, 0.11f, 1.f );
            ImGui::Begin( "Get started", nullptr, ImGuiWindowFlags_AlwaysAutoResize );
            char buf[128];
            sprintf(buf, "Urho3D Profiler %i.%i.%i", tracy::Version::Major, tracy::Version::Minor, tracy::Version::Patch);
            ImGui::PushFont( bigFont );
            tracy::TextCentered( buf );
            ImGui::PopFont();
            ImGui::Spacing();
            if( ImGui::Button( ICON_FA_BOOK " Manual" ) )
            {
                OpenWebpage( "https://bitbucket.org/wolfpld/tracy/downloads/tracy.pdf" );
            }
            ImGui::SameLine();
            if( ImGui::Button( ICON_FA_GLOBE_AMERICAS " Web" ) )
            {
                OpenWebpage( "https://bitbucket.org/wolfpld/tracy" );
            }
            ImGui::SameLine();
            if( ImGui::Button( ICON_FA_COMMENT " Chat" ) )
            {
                OpenWebpage( "https://discord.gg/pk78auc" );
            }
            ImGui::SameLine();
            if( ImGui::Button( ICON_FA_VIDEO " Tutorial" ) )
            {
                ImGui::OpenPopup( "tutorial" );
            }
            if( ImGui::BeginPopup( "tutorial" ) )
            {
                if( ImGui::Selectable( ICON_FA_VIDEO " Introduction to the Tracy Profiler" ) )
                {
                    OpenWebpage( "https://www.youtube.com/watch?v=fB5B46lbapc" );
                }
                if( ImGui::Selectable( ICON_FA_VIDEO " New features in Tracy Profiler v0.3" ) )
                {
                    OpenWebpage( "https://www.youtube.com/watch?v=3SXpDpDh2Uo" );
                }
                if( ImGui::Selectable( ICON_FA_VIDEO " New features in Tracy Profiler v0.4" ) )
                {
                    OpenWebpage( "https://www.youtube.com/watch?v=eAkgkaO8B9o" );
                }
                if( ImGui::Selectable( ICON_FA_VIDEO " New features in Tracy Profiler v0.5" ) )
                {
                    OpenWebpage( "https://www.youtube.com/watch?v=P6E7qLMmzTQ" );
                }
                ImGui::EndPopup();
            }
            ImGui::Separator();
            ImGui::TextUnformatted( "Client address" );
            bool connectClicked = false;
            connectClicked |= ImGui::InputTextWithHint( "###connectaddress", "Enter address", addr, 1024, ImGuiInputTextFlags_EnterReturnsTrue );
            if( !connHistVec.empty() )
            {
                ImGui::SameLine();
                if( ImGui::BeginCombo( "##frameCombo", nullptr, ImGuiComboFlags_NoPreview ) )
                {
                    int idxRemove = -1;
                    const auto sz = std::min<size_t>( 5, connHistVec.size() );
                    for( size_t i=0; i<sz; i++ )
                    {
                        const auto& str = connHistVec[i]->first;
                        if( ImGui::Selectable( str.c_str() ) )
                        {
                            memcpy( addr, str.c_str(), str.size() + 1 );
                        }
                        if( ImGui::IsItemHovered() && ImGui::IsKeyPressed( ImGui::GetKeyIndex( ImGuiKey_Delete ), false ) )
                        {
                            idxRemove = (int)i;
                        }
                    }
                    if( idxRemove >= 0 )
                    {
                        connHistMap.erase( connHistVec[idxRemove] );
                        connHistVec = RebuildConnectionHistory( connHistMap );
                    }
                    ImGui::EndCombo();
                }
            }
            connectClicked |= ImGui::Button( ICON_FA_WIFI " Connect" );
            if( connectClicked && *addr && !loadThread.joinable() )
            {
                std::string addrStr( addr );
                auto it = connHistMap.find( addrStr );
                if( it != connHistMap.end() )
                {
                    it->second++;
                }
                else
                {
                    connHistMap.emplace( std::move( addrStr ), 1 );
                }
                connHistVec = RebuildConnectionHistory( connHistMap );

                view = std::make_unique<tracy::View>( addr, port, fixedWidth, smallFont, bigFont, SetWindowTitleCallback );
            }
            ImGui::SameLine( 0, ImGui::GetFontSize() * 2 );
            if( ImGui::Button( ICON_FA_FOLDER_OPEN " Open saved trace" ) && !loadThread.joinable() )
            {
                nfdchar_t* fn;
                auto res = NFD_OpenDialog( "tracy", nullptr, &fn );
                if( res == NFD_OKAY )
                {
                    try
                    {
                        auto f = std::shared_ptr<tracy::FileRead>( tracy::FileRead::Open( fn ) );
                        if( f )
                        {
                            loadThread = std::thread([this, f] {
                                try
                                {
                                    view = std::make_unique<tracy::View>( *f, fixedWidth, smallFont, bigFont, SetWindowTitleCallback );
                                }
                                catch( const tracy::UnsupportedVersion& e )
                                {
                                    badVer.state = tracy::BadVersionState::UnsupportedVersion;
                                    badVer.version = e.version;
                                }
                                catch( const tracy::LegacyVersion& e )
                                {
                                    badVer.state = tracy::BadVersionState::LegacyVersion;
                                    badVer.version = e.version;
                                }
                            } );
                        }
                    }
                    catch( const tracy::NotTracyDump& )
                    {
                        badVer.state = tracy::BadVersionState::BadFile;
                    }
                }
            }

            if( badVer.state != tracy::BadVersionState::Ok )
            {
                if( loadThread.joinable() ) { loadThread.join(); }
                tracy::BadVersion( badVer );
            }

            if( !clients.empty() )
            {
                ImGui::Separator();
                ImGui::TextUnformatted( "Discovered clients:" );
                ImGui::Separator();
                static bool widthSet = false;
                ImGui::Columns( 3 );
                if( !widthSet )
                {
                    widthSet = true;
                    const auto w = ImGui::GetWindowWidth();
                    ImGui::SetColumnWidth( 0, w * 0.35f );
                    ImGui::SetColumnWidth( 1, w * 0.175f );
                    ImGui::SetColumnWidth( 2, w * 0.425f );
                }
                std::lock_guard<std::mutex> lock( resolvLock );
                for( auto& v : clients )
                {
                    const bool badProto = v.second.protocolVersion != tracy::ProtocolVersion;
                    bool sel = false;
                    const auto& name = resolvMap.find( v.second.address );
                    assert( name != resolvMap.end() );
                    ImGuiSelectableFlags flags = ImGuiSelectableFlags_SpanAllColumns;
                    if( badProto ) flags |= ImGuiSelectableFlags_Disabled;
                    if( ImGui::Selectable( name->second.c_str(), &sel, flags ) && !loadThread.joinable() )
                    {
                        view = std::make_unique<tracy::View>( v.second.address.c_str(), port, fixedWidth, smallFont, bigFont, SetWindowTitleCallback );
                    }
                    ImGui::NextColumn();
                    const auto acttime = ( v.second.activeTime + ( time - v.second.time ) / 1000 ) * 1000000000ll;
                    if( badProto )
                    {
                        tracy::TextDisabledUnformatted( tracy::TimeToString( acttime ) );
                    }
                    else
                    {
                        ImGui::TextUnformatted( tracy::TimeToString( acttime ) );
                    }
                    ImGui::NextColumn();
                    if( badProto )
                    {
                        tracy::TextDisabledUnformatted( v.second.procName.c_str() );
                    }
                    else
                    {
                        ImGui::TextUnformatted( v.second.procName.c_str() );
                    }
                    ImGui::NextColumn();
                }
                ImGui::EndColumns();
            }

            ImGui::End();
        }
        else
        {
            if( broadcastListen )
            {
                broadcastListen.reset();
                clients.clear();
            }
            if( loadThread.joinable() ) loadThread.join();
            view->NotifyRootWindowSize(context_->GetGraphics()->GetWidth(), context_->GetGraphics()->GetHeight());
            if( !view->Draw() )
            {
                viewShutdown = ViewShutdown::True;
                loadThread = std::thread([this, view = std::move( view)]() mutable {
                    tracy::rpmalloc_thread_initialize();
                    view.reset();
                    viewShutdown = ViewShutdown::Join;
                } );
            }
        }
        auto& progress = tracy::Worker::GetLoadProgress();
        auto totalProgress = progress.total.load( std::memory_order_relaxed );
        if( totalProgress != 0 )
        {
            ImGui::OpenPopup( "Loading trace..." );
        }
        if( ImGui::BeginPopupModal( "Loading trace...", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) )
        {
            tracy::TextCentered( ICON_FA_HOURGLASS_HALF );

            tracy::DrawWaitingDots(context_->GetTime()->GetElapsedTime());

            auto currProgress = progress.progress.load( std::memory_order_relaxed );
            if( totalProgress == 0 )
            {
                ImGui::CloseCurrentPopup();
                totalProgress = currProgress;
            }
            switch( currProgress )
            {
            case tracy::LoadProgress::Initialization:
                ImGui::TextUnformatted( "Initialization..." );
                break;
            case tracy::LoadProgress::Locks:
                ImGui::TextUnformatted( "Locks..." );
                break;
            case tracy::LoadProgress::Messages:
                ImGui::TextUnformatted( "Messages..." );
                break;
            case tracy::LoadProgress::Zones:
                ImGui::TextUnformatted( "CPU zones..." );
                break;
            case tracy::LoadProgress::GpuZones:
                ImGui::TextUnformatted( "GPU zones..." );
                break;
            case tracy::LoadProgress::Plots:
                ImGui::TextUnformatted( "Plots..." );
                break;
            case tracy::LoadProgress::Memory:
                ImGui::TextUnformatted( "Memory..." );
                break;
            case tracy::LoadProgress::CallStacks:
                ImGui::TextUnformatted( "Call stacks..." );
                break;
            case tracy::LoadProgress::FrameImages:
                ImGui::TextUnformatted( "Frame images..." );
                break;
            case tracy::LoadProgress::ContextSwitches:
                ImGui::TextUnformatted( "Context switches..." );
                break;
            case tracy::LoadProgress::ContextSwitchesPerCpu:
                ImGui::TextUnformatted( "CPU context switches..." );
                break;
            default:
                assert( false );
                break;
            }
            ImGui::ProgressBar( float( currProgress ) / totalProgress, ImVec2( 200 * dpiScale, 0 ) );

            ImGui::TextUnformatted( "Progress..." );
            auto subTotal = progress.subTotal.load( std::memory_order_relaxed );
            auto subProgress = progress.subProgress.load( std::memory_order_relaxed );
            if( subTotal == 0 )
            {
                ImGui::ProgressBar( 1.f, ImVec2( 200 * dpiScale, 0 ) );
            }
            else
            {
                ImGui::ProgressBar( float( subProgress ) / subTotal, ImVec2( 200 * dpiScale, 0 ) );
            }
            ImGui::EndPopup();
        }
        switch( viewShutdown )
        {
        case ViewShutdown::True:
            ImGui::OpenPopup( "Capture cleanup..." );
            break;
        case ViewShutdown::Join:
            loadThread.join();
            viewShutdown = ViewShutdown::False;
            break;
        default:
            break;
        }
        if( ImGui::BeginPopupModal( "Capture cleanup...", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) )
        {
            if( viewShutdown != ViewShutdown::True ) ImGui::CloseCurrentPopup();
            tracy::TextCentered( ICON_FA_BROOM );
            tracy::DrawWaitingDots( context_->GetTime()->GetElapsedTime() );
            ImGui::Text( "Please wait, cleanup is in progress" );
            ImGui::EndPopup();
        }
    }
    int port = 8086;
    ea::string readCapture{};
    std::unordered_map<std::string, uint64_t> connHistMap{};
    std::vector<std::unordered_map<std::string, uint64_t>::const_iterator> connHistVec{};
    std::unique_ptr<tracy::View> view{};
    tracy::BadVersionState badVer;
    ImFont* fixedWidth = nullptr;
    ImFont* bigFont = nullptr;
    ImFont* smallFont = nullptr;
    char addr[16]{};
    std::thread loadThread{};
    std::unique_ptr<tracy::UdpListen> broadcastListen{};
    enum class ViewShutdown { False, True, Join };
    ViewShutdown viewShutdown = ViewShutdown::False;
    std::mutex resolvLock;
    tracy::flat_hash_map<std::string, std::string> resolvMap;
    ResolvService resolv{port};
    tracy::flat_hash_map<uint32_t, ClientData> clients;
};

URHO3D_DEFINE_APPLICATION_MAIN(ProfilerApplication);
