#include "explorer.hpp"
#include "switch/dmntcht.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace ex {
namespace {
State g;
DmntCheatProcessMetadata meta{};
bool dmntInitDone = false;
constexpr u64 CHUNK = 0x100000;      // 1 MiB per overlay tick while scanning.
constexpr size_t MAX_CANDIDATES = 30000;
constexpr float MAX_ABS_COORD = 20000.0f;
constexpr float MOVE_EPS = 0.25f;
constexpr float JUMP_EPS = 1.0f;

std::string profilePath() { return "sdmc:/switch/totk_explorer/277178B7DBA1B6D4.profile"; }

bool finiteCoord(float v) { return std::isfinite(v) && std::fabs(v) <= MAX_ABS_COORD; }
bool plausible(Vec3 p) { return finiteCoord(p.x) && finiteCoord(p.y) && finiteCoord(p.z) && !(p.x==0 && p.y==0 && p.z==0); }

void setFailed(const char* s) { g.stage = ScanStage::Failed; g.error = s; g.message = s; }

bool findHeap() {
    if (R_FAILED(dmntchtGetCheatProcessMetadata(&meta))) return false;
    if (meta.title_id != TITLE_ID) return false;
    u64 pid = meta.process_id;
    if (!pid || !meta.heap_extents.base || !meta.heap_extents.size) return false;
    g.processId = pid; g.heapBase = meta.heap_extents.base; g.heapSize = meta.heap_extents.size;
    return true;
}

void ensureDir() {
    mkdir("sdmc:/switch", 0777); mkdir("sdmc:/switch/totk_explorer", 0777);
}

bool readProfileFile(Profile& p) {
    FILE* f = std::fopen(profilePath().c_str(), "rb");
    if (!f) return false;
    unsigned long long off=0; float x=0,y=0,z=0; int score=0;
    const int n = std::fscanf(f, "%llx %f %f %f %d", &off, &x, &y, &z, &score);
    std::fclose(f);
    if (n != 5) return false;
    p = {true, (u64)off, {x,y,z}, score};
    return true;
}

void scanChunk() {
    const u64 remaining = g.heapSize - g.cursor;
    const size_t n = (size_t)std::min<u64>(CHUNK, remaining);
    if (n < 16) { g.stage = ScanStage::WaitMove; g.cursor = 0; g.scanned = g.heapSize; g.message = "Scan 1 complete. Move Link and press A."; g.candidates = g.snapshot.size(); return; }

    std::vector<u8> buf(n + 8);
    if (R_FAILED(dmntchtReadCheatProcessMemory(g.heapBase + g.cursor, buf.data(), n))) {
        g.cursor += n; g.scanned += n; return;
    }
    for (size_t i=0; i + 12 <= n && g.snapshot.size() < MAX_CANDIDATES; i += 4) {
        Vec3 p{};
        std::memcpy(&p.x, buf.data()+i, 4);
        std::memcpy(&p.y, buf.data()+i+4, 4);
        std::memcpy(&p.z, buf.data()+i+8, 4);
        if (!plausible(p)) continue;
        Candidate c{}; c.addr = g.heapBase + g.cursor + i; c.heapOffset = g.cursor + i; c.value = p; c.score = 1;
        g.snapshot.push_back(c);
    }
    g.cursor += n; g.scanned += n;
    if (g.cursor >= g.heapSize) {
        g.stage = ScanStage::WaitMove;
        g.message = "Scan 1 complete. Move Link and press A.";
        g.candidates = g.snapshot.size();
    } else {
        g.message = "Scanning memory…";
    }
}

void filterMoved(bool jumpPhase) {
    std::vector<Candidate> out;
    out.reserve(g.moving.size());
    for (const auto& c : g.moving) {
        Vec3 p{};
        if (R_FAILED(dmntchtReadCheatProcessMemory(c.addr, &p, sizeof(p)))) continue;
        if (!plausible(p)) continue;
        const float dx = p.x - c.value.x;
        const float dy = p.y - c.value.y;
        const float dz = p.z - c.value.z;
        if (!jumpPhase) {
            if (std::sqrt(dx*dx+dz*dz) < MOVE_EPS) continue;
            Candidate n = c; n.value = p; n.score += 2; out.push_back(n);
        } else {
            if (std::fabs(dy) < JUMP_EPS) continue;
            Candidate n = c; n.value = p; n.score += 4; out.push_back(n);
        }
    }
    g.moving.swap(out);
    g.candidates = g.moving.size();
}

void selectBest() {
    if (g.moving.empty()) { setFailed("No stable coordinate candidate. Try Auto Scan again."); return; }
    auto it = std::max_element(g.moving.begin(), g.moving.end(), [](const Candidate& a, const Candidate& b){ return a.score < b.score; });
    const Candidate c = *it;
    g.profile = {true, c.heapOffset, c.value, c.score};
    g.player = c.value; g.playerValid = true;
    saveProfile();
    g.stage = ScanStage::Ready;
    g.message = "Coordinate profile found and saved.";
}

}

