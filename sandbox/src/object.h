#ifndef __OBJECT_H__
#define __OBJECT_H__

#include "node.h"

/**
 * Base class for objects. Stores node and color.
 */
class Object
{
public:
	Object(scythe::Node * node, const scythe::Vector3& color);
	Object(Object && other);
	virtual ~Object();

	scythe::Node * node() const;
	const scythe::Vector3& color() const;

private:

	scythe::Node * node_;
	scythe::Vector3 color_;
};

#endif