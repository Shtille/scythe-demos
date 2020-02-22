#include "wall_data.h"

#include "model/mesh.h"
#include "graphics/text.h"
#include "camera.h"

#include "ui/slider.h"
#include "ui/board.h"
#include "ui/label.h"

// Physics specific modules
#include "common/sc_delete.h"
#include "model/model.h"
#include "node.h"
#include "physics/physics_controller.h"
#include "physics/physics_rigid_body.h"

#include "declare_main.h"

#include <cmath>

//#define ROTATING_PLATFORM

namespace {
#ifdef ROTATING_PLATFORM
	const float kMaxAngle = 0.5f;
#endif
	const float kBallRadius = 1.0f;
}

/**
 * Defines main application class
 */
class MarbleMazeApp : public scythe::OpenGlApplication
					, public scythe::DesktopInputListener
{
public:
	MarbleMazeApp()
	: sphere_mesh_(nullptr)
	, quad_mesh_(nullptr)
	, floor_mesh_(nullptr)
	, wall_mesh_(nullptr)
	, sphere_model_(nullptr)
	, floor_model_(nullptr)
	, wall_model_(nullptr)
	, ball_node_(nullptr)
	, floor_node_(nullptr)
	, wall_node_(nullptr)
	, font_(nullptr)
	, fps_text_(nullptr)
	, camera_manager_(nullptr)
#ifdef ROTATING_PLATFORM
	, target_angle_x_(0.0f)
	, target_angle_y_(0.0f)
	, velocity_x_(0.0f)
	, velocity_y_(0.0f)
	, acceleration_x_(0.0f)
	, acceleration_y_(0.0f)
	, maze_angle_x_(0.0f)
	, maze_angle_y_(0.0f)
#endif
	, camera_distance_(10.0f)
	, camera_alpha_(0.0f)
	, camera_theta_(0.5f)
#ifdef ROTATING_PLATFORM
	, update_rotation_x_(false)
	, update_rotation_y_(false)
#endif
	, need_update_projection_matrix_(true)
	{
		SetInputListener(this);
	}
	const char* GetTitle() final
	{
		return "Marble maze";
	}
	const bool IsMultisample() final
	{
		return true;
	}
	void BindShaderConstants()
	{
		env_shader_->Bind();
		env_shader_->Uniform1i("u_texture", 0);

		object_shader_->Bind();
		object_shader_->Uniform3f("u_light.color", 1.0f, 1.0f, 1.0f);
		object_shader_->Uniform3f("u_light.direction", 0.825f, 0.564f, 0.0f);

		object_shader_->Uniform1i("u_diffuse_env_sampler", 0);
		object_shader_->Uniform1i("u_specular_env_sampler", 1);
		object_shader_->Uniform1i("u_preintegrated_fg_sampler", 2);
		object_shader_->Uniform1i("u_albedo_sampler", 3);
		object_shader_->Uniform1i("u_normal_sampler", 4);
		object_shader_->Uniform1i("u_roughness_sampler", 5);
		object_shader_->Uniform1i("u_metal_sampler", 6);
		object_shader_->Unbind();
	}
	void BindShaderVariables()
	{
		object_shader_->Bind();
		object_shader_->UniformMatrix4fv("u_projection_view", projection_view_matrix_);
		object_shader_->Uniform3fv("u_camera.position", *camera_manager_->position());
		object_shader_->Unbind();
	}
	bool Load() final
	{
		const float kCS = 10.0f; // cell size
		const float kMaterialSize = 3.0f;
		const float kWallWidth = 1.0f;
		const float kWallHeight = 2.0f;
		const scythe::Vector3 kFloorSizes(12.0f * kCS, 2.0f, 12.0f * kCS);

		scythe::PhysicsController::CreateInstance();
		if (!scythe::PhysicsController::GetInstance()->Initialize())
			return false;

		// Vertex formats
		scythe::VertexFormat * object_vertex_format;
		{
			scythe::VertexAttribute attributes[] = {
				scythe::VertexAttribute(scythe::VertexAttribute::kVertex, 3),
				scythe::VertexAttribute(scythe::VertexAttribute::kNormal, 3),
				scythe::VertexAttribute(scythe::VertexAttribute::kTexcoord, 2),
				//scythe::VertexAttribute(scythe::VertexAttribute::kTangent, 3),
				//scythe::VertexAttribute(scythe::VertexAttribute::kBinormal, 3)
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

		// Sphere mesh
		sphere_mesh_ = new scythe::Mesh(renderer_);
		sphere_mesh_->CreateSphere(kBallRadius, 128, 64);
		if (!sphere_mesh_->MakeRenderable(object_vertex_format))
			return false;

		// Screen quad mesh
		quad_mesh_ = new scythe::Mesh(renderer_);
		quad_mesh_->CreateQuadFullscreen();
		if (!quad_mesh_->MakeRenderable(quad_vertex_format))
			return false;

		// Floor mesh
		floor_mesh_ = new scythe::Mesh(renderer_);
		floor_mesh_->CreatePhysicalBox(kFloorSizes.x, kFloorSizes.y, kFloorSizes.z, kMaterialSize, kMaterialSize);
		if (!floor_mesh_->MakeRenderable(object_vertex_format))
			return false;

		// Wall mesh
		{
			std::vector<WallData> wall_data;
			::GetWallData(&wall_data, kCS, kFloorSizes.y, kWallWidth, kWallHeight);

			wall_mesh_ = new scythe::Mesh(renderer_);
			wall_mesh_->ForceTriangles();
			for (const auto& data : wall_data)
			{
				// Each separate box is being put as mesh part and placed at desired position
				wall_mesh_->CreatePhysicalBox(data.sizes.x, data.sizes.y, data.sizes.z, kMaterialSize, kMaterialSize, &data.center);
			}
			if (!wall_mesh_->MakeRenderable(object_vertex_format))
				return false;
		}

		// Models
		sphere_model_ = scythe::Model::Create(sphere_mesh_);
		floor_model_ = scythe::Model::Create(floor_mesh_);
		wall_model_ = scythe::Model::Create(wall_mesh_);

		// Ball
		{
			const float mass = 1.0f;
			const scythe::Vector3 position(0.0f, kFloorSizes.y + kBallRadius, 0.0f);
			scythe::PhysicsRigidBody::Parameters params(mass);

			scythe::Node * node = scythe::Node::Create("ball");
			node->SetTranslation(position);
			node->SetDrawable(sphere_model_);
			node->SetCollisionObject(scythe::PhysicsCollisionObject::kRigidBody,
				scythe::PhysicsCollisionShape::DefineSphere(kBallRadius),
				&params);
			ball_node_ = node;
			nodes_.push_back(node);
			/*
			ball_->SetFriction(1.28f);
			ball_->SetRollingFriction(0.2f);
			ball_->SetSpinningFriction(0.5f);
			ball_->SetRestitution(0.0f);
			ball_->DisableDeactivation();

#ifndef ROTATING_PLATFORM
			sht::physics::ClampSpeedInfo clamp_speed_info;
			const float kMaxLinearVelocity = 3.0f;
			clamp_speed_info.max_linear_velocity_ = kMaxLinearVelocity;
			clamp_speed_info.max_angular_velocity_ = kMaxLinearVelocity / kBallRadius; // w = V / R
			clamp_speed_info.clamp_linear_velocity_ = true;
			clamp_speed_info.clamp_angular_velocity_ = true;
			physics_->AddClampedSpeedObject(ball_, clamp_speed_info);
#endif
			*/
		}
		// Floor
		{
			const float mass = 0.0f; // static object
			const scythe::Vector3 position(0.0f, 0.0f, 0.0f);
			scythe::PhysicsRigidBody::Parameters params(mass);

			scythe::Node * node = scythe::Node::Create("floor");
			node->SetTranslation(position);
			node->SetDrawable(floor_model_);
			node->SetCollisionObject(scythe::PhysicsCollisionObject::kRigidBody,
				scythe::PhysicsCollisionShape::DefineBox(kFloorSizes),
				&params);
			floor_node_ = node;
			nodes_.push_back(node);
		}
		// Walls
		{
			const float mass = 0.0f; // static object
			const scythe::Vector3 position(0.0f, 0.0f, 0.0f);
			scythe::PhysicsRigidBody::Parameters params(mass);

			scythe::Node * node = scythe::Node::Create("walls");
			node->SetTranslation(position);
			node->SetDrawable(wall_model_);
			node->SetCollisionObject(scythe::PhysicsCollisionObject::kRigidBody,
				scythe::PhysicsCollisionShape::DefineMesh(wall_mesh_),
				&params);
			wall_node_ = node;
			nodes_.push_back(node);
		}
		
		// Load shaders
		if (!renderer_->AddShader(text_shader_, "data/shaders/text")) return false;
		if (!renderer_->AddShader(gui_shader_, "data/shaders/gui_colored")) return false;
		if (!renderer_->AddShader(env_shader_, "data/shaders/skybox")) return false;
		if (!renderer_->AddShader(irradiance_shader_, "data/shaders/pbr/irradiance")) return false;
		if (!renderer_->AddShader(prefilter_shader_, "data/shaders/pbr/prefilter")) return false;
		if (!renderer_->AddShader(integrate_shader_, "data/shaders/pbr/integrate")) return false; // dont use
		if (!renderer_->AddShader(object_shader_, "data/shaders/pbr/object_pbr_no_tb")) return false;
		
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

		if (!renderer_->AddTexture(ball_albedo_texture_, "data/textures/pbr/metal/rusted_iron/albedo.png",
								   scythe::Texture::Wrap::kRepeat,
								   scythe::Texture::Filter::kTrilinearAniso)) return false;
		if (!renderer_->AddTexture(ball_normal_texture_, "data/textures/pbr/metal/rusted_iron/normal.png",
								   scythe::Texture::Wrap::kRepeat,
								   scythe::Texture::Filter::kTrilinearAniso)) return false;
		if (!renderer_->AddTexture(ball_roughness_texture_, "data/textures/pbr/metal/rusted_iron/roughness.png",
								   scythe::Texture::Wrap::kRepeat,
								   scythe::Texture::Filter::kTrilinearAniso)) return false;
		if (!renderer_->AddTexture(ball_metal_texture_, "data/textures/pbr/metal/rusted_iron/metallic.png",
								   scythe::Texture::Wrap::kRepeat,
								   scythe::Texture::Filter::kTrilinearAniso)) return false;

		if (!renderer_->AddTexture(maze_albedo_texture_, "data/textures/pbr/stone/marble/albedo.png",
								   scythe::Texture::Wrap::kRepeat,
								   scythe::Texture::Filter::kTrilinearAniso)) return false;
		if (!renderer_->AddTexture(maze_normal_texture_, "data/textures/pbr/stone/marble/normal.png",
								   scythe::Texture::Wrap::kRepeat,
								   scythe::Texture::Filter::kTrilinearAniso)) return false;
		if (!renderer_->AddTexture(maze_roughness_texture_, "data/textures/pbr/stone/marble/roughness.png",
								   scythe::Texture::Wrap::kRepeat,
								   scythe::Texture::Filter::kTrilinearAniso)) return false;
		if (!renderer_->AddTexture(maze_metal_texture_, "data/textures/pbr/stone/marble/metallic.png",
								   scythe::Texture::Wrap::kRepeat,
								   scythe::Texture::Filter::kTrilinearAniso)) return false;

		if (!renderer_->AddTexture(fg_texture_, "data/textures/pbr/brdfLUT.png",
								   scythe::Texture::Wrap::kClampToEdge,
								   scythe::Texture::Filter::kTrilinearAniso)) return false;

		// Render targets
		renderer_->CreateTextureCubemap(irradiance_rt_, 32, 32, scythe::Image::Format::kRGB8, scythe::Texture::Filter::kLinear);
		renderer_->CreateTextureCubemap(prefilter_rt_, 512, 512, scythe::Image::Format::kRGB8, scythe::Texture::Filter::kTrilinear);
		renderer_->GenerateMipmap(prefilter_rt_);

		renderer_->AddFont(font_, "data/fonts/GoodDog.otf");
		if (font_ == nullptr)
			return false;

		fps_text_ = scythe::DynamicText::Create(renderer_, 30);
		if (!fps_text_)
			return false;

		// Create camera
		camera_manager_ = new scythe::CameraManager();
		UpdateCamera();

		CreateUI();

		// Finally bind constants
		BindShaderConstants();

		BakeCubemaps();
		
		return true;
	}
	void Unload() final
	{
		SC_SAFE_DELETE(ui_root_);
		SC_SAFE_DELETE(camera_manager_);
		SC_SAFE_DELETE(fps_text_)
		// Release nodes
		for (auto node : nodes_)
		{
			SC_SAFE_RELEASE(node);
		}
		// Release models
		SC_SAFE_RELEASE(wall_model_);
		SC_SAFE_RELEASE(floor_model_);
		SC_SAFE_RELEASE(sphere_model_);
		// Release meshes
		SC_SAFE_RELEASE(wall_mesh_);
		SC_SAFE_RELEASE(floor_mesh_);
		SC_SAFE_RELEASE(quad_mesh_);
		SC_SAFE_RELEASE(sphere_mesh_);

		scythe::PhysicsController::GetInstance()->Deinitialize();
		scythe::PhysicsController::DestroyInstance();
	}
	void Update() final
	{
		const float kFrameTime = GetFrameTime();

		camera_manager_->Update(kFrameTime);

		// Update UI
		ui_root_->UpdateAll(kFrameTime);

		if (ball_node_->GetTranslationY() < 0.0f)
		{
			victory_board_->Move();
		}

#ifdef ROTATING_PLATFORM
		// Update maze rotation
		UpdateRotation(kFrameTime);
#endif

		// Update matrices
		renderer_->SetViewMatrix(camera_manager_->view_matrix());
		UpdateProjectionMatrix();
		projection_view_matrix_ = renderer_->projection_matrix() * renderer_->view_matrix();

		BindShaderVariables();
	}
#ifndef ROTATING_PLATFORM
	void ApplyForces(float sec)
	{
		const float kPushPower = 10.0f;
		scythe::Vector3 force(0.0f);
		bool any_key_pressed = false;
		if (keys_.key_down(scythe::PublicKey::kA))
		{
			any_key_pressed = true;
			force.z -= kPushPower;
		}
		if (keys_.key_down(scythe::PublicKey::kD))
		{
			any_key_pressed = true;
			force.z += kPushPower;
		}
		if (keys_.key_down(scythe::PublicKey::kS))
		{
			any_key_pressed = true;
			force.x -= kPushPower;
		}
		if (keys_.key_down(scythe::PublicKey::kW))
		{
			any_key_pressed = true;
			force.x += kPushPower;
		}
		if (any_key_pressed)
		{
			scythe::PhysicsRigidBody * body = dynamic_cast<scythe::PhysicsRigidBody *>(ball_node_->GetCollisionObject());
			body->ApplyForce(force);
		}
	}
#endif
	void UpdatePhysics(float sec) final
	{
#ifndef ROTATING_PLATFORM
		ApplyForces(sec);
#endif
		scythe::PhysicsController::GetInstance()->Update(sec);
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
			quad_mesh_->Render();
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
				quad_mesh_->Render();
			}
		}
		renderer_->ChangeRenderTarget(nullptr, nullptr); // back to main framebuffer
		prefilter_shader_->Unbind();
		renderer_->ChangeTexture(nullptr);

		renderer_->EnableDepthTest();
	}
	void RenderEnvironment()
	{
		renderer_->DisableDepthTest();

		renderer_->ChangeTexture(env_texture_);
		env_shader_->Bind();
		env_shader_->UniformMatrix4fv("u_projection", renderer_->projection_matrix());
		env_shader_->UniformMatrix4fv("u_view", renderer_->view_matrix());
		quad_mesh_->Render();
		env_shader_->Unbind();
		renderer_->ChangeTexture(nullptr);

		renderer_->EnableDepthTest();
	}
	void MazeTextureBinding()
	{
		renderer_->ChangeTexture(irradiance_rt_, 0);
		renderer_->ChangeTexture(prefilter_rt_, 1);
		renderer_->ChangeTexture(fg_texture_, 2);
		renderer_->ChangeTexture(maze_albedo_texture_, 3);
		renderer_->ChangeTexture(maze_normal_texture_, 4);
		renderer_->ChangeTexture(maze_roughness_texture_, 5);
		renderer_->ChangeTexture(maze_metal_texture_, 6);
	}
	void BallTextureBinding()
	{
		renderer_->ChangeTexture(irradiance_rt_, 0);
		renderer_->ChangeTexture(prefilter_rt_, 1);
		renderer_->ChangeTexture(fg_texture_, 2);
		renderer_->ChangeTexture(ball_albedo_texture_, 3);
		renderer_->ChangeTexture(ball_normal_texture_, 4);
		renderer_->ChangeTexture(ball_roughness_texture_, 5);
		renderer_->ChangeTexture(ball_metal_texture_, 6);
	}
	void EmptyTextureBinding()
	{
		renderer_->ChangeTexture(nullptr, 6);
		renderer_->ChangeTexture(nullptr, 5);
		renderer_->ChangeTexture(nullptr, 4);
		renderer_->ChangeTexture(nullptr, 3);
		renderer_->ChangeTexture(nullptr, 2);
		renderer_->ChangeTexture(nullptr, 1);
		renderer_->ChangeTexture(nullptr, 0);
	}
	void RenderObjects()
	{
		object_shader_->Bind();

		MazeTextureBinding();

		// Floor
		renderer_->PushMatrix();
		renderer_->LoadMatrix(floor_node_->GetWorldMatrix());
		object_shader_->UniformMatrix4fv("u_model", renderer_->model_matrix());
		floor_node_->GetDrawable()->Draw();
		renderer_->PopMatrix();

		// Walls
		renderer_->PushMatrix();
		renderer_->LoadMatrix(wall_node_->GetWorldMatrix());
		object_shader_->UniformMatrix4fv("u_model", renderer_->model_matrix());
		wall_node_->GetDrawable()->Draw();
		renderer_->PopMatrix();

		BallTextureBinding();

		// Ball
		renderer_->PushMatrix();
		renderer_->LoadMatrix(ball_node_->GetWorldMatrix());
		object_shader_->UniformMatrix4fv("u_model", renderer_->model_matrix());
		ball_node_->GetDrawable()->Draw();
		renderer_->PopMatrix();

		EmptyTextureBinding();
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

		// Render UI
		gui_shader_->Bind();
		if (!info_board_->IsPosMax())
		{
			if (info_board_->IsPosMin())
				info_board_->RenderAll(); // render entire tree
			else
				info_board_->Render(); // render only board rect (smart hack for labels :D)
		}
		if (!victory_board_->IsPosMax())
		{
			if (victory_board_->IsPosMin())
				victory_board_->RenderAll(); // render entire tree
			else
				victory_board_->Render(); // render only board rect (smart hack for labels :D)
		}
#ifdef ROTATING_PLATFORM
		horizontal_slider_->RenderAll();
		vertical_slider_->RenderAll();
#endif
		
		renderer_->EnableDepthTest();
	}
	void Render() final
	{
		renderer_->SetViewport(width_, height_);
		
		renderer_->ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		renderer_->ClearColorAndDepthBuffers();
		
		RenderEnvironment();
		RenderObjects();
		RenderInterface();
	}
#ifdef ROTATING_PLATFORM
	void UpdateRotation(float frame_time)
	{
		const float kDesiredTime = 3.0f;
		const float kAccuracy = 0.01f;
		if (update_rotation_x_)
		{
			// Calculate desired acceleration to reach the target angle within desired time
			acceleration_x_ = (target_angle_x_ - maze_angle_x_ - velocity_x_ * kDesiredTime) * 2.0f / (kDesiredTime * kDesiredTime);
			// Then do Euler integration
			velocity_x_ += acceleration_x_ * frame_time;
			maze_angle_x_ += velocity_x_ * frame_time;
		}
		if (update_rotation_y_)
		{
			// Calculate desired acceleration to reach the target angle within desired time
			acceleration_y_ = (target_angle_y_ - maze_angle_y_ - velocity_y_ * kDesiredTime) * 2.0f / (kDesiredTime * kDesiredTime);
			// Then do Euler integration
			velocity_y_ += acceleration_y_ * frame_time;
			maze_angle_y_ += velocity_y_ * frame_time;
		}
		if (update_rotation_x_ || update_rotation_y_)
		{
			UpdateMazeMatrix();
			// Check if we've reached target angle within accuracy
			if (update_rotation_x_ && fabs(target_angle_x_ - maze_angle_x_) < kAccuracy)
			{
				update_rotation_x_ = false;
			}
			if (update_rotation_y_ && fabs(target_angle_y_ - maze_angle_y_) < kAccuracy)
			{
				update_rotation_y_ = false;
			}
		}
	}
	void OnHorizontalSliderMoved()
	{
		target_angle_x_ = (horizontal_slider_->pin_position() * 2.0f - 1.0f) * kMaxAngle;
		update_rotation_x_ = true;
	}
	void OnVerticalSliderMoved()
	{
		target_angle_y_ = (vertical_slider_->pin_position() * 2.0f - 1.0f) * kMaxAngle;
		update_rotation_y_ = true;
	}
#endif
	void OnChar(unsigned short code) final
	{
	}
	void OnKeyDown(scythe::PublicKey key, int mods) final
	{
		const float kDeltaAngle = 0.1f;
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
			camera_alpha_ += kDeltaAngle;
			UpdateCamera();
		}
		else if (key == scythe::PublicKey::kRight)
		{
			camera_alpha_ -= kDeltaAngle;
			UpdateCamera();
		}
		else if (key == scythe::PublicKey::kUp)
		{
			if (camera_theta_ < 1.4f)
			{
				camera_theta_ += kDeltaAngle;
				UpdateCamera();
			}
		}
		else if (key == scythe::PublicKey::kDown)
		{
			if (camera_theta_ > 0.1f)
			{
				camera_theta_ -= kDeltaAngle;
				UpdateCamera();
			}
		}
	}
	void OnKeyUp(scythe::PublicKey key, int modifiers) final
	{
	}
	void OnMouseDown(scythe::MouseButton button, int modifiers) final
	{
		if (scythe::MouseButton::kLeft == button)
		{
			if (victory_exit_rect_->active())
			{
				DesktopApplication::Terminate();
			}
			else if (info_ok_rect_->active())
			{
				info_ok_rect_->set_active(false);
				info_board_->Move();
			}
		}
#ifdef ROTATING_PLATFORM
		scythe::Vector2 position(mouse_.x() / height_, mouse_.y() / height_);
		horizontal_slider_->OnTouchDown(position);
		vertical_slider_->OnTouchDown(position);
#endif
	}
	void OnMouseUp(scythe::MouseButton button, int modifiers) final
	{
#ifdef ROTATING_PLATFORM
		scythe::Vector2 position(mouse_.x() / height_, mouse_.y() / height_);
		horizontal_slider_->OnTouchUp(position);
		vertical_slider_->OnTouchUp(position);
#endif
	}
	void OnMouseMove() final
	{
		scythe::Vector2 position(mouse_.x() / height_, mouse_.y() / height_);
		if (info_board_->IsPosMin())
			info_board_->SelectAll(position.x, position.y);
		if (victory_board_->IsPosMin())
			victory_board_->SelectAll(position.x, position.y);
#ifdef ROTATING_PLATFORM
		horizontal_slider_->OnTouchMove(position);
		if (horizontal_slider_->is_touched())
			OnHorizontalSliderMoved();
		vertical_slider_->OnTouchMove(position);
		if (vertical_slider_->is_touched())
			OnVerticalSliderMoved();
#endif
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
#ifdef ROTATING_PLATFORM
	void UpdateMazeMatrix()
	{
		scythe::Quaternion horizontal(vec3(1.0f, 0.0f, 0.0f), maze_angle_x_);
		scythe::Quaternion vertical(vec3(0.0f, 0.0f, 1.0f), maze_angle_y_);
		scythe::Quaternion orient(horizontal * vertical);
		scythe::Vector3 base_gravity(0.0f, -9.81f, 0.0f);
		scythe::Vector3 gravity;
		orient.RotatePoint(base_gravity, &gravity);
		physics_->SetGravity(gravity);
	}
#endif
	void CreateUI()
	{
		scythe::Rect * rect;
		scythe::Label * label;

		ui_root_ = new scythe::Widget();

		// Info menu
		info_board_ = new scythe::ColoredBoard(renderer_, gui_shader_,
			scythe::Vector4(0.5f, 0.5f, 0.3f, 0.3f), // color
			0.8f, // f32 width
			0.6f, // f32 height
			aspect_ratio_*0.5f - 0.4f, // f32 left
			0.2f, // f32 hmin
			1.0f, // f32 hmax
			1.0f, // f32 velocity
			true, // bool is_down
			true, // bool is vertical
			(U32)scythe::Flags::kRenderAlways // U32 flags
			);
		ui_root_->AttachWidget(info_board_);
		{
			const wchar_t * kText = L"Controls:";
			label = new scythe::Label(renderer_, text_shader_, font_,
				scythe::Vector4(0.2f, 0.2f, 0.2f, 1.0f), // color
				0.10f, // text height
				wcslen(kText)+1, // buffer size
				0.25f, // x
				0.50f, // y
				(U32)scythe::Flags::kRenderAlways // flags
				);
			info_board_->AttachWidget(label);
			label->SetText(kText);
		}
		{
			const wchar_t * kText = L"WASD - movement";
			label = new scythe::Label(renderer_, text_shader_, font_,
				scythe::Vector4(0.2f, 0.2f, 0.2f, 1.0f), // color
				0.10f, // text height
				wcslen(kText) + 1, // buffer size
				0.05f, // x
				0.30f, // y
				(U32)scythe::Flags::kRenderAlways // flags
			);
			info_board_->AttachWidget(label);
			label->SetText(kText);
		}
		{
			const wchar_t * kText = L"Arrow keys - camera";
			label = new scythe::Label(renderer_, text_shader_, font_,
				scythe::Vector4(0.2f, 0.2f, 0.2f, 1.0f), // color
				0.10f, // text height
				wcslen(kText) + 1, // buffer size
				0.05f, // x
				0.20f, // y
				(U32)scythe::Flags::kRenderAlways // flags
			);
			info_board_->AttachWidget(label);
			label->SetText(kText);
		}
		{
			rect = new scythe::RectColored(renderer_, gui_shader_, scythe::Vector4(0.5f),
				0.20f, // x
				0.0f, // y
				0.4f, // width
				0.1f, // height
				(U32)scythe::Flags::kRenderIfActive | (U32)scythe::Flags::kSelectable // U32 flags
				);
			info_board_->AttachWidget(rect);
			info_ok_rect_ = rect;
			const wchar_t * kText = L"OK";
			label = new scythe::Label(renderer_, text_shader_, font_,
				scythe::Vector4(0.2f, 0.2f, 0.2f, 1.0f), // color
				0.1f, // text height
				wcslen(kText)+1, // buffer size
				0.0f, // x
				0.0f, // y
				(U32)scythe::Flags::kRenderAlways // flags
				);
			rect->AttachWidget(label);
			label->SetText(kText);
			label->AlignCenter(rect->width(), rect->height());
		}

		// Victory menu
		victory_board_ = new scythe::ColoredBoard(renderer_, gui_shader_,
			scythe::Vector4(0.5f, 0.5f, 0.3f, 0.3f), // color
			0.8f, // f32 width
			0.6f, // f32 height
			aspect_ratio_*0.5f - 0.4f, // f32 left
			0.2f, // f32 hmin
			1.0f, // f32 hmax
			1.0f, // f32 velocity
			false, // bool is_down
			true, // bool is vertical
			(U32)scythe::Flags::kRenderAlways // U32 flags
			);
		ui_root_->AttachWidget(victory_board_);
		{
			const wchar_t * kText = L"Congratulations!";
			label = new scythe::Label(renderer_, text_shader_, font_,
				scythe::Vector4(0.2f, 0.2f, 0.2f, 1.0f), // color
				0.10f, // text height
				wcslen(kText)+1, // buffer size
				0.15f, // x
				0.50f, // y
				(U32)scythe::Flags::kRenderAlways // flags
				);
			victory_board_->AttachWidget(label);
			label->SetText(kText);
		}
		{
			const wchar_t * kText = L"You have finished!";
			label = new scythe::Label(renderer_, text_shader_, font_,
				scythe::Vector4(0.2f, 0.2f, 0.2f, 1.0f), // color
				0.10f, // text height
				wcslen(kText) + 1, // buffer size
				0.15f, // x
				0.30f, // y
				(U32)scythe::Flags::kRenderAlways // flags
			);
			victory_board_->AttachWidget(label);
			label->SetText(kText);
		}
		{
			rect = new scythe::RectColored(renderer_, gui_shader_, scythe::Vector4(0.5f),
				0.2f, // x
				0.0f, // y
				0.4f, // width
				0.1f, // height
				(U32)scythe::Flags::kRenderIfActive | (U32)scythe::Flags::kSelectable // U32 flags
				);
			victory_board_->AttachWidget(rect);
			victory_exit_rect_ = rect;
			const wchar_t * kText = L"Exit";
			label = new scythe::Label(renderer_, text_shader_, font_,
				scythe::Vector4(0.2f, 0.2f, 0.2f, 1.0f), // color
				0.1f, // text height
				wcslen(kText)+1, // buffer size
				0.0f, // x
				0.0f, // y
				(U32)scythe::Flags::kRenderAlways // flags
				);
			rect->AttachWidget(label);
			label->SetText(kText);
			label->AlignCenter(rect->width(), rect->height());
		}

#ifdef ROTATING_PLATFORM
		// ----- User interaction UI -----
		const scythe::Vector4 kBarColor(0.5f, 0.5f, 0.5f, 0.5f);
		const scythe::Vector4 kPinColorNormal(1.0f, 1.0f, 1.0f, 1.0f);
		const scythe::Vector4 kPinColorTouch(1.0f, 1.0f, 0.0f, 1.0f);

		horizontal_slider_ = new scythe::SliderColored(renderer_, gui_shader_,
			kBarColor, // vec4 bar_color
			kPinColorNormal, // vec4 pin_color_normal
			kPinColorTouch, // vec4 pin_color_touch
			0.0f, // f32 x
			0.0f, // f32 y
			renderer_->aspect_ratio(), // f32 width
			0.2f, // f32 height
			0.03f, // f32 bar_radius
			(U32)scythe::Flags::kRenderAlways, // U32 flags
			scythe::SliderColored::kCircle, // bar form
			scythe::SliderColored::kCircle // pin form
			);
		horizontal_slider_->SetPinPosition(0.5f);
		ui_root_->AttachWidget(horizontal_slider_);
		vertical_slider_ = new scythe::SliderColored(renderer_, gui_shader_,
			kBarColor, // vec4 bar_color
			kPinColorNormal, // vec4 pin_color_normal
			kPinColorTouch, // vec4 pin_color_touch
			0.0f, // f32 x
			0.2f, // f32 y
			0.2f, // f32 width
			0.8f, // f32 height
			0.03f, // f32 bar_radius
			(U32)scythe::Flags::kRenderAlways, // U32 flags
			scythe::SliderColored::kCircle, // bar form
			scythe::SliderColored::kCircle // pin form
			);
		vertical_slider_->SetPinPosition(0.5f);
		ui_root_->AttachWidget(vertical_slider_);
#endif
	}
	void UpdateCamera()
	{
		scythe::Quaternion horizontal(-scythe::Vector3::UnitY(), camera_alpha_);
		scythe::Quaternion vertical(-scythe::Vector3::UnitZ(), camera_theta_);
		scythe::Quaternion orient(horizontal * vertical);
		const scythe::Vector3 * position_ptr = &ball_node_->GetTranslation();
		camera_manager_->MakeAttached(orient, position_ptr, camera_distance_);
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
	scythe::Mesh * sphere_mesh_;
	scythe::Mesh * quad_mesh_;
	scythe::Mesh * floor_mesh_;
	scythe::Mesh * wall_mesh_;

	scythe::Model * sphere_model_;
	scythe::Model * floor_model_;
	scythe::Model * wall_model_;

	scythe::Node * ball_node_;
	scythe::Node * floor_node_;
	scythe::Node * wall_node_;
	std::vector<scythe::Node *> nodes_;

	scythe::Shader * text_shader_;
	scythe::Shader * gui_shader_;
	scythe::Shader * env_shader_;
	scythe::Shader * object_shader_;
	scythe::Shader * irradiance_shader_;
	scythe::Shader * prefilter_shader_;
	scythe::Shader * integrate_shader_;

	scythe::Texture * env_texture_;
	scythe::Texture * ball_albedo_texture_;
	scythe::Texture * ball_normal_texture_;
	scythe::Texture * ball_roughness_texture_;
	scythe::Texture * ball_metal_texture_;
	scythe::Texture * maze_albedo_texture_;
	scythe::Texture * maze_normal_texture_;
	scythe::Texture * maze_roughness_texture_;
	scythe::Texture * maze_metal_texture_;
	scythe::Texture * fg_texture_;
	scythe::Texture * irradiance_rt_;
	scythe::Texture * prefilter_rt_;

	scythe::Font * font_;
	scythe::DynamicText * fps_text_;
	scythe::CameraManager * camera_manager_;

	scythe::Widget * ui_root_;
	scythe::ColoredBoard * info_board_;
	scythe::ColoredBoard * victory_board_;
	scythe::Rect * info_ok_rect_;
	scythe::Rect * victory_exit_rect_;
#ifdef ROTATING_PLATFORM
	scythe::SliderColored * horizontal_slider_;
	scythe::SliderColored * vertical_slider_;
	sht::math::Matrix4 maze_matrix_;
#endif
	
	scythe::Matrix4 projection_view_matrix_;
	
#ifdef ROTATING_PLATFORM
	// Since slider shouldn't control angle directly we will use target angle
	float target_angle_x_;
	float target_angle_y_;
	float velocity_x_;
	float velocity_y_;
	float acceleration_x_;
	float acceleration_y_;
	float maze_angle_x_;
	float maze_angle_y_;
#endif

	float camera_distance_;
	float camera_alpha_;
	float camera_theta_;

#ifdef ROTATING_PLATFORM
	bool update_rotation_x_;
	bool update_rotation_y_;
#endif
	bool need_update_projection_matrix_;
};

DECLARE_MAIN(MarbleMazeApp);