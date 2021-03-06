#include "model/mesh.h"
#include "graphics/text.h"
#include "camera.h"
#include "math/matrix3.h"

#include "declare_main.h"

#include <cmath>

/*
PBR shader to use in application
*/
namespace {
	const int kShadowMapSize = 1024;
}

#define APP_NAME PbrApp

class APP_NAME : public scythe::OpenGlApplication
			   , public scythe::DesktopInputListener
{
public:
	APP_NAME()
	: sphere_(nullptr)
	, quad_(nullptr)
	, font_(nullptr)
	, fps_text_(nullptr)
	, camera_manager_(nullptr)
	, light_angle_(0.0f)
	, light_distance_(10.0f)
	, need_update_projection_matrix_(true)
	, show_shadow_texture_(false)
	{
		SetInputListener(this);
	}
	const char* GetTitle() final
	{
		return "Physics Based Rendering";
	}
	const bool IsMultisample() final
	{
		return true;
	}
	void BindShaderConstants()
	{
		env_shader_->Bind();
		env_shader_->Uniform1i("u_texture", 0);

		quad_shader_->Bind();
		quad_shader_->Uniform1i("u_texture", 0);

		blur_shader_->Bind();
		blur_shader_->Uniform1i("u_texture", 0);

		object_shader_->Bind();
		object_shader_->Uniform3f("u_light.color", 1.0f, 1.0f, 1.0f);
		//object_shader_->Uniform3f("u_light.direction", 1.0f, 1.0f, -1.0f);
		object_shader_->Uniform1f("u_shadow_scale", 0.4f);
		object_shader_->Uniform1i("u_diffuse_env_sampler", 0);
		object_shader_->Uniform1i("u_specular_env_sampler", 1);
		object_shader_->Uniform1i("u_preintegrated_fg_sampler", 2);
		object_shader_->Uniform1i("u_albedo_sampler", 3);
		object_shader_->Uniform1i("u_normal_sampler", 4);
		object_shader_->Uniform1i("u_roughness_sampler", 5);
		object_shader_->Uniform1i("u_metal_sampler", 6);
		object_shader_->Uniform1i("u_shadow_sampler", 7);
		object_shader_->Unbind();
	}
	void BindShaderVariables()
	{
	}
	bool Load() final
	{
		// Vertex formats
		scythe::VertexFormat * object_vertex_format;
		{
			scythe::VertexAttribute attributes[] = {
				scythe::VertexAttribute(scythe::VertexAttribute::kVertex, 3),
				scythe::VertexAttribute(scythe::VertexAttribute::kNormal, 3),
				scythe::VertexAttribute(scythe::VertexAttribute::kTexcoord, 2),
				scythe::VertexAttribute(scythe::VertexAttribute::kTangent, 3),
				scythe::VertexAttribute(scythe::VertexAttribute::kBinormal, 3)
			};
			renderer_->AddVertexFormat(object_vertex_format, attributes, _countof(attributes));
		}
		scythe::VertexFormat * quad_vertex_format;
		{
			scythe::VertexAttribute attributes[] = {
				scythe::VertexAttribute(scythe::VertexAttribute::kVertex, 3)
			};
			renderer_->AddVertexFormat(quad_vertex_format, attributes, _countof(attributes));
		}

		// Sphere model
		sphere_ = new scythe::Mesh(renderer_);
		sphere_->CreateSphere(1.0f, 128, 64);
		if (!sphere_->MakeRenderable(object_vertex_format))
			return false;

		// Screen quad model
		quad_ = new scythe::Mesh(renderer_);
		quad_->CreateQuadFullscreen();
		if (!quad_->MakeRenderable(quad_vertex_format))
			return false;
		
		// Load shaders
		const char* object_shader_defines[] = {
			"USE_TANGENT",
			"USE_SHADOW"
		};
		scythe::ShaderInfo object_shader_info(
			"data/shaders/pbr/object_pbr", // base filename
			nullptr, // no vertex filename
			nullptr, // no fragment filename
			nullptr, // array of attribs
			0, // number of attribs
			object_shader_defines, // array of defines
			_countof(object_shader_defines) // number of defines
			);
		if (!renderer_->AddShader(text_shader_, "data/shaders/text")) return false;
		if (!renderer_->AddShader(quad_shader_, "data/shaders/quad")) return false;
		if (!renderer_->AddShader(gui_shader_, "data/shaders/gui_colored")) return false;
		if (!renderer_->AddShader(env_shader_, "data/shaders/skybox")) return false;
		if (!renderer_->AddShader(irradiance_shader_, "data/shaders/pbr/irradiance")) return false;
		if (!renderer_->AddShader(prefilter_shader_, "data/shaders/pbr/prefilter")) return false;
		if (!renderer_->AddShader(integrate_shader_, "data/shaders/pbr/integrate")) return false;
		if (!renderer_->AddShader(object_shader_, object_shader_info)) return false;
		if (!renderer_->AddShader(object_shadow_shader_, "data/shaders/shadows/depth_vsm")) return false;
		if (!renderer_->AddShader(blur_shader_, "data/shaders/blur")) return false;
		
		// Load textures
		const char * cubemap_filenames[6] = {
			"data/textures/skybox/ashcanyon_ft.jpg",
			"data/textures/skybox/ashcanyon_bk.jpg",
			"data/textures/skybox/ashcanyon_up.jpg",
			"data/textures/skybox/ashcanyon_dn.jpg",
			"data/textures/skybox/ashcanyon_rt.jpg",
			"data/textures/skybox/ashcanyon_lf.jpg"
		};
		if (!renderer_->AddTextureCubemap(env_texture_, cubemap_filenames)) return false;
		if (!renderer_->AddTexture(albedo_texture_, "data/textures/pbr/metal/rusted_iron/albedo.png",
								   scythe::Texture::Wrap::kClampToEdge,
								   scythe::Texture::Filter::kTrilinearAniso)) return false;
		if (!renderer_->AddTexture(normal_texture_, "data/textures/pbr/metal/rusted_iron/normal.png",
		 						   scythe::Texture::Wrap::kClampToEdge,
		 						   scythe::Texture::Filter::kTrilinearAniso)) return false;
		if (!renderer_->AddTexture(roughness_texture_, "data/textures/pbr/metal/rusted_iron/roughness.png",
								   scythe::Texture::Wrap::kClampToEdge,
								   scythe::Texture::Filter::kTrilinearAniso)) return false;
		if (!renderer_->AddTexture(metal_texture_, "data/textures/pbr/metal/rusted_iron/metallic.png",
								   scythe::Texture::Wrap::kClampToEdge,
								   scythe::Texture::Filter::kTrilinearAniso)) return false;
		if (!renderer_->AddTexture(fg_texture_, "data/textures/pbr/brdfLUT.png",
								   scythe::Texture::Wrap::kClampToEdge,
								   scythe::Texture::Filter::kTrilinearAniso)) return false;

		// Render targets
		renderer_->CreateTextureCubemap(irradiance_rt_, 32, 32, scythe::Image::Format::kRGB8, scythe::Texture::Filter::kLinear);
		renderer_->CreateTextureCubemap(prefilter_rt_, 512, 512, scythe::Image::Format::kRGB8, scythe::Texture::Filter::kTrilinear);
		renderer_->GenerateMipmap(prefilter_rt_);
		renderer_->AddRenderTarget(integrate_rt_, 512, 512, scythe::Image::Format::kRGB8); // RG16
		renderer_->AddRenderTarget(shadow_color_rt_, kShadowMapSize, kShadowMapSize, scythe::Image::Format::kRG32);
		renderer_->AddRenderDepthStencil(shadow_depth_rt_, kShadowMapSize, kShadowMapSize, 32, 0);
		renderer_->AddRenderTarget(blur_color_rt_, kShadowMapSize, kShadowMapSize, scythe::Image::Format::kRG32);

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

		BakeCubemaps();
		
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
		if (sphere_)
			delete sphere_;
	}
	void Update() final
	{
		const float kFrameTime = GetFrameTime();

		camera_manager_->Update(kFrameTime);

		light_angle_ += 0.1f * kFrameTime;
		scythe::Matrix3 light_rotation;
		scythe::Matrix3::CreateRotationY(light_angle_, &light_rotation);
		light_rotation.GetBackVector(&light_direction_);

		// Light direction = direction to light!
		light_position_.Set(scythe::Vector3(0.0f, 1.0f, 0.0f) + light_distance_ * light_direction_);
		scythe::Matrix4::CreateView(light_rotation, light_position_, &light_view_matrix_);

		// Update matrices
		renderer_->SetViewMatrix(camera_manager_->view_matrix());
		UpdateProjectionMatrix();
		projection_view_matrix_ = renderer_->projection_matrix() * renderer_->view_matrix();

		BindShaderVariables();
	}
	void BakeCubemaps()
	{
		scythe::Matrix4 projection_matrix;
		scythe::Matrix4::CreatePerspective(90.0f, 1.0f, 0.1f, 100.0f, &projection_matrix);

		renderer_->DisableDepthTest();

		// Irradiance cubemap
		renderer_->ChangeTexture(env_texture_);
		irradiance_shader_->Bind();
		irradiance_shader_->Uniform1i("u_texture", 0);
		irradiance_shader_->UniformMatrix4fv("u_projection", projection_matrix);
		for (int face = 0; face < 6; ++face)
		{
			scythe::Matrix4 view_matrix;
			scythe::Matrix4::CreateLookAtCube(scythe::Vector3(0.0f), face, &view_matrix);
			irradiance_shader_->UniformMatrix4fv("u_view", view_matrix);
			renderer_->ChangeRenderTargetsToCube(1, &irradiance_rt_, nullptr, face, 0);
			renderer_->ClearColorBuffer();
			quad_->Render();
		}
		renderer_->ChangeRenderTarget(nullptr, nullptr); // back to main framebuffer
		irradiance_shader_->Unbind();
		renderer_->ChangeTexture(nullptr);

		// Prefilter cubemap
		renderer_->ChangeTexture(env_texture_);
		prefilter_shader_->Bind();
		prefilter_shader_->Uniform1i("u_texture", 0);
		//prefilter_shader_->Uniform1f("u_cube_resolution", (float)prefilter_rt_->width());
		prefilter_shader_->UniformMatrix4fv("u_projection", projection_matrix);
		const int kMaxMipLevels = 5;
		for (int mip = 0; mip < kMaxMipLevels; ++mip)
		{
			float roughness = (float)mip / (float)(kMaxMipLevels - 1);
			prefilter_shader_->Uniform1f("u_roughness", roughness);
			for (int face = 0; face < 6; ++face)
			{
				scythe::Matrix4 view_matrix;
				scythe::Matrix4::CreateLookAtCube(scythe::Vector3(0.0f), face, &view_matrix);
				prefilter_shader_->UniformMatrix4fv("u_view", view_matrix);
				renderer_->ChangeRenderTargetsToCube(1, &prefilter_rt_, nullptr, face, mip);
				renderer_->ClearColorBuffer();
				quad_->Render();
			}
		}
		renderer_->ChangeRenderTarget(nullptr, nullptr); // back to main framebuffer
		prefilter_shader_->Unbind();
		renderer_->ChangeTexture(nullptr);

		// Integrate LUT
		integrate_shader_->Bind();
		renderer_->ChangeRenderTarget(integrate_rt_, nullptr);
		renderer_->ClearColorBuffer();
		quad_->Render();
		renderer_->ChangeRenderTarget(nullptr, nullptr); // back to main framebuffer
		integrate_shader_->Unbind();

		renderer_->EnableDepthTest();
	}
	void RenderEnvironment()
	{
		renderer_->DisableDepthTest();

		renderer_->ChangeTexture(env_texture_);
		env_shader_->Bind();
		env_shader_->UniformMatrix4fv("u_projection", renderer_->projection_matrix());
		env_shader_->UniformMatrix4fv("u_view", renderer_->view_matrix());
		quad_->Render();
		env_shader_->Unbind();
		renderer_->ChangeTexture(nullptr);

		renderer_->EnableDepthTest();
	}
	void RenderObjects(scythe::Shader * shader, bool normal_mode)
	{
		if (normal_mode)
		{
			renderer_->ChangeTexture(irradiance_rt_, 0);
			renderer_->ChangeTexture(prefilter_rt_, 1);
			renderer_->ChangeTexture(fg_texture_, 2);
			renderer_->ChangeTexture(albedo_texture_, 3);
			renderer_->ChangeTexture(normal_texture_, 4);
			renderer_->ChangeTexture(roughness_texture_, 5);
			renderer_->ChangeTexture(metal_texture_, 6);
			renderer_->ChangeTexture(shadow_color_rt_, 7);
		}
	
		renderer_->PushMatrix();
		renderer_->Translate(scythe::Vector3(0.0f, 0.0f, 0.0f));
		shader->UniformMatrix4fv("u_model", renderer_->model_matrix());
		sphere_->Render();
		renderer_->PopMatrix();

		renderer_->PushMatrix();
		renderer_->Translate(scythe::Vector3(2.0f, 0.0f, 0.0f));
		shader->UniformMatrix4fv("u_model", renderer_->model_matrix());
		sphere_->Render();
		renderer_->PopMatrix();

		renderer_->PushMatrix();
		renderer_->Translate(scythe::Vector3(0.0f, 0.0f, 2.0f));
		shader->UniformMatrix4fv("u_model", renderer_->model_matrix());
		sphere_->Render();
		renderer_->PopMatrix();

		if (normal_mode)
		{
			renderer_->ChangeTexture(nullptr, 7);
			renderer_->ChangeTexture(nullptr, 6);
			renderer_->ChangeTexture(nullptr, 5);
			renderer_->ChangeTexture(nullptr, 4);
			renderer_->ChangeTexture(nullptr, 3);
			renderer_->ChangeTexture(nullptr, 2);
			renderer_->ChangeTexture(nullptr, 1);
			renderer_->ChangeTexture(nullptr, 0);
		}
	}
	void ShadowPass()
	{
		// Generate matrix for shadows
		// Ortho matrix is used for directional light sources and perspective for spot ones.
		scythe::Matrix4 depth_projection;
		scythe::Matrix4::CreateOrthographic(10.0f, 10.0f, 0.0f, 20.0f, &depth_projection);
		scythe::Matrix4 depth_projection_view = depth_projection * light_view_matrix_;
		/*
			Native view of bias matrix is:
			    | 0.5 0.0 0.0 0.5 |
			M = | 0.0 0.5 0.0 0.5 |
			    | 0.0 0.0 0.5 0.5 |
			    | 0.0 0.0 0.0 1.0 |
		*/
		scythe::Matrix4 bias_matrix(
			0.5f, 0.0f, 0.0f, 0.5f,
			0.0f, 0.5f, 0.0f, 0.5f,
			0.0f, 0.0f, 0.5f, 0.5f,
			0.0f, 0.0f, 0.0f, 1.0f
		);
		depth_bias_projection_view_matrix_ = bias_matrix * depth_projection_view;

		renderer_->ChangeRenderTarget(shadow_color_rt_, shadow_depth_rt_);
		renderer_->ClearColorAndDepthBuffers();

		object_shadow_shader_->Bind();
		object_shadow_shader_->UniformMatrix4fv("u_projection_view", depth_projection_view);

		RenderObjects(object_shadow_shader_, false);

		object_shadow_shader_->Unbind();

		renderer_->ChangeRenderTarget(nullptr, nullptr);

		const float kBlurScale = 1.0f;
		const float kBlurSize = kBlurScale / static_cast<float>(kShadowMapSize);

		renderer_->DisableDepthTest();
		blur_shader_->Bind();

		// Blur horizontally
		renderer_->ChangeRenderTarget(blur_color_rt_, nullptr);
		renderer_->ChangeTexture(shadow_color_rt_, 0);
		renderer_->ClearColorBuffer();
		blur_shader_->Uniform2f("u_scale", kBlurSize, 0.0f);
		quad_->Render();

		// Blur vertically
		renderer_->ChangeRenderTarget(shadow_color_rt_, nullptr);
		renderer_->ChangeTexture(blur_color_rt_, 0);
		renderer_->ClearColorBuffer();
		blur_shader_->Uniform2f("u_scale", 0.0f, kBlurSize);
		quad_->Render();

		// Back to main framebuffer
		renderer_->ChangeRenderTarget(nullptr, nullptr);

		blur_shader_->Unbind();
		renderer_->EnableDepthTest();
	}
	void RenderScene()
	{
		// First render shadows
		ShadowPass();

		if (show_shadow_texture_)
		{
			quad_shader_->Bind();

			renderer_->ChangeTexture(shadow_color_rt_, 0);
			quad_->Render();
			renderer_->ChangeTexture(nullptr, 0);

			quad_shader_->Unbind();
		}
		else
		{
			// Render objects
			object_shader_->Bind();
			object_shader_->UniformMatrix4fv("u_projection_view", projection_view_matrix_);
			object_shader_->UniformMatrix4fv("u_depth_bias_projection_view", depth_bias_projection_view_matrix_);
			object_shader_->Uniform3fv("u_camera.position", *camera_manager_->position());
			object_shader_->Uniform3fv("u_light.direction", light_direction_);

			RenderObjects(object_shader_, true);

			object_shader_->Unbind();
		}
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
		
		RenderEnvironment();
		RenderScene();
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
		else if (key == scythe::PublicKey::kSpace)
		{
			show_shadow_texture_ = !show_shadow_texture_;
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
			scythe::Matrix4 projection_matrix;
			scythe::Matrix4::CreatePerspective(45.0f, aspect_ratio_, 0.1f, 100.0f, &projection_matrix);
			renderer_->SetProjectionMatrix(projection_matrix);
		}
	}
	
private:
	scythe::Mesh * sphere_;
	scythe::Mesh * quad_;

	scythe::Shader * text_shader_;
	scythe::Shader * quad_shader_;
	scythe::Shader * gui_shader_;
	scythe::Shader * env_shader_;
	scythe::Shader * object_shader_;
	scythe::Shader * object_shadow_shader_;
	scythe::Shader * irradiance_shader_;
	scythe::Shader * prefilter_shader_;
	scythe::Shader * integrate_shader_;
	scythe::Shader * blur_shader_;

	scythe::Texture * env_texture_;
	scythe::Texture * albedo_texture_;
	scythe::Texture * normal_texture_;
	scythe::Texture * roughness_texture_;
	scythe::Texture * metal_texture_;
	scythe::Texture * fg_texture_;
	scythe::Texture * irradiance_rt_;
	scythe::Texture * prefilter_rt_;
	scythe::Texture * integrate_rt_;
	scythe::Texture * shadow_color_rt_;
	scythe::Texture * shadow_depth_rt_;
	scythe::Texture * blur_color_rt_;

	scythe::Font * font_;
	scythe::DynamicText * fps_text_;
	scythe::CameraManager * camera_manager_;
	
	scythe::Matrix4 projection_view_matrix_;
	scythe::Matrix4 depth_bias_projection_view_matrix_;
	scythe::Matrix4 light_view_matrix_;

	scythe::Vector3 light_position_;
	scythe::Vector3 light_direction_;
	float light_angle_;
	const float light_distance_;
	
	bool need_update_projection_matrix_;
	bool show_shadow_texture_;
};

DECLARE_MAIN(APP_NAME);