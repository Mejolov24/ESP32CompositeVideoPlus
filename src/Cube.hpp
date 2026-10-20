#pragma once
#include "Scene.hpp"
#include "Primitives.hpp"
#include <cmath>

// All example content lives here. Replace these two functions for a new demo.
namespace Cube {
inline Renderer::Camera camera;
inline Renderer::Object* mesh = nullptr;
inline float pitch = 20.0f, yaw = 30.0f, roll = 0.0f;

inline void init(Renderer::Scene& scene) {
    camera.setPosition(0, 0, -550);
    camera.setRotation(0, 0, 0);
    camera.setFOV(60.0f, 90);
    camera.nearPlane = 16;
    camera.farPlane = 2000;
    scene.setCamera(&camera);
    scene.setClearBuffer(true);
    scene.setBackcolor(0x0841);
    mesh = Primitives::createDebugCube(200, 200, 200);
    mesh->setRotation(20, 30, 0);
    scene.addObject(mesh);
}

inline void update(float seconds) {
    pitch = std::fmod(pitch + 92.0f * seconds, 360.0f);
    yaw = std::fmod(yaw + 148.0f * seconds, 360.0f);
    roll = std::fmod(roll + 44.0f * seconds, 360.0f);
    mesh->setRotation(int(pitch), int(yaw), int(roll));
}
}