State& state() { return g; }

Result initMemory() {
    if (dmntInitDone) return 0;
    Result rc = dmntchtInitialize();
    if (R_FAILED(rc)) { setFailed("dmnt:cht unavailable"); return rc; }
    dmntInitDone = true; g.dmntReady = true;
    bool has=false;
    if (R_SUCCEEDED(dmntchtHasCheatProcess(&has)) && !has) {
        if (R_SUCCEEDED(dmntchtForceOpenCheatProcess())) g.attachedByUs = true;
    }
    if (!findHeap()) { setFailed("TOTK process/BID not detected"); return MAKERESULT(Module_Libnx, LibnxError_NotInitialized); }
    loadProfile();
    if (g.profile.valid) {
        refreshPlayer();
        if (g.playerValid) { g.stage = ScanStage::Ready; g.message = "Saved coordinate profile loaded."; }
    }
    return 0;
}

void shutdownMemory() {
    if (g.attachedByUs) dmntchtForceCloseCheatProcess();
    g.attachedByUs = false;
    if (dmntInitDone) dmntchtExit();
    dmntInitDone = false; g.dmntReady = false;
}

void startAutoScan() {
    if (!g.dmntReady && R_FAILED(initMemory())) return;
    g.snapshot.clear(); g.moving.clear(); g.candidates=0; g.cursor=0; g.scanned=0; g.profile.valid=false; g.playerValid=false;
    g.stage = ScanStage::Scanning; g.message = "Scanning readable heap…"; g.error.clear();
}

void captureMove() {
    if (g.stage != ScanStage::WaitMove) return;
    filterMoved(false);
    if (g.moving.empty()) { g.message = "No moving candidates. Start Auto Scan again."; g.stage = ScanStage::Failed; return; }
    // If this is the first filtering pass, capture current values as the baseline for jump.
    g.stage = ScanStage::WaitJump;
    g.message = "Now jump or change elevation, then press A.";
}

void captureJump() {
    if (g.stage != ScanStage::WaitJump) return;
    filterMoved(true);
    selectBest();
}

void resetScan() {
    g.stage = ScanStage::Idle; g.message = "Ready"; g.error.clear(); g.snapshot.clear(); g.moving.clear(); g.candidates=0; g.cursor=0; g.scanned=0;
}

void refreshPlayer() {
    if (!g.profile.valid || !g.heapBase) { g.playerValid=false; return; }
    Vec3 p{};
    if (R_FAILED(dmntchtReadCheatProcessMemory(g.heapBase + g.profile.offset, &p, sizeof(p))) || !plausible(p)) { g.playerValid=false; return; }
    g.player=p; g.playerValid=true;
}

void tick() {
    if (!g.dmntReady) return;
    if (g.stage == ScanStage::Scanning) scanChunk();
    else if (g.stage == ScanStage::Ready) refreshPlayer();
}

void saveProfile() {
    if (!g.profile.valid) return;
    ensureDir();
    FILE* f = std::fopen(profilePath().c_str(), "wb");
    if (!f) return;
    std::fprintf(f, "%llx %.7g %.7g %.7g %d\n", (unsigned long long)g.profile.offset, (double)g.profile.value.x, (double)g.profile.value.y, (double)g.profile.value.z, g.profile.score);
    std::fclose(f);
}

void loadProfile() {
    Profile p{}; if (!readProfileFile(p)) return;
    g.profile = p;
}

const char* stageText(ScanStage s) {
    switch(s) {
        case ScanStage::Idle: return "Ready";
        case ScanStage::Preparing: return "Preparing";
        case ScanStage::Scanning: return "Scanning";
        case ScanStage::WaitMove: return "Move Link / press A";
        case ScanStage::WaitJump: return "Jump / press A";
        case ScanStage::Finalizing: return "Finalizing";
        case ScanStage::Ready: return "Ready";
        default: return "Failed";
    }
}

std::string regionName(const Vec3& p) {
    if (p.y > 500) return "Sky";
    if (p.y < -100) return "Depths";
    return "Hyrule";
}

std::string layerName(const Vec3& p) {
    if (p.y > 500) return "Sky layer";
    if (p.y < -100) return "Depths layer";
    return "Surface layer";
}

}
