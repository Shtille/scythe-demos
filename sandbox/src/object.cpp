#include "object.h"

#include "common/sc_delete.h"

Object::Object(scythe::Node * node, const scythe::Vector3& color)
: node_(node)
, color_(color)
{
	// node already has reference count of 1
}
Object::Object(Object && other) // move contructor
: node_(other.node_)
, color_(other.color_)
{
	// Nullify other node, because we don't own it anymore
	other.node_ = nullptr;
}
Object::~Object()
{
	SC_SAFE_RELEASE(node_);
}
scythe::Node * Object::node() const
{
	return node_;
}
const scythe::Vector3& Object::color() const
{
	return color_;
}