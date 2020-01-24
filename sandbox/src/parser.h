#ifndef __PARSER_H__
#define __PARSER_H__

#include "object_creator.h"

#include "common/non_copyable.h"

#include "script.h"

/**
 * Wrapper for console_script::Parser class.
 * Initializes all the necessary functions.
 */
class Parser final : public scythe::NonCopyable {
public:
	Parser(IObjectCreator * object_creator);
	~Parser();

	console_script::Parser * object();

protected:
	void SetupFunctions();

private:
	console_script::Parser parser_;
	IObjectCreator * object_creator_;
};

#endif