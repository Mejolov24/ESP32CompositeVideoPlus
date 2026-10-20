#include "Scene.hpp"
#include "ObjLoader.h"
#include "EnvironmentMapping.hpp"
#include "Primitives.hpp"
#include "Assets.hpp"
#include "Labels.hpp"
#include <cmath>
#include <cstring>
#include <array>
#if defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#endif

namespace Workshop {
using namespace Renderer;
inline constexpr float pi=3.14159265359f;
inline Scene* scene=nullptr;
inline Camera camera;
inline DirectionalLight key({220,52,0},{205,231,255},255);
inline AmbientLight ambient({130,123,150});
inline float time=0;
inline Object* car=nullptr;
inline std::array<Object*,2> windows{};
inline uint16_t sky[256];
inline Texture livery(256,256,reinterpret_cast<uint16_t*>(const_cast<uint8_t*>(Assets::liveryIndices)),false,0,false,CLAMP,const_cast<uint16_t*>(Assets::liveryPalette));
inline Texture environment(128,64,const_cast<uint16_t*>(Assets::environment));
inline std::vector<Material*> materials;
inline uint16_t rgb(unsigned hex) {return uint16_t(((hex>>19)&31)<<11|((hex>>10)&63)<<5|((hex>>3)&31));}
inline Material* paint(unsigned hex) {auto* m=new Material(rgb(hex));m->shadingMode=ShadingMode::UNLIT;return m;}
inline void put(Object* o,bool background=false) {
 o->noWriteZBuffer=background;o->calculateBoundingBox();o->cachePositions();scene->addObject(o);
}
inline void quad(Object* o,Vector3 a,Vector3 b,Vector3 c,Vector3 d,Material* m) {
 const int n=int(o->vertices.size());
 for(auto p:{a,b,c,d})o->addVertex({p,{0,0},{0,1024,0}});
 o->addFace(n,n+1,n+2,n+3,m);
}
inline void box(int x,int y,int z,int w,int h,int d,Material* m,bool background=false) {
 auto* o=Primitives::createCube(w,h,d,m);o->setPosition(x,y,z);put(o,background);
}
inline void makeRoom() {
 auto* floor=paint(0x19283A);auto* wall=paint(0x141C2F);auto* wallAlt=paint(0x231733);
 auto* frame=paint(0x26354A);auto* cyan=paint(0x32D7EC);auto* pink=paint(0xE542BC);auto* white=paint(0xA4C5D8);
 auto* room=new Object;room->cullingMode=CullingMode::NO_CULLING;
 quad(room,{-2300,0,-2300},{2300,0,-2300},{2300,0,2300},{-2300,0,2300},floor);
 for(int i=0;i<8;++i) {
  float a=2*pi*i/8,b=2*pi*(i+1)/8;
  int x=int(1900*std::sin(a)),z=int(1900*std::cos(a));
  int xx=int(1900*std::sin(b)),zz=int(1900*std::cos(b));
  quad(room,{x,0,z},{xx,0,zz},{xx,1000,zz},{x,1000,z},i%2?wall:wallAlt);
 }
 put(room,true);
 // Floor lanes, octagonal service platform and inset rim share a background
 // band; no depth buffer and no coplanar painter fights with the car.
 auto* lines=new Object;lines->cullingMode=CullingMode::NO_CULLING;
 auto* grid=paint(0x2D4E60);
 for(int n=-4;n<=4;++n) {
  int q=n*440;
  quad(lines,{q-2,1,-1800},{q+2,1,-1800},{q+2,1,1800},{q-2,1,1800},grid);
  quad(lines,{-1800,1,q-2},{1800,1,q-2},{1800,1,q+2},{-1800,1,q+2},grid);
 }
 put(lines,true);lines=new Object;lines->cullingMode=CullingMode::NO_CULLING;
 auto* deck=paint(0x2A3C51);auto* edge=paint(0x131F2F);
 for(int i=0;i<32;++i) {
  float a=2*pi*i/32,b=2*pi*(i+1)/32;
  auto pos=[](float a,float radius,int y){return Vector3{int(radius*std::sin(a)),y,int(radius*std::cos(a))};};
  quad(lines,{0,18,0},pos(a,720,18),pos(b,720,18),{0,18,0},deck);
  quad(lines,pos(a,720,0),pos(b,720,0),pos(b,720,18),pos(a,720,18),edge);
  quad(lines,pos(a,685,19),pos(a,694,19),pos(b,694,19),pos(b,685,19),i%8<4?cyan:pink);
 }
 put(lines,true);lines=new Object;lines->cullingMode=CullingMode::NO_CULLING;
 // Inset from the car's roughly 398-by-1019 footprint on the platform.
 auto* shadow=paint(0x121D2D);
 quad(lines,{-180,20,-465},{180,20,-465},{180,20,465},{-180,20,465},shadow);
 put(lines,true);
 // Wall-mounted workshop ribs stay outside the camera orbit, so the car
 // remains unobstructed while different light banks pass behind it.
 for(int i=0;i<8;++i) {
  float a=2*pi*i/8;int x=int(1780*std::sin(a)),z=int(1780*std::cos(a));
  box(x,425,z,45,850,45,frame,true);
  box(int(x*.992f),435,int(z*.992f),13,630,13,i%2?pink:cyan,true);
  box(int(x*.86f),855,int(z*.86f),220,15,120,white,true);
 }
}
inline Object* extract(Object* source,Material* material) {
 auto* o=new Object;std::vector<int> map(source->vertices.size(),-1);
 for(const auto& t:source->triangles)if(t.material==material) {
  uint16_t ids[3];int j=0;
  for(auto i:{t.v1,t.v2,t.v3}) {
   if(map[i]<0){map[i]=int(o->vertices.size());o->addVertex(source->vertices[i]);}
   ids[j++]=uint16_t(map[i]);
  }
  o->addTriangle(ids[0],ids[1],ids[2],material);
 }
 o->setPosition(0,52,0);return o;
}
inline void loadCar() {
 static char textureName[]="outrun.png";livery.name=textureName;livery.bilinear=false;
 environment.bilinear=false;
 std::vector<Texture*> textures{&livery};
 Loader::LoadMtlData(Assets::mtl,&materials,&textures);
 auto* imported=Loader::LoadFromObjData(Assets::obj,nullptr,&materials,1.f/64.f);
 // Asset coordinate conversion: reverse X to correct the mirrored livery,
 // and orient its inward-authored normals outward. The reflection reverses
 // winding as well, so the original index order now faces outward. Keep UV
 // islands intact: flipping the atlas would move tyre and body regions.
 for(auto& v:imported->vertices) {
  v.position.x=-v.position.x;
  v.normal={v.normal.x,-v.normal.y,-v.normal.z};
 }
 car=new Object;
 unsigned window=0;
 for(auto* m:materials) {
  if(std::strstr(m->name,"Window")) {
   m->diffuseMap=&environment;m->shadingMode=ShadingMode::UNLIT;
   m->perspectiveCorrect=true;
   if(window<2)windows[window++]=extract(imported,m);
  } else {
   const bool wheel=std::strcmp(m->name,"01_-_Default.004")==0 ||
                    std::strcmp(m->name,"01_-_Default.005")==0 ||
                    std::strcmp(m->name,"01_-_Default.006")==0;
   m->shadingMode=wheel?ShadingMode::UNLIT:ShadingMode::PHONG;
   m->diffuse=wheel?255:165;m->specular=wheel?0:205;
   m->specularExponent=wheel?0:32;m->perspectiveCorrect=true;
   auto* part=extract(imported,m);
   const int base=int(car->vertices.size());
   for(const auto& v:part->vertices)car->addVertex(v);
   for(const auto& t:part->triangles)car->addTriangle(base+t.v1,base+t.v2,base+t.v3,m);
   delete part;
  }
 }
 delete imported;car->setPosition(0,52,0);put(car);
 for(auto* o:windows)if(o)put(o);
 std::printf("OBJ loaded: %u body/wheel tris, %u + %u window tris; %u body vertices\n",
  unsigned(car->triangles.size()),unsigned(windows[0]->triangles.size()),unsigned(windows[1]->triangles.size()),unsigned(car->vertices.size()));
}
inline void update(float dt) {
 time=std::fmod(time+dt,3600.f);float a=.7f+time*2*pi/20.f;
 float radius=1460+80*std::sin(time*2*pi/40.f);
 camera.setPosition(int(radius*std::sin(a)),480+int(60*std::sin(a*2)),int(radius*std::cos(a)));
 camera.lookAt({0,175,0});
 for(auto* o:windows)if(o) {
  int anchor=0;bool first=true;
  for(auto& v:o->vertices) {
   v.uv=environmentReflectionUV(v.position+o->position,v.normal,camera.position);
   if(first){anchor=v.uv.x;first=false;}
   else v.uv.x=unwrapEnvironmentU(v.uv.x,anchor);
  }
 }
}
inline void init(Scene& target) {
 scene=&target;camera.setFOV(62.0f,240);camera.nearPlane=96;camera.farPlane=6000;
 scene->setCamera(&camera);scene->setDirectionalLight(&key);scene->setAmbientLight(&ambient);
 scene->setClearBuffer(true);
 for(int y=0;y<256;++y)sky[y]=rgb(0x0F1827);
 scene->backgroundGradientColors=sky;
#if defined(ESP_PLATFORM) && defined(CONFIG_SPIRAM)
 // Mesh arrays are static after startup. Keep larger allocations in PSRAM
 // so internal memory remains available for the hot transform scratch.
 heap_caps_malloc_extmem_enable(1024);
#endif
 makeRoom();loadCar();
#if defined(ESP_PLATFORM) && defined(CONFIG_SPIRAM)
 heap_caps_malloc_extmem_enable(CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL);
#endif
 Labels::add(*scene);update(0);
}
}
