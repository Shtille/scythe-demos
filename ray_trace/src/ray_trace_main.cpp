#include "model/mesh.h"
#include "graphics/text.h"
#include "camera.h"

#include "declare_main.h"

#include <cmath>

/*
The main concept of creating this application is testing ray trace.
*/

#define APP_NAME RayTraceApp

class APP_NAME : public scythe::OpenGlApplication
			   , public scythe::DesktopInputListener
{
public:
	APP_NAME()
	: quad_(nullptr)
	, font_(nullptr)
	, fps_text_(nullptr)
	, camera_manager_(nullptr)
	, need_update_projection_matrix_(true)
	{
		SetInputListener(this);
	}
	const char* GetTitle() final
	{
		return "Ray trace test";
	}
	const bool IsMultisample() final
	{
		return false;
	}
	void BindShaderConstants()
	{
		const scythe::Vector3 kSpherePosition(0.0f);

		text_shader_->Bind();
		text_shader_->Uniform1i("u_texture", 0);

		cast_shader_->Bind();
		// Plane
		cast_shader_->Uniform3f("u_planes[0].normal", 0.0f, 1.0f, 0.0f);
		cast_shader_->Uniform1f("u_planes[0].d", 1.0f); // height = -d
		// Sphere
		cast_shader_->Uniform3fv("u_spheres[0].position", kSpherePosition);
		cast_shader_->Uniform1f("u_spheres[0].radius", 1.0f);
		// Boxes
		cast_shader_->Uniform3fv("u_boxes[0].min", scythe::Vector3(-1.0f, -1.0f, 2.5f));
		cast_shader_->Uniform3fv("u_boxes[0].max", scythe::Vector3(1.0f, 1.0f, 3.f));

		cast_shader_->Uniform1i("u_num_planes", 1);
		cast_shader_->Uniform1i("u_num_spheres", 1);
		cast_shader_->Uniform1i("u_num_boxes", 1);
		// Light
		scythe::Vector3 light_direction(1.0f, 0.5f, 0.5f);
		light_direction.Normalize();
		cast_shader_->Uniform3fv("u_light.color", scythe::Vector3(1e3));
		cast_shader_->Uniform3fv("u_light.direction", light_direction);
		cast_shader_->Unbind();
	}
	void BindShaderVariables()
	{
	}
	bool Load() final
	{
		// Quad model
		quad_ = new scythe::Mesh(renderer_);
		quad_->AddFormat(scythe::VertexAttribute(scythe::VertexAttribute::kVertex, 3));
		quad_->CreateQuadFullscreen();
		if (!quad_->MakeRenderable())
			return false;
		
		// Load shaders
		if (!renderer_->AddShader(text_shader_, "data/shaders/text")) return false;
		if (!renderer_->AddShader(cast_shader_, "data/shaders/raytrace/raytrace")) return false;

		renderer_->AddFont(font_, "data/fonts/GoodDog.otf");
		if (font_ == nullptr)
			return false;

		fps_text_ = scythe::DynamicText::Create(renderer_, 30);
		if (!fps_text_)
			return false;

		camera_manager_ = new scythe::CameraManager();
		camera_manager_->MakeFree(scythe::Vector3(5.0f), scythe::Vector3(0.0f));

		// Finally bind constants
		BindShaderConstants();
		
		return true;
	}
	void Unload() final
	{
		if (camera_manager_)
			delete camera_manager_;
		if (fps_text_)
			delete fps_text_;
		if (quad_)
			delete quad_;
	}
	void Update() final
	{
		const float kFrameTime = GetFrameTime();

		camera_manager_->Update(kFrameTime);

		// Update matrices
		renderer_->SetViewMatrix(camera_manager_->view_matrix());
		UpdateProjectionMatrix();
		projection_view_matrix_ = renderer_->projection_matrix() * renderer_->view_matrix();
		UpdateRays();

		BindShaderVariables();
	}
	void RenderObjects()
	{
		renderer_->DisableDepthTest();

		//renderer_->ChangeTexture(env_texture);
		cast_shader_->Bind();
		quad_->Render();
		cast_shader_->Unbind();
		//renderer_->ChangeTexture(nullptr);

		renderer_->EnableDepthTest();
	}
	void RenderInterface()
	{
		renderer_->DisableDepthTest();
		
		// Draw FPS
		text_shader_->Bind();
		text_shader_->Uniform4f("u_color", 1.0f, 0.5f, 1.0f, 1.0f);
		fps_text_->SetText(font_, 0.0f, 0.8f, 0.05f, L"fps: %.2f", GetFrameRate());
		fps_text_->Render();
		text_shader_->Unbind();

		renderer_->ChangeTexture(nullptr);
		
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
			DesktopApplication::ToggleFullscreen();
		}
		else if (key == scythe::PublicKey::kEscape)
		{
			DesktopApplication::Terminate();
		}
		else if (key == scythe::PublicKey::kF5)
		{
			renderer_->TakeScreenshot("screenshots");
		}
		else if (key == scythe::PublicKey::kLeft)
		{
			camera_manager_->RotateAroundTargetInY(0.1f);
		}
		else if (key == scythe::PublicKey::kRight)
		{
			camera_manager_->RotateAroundTargetInY(-0.1f);
		}
		else if (key == scythe::PublicKey::kUp)
		{
			camera_manager_->RotateAroundTargetInZ(0.1f);
		}
		else if (key == scythe::PublicKey::kDown)
		{
			camera_manager_->RotateAroundTargetInZ(-0.1f);
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
		// To have correct perspective when resizing
		need_update_projection_matrix_ = true;
	}
	void UpdateProjectionMatrix()
	{
		if (need_update_projection_matrix_ || camera_manager_->animated())
		{
			need_update_projection_matrix_ = false;
			scythe::Matrix4 perspective_matrix;
			scythe::Matrix4::CreatePerspective(45.0f, aspect_ratio_, 0.1f, 100.0f, &perspective_matrix);
			renderer_->SetProjectionMatrix(perspective_matrix);
		}
	}
	void UpdateRays()
	{
		const scythe::Matrix4& proj = renderer_->projection_matrix();
		const scythe::Matrix4& view = renderer_->view_matrix();
		scythe::Matrix4 inverse_proj;
		proj.Invert(&inverse_proj);
		scythe::Matrix4 inverse_view;
		view.Invert(&inverse_view);
		scythe::Vector2 ndc[4] = {
			scythe::Vector2(-1.0f, -1.0f),
			scythe::Vector2( 1.0f, -1.0f),
			scythe::Vector2(-1.0f,  1.0f),
			scythe::Vector2( 1.0f,  1.0f)
		};
		scythe::Vector3 rays[4];
		for (int i = 0; i < 4; ++i)
		{
			// Step 2: 4d Homogeneous Clip Coordinates ( range [-1:1, -1:1, -1:1, -1:1] )
			scythe::Vector4 ray_clip(
				ndc[i].x,
				ndc[i].y,
				-1.0, // We want our ray's z to point forwards - this is usually the negative z direction in OpenGL style.
				1.0
				);
			// Step 3: 4d Eye (Camera) Coordinates ( range [-x:x, -y:y, -z:z, -w:w] )
			scythe::Vector4 ray_eye = inverse_proj * ray_clip;
			// Now, we only needed to un-project the x,y part, so let's manually set the z,w part to mean "forwards, and not a point".
			ray_eye.z = -1.0f;
			ray_eye.w = 0.0f;
			// Step 4: 4d World Coordinates ( range [-x:x, -y:y, -z:z, -w:w] )
			inverse_view.TransformVector(scythe::Vector3(ray_eye.x, ray_eye.y, ray_eye.z), &rays[i]);
			rays[i].Normalize();
		}
		cast_shader_->Bind();
		cast_shader_->Uniform3fv("u_eye", *camera_manager_->position());
		cast_shader_->Uniform3fv("u_ray00", rays[0]);
		cast_shader_->Uniform3fv("u_ray10", rays[1]);
		cast_shader_->Uniform3fv("u_ray01", rays[2]);
		cast_shader_->Uniform3fv("u_ray11", rays[3]);
		cast_shader_->Unbind();
	}
	
private:
	scythe::Mesh * quad_;

	scythe::Shader * text_shader_;
	scythe::Shader * cast_shader_;

	scythe::Font * font_;
	scythe::DynamicText * fps_text_;
	scythe::CameraManager * camera_manager_;
	
	scythe::Matrix4 projection_view_matrix_;
	
	bool need_update_projection_matrix_;
};

DECLARE_MAIN(APP_NAME);