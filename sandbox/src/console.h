#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include "ui/console.h"

namespace console_script {
	class Parser;
}

class Console final : public scythe::Console {
public:
	//! Requires gui colored shader to render
	Console(scythe::Renderer * renderer, scythe::Font * font,
			scythe::Shader * gui_shader, scythe::Shader * text_shader,
			float bottom, float text_height, float velocity, float aspect_ratio);
	~Console();

	void set_parser(console_script::Parser * parser);

protected:

	virtual void RecognizeString() final;

private:

	// Console doesn't own the parser instance
	console_script::Parser * parser_;
};

#endif