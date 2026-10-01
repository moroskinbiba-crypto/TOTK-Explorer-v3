#include <tesla.hpp>
#include "explorer.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

namespace {
std::string f2(float v) { char b[32]; std::snprintf(b,sizeof(b),"%.2f",(double)v); return b; }
std::string dist3(const ex::Vec3& a,const ex::Point& b) {
    const float dx=b.x-a.x, dy=b.y-a.y, dz=b.z-a.z;
    char s[48]; std::snprintf(s,sizeof(s),"%.0f m",(double)std::sqrt(dx*dx+0.25f*dy*dy+dz*dz)); return s;
}

class MainGui;
class ScanGui;
class MapGui;
class NearbyGui;
class InfoGui;

class MainGui final : public tsl::Gui {
public:
    tsl::elm::Element* createUI() override {
        auto* f = new tsl::elm::OverlayFrame("TOTK EXPLORER", ex::VERSION);
        auto* l = new tsl::elm::List();
        l->addItem(new tsl::elm::CategoryHeader("Tears of the Kingdom 1.4.3"));
        l->addItem(new tsl::elm::ListItem("BID", ex::BID_TEXT));
        l->addItem(new tsl::elm::ListItem("Memory", ex::state().dmntReady ? "dmnt:cht connected" : "not connected"));
        l->addItem(new tsl::elm::ListItem("Player", ex::state().playerValid ? "Live coordinates" : "Not discovered"));
        auto* scan = new tsl::elm::ListItem("Auto Discovery");
        scan->setClickListener([](u64 k){ if(k&HidNpadButton_A){tsl::changeTo<ScanGui>();return true;}return false;}); l->addItem(scan);
        auto* map = new tsl::elm::ListItem("Dynamic Map");
        map->setClickListener([](u64 k){ if(k&HidNpadButton_A){tsl::changeTo<MapGui>();return true;}return false;}); l->addItem(map);
        auto* near = new tsl::elm::ListItem("Nearby Objects");
        near->setClickListener([](u64 k){ if(k&HidNpadButton_A){tsl::changeTo<NearbyGui>();return true;}return false;}); l->addItem(near);
        auto* info = new tsl::elm::ListItem("Build / Diagnostics");
        info->setClickListener([](u64 k){ if(k&HidNpadButton_A){tsl::changeTo<InfoGui>();return true;}return false;}); l->addItem(info);
        f->setContent(l); return f;
    }
    void update() override { ex::tick(); }
    bool handleInput(u64 k,u64,const HidTouchState&,HidAnalogStickState,HidAnalogStickState) override { if(k&HidNpadButton_B){tsl::Overlay::get()->close();return true;} return false; }
};

class ScanGui final : public tsl::Gui {
public:
    tsl::elm::Element* createUI() override {
        auto* f=new tsl::elm::OverlayFrame("TOTK EXPLORER","Auto Discovery"); auto* l=new tsl::elm::List();
        l->addItem(new tsl::elm::CategoryHeader("Automatic coordinate detection"));
        l->addItem(new tsl::elm::ListItem("Stage", ex::stageText(ex::state().stage)));
        l->addItem(new tsl::elm::ListItem("Status", ex::state().message));
        char p[64]; const auto& s=ex::state();
        if(s.heapSize){ std::snprintf(p,sizeof(p),"%.1f%%",100.0*(double)s.scanned/(double)s.heapSize); l->addItem(new tsl::elm::ListItem("Scan progress",p)); }
        l->addItem(new tsl::elm::ListItem("Candidates",std::to_string(s.candidates)));
        auto* start=new tsl::elm::ListItem("Start / Restart Scan"); start->setClickListener([](u64 k){if(k&HidNpadButton_A){ex::startAutoScan();return true;}return false;}); l->addItem(start);
        auto* cap=new tsl::elm::ListItem("Capture / Continue (A)","Use X button"); l->addItem(cap);
        l->addItem(new tsl::elm::CategoryHeader("Calibration"));
        l->addItem(new tsl::elm::ListItem("X button","After walking, capture movement"));
        l->addItem(new tsl::elm::ListItem("Y button","After jumping, reset"));
        l->addItem(new tsl::elm::ListItem("Saved profile",s.profile.valid?"yes":"no"));
        if(s.playerValid){l->addItem(new tsl::elm::ListItem("X/Y/Z",f2(s.player.x)+" / "+f2(s.player.y)+" / "+f2(s.player.z)));}
        f->setContent(l); return f;
    }
    void update() override { ex::tick(); }
    bool handleInput(u64 k,u64,const HidTouchState&,HidAnalogStickState,HidAnalogStickState) override {
        if(k&HidNpadButton_X){ex::captureMove();return true;} if(k&HidNpadButton_Y){ if(ex::state().stage==ex::ScanStage::WaitJump) ex::captureJump(); else ex::resetScan(); return true; } if(k&HidNpadButton_B){tsl::goBack();return true;} return false;
    }
};

class MapGui final : public tsl::Gui {
public:
    tsl::elm::Element* createUI() override {
        auto* f=new tsl::elm::OverlayFrame("TOTK EXPLORER","Dynamic Map"); auto* l=new tsl::elm::List();
        auto&s=ex::state(); l->addItem(new tsl::elm::CategoryHeader("Live local map"));
        if(!s.playerValid){l->addItem(new tsl::elm::ListItem("Player","Run Auto Discovery first")); f->setContent(l);return f;}
        l->addItem(new tsl::elm::ListItem("Layer",ex::layerName(s.player))); l->addItem(new tsl::elm::ListItem("Position",f2(s.player.x)+", "+f2(s.player.y)+", "+f2(s.player.z)));
        // Text-mode dynamic map: 9x9 grid, 250 units per cell, player centered.
        constexpr int N=9; constexpr float cell=250.0f;
        std::array<std::string,N> rows; for(auto& r:rows) r=std::string(N,'.');
        rows[N/2][N/2]='@';
        for(const auto&p:s.points){ float dx=p.x-s.player.x,dz=p.z-s.player.z; int gx=(int)std::lround(dx/cell)+N/2, gy=(int)std::lround(dz/cell)+N/2; if(gx<0||gx>=N||gy<0||gy>=N)continue; char c='*'; if(p.type=="Shrine")c='S'; else if(p.type=="Korok")c='K'; else if(p.type=="Lightroot")c='L'; else if(p.type=="Tower")c='T'; rows[gy][gx]=c; }
        l->addItem(new tsl::elm::CategoryHeader("N ↑  @ Link  S Shrine  K Korok  L Lightroot  T Tower"));
        for(auto it=rows.rbegin();it!=rows.rend();++it) l->addItem(new tsl::elm::ListItem(*it));
        l->addItem(new tsl::elm::CategoryHeader("Nearby"));
        auto nearby = ex::nearby(1000.0f,8);
        for(const auto&p:nearby) l->addItem(new tsl::elm::ListItem(p.type+" · "+p.name,dist3(s.player,p)));
        if(nearby.empty()) l->addItem(new tsl::elm::ListItem("No nearby points","Add entries to points.csv"));
        f->setContent(l); return f;
    }
    void update() override { ex::tick(); }
    bool handleInput(u64 k,u64,const HidTouchState&,HidAnalogStickState,HidAnalogStickState) override {if(k&HidNpadButton_B){tsl::goBack();return true;}return false;}
};

class NearbyGui final : public tsl::Gui {
public:
    tsl::elm::Element* createUI() override { auto*f=new tsl::elm::OverlayFrame("TOTK EXPLORER","Nearby");auto*l=new tsl::elm::List();auto&s=ex::state(); if(!s.playerValid){l->addItem(new tsl::elm::ListItem("Status","Run Auto Discovery first"));f->setContent(l);return f;} auto nearby = ex::nearby(2000,20); for(const auto&p:nearby) l->addItem(new tsl::elm::ListItem(p.type+" · "+p.name,dist3(s.player,p))); if(nearby.empty()) l->addItem(new tsl::elm::ListItem("No points","Add points.csv"));f->setContent(l);return f; }
    void update() override { ex::tick(); }
    bool handleInput(u64 k,u64,const HidTouchState&,HidAnalogStickState,HidAnalogStickState) override {if(k&HidNpadButton_B){tsl::goBack();return true;}return false;}
};

class InfoGui final : public tsl::Gui {
public:
    tsl::elm::Element* createUI() override {auto*f=new tsl::elm::OverlayFrame("TOTK EXPLORER","Diagnostics");auto*l=new tsl::elm::List();auto&s=ex::state();l->addItem(new tsl::elm::CategoryHeader("Target"));l->addItem(new tsl::elm::ListItem("Title ID",ex::TITLE_TEXT));l->addItem(new tsl::elm::ListItem("Version",ex::GAME_VERSION));l->addItem(new tsl::elm::ListItem("Build ID",ex::BID_TEXT));l->addItem(new tsl::elm::CategoryHeader("Runtime"));l->addItem(new tsl::elm::ListItem("dmnt:cht",s.dmntReady?"yes":"no"));l->addItem(new tsl::elm::ListItem("PID",std::to_string(s.processId)));l->addItem(new tsl::elm::ListItem("Heap",std::to_string((unsigned long long)s.heapSize)));l->addItem(new tsl::elm::ListItem("Points",std::to_string(s.points.size())));if(s.playerValid){l->addItem(new tsl::elm::ListItem("Region",ex::regionName(s.player)));l->addItem(new tsl::elm::ListItem("Layer",ex::layerName(s.player)));}l->addItem(new tsl::elm::CategoryHeader("Progress provider"));l->addItem(new tsl::elm::ListItem("Save flags","Not enabled in v3.0"));f->setContent(l);return f;}
    bool handleInput(u64 k,u64,const HidTouchState&,HidAnalogStickState,HidAnalogStickState) override {if(k&HidNpadButton_B){tsl::goBack();return true;}return false;}
};
}

class TotkExplorerOverlay final : public tsl::Overlay {
public:
    void initServices() override { ex::loadPoints(); ex::initMemory(); }
    void exitServices() override { ex::shutdownMemory(); }
    std::unique_ptr<tsl::Gui> loadInitialGui() override { return initially<MainGui>(); }
};

int main(int argc,char**argv){return tsl::loop<TotkExplorerOverlay>(argc,argv);}
