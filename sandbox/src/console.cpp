#include "console.h"

#include "script.h"

static inline void WideToAnsi(const std::wstring& wide_string, std::string& ansi_string)
{
	ansi_string.assign(wide_string.begin(), wide_string.end());
}
static inline void AnsiToWide(const std::string& ansi_string, std::wstring& wide_string)
{
	wide_string.assign(ansi_string.begin(), ansi_string.end());
}

Console::Console(scythe::Renderer * renderer, scythe::Font * font,
				 scythe::Shader * gui_shader, scythe::Shader * text_shader,
				 float bottom, float text_height, float velocity, float aspect_ratio)
: scythe::Console(renderer, font, gui_shader, text_shader, bottom, text_height, velocity, aspect_ratio)
, parser_(nullptr)
{
}
Console::~Console()
{
}
void Console::set_parser(console_script::Parser * parser)
{
	parser_ = parser;
}
void Console::RecognizeString()
{
	if (parser_)
	{
		// Transform input from wide to ansi
		std::string result_string;
		std::string ansi_string;
		std::wstring wide_string;
		WideToAnsi(input_string(), ansi_string);
		if (parser_->Evaluate(ansi_string, &result_string))
		{
			// Print result string
			AnsiToWide(result_string, wide_string);
			AddString(wide_string);
		}
		else
		{
			// Transform error message from ansi to wide
			AnsiToWide(parser_->error(), wide_string);
			AddString(wide_string);
		}
	}
}