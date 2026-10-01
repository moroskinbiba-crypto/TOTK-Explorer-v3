#include "explorer.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ex {

void loadPoints() {
    state().points.clear();
    FILE* f = std::fopen("sdmc:/switch/totk_explorer/points.csv", "rb");
    if (!f) f = std::fopen("sdmc:/switch/totk_explorer/points.csv", "r");
    if (!f) return;
    char line[512];
    while (std::fgets(line, sizeof(line), f)) {
        if (line[0]=='#' || line[0]=='\n' || line[0]=='\r') continue;
        char type[64]{}, name[192]{}; float x=0,y=0,z=0;
        if (std::sscanf(line, "%63[^,],%191[^,],%f,%f,%f", type, name, &x, &y, &z) == 5)
            state().points.push_back(Point{type,name,x,y,z});
    }
    std::fclose(f);
}

std::vector<Point> nearby(float radius, size_t maxCount) {
    std::vector<Point> result;
    if (!state().playerValid) return result;
    struct D { float d; Point p; };
    std::vector<D> tmp;
    const float r2 = radius*radius;
    for (const auto& p: state().points) {
        const float dx=p.x-state().player.x, dz=p.z-state().player.z, dy=p.y-state().player.y;
        const float d2=dx*dx+dz*dz+0.25f*dy*dy;
        if (d2 <= r2) tmp.push_back({std::sqrt(d2),p});
    }
    std::sort(tmp.begin(), tmp.end(), [](const D&a,const D&b){return a.d<b.d;});
    for (size_t i=0;i<tmp.size() && i<maxCount;++i) result.push_back(tmp[i].p);
    return result;
}

} // namespace ex
