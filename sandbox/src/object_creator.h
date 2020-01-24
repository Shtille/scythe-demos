#ifndef __OBJECT_CREATOR_H__
#define __OBJECT_CREATOR_H__

/**
 * Object creator class interface
 */
class IObjectCreator {
public:
	virtual ~IObjectCreator() = default;
	
	virtual void CreateSphere(float pos_x, float pos_y, float pos_z, float radius,
		float color_x, float color_y, float color_z, float mass) = 0;

	virtual void CreateBox(float pos_x, float pos_y, float pos_z,
		float extent_x, float extent_y, float extent_z,
		float color_x, float color_y, float color_z, float mass) = 0;
};

#endif