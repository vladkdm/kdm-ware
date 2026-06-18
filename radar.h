#pragma once
#include "interfaces.h"
#include <vector>

enum class RadarEntType { Enemy, Weapon };

struct RadarEntity {
    Vector3 origin;
    RadarEntType type;
};

class Radar {
public:
    void Render();

private:
    std::vector<RadarEntity> CollectEntities();
};
