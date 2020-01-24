#include "parser.h"

Parser::Parser(IObjectCreator * object_creator)
: parser_()
, object_creator_(object_creator)
{
	SetupFunctions();
}
Parser::~Parser()
{

}
console_script::Parser * Parser::object()
{
	return &parser_;
}
void Parser::SetupFunctions()
{
	parser_.AddClassFunction("CreateSphere", &IObjectCreator::CreateSphere, object_creator_);
	parser_.AddClassFunction("CreateBox", &IObjectCreator::CreateBox, object_creator_);
}