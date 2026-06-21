#pragma once

class BeamRenderer;
class CylinderRenderer;
class DistortionRenderer;
class ElectricOrbStrikeRenderer;
class OrbitRibbonRenderer;
class ParticleRenderer;
class RingRenderer;
class SpearRenderer;
class TrailRenderer;

struct AppVfxRendererSet {
    ParticleRenderer* particle = nullptr;
    TrailRenderer* trail = nullptr;
    BeamRenderer* beam = nullptr;
    DistortionRenderer* distortion = nullptr;
    RingRenderer* ring = nullptr;
    SpearRenderer* spear = nullptr;
    OrbitRibbonRenderer* orbitRibbon = nullptr;
    CylinderRenderer* cylinder = nullptr;
    ElectricOrbStrikeRenderer* electricOrbStrike = nullptr;

    bool IsValid() const {
        return particle != nullptr &&
            trail != nullptr &&
            beam != nullptr &&
            distortion != nullptr &&
            ring != nullptr &&
            spear != nullptr &&
            orbitRibbon != nullptr &&
            cylinder != nullptr &&
            electricOrbStrike != nullptr;
    }
};
