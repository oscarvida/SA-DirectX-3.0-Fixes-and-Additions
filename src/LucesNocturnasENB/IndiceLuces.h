#pragma once
#include <vector>
#include <array>
#include <cmath>
#include <cstdint>
// Rebuilt from the exact immutable input list each frame. No persisted entity pointers.
// Each result is the smallest original index satisfying the ORIGINAL float predicate.
class IndiceLuces {
    struct Nodo { int x,y,z,next,index; };
    std::array<int,512> heads;
    std::vector<Nodo> nodes;
    bool indexed=false;
    static unsigned bucket(int x,int y,int z) {
        return (uint32_t(x)*73856093u ^ uint32_t(y)*19349663u ^ uint32_t(z)*83492791u)&511u;
    }
    static bool cell(const float* p,int* c) {
        for(int k=0;k<3;k++) {
            // Reject nonfinite/huge positions: exact linear fallback, never truncate.
            if(!std::isfinite(p[k])||std::fabs(p[k])>1000000.f)return false;
            c[k]=int(std::floor(p[k]));
        }
        return true;
    }
public:
    template<class T> void build(const T* points,int n) {
        indexed=false;nodes.clear();
        if(n<128)return; // Avoid index construction for short input lists.
        heads.fill(-1);nodes.reserve(n);
        std::array<unsigned short,512> occupancy{};
        for(int i=0;i<n;i++) {
            int c[3];if(!cell(points[i].pos,c))return;
            unsigned b=bucket(c[0],c[1],c[2]);
            if(++occupancy[b]>8)return; // Dense/hash-degenerate scene: preserve fast linear path.
            nodes.push_back({c[0],c[1],c[2],heads[b],i});heads[b]=int(nodes.size())-1;
        }
        indexed=true;
    }
    template<class T> int first(const T* points,int n,const float* p,float epsilon) const {
        auto matches=[&](int i) { const float* a=points[i].pos;
            return std::fabs(a[0]-p[0])<epsilon&&std::fabs(a[1]-p[1])<epsilon&&std::fabs(a[2]-p[2])<epsilon; };
        int c[3];
        if(!indexed||!(epsilon>0&&epsilon<=1.f)||!cell(p,c)) {
            for(int i=0;i<n;i++)if(matches(i))return i;return -1;
        }
        int result=-1;
        // Conservative double bounds include float-subtraction rounding at the strict threshold.
        const double extent=double(epsilon)*(1.0+1.0/8388608.0);
        int lo[3],hi[3];for(int k=0;k<3;k++){lo[k]=int(std::floor(double(p[k])-extent));hi[k]=int(std::floor(double(p[k])+extent));}
        for(int z=lo[2];z<=hi[2];z++)for(int y=lo[1];y<=hi[1];y++)for(int x=lo[0];x<=hi[0];x++) {
            for(int k=heads[bucket(x,y,z)];k>=0;k=nodes[k].next) {
                const Nodo& v=nodes[k];
                if(v.x==x&&v.y==y&&v.z==z&&(result<0||v.index<result)&&matches(v.index))result=v.index;
            }
        }
        return result;
    }
};
