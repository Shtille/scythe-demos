#include "model/mesh.h"
#include "graphics/text.h"
#include "camera.h"
#include "common/string_format.h"
#include "common/sc_delete.h"
#include "math/frustum.h"
#include "math/matrix3.h"

#include "declare_main.h"

#include <cmath>

/*
The main concept of creating this application is testing cubemap edge artefacts during accessing mipmaps.
*/
namespace {
	const int kShadowMapSize = 1024;
	const U32 kMaxCSMSplits = 4;
	const U32 kNumSplits = 3;
	const float kSplitLambda = 0.5f;
}

/**
 * Cascaded shadow maps demo.
 * Keys:
 * - C - switch color mode to show different shadow levels
 * - B - enable/disable blur
 */
class CascadedShadowsApp
: public scythe::OpenGlApplication
, public scythe::DesktopInputListener
{
public:
	CascadedShadowsApp()
	: quad_(nullptr)
	, cube_(nullptr)
	, font_(nullptr)
	, fps_text_(nullptr)
	, camera_distance_(10.0f)
	, camera_alpha_(0.0f)
	, camera_theta_(0.5f)
	, cos_camera_alpha_(1.0f)
	, sin_camera_alpha_(0.0f)
	, light_direction_(scythe::Vector3(5.0f, 2.0f, 2.0f).Normalize())
	, fov_degrees_(45.0f)
	, z_near_(0.1f)
	, z_far_(20.0f)
	, need_update_projection_matrix_(true)
	, need_update_view_matrix_(true)
	, need_update_frustum_(true)
	, show_color_(false)
	, use_blur_(false)
	, is_vsm_(true)
	{
		SetInputListener(this);
	}
	const char* GetTitle() final
	{
		return "Shadows test";
	}
	const bool IsMultisample() final
	{
		return true;
	}
	void BindShaderConstants()
	{
		const scythe::Vector3 kLightColor(1.0f);

		blur_shader_->Bind();
		blur_shader_->Uniform1i("u_texture", 0);

		object_shader_->Bind();
		object_shader_->Uniform3fv("u_light.color", kLightColor);
		object_shader_->Uniform3fv("u_light.direction", light_direction_);
		const int array_units[] = {0, 1, 2, 3};
		static_assert(_countof(array_units) == kMaxCSMSplits, "Array units count mismatch");
		object_shader_->Uniform1iv("u_shadow_samplers", array_units, kNumSplits);
		object_shader_->Unbind();
	}
	void BindShaderVariables()
	{
	}
	bool Load() final
	{
		// Vertex formats
		scythe::VertexFormat * quad_vertex_format;
		{
			scythe::VertexAttribute attributes[] = {
				scythe::VertexAttribute(scythe::VertexAttribute::kVertex, 3)
			};
			renderer_->AddVertexFormat(quad_vertex_format, attributes, _countof(attributes));
		}
		scythe::VertexFormat * object_vertex_format;
		{
			scythe::VertexAttribute attributes[] = {
				scythe::VertexAttribute(scythe::VertexAttribute::kVertex, 3),
				scythe::VertexAttribute(scythe::VertexAttribute::kNormal, 3)
			};
			renderer_->AddVertexFormat(object_vertex_format, attributes, _countof(attributes));
		}

		// Quad model
		quad_ = new scythe::Mesh(renderer_);
		quad_->CreateQuadFullscreen();
		if (!quad_->MakeRenderable(quad_vertex_format))
			return false;

		// Cube model
		cube_ = new scythe::Mesh(renderer_);
		cube_->CreateCube();
		if (!cube_->MakeRenderable(object_vertex_format))
			return false;
		
		// Load shaders
		if (!renderer_->AddShader(text_shader_, "data/shaders/text")) return false;
		if (!renderer_->AddShader(blur_shader_, "data/shaders/blur")) return false;
		if (is_vsm_)
		{
			std::string num_splits_string = scythe::string_format("#define NUM_SPLITS %u", kNumSplits);
			const char* object_shader_defines[] = {
				"USE_CSM",
				num_splits_string.c_str(),
			};
			scythe::ShaderInfo object_shader_info(
				"data/shaders/shadows/object_csm_vsm", // base filename
				nullptr, // no vertex filename
				nullptr, // no fragment filename
				nullptr, // array of attribs
				0, // number of attribs
				object_shader_defines, // array of defines
				_countof(object_shader_defines) // number of defines
				);
			if (!renderer_->AddShader(object_shader_, object_shader_info)) return false;
			if (!renderer_->AddShader(object_shadow_shader_, "data/shaders/shadows/depth_vsm")) return false;
		}
		else
		{
			std::string num_splits_string = scythe::string_format("#define NUM_SPLITS %u", kNumSplits);
			const char* object_shader_defines[] = {
				"USE_CSM",
				num_splits_string.c_str(),
			};
			scythe::ShaderInfo object_shader_info(
				"data/shaders/shadows/object_csm", // base filename
				nullptr, // no vertex filename
				nullptr, // no fragment filename
				nullptr, // array of attribs
				0, // number of attribs
				object_shader_defines, // array of defines
				_countof(object_shader_defines) // number of defines
				);
			if (!renderer_->AddShader(object_shader_, object_shader_info)) return false;
			if (!renderer_->AddShader(object_shadow_shader_, "data/shaders/shadows/object_shadow")) return false;
		}

		// Render targets
		if (is_vsm_)
		{
			for (U32 i = 0; i < kNumSplits; ++i)
				renderer_->AddRenderTarget(shadow_color_rts_[i], kShadowMapSize, kShadowMapSize, scythe::Image::Format::kRG32);
			renderer_->AddRenderDepthStencil(shadow_depth_rts_[0], kShadowMapSize, kShadowMapSize, 32, 0);
			renderer_->AddRenderTarget(blur_color_rt_, kShadowMapSize, kShadowMapSize, scythe::Image::Format::kRG32);
		}
		else
		{
			for (U32 i = 0; i < kNumSplits; ++i)
				renderer_->CreateTextureDepth(shadow_depth_rts_[i], kShadowMapSize, kShadowMapSize, 32);
		}

		// Fonts
		renderer_->AddFont(font_, "data/fonts/GoodDog.otf");
		if (font_ == nullptr)
			return false;

		fps_text_ = scythe::DynamicText::Create(renderer_, 30);
		if (!fps_text_)
			return false;

		UpdateCameraOrientation();
		UpdateCameraPosition();

		// Since we use light basis also for view matrix we should use inverse direction vector
		scythe::Matrix3::CreateBasis(-light_direction_, scythe::Vector3::UnitY(), &light_basis_);
		light_basis_.Invert(&light_basis_inverse_);
		CalculateSplitDistances();

		// Finally bind constants
		BindShaderConstants();
		
		return true;
	}
	void Unload() final
	{
		SC_SAFE_DELETE(fps_text_);
		SC_SAFE_RELEASE(quad_);
		SC_SAFE_RELEASE(cube_);
	}
	void Update() final
	{
		const float kFrameTime = GetFrameTime();

		UpdateCamera();

		// Update matrices
		UpdateProjectionMatrix();
		UpdateViewMatrix();
		projection_view_matrix_ = renderer_->projection_matrix() * renderer_->view_matrix();
		UpdateFrustum();

		UpdateLightMatrices();

		BindShaderVariables();
	}
	void RenderObjects(scythe::Shader * shader)
	{
		// // Render cube
		// renderer_->PushMatrix();
		// shader->UniformMatrix4fv("u_model", renderer_->model_matrix());
		// cube_->Render();
		// renderer_->PopMatrix();

		// Render wall 1
		renderer_->PushMatrix();
		renderer_->Translate(0.0f, 0.0f, 0.0f);
		renderer_->Scale(1.0f, 1.0f, 10.0f);
		shader->UniformMatrix4fv("u_model", renderer_->model_matrix());
		cube_->Render();
		renderer_->PopMatrix();
		// Render wall 2
		renderer_->PushMatrix();
		renderer_->Translate(0.0f, 0.0f, 0.0f);
		renderer_->Scale(10.0f, 1.0f, 1.0f);
		shader->UniformMatrix4fv("u_model", renderer_->model_matrix());
		cube_->Render();
		renderer_->PopMatrix();

		// Render floor
		renderer_->PushMatrix();
		renderer_->Translate(0.0f, -2.0f, 0.0f);
		renderer_->Scale(10.0f, 1.0f, 10.0f);
		shader->UniformMatrix4fv("u_model", renderer_->model_matrix());
		cube_->Render();
		renderer_->PopMatrix();
	}
	void ShadowPass()
	{
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

		const float kBlurScale = 1.0f;
		const float kBlurSize = kBlurScale / static_cast<float>(kShadowMapSize);

		for (U32 i = 0; i < kNumSplits; ++i)
		{
			scythe::Matrix4 depth_projection_view = light_projection_matrices_[i] * light_view_matrices_[i];
			depth_bias_projection_view_matrices_[i] = bias_matrix * depth_projection_view;

			// Render shadows
			if (is_vsm_)
				renderer_->ChangeRenderTarget(shadow_color_rts_[i], shadow_depth_rts_[0]);
			else
				renderer_->ChangeRenderTarget(nullptr, shadow_depth_rts_[i]);
			renderer_->ClearColorAndDepthBuffers();

			object_shadow_shader_->Bind();
			object_shadow_shader_->UniformMatrix4fv("u_projection_view", depth_projection_view);

			RenderObjects(object_shadow_shader_);

			object_shadow_shader_->Unbind();

			renderer_->ChangeRenderTarget(nullptr, nullptr);

			if (use_blur_)
			{
				renderer_->DisableDepthTest();
				blur_shader_->Bind();

				// Blur horizontally
				renderer_->ChangeRenderTarget(blur_color_rt_, nullptr);
				renderer_->ChangeTexture(shadow_color_rts_[i], 0);
				renderer_->ClearColorBuffer();
				blur_shader_->Uniform2f("u_scale", kBlurSize, 0.0f);
				quad_->Render();

				// Blur vertically
				renderer_->ChangeRenderTarget(shadow_color_rts_[i], nullptr);
				renderer_->ChangeTexture(blur_color_rt_, 0);
				renderer_->ClearColorBuffer();
				blur_shader_->Uniform2f("u_scale", 0.0f, kBlurSize);
				quad_->Render();

				// Back to main framebuffer
				renderer_->ChangeRenderTarget(nullptr, nullptr);

				blur_shader_->Unbind();
				renderer_->EnableDepthTest();
			}
		}
	}
	void BindTextures()
	{
		if (is_vsm_)
		{
			for (U32 i = 0 ; i < kNumSplits; ++i)
				renderer_->ChangeTexture(shadow_color_rts_[i], i);
		}
		else
		{
			for (U32 i = 0 ; i < kNumSplits; ++i)
				renderer_->ChangeTexture(shadow_depth_rts_[i], i);
		}
	}
	void UnbindTextures()
	{
		for (U32 i = 0 ; i < kNumSplits; ++i)
			renderer_->ChangeTexture(nullptr, i);
	}
	void RenderScene()
	{
		// First render shadows
		if (is_vsm_)
		{
			ShadowPass();
		}
		else
		{
			renderer_->CullFace(scythe::CullFaceType::kFront);
			ShadowPass();
			renderer_->CullFace(scythe::CullFaceType::kBack);
		}

		// Render objects
		object_shader_->Bind();
		object_shader_->UniformMatrix4fv("u_projection_view", projection_view_matrix_);
		object_shader_->UniformMatrix4fv("u_depth_bias_projection_view", 
			depth_bias_projection_view_matrices_[0], false, kNumSplits);
		// First split distance stores near value
		object_shader_->Uniform1fv("u_clip_space_split_distances", 
			clip_space_split_distances_, kNumSplits);
		object_shader_->Uniform1f("u_color_factor", show_color_ ? 1.0f : 0.0f);
		// object_shader_->Uniform3fv("u_camera.position", camera_position_);

		BindTextures();
		RenderObjects(object_shader_);
		UnbindTextures();

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
		text_shader_->Unbind();

		renderer_->ChangeTexture(nullptr);
		
		renderer_->EnableDepthTest();
	}
	void Render() final
	{
		renderer_->SetViewport(width_, height_);

		renderer_->ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		renderer_->ClearColorAndDepthBuffers();

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
		else if (key == scythe::PublicKey::kC)
		{
			show_color_ = !show_color_;
		}
		else if (key == scythe::PublicKey::kB)
		{
			use_blur_ = !use_blur_;
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
	void SetCameraAlpha(float value)
	{
		camera_alpha_ = value;
		cos_camera_alpha_ = cos(value);
		sin_camera_alpha_ = sin(value);
	}
	void UpdateCameraOrientation()
	{
		scythe::Quaternion horizontal(scythe::Vector3::UnitY(), -camera_alpha_);
		scythe::Quaternion vertical(scythe::Vector3::UnitZ(), -camera_theta_);
		camera_orientation_.Set(horizontal * vertical);
	}
	void UpdateCameraPosition()
	{
		const scythe::Vector3 target_position(0.0f, 0.0f, 0.0f);
		scythe::Vector3 camera_direction;
		camera_orientation_.GetDirection(&camera_direction);
		camera_position_ = target_position - camera_direction * camera_distance_;
	}
	void UpdateCamera()
	{
		const float kFrameTime = GetFrameTime();
		const float kAngleVelocity = 1.0f;
		const float kDeltaAngle = kAngleVelocity * kFrameTime;

		bool orientation_update = false;
		if (keys_.key_down(scythe::PublicKey::kLeft))
		{
			SetCameraAlpha(camera_alpha_ + kDeltaAngle);
			orientation_update = true;
		}
		else if (keys_.key_down(scythe::PublicKey::kRight))
		{
			SetCameraAlpha(camera_alpha_ - kDeltaAngle);
			orientation_update = true;
		}
		else if (keys_.key_down(scythe::PublicKey::kUp))
		{
			if (camera_theta_ + kDeltaAngle < 1.4f)
			{
				camera_theta_ += kDeltaAngle;
				orientation_update = true;
			}
		}
		else if (keys_.key_down(scythe::PublicKey::kDown))
		{
			if (camera_theta_ > kDeltaAngle + 0.1f)
			{
				camera_theta_ -= kDeltaAngle;
				orientation_update = true;
			}
		}
		if (orientation_update)
			UpdateCameraOrientation();

		// Since ball is being moved always we need to update camera position
		UpdateCameraPosition();
		need_update_view_matrix_ = true;
	}
	void UpdateProjectionMatrix()
	{
		if (need_update_projection_matrix_)
		{
			need_update_projection_matrix_ = false;
			need_update_frustum_ = true;
			scythe::Matrix4 projection_matrix;
			scythe::Matrix4::CreatePerspective(fov_degrees_, aspect_ratio_, 
				z_near_, z_far_, &projection_matrix);
			renderer_->SetProjectionMatrix(projection_matrix);
			UpdateClipSpaceSplitDistances(projection_matrix);
		}
	}
	void UpdateViewMatrix()
	{
		if (need_update_view_matrix_)
		{
			need_update_view_matrix_ = false;
			need_update_frustum_ = true;
			scythe::Matrix4 view_matrix;
			scythe::Matrix4::CreateView(camera_orientation_, camera_position_, &view_matrix);
			renderer_->SetViewMatrix(view_matrix);
		}
	}
	void UpdateFrustum()
	{
		if (need_update_frustum_)
		{
			need_update_frustum_ = false;
			frustum_.Set(projection_view_matrix_);
		}
	}
	/**
	 * Calculates split distances in world space.
	 * Depends on z near and z far.
	 */
	void CalculateSplitDistances()
	{
		for (U32 i = 0; i < kNumSplits + 1; ++i)
		{
			float fraction = (float)i / (float)kNumSplits;
			float exponential = z_near_ * pow(z_far_ / z_near_, fraction);
			float linear = z_near_ + (z_far_ - z_near_) * fraction;
			split_distances_[i] = exponential * kSplitLambda + linear * (1.0f - kSplitLambda);
		}
		split_distances_[0] = z_near_;
		split_distances_[kNumSplits] = z_far_;
	}
	/**
	 * Calculates split distances in clip space.
	 * Depends on projection matrix.
	 */
	void UpdateClipSpaceSplitDistances(const scythe::Matrix4& projection_matrix)
	{
		for (U32 i = 0; i < kNumSplits; ++i)
		{
			// The default view coordinate system has its Z axis "on us".
			// To make point in view direction, just use -Z.
			scythe::Vector3 point(0.0f, 0.0f, -split_distances_[i + 1]);
			projection_matrix.TransformPoint(&point);
			clip_space_split_distances_[i] = point.z;
		}
	}
	void CalculateSplitMatrices()
	{
		// Get frustum corners
		scythe::Vector3 frustum_corners[8];
		frustum_.GetCorners(frustum_corners);
		// Then we can obtain splitted frustums from those corners
		for (U32 i = 0; i < kNumSplits; ++i)
		{
			// Get near and far planes for split
			float near_distance = split_distances_[i];
			float far_distance = split_distances_[i+1];
			// Get near and far fractions
			float near_fraction = (near_distance - z_near_) / (z_far_ - z_near_);
			float far_fraction = (far_distance - z_near_) / (z_far_ - z_near_);
			// Get corner points for split via four lines
			scythe::Vector3 corners[8];
			scythe::Vector3 line;
			int near_index, far_index;
			// Left top
			near_index = 0;
			far_index = 7;
			line = frustum_corners[far_index] - frustum_corners[near_index];
			corners[near_index] = frustum_corners[near_index] + line * near_fraction;
			corners[far_index] = frustum_corners[near_index] + line * far_fraction;
			// Left bottom
			near_index = 1;
			far_index = 6;
			line = frustum_corners[far_index] - frustum_corners[near_index];
			corners[near_index] = frustum_corners[near_index] + line * near_fraction;
			corners[far_index] = frustum_corners[near_index] + line * far_fraction;
			// Right bottom
			near_index = 2;
			far_index = 5;
			line = frustum_corners[far_index] - frustum_corners[near_index];
			corners[near_index] = frustum_corners[near_index] + line * near_fraction;
			corners[far_index] = frustum_corners[near_index] + line * far_fraction;
			// Right top
			near_index = 3;
			far_index = 4;
			line = frustum_corners[far_index] - frustum_corners[near_index];
			corners[near_index] = frustum_corners[near_index] + line * near_fraction;
			corners[far_index] = frustum_corners[near_index] + line * far_fraction;
			// Then transform corners to light space (light basis) and calc bounding box
			scythe::Vector3 light_corners[8];
			scythe::BoundingBox bounding_box;
			bounding_box.Prepare();
			for (U32 j = 0; j < 8; ++j)
			{
				light_corners[j] = light_basis_inverse_ * corners[j];
				bounding_box.AddPoint(light_corners[j]);
			}
			// Assuming forward direction is +X
			float ortho_width = bounding_box.max.z - bounding_box.min.z;
			float ortho_height = bounding_box.max.y - bounding_box.min.y;
			float ortho_near = 0.0f;
			float ortho_far = bounding_box.max.x - bounding_box.min.x;
			scythe::Matrix4::CreateOrthographic(ortho_width, ortho_height, 
				ortho_near, ortho_far, &light_projection_matrices_[i]);
			// Transform center back to world space
			scythe::Vector3 center = bounding_box.GetCenter();
			light_basis_.TransformVector(&center);
			float light_distance = 0.5f * (ortho_far - ortho_near);
			scythe::Vector3 light_position = center + light_direction_ * light_distance;
			scythe::Matrix4::CreateView(light_basis_, light_position, &light_view_matrices_[i]);
		}
	}
	void UpdateLightMatrices()
	{
		CalculateSplitMatrices();
	}
	
private:
	scythe::Frustum frustum_;

	scythe::Mesh * quad_;
	scythe::Mesh * cube_;

	scythe::Shader * text_shader_;
	scythe::Shader * object_shader_;
	scythe::Shader * object_shadow_shader_; //!< simplified version for shadows generation
	scythe::Shader * blur_shader_;

	scythe::Texture * shadow_color_rts_[kMaxCSMSplits];
	scythe::Texture * shadow_depth_rts_[kMaxCSMSplits];
	scythe::Texture * blur_color_rt_;

	scythe::Font * font_;
	scythe::DynamicText * fps_text_;
	
	scythe::Matrix4 projection_view_matrix_;
	scythe::Matrix3 light_basis_;
	scythe::Matrix3 light_basis_inverse_;
	scythe::Matrix4 depth_bias_projection_view_matrices_[kMaxCSMSplits];
	scythe::Matrix4 light_projection_matrices_[kMaxCSMSplits];
	scythe::Matrix4 light_view_matrices_[kMaxCSMSplits];
	float split_distances_[kMaxCSMSplits + 1];
	float clip_space_split_distances_[kMaxCSMSplits];

	scythe::Quaternion camera_orientation_;
	scythe::Vector3 camera_position_;
	float camera_distance_;
	float camera_alpha_;
	float camera_theta_;
	float cos_camera_alpha_;
	float sin_camera_alpha_;

	const scythe::Vector3 light_direction_; // direction to light
	const float fov_degrees_;
	const float z_near_;
	const float z_far_;

	bool need_update_projection_matrix_;
	bool need_update_view_matrix_;
	bool need_update_frustum_;
	bool show_color_;
	bool use_blur_;
	const bool is_vsm_;
};

DECLARE_MAIN(CascadedShadowsApp);