#include "model/mesh.h"
#include "graphics/text.h"
#include "math/constants.h"

#include "declare_main.h"

#include <cmath>

class AnchorApp : public scythe::OpenGlApplication
			    , public scythe::DesktopInputListener
{
public:
	AnchorApp()
	: sphere_(nullptr)
	, box_(nullptr)
	, font_(nullptr)
	, fps_text_(nullptr)
	{
		SetInputListener(this);
	}
	const char* GetTitle() final
	{
		return "Anchor test";
	}
	const bool IsMultisample() final
	{
		return true;
	}
	void BindShaderConstants()
	{
	}
	void BindShaderVariables()
	{
	}
	bool Load() final
	{
		// Sphere model
		sphere_ = new scythe::Mesh(renderer_);
		sphere_->AddFormat(scythe::VertexAttribute(scythe::VertexAttribute::kVertex, 3));
		sphere_->CreateSphere(1.0f, 128, 64);
		if (!sphere_->MakeRenderable())
			return false;

		// Box model
		box_ = new scythe::Mesh(renderer_);
		box_->AddFormat(scythe::VertexAttribute(scythe::VertexAttribute::kVertex, 3));
		box_->CreateBox(0.1f, 1.0f, 20.0f);
		if (!box_->MakeRenderable())
			return false;
		
		// Load shaders
		const char *attribs[] = {"a_position"};
		if (!renderer_->AddShader(object_shader_, "data/shaders/anchor/object", attribs, _countof(attribs))) return false;
		if (!renderer_->AddShader(text_shader_, "data/shaders/text", attribs, 1)) return false;

		renderer_->AddFont(font_, "data/fonts/GoodDog.otf");
		if (font_ == nullptr)
			return false;

		fps_text_ = scythe::DynamicText::Create(renderer_, 30);
		if (!fps_text_)
			return false;

		// Matrices setup
		scythe::Matrix4 projection;
		scythe::Matrix4::CreatePerspective(90.0f, aspect_ratio_, 0.1f, 100.0f, &projection);
		renderer_->SetProjectionMatrix(projection);

		scythe::Vector3 eye(0.0f, 5.0f, 0.0f), target(-10.0f, 0.0f, 0.0f);
		scythe::Matrix4 view_matrix;
		scythe::Matrix4::CreateLookAt(eye, target, scythe::Vector3::UnitY(), &view_matrix);
		renderer_->SetViewMatrix(view_matrix);

		projection_view_matrix_ = renderer_->projection_matrix() * renderer_->view_matrix();

		// Finally bind constants
		BindShaderConstants();
		
		return true;
	}
	void Unload() final
	{
		if (fps_text_)
			delete fps_text_;
		if (box_)
			delete box_;
		if (sphere_)
			delete sphere_;
	}
	void Update() final
	{
		BindShaderVariables();
	}
	void RenderObject(float angle)
	{
		scythe::Matrix4::CreateRotationY(angle, &rotate_matrix_);

		renderer_->PushMatrix();
		renderer_->MultMatrix(rotate_matrix_);

		object_shader_->UniformMatrix4fv("u_model", renderer_->model_matrix());
		
		box_->Render();
		
		renderer_->PopMatrix();
	}
	void RenderObjects()
	{
		object_shader_->Bind();
		object_shader_->UniformMatrix4fv("u_projection_view", projection_view_matrix_);

		for (int i = 0; i < 4; ++i)
		{
			float angle = scythe::kPi * 0.25f * static_cast<float>(i);
			RenderObject(angle);
		}

		object_shader_->Unbind();
	}
	void RenderInterface()
	{
		renderer_->DisableDepthTest();
		
		// Draw FPS
		text_shader_->Bind();
		text_shader_->Uniform1i("u_texture", 0);
		text_shader_->Uniform4f("u_color", 1.0f, 0.5f, 1.0f, 1.0f);
		fps_text_->SetText(font_, 0.0f, 0.8f, 0.05f, L"fps: %.2f", GetFrameRate());
		fps_text_->Render();
		
		renderer_->EnableDepthTest();
	}
	void Render() final
	{
		renderer_->SetViewport(width_, height_);
		
		renderer_->ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		renderer_->ClearColorAndDepthBuffers();
		
		RenderObjects();

		RenderInterface();
	}
	void OnChar(unsigned short code) final
	{

	}
	void OnKeyDown(scythe::PublicKey key, int mods) final
	{
		if (key == scythe::PublicKey::kF)
		{
			ToggleFullscreen();
		}
		else if (key == scythe::PublicKey::kEscape)
		{
			DesktopApplication::Terminate();
		}
	}
	void OnKeyUp(scythe::PublicKey key, int modifiers) final
	{

	}
	void OnMouseDown(scythe::MouseButton button, int modifiers) final
	{
	}
	void OnMouseUp(scythe::MouseButton button, int modifiers) final
	{
	}
	void OnMouseMove() final
	{
	}
	void OnScroll(float delta_x, float delta_y) final
	{

	}
	void OnSize(int w, int h) final
	{
		DesktopApplication::OnSize(w, h);
	}
	
private:
	scythe::Mesh * sphere_;
	scythe::Mesh * box_;

	scythe::Shader * object_shader_;
	scythe::Shader * text_shader_;

	scythe::Font * font_;
	scythe::DynamicText * fps_text_;
	
	scythe::Matrix4 projection_view_matrix_;
	scythe::Matrix4 rotate_matrix_;
};

DECLARE_MAIN(AnchorApp);