#ifndef __CONSTANTS_H__
#define __CONSTANTS_H__

const float kEarthRadius = 6371000.0f;
const float kEarthAtmosphereHeight = 100000.0f;
const float kEarthCloudsHeight = 12000.0f;
const float kEarthAtmosphereRadius = kEarthRadius + kEarthAtmosphereHeight;
const float kEarthCloudsRadius = kEarthRadius + kEarthCloudsHeight;

#endif