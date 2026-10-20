#pragma once
#include "Scene.hpp"
#include "LensFlare.hpp"
#include "Labels.hpp"
#include <cmath>
#include <array>
#include <algorithm>

namespace Island {
using namespace Renderer;
inline constexpr float pi=3.14159265359f;
inline Scene* scene=nullptr;
inline Camera camera;
inline float time=0;
inline uint16_t sky[256];
inline LensFlare* flare=nullptr;
inline Material* water=nullptr;
inline std::array<Object*,5> palms{};
inline uint16_t rgb(unsigned r,unsigned g,unsigned b) {
    return uint16_t((r>>3)<<11|(g>>2)<<5|(b>>3));
}
inline Material* paint(unsigned hex) {
    auto* m=new Material(rgb(hex>>16,(hex>>8)&255,hex&255));
    m->shadingMode=ShadingMode::UNLIT;
    return m;
}
inline uint16_t vertex(Object* o,Vec3f p) {
    const auto index=uint16_t(o->vertices.size());
    o->addVertex({{int32_t(std::lround(p.x)),int32_t(std::lround(p.y)),int32_t(std::lround(p.z))},
        {0,0},{0,1024,0}});
    return index;
}
inline void put(Object* o) {
    o->calculateBoundingBox();o->cachePositions();scene->addObject(o);
}
inline void makeBeach() {
    auto* o=new Object;
    o->cullingMode=CullingMode::NO_CULLING;
    Material* sand[]={paint(0xE6C587),paint(0xFFE2A0),paint(0xF4D395),paint(0xFADDA0)};
    Material* grass[]={paint(0x75AA3E),paint(0x85BC43),paint(0x639B32)};
    constexpr int n=24;
    const float scales[]={1.14f,1.0f,.82f,.56f};
    const float heights[]={1,7,42,73};
    for(int ring=0;ring<4;++ring) for(int i=0;i<n;++i) {
        float a=2*pi*i/n;
        float coast=1+.09f*std::sin(a*3+.5f)+.05f*std::cos(a*5);
        vertex(o,{650*scales[ring]*coast*std::cos(a),heights[ring],
            470*scales[ring]*coast*std::sin(a)});
    }
    for(int ring=0;ring<3;++ring) for(int i=0;i<n;++i) {
        int j=(i+1)%n;
        o->addFace(ring*n+i,ring*n+j,(ring+1)*n+j,(ring+1)*n+i,sand[(i+ring)%4]);
    }
    auto centre=vertex(o,{0,96,0});
    for(int i=0;i<n;++i) o->addTriangle(3*n+i,3*n+(i+1)%n,centre,grass[i%3]);
    put(o);
    // Narrow, irregular pale lip at the shore. Its vertices sit above the sand
    // instead of using depth bias, avoiding coplanar flicker as the camera moves.
    auto* foam=new Object;
    foam->cullingMode=CullingMode::NO_CULLING;
    auto* white=paint(0xC6F1CE);
    for(int i=0;i<n;++i) {
        float a=2*pi*i/n, coast=1+.09f*std::sin(a*3+.5f)+.05f*std::cos(a*5);
        vertex(foam,{650*coast*1.022f*std::cos(a),3,470*coast*1.022f*std::sin(a)});
        vertex(foam,{650*coast*1.014f*std::cos(a),5,470*coast*1.014f*std::sin(a)});
    }
    for(int i=0;i<n;++i) if(i%4!=0) {
        int j=(i+1)%n;foam->addFace(i*2,j*2,j*2+1,i*2+1,white);
    }
    put(foam);
}
inline Object* makePalm(float x,float z,float height,float phase) {
    auto* o=new Object;
    o->cullingMode=CullingMode::NO_CULLING;
    Material* bark[]={paint(0x94613B),paint(0xBB8852),paint(0xD09D60),paint(0xAA7844),paint(0x785335)};
    Material* leaf[]={paint(0x228546),paint(0x55B94E),paint(0x8BCF50),paint(0x389D40)};
    constexpr int sides=5;
    for(int row=0;row<=3;++row) {
        float t=row/3.0f, bend=t*t*height*.17f;
        for(int i=0;i<sides;++i) {
            float a=2*pi*i/sides, radius=20-t*9;
            vertex(o,{std::cos(phase)*bend+radius*std::cos(a),height*t,
                std::sin(phase)*bend+radius*std::sin(a)});
        }
    }
    for(int row=0;row<3;++row) for(int i=0;i<sides;++i) {
        int j=(i+1)%sides;o->addFace(row*sides+i,row*sides+j,(row+1)*sides+j,(row+1)*sides+i,bark[i]);
    }
    Vec3f crown{std::cos(phase)*height*.17f,height,std::sin(phase)*height*.17f};
    for(int i=0;i<7;++i) {
        float a=phase+i*2*pi/7;
        Vec3f dir{std::cos(a),0,std::sin(a)}, side{-dir.z,0,dir.x};
        float length=height*(.46f+.03f*(i%3));
        Vec3f ridge=crown+dir*(length*.43f)+Vec3f{0,height*.09f,0};
        auto b=vertex(o,crown);
        vertex(o,ridge+side*(length*.15f)-Vec3f{0,23,0});
        vertex(o,ridge);
        vertex(o,ridge-side*(length*.15f)-Vec3f{0,23,0});
        vertex(o,crown+dir*length-Vec3f{0,height*.15f,0});
        o->addTriangle(b,b+1,b+2,leaf[i%4]);o->addTriangle(b,b+2,b+3,leaf[(i+1)%4]);
        o->addTriangle(b+1,b+4,b+2,leaf[i%4]);o->addTriangle(b+2,b+4,b+3,leaf[(i+1)%4]);
    }
    o->setPosition(int(x),72,int(z));put(o);return o;
}
inline void makeRock(float x,float z,float size,float phase) {
    auto* o=new Object;
    o->cullingMode=CullingMode::NO_CULLING;
    Material* grey[]={paint(0x879B97),paint(0xAABAB1),paint(0x6A837F),paint(0xC0C5A8)};
    for(int row=0;row<2;++row) for(int i=0;i<5;++i) {
        float a=phase+i*2*pi/5, r=size*(row?.64f:1.f);
        vertex(o,{x+r*std::cos(a),row?size*.8f:8,z+r*.75f*std::sin(a)});
    }
    auto top=vertex(o,{x+size*.1f,size*1.03f,z});
    for(int i=0;i<5;++i) {
        int j=(i+1)%5;o->addFace(i,j,j+5,i+5,grey[i%4]);o->addTriangle(i+5,j+5,top,grey[(i+1)%4]);
    }
    put(o);
}
inline void makeWater() {
    water=paint(0x088C9C);
    water->shadingMode=ShadingMode::WATER_REFLECT;
    water->alpha=160;water->specular=30;water->waterYBias=0;
    // Tile the plane for useful frustum culling and bounded near-plane clipping.
    for(int z=-3;z<3;++z) for(int x=-3;x<3;++x) {
        auto* o=new Object;
        o->cullingMode=CullingMode::NO_CULLING;
        o->zBias=-2; // Retained if this example is switched back to a depth buffer.
        o->noWriteZBuffer=true; // Background sort band: flat sea precedes all above-water land.
        constexpr float step=1600;
        vertex(o,{x*step,0,z*step});vertex(o,{(x+1)*step,0,z*step});
        vertex(o,{(x+1)*step,0,(z+1)*step});vertex(o,{x*step,0,(z+1)*step});
        o->addFace(0,1,2,3,water);put(o);
    }
}
inline std::array<uint16_t,16*16> glow{},ring{},disc{};
inline Texture glowTexture{16,16,glow.data()},ringTexture{16,16,ring.data()},discTexture{16,16,disc.data()};
inline Material glowMaterial{0xffff,&glowTexture},ringMaterial{0xffff,&ringTexture},discMaterial{0xffff,&discTexture};
inline void makeSun() {
    // The game's mirrored-quarter sprite technique: only 1.5 KiB for three
    // 32x32 effects, generated once. Pixel storage stays immutable in flight.
    for(int y=0;y<16;++y) for(int x=0;x<16;++x) {
        float dx=x-15.5f,dy=y-15.5f,r=std::sqrt(dx*dx+dy*dy)/16;
        float g=std::max(0.f,1-r);g=g*g*.33f;
        float h=std::max(0.f,1-std::abs(r-.65f)/.18f)*.3f;
        float d=std::clamp((.57f-r)*20,0.f,1.f);
        glow[y*16+x]=rgb(unsigned(255*g),unsigned(226*g),unsigned(151*g));
        ring[y*16+x]=rgb(unsigned(132*h),unsigned(203*h),unsigned(235*h));
        disc[y*16+x]=rgb(unsigned(255*d),unsigned(246*d),unsigned(202*d));
    }
    constexpr uint8_t mirror=Sprite2D::MIRROR_X|Sprite2D::MIRROR_Y;
    LensFlare::Element elements[]={
        {&glowMaterial,32,32,0,1,BlendMode::BLEND_ADD,4,mirror},
        {&discMaterial,32,32,0,1,BlendMode::BLEND_ADD,1,mirror},
        {&ringMaterial,32,32,-.10f,.45f,BlendMode::BLEND_ADD,1,mirror},
        {&glowMaterial,32,32,.19f,.45f,BlendMode::BLEND_ADD,1,mirror},
        {&ringMaterial,32,32,.36f,.8f,BlendMode::BLEND_ADD,2,mirror},
        {&glowMaterial,32,32,.58f,.35f,BlendMode::BLEND_ADD,1,mirror},
        {&ringMaterial,32,32,.77f,.55f,BlendMode::BLEND_ADD,1,mirror}};
    flare=new LensFlare(scene,elements,7,0,{.20f,.18f,.963f},6);
}
inline void update(float seconds) {
    time=std::fmod(time+seconds,3600.f);
    float angle=time*(pi/7.5f); // One orbit every 15 seconds.
    const float radius=1800+260*std::sin(time*pi/15); // Gentle 30-second approach and retreat.
    camera.setPosition(int(std::sin(angle)*radius),int(180+30*std::sin(angle*2)),int(-std::cos(angle)*radius));
    camera.lookAt(Vector3{0,160,0});
    // Track the projected water plane at the island centre. This includes
    // camera height, pitch and distance, rather than a fixed pixel offset.
    const Vector3 waterView=camera.transformDirection(Vector3{0,0,0}-camera.position);
    int32_t cx,sx,cy,sy,cz,sz;camera.getRotationMatrix(cx,sx,cy,sy,cz,sz);
    const int horizon=256/2+int(sx*camera.fovFactor/1024.f);
    const float shore=256*.5f-waterView.y*camera.fovFactor/waterView.z;
    water->waterYBias=uint8_t(std::clamp(int(std::lround(2*(shore-horizon))),0,255));
    water->waterReflectionMaxY=int16_t(std::lround(shore));
    scene->waterTime=time;
    flare->prepare(&camera,240,256);
}
inline void afterRender(float seconds) {
    flare->update(seconds,240,256);
}
inline void init(Scene& target) {
    scene=&target;
    camera.setFOV(64.f,240);camera.nearPlane=64;camera.farPlane=8500;
    scene->setCamera(&camera);scene->setClearBuffer(true);
    for(int y=0;y<256;++y) {
        float t=std::min(1.f,y/175.f);
        sky[y]=rgb(unsigned(30+155*t),unsigned(130+98*t),unsigned(210+30*t));
    }
    scene->backgroundGradientColors=sky;
    makeBeach();
    palms[0]=makePalm(-235,-40,505,.25f);
    palms[1]=makePalm(105,110,600,2.4f);
    palms[2]=makePalm(280,-70,395,1.2f);
    palms[3]=makePalm(-50,-200,420,4.5f);
    palms[4]=makePalm(-295,180,380,3.5f);
    makeRock(550,90,68,.2f);makeRock(605,125,44,.7f);
    makeRock(-470,-240,80,.5f);makeRock(-535,-225,46,.3f);
    makeWater();makeSun();Labels::add(target);update(0);
}
}
