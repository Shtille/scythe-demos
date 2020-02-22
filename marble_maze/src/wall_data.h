#ifndef __WALL_DATA_H__
#define __WALL_DATA_H__

#include "math/vector3.h"

#include <vector>

struct WallBaseData {
	float start_x;
	float start_y;
	float end_x;
	float end_y;
};
struct WallData {
	scythe::Vector3 center;
	scythe::Vector3 sizes;
};

void GetWallData(std::vector<WallData> * wall_data, float cell_size, float base_height, float wall_width, float wall_height);

#endif