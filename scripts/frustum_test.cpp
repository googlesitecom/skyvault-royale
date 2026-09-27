// Test rapido del frustum con parametros de la camara de menu
#include "render/Renderer.h"
#include <cstdio>

using namespace sv;

int main() {
    Camera cam;
    cam.pos = vec3(1500, 520, 0);
    cam.yaw = PI_F * 0.5f;      // mirando +X (hacia el centro de la isla)
    cam.pitch = -0.28f;
    cam.fovY = degToRad(70.0f);
    cam.aspect = 16.0f / 9.0f;
    cam.nearZ = 0.1f;
    cam.farZ = 4000.0f;
    cam.update();

    const AABB island{vec3(-1900, -100, -1900), vec3(1900, 800, 1900)};
    const AABB center{{-128, 0, -128}, {128, 256, 128}};
    const AABB chunkAtCam{{1372, 400, -128}, {1628, 656, 128}};

    std::printf("isla completa:      %d (esperado 1)\n", (i32)cam.frustum.intersectsAABB(island));
    std::printf("chunk del centro:   %d (esperado 1)\n", (i32)cam.frustum.intersectsAABB(center));
    std::printf("chunk bajo camara:  %d (esperado 1)\n", (i32)cam.frustum.intersectsAABB(chunkAtCam));
    std::printf("punto(0,50,0):      %d (esperado 1)\n",
                (i32)cam.frustum.containsPoint(vec3(0, 50, 0)));
    std::printf("punto(-5000,0,0):   %d (esperado 0)\n",
                (i32)cam.frustum.containsPoint(vec3(-5000, 0, 0)));
    // planos
    const char* names[6] = {"izq", "der", "aba", "arr", "cerca", "lejos"};
    for (i32 i = 0; i < 6; ++i)
        std::printf("plano %s: n=(%.3f,%.3f,%.3f) d=%.1f dist(0,50,0)=%.1f\n",
                    names[i], cam.frustum.p[i].n.x, cam.frustum.p[i].n.y,
                    cam.frustum.p[i].n.z, cam.frustum.p[i].d,
                    cam.frustum.p[i].distance(vec3(0, 50, 0)));
    // forward
    const vec3 f = cam.forward();
    std::printf("forward=(%.3f,%.3f,%.3f)\n", f.x, f.y, f.z);
    return 0;
}
