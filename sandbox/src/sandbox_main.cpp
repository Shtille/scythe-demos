/**
 * Main source file for sandbox demo.
 *
 * Task list.
 * [+] 1. Add possibility of adding objects with custom sizes.
 * [+] 2. Add console and objects addition via its input.
 * [-] 3. Add mesh-based physical objects (tetrahedron as example).
 */

#include "object.h"
#include "parser.h"
#include "console.h"

#include "model/mesh.h"
#include "graphics/text.h"
#include "math/constants.h"

// Physics specific modules
#include "common/sc_delete.h"
#include "model/model.h"
#include "node.h"
#include "physics/physics_controller.h"
#include "physics/physics_rigid_body.h"

#include "declare_main.h"

#include <vector>

class SandboxApp : public scythe::OpenGlApplication
				 , public scythe::DesktopInputListener
				 , public IObjectCreator
{
public:
	SandboxApp()
	: sphere_mesh_(nullptr)
	, box_mesh_(nullptr)
	, sphere_model_(nullptr)
	, box_model_(nullptr)
	, font_(nullptr)
	, fps_text_(nullptr)
	, parser_(nullptr)
	, console_(nullptr)
	{
		SetInputListener(this);
	}
	const char* GetTitle() final
	{
		return "Physics sandbox";
	}
	const bool IsMultisample() final
	{
		return true;
	}
	void BindShaderConstants()
	{
		const scythe::Vector3 kLightColor(1.0f);

		object_shader_->Bind();
		object_shader_->Uniform3fv("u_light.color", kLightColor);
		object_shader_->Unbind();
	}
	void BindShaderVariables()
	{
		object_shader_->Bind();
		object_shader_->Uniform3fv("u_light.position", light_position_);
		object_shader_->Unbind();
	}
	void CreateSphere(const scythe::Vector3& position, float radius, const scythe::Vector3& color, float mass)
	{
		scythe::PhysicsRigidBody::Parameters params(mass);
		scythe::Node * node = scythe::Node::Create("sphere");
		node->SetTranslation(position);
		node->SetScale(radius);
		node->SetDrawable(sphere_model_);
		node->SetCollisionObject(scythe::PhysicsCollisionObject::kRigidBody,
			scythe::PhysicsCollisionShape::DefineSphere(radius),
			&params);
		objects_.push_back(Object(node, color));
	}
	void CreateBox(const scythe::Vector3& position, const scythe::Vector3& extents, const scythe::Vector3& color, float mass)
	{
		scythe::PhysicsRigidBody::Parameters params(mass);
		scythe::Node * node = scythe::Node::Create("box");
		node->SetTranslation(position);
		node->SetScale(extents);
		node->SetDrawable(box_model_);
		node->SetCollisionObject(scythe::PhysicsCollisionObject::kRigidBody,
			scythe::PhysicsCollisionShape::DefineBox(extents),
			&params);
		objects_.push_back(Object(node, color));
	}
	void CreateSphere(float pos_x, float pos_y, float pos_z, float radius,
		float color_x, float color_y, float color_z, float mass) final
	{
		CreateSphere(scythe::Vector3(pos_x, pos_y, pos_z), radius, scythe::Vector3(color_x, color_y, color_z), mass);
	}
	void CreateBox(float pos_x, float pos_y, float pos_z,
		float extent_x, float extent_y, float extent_z,
		float color_x, float color_y, float color_z, float mass) final
	{
		CreateBox(scythe::Vector3(pos_x, pos_y, pos_z), scythe::Vector3(extent_x, extent_y, extent_z),
			scythe::Vector3(color_x, color_y, color_z), mass);
	}
	bool Load() final
	{
		scythe::PhysicsController::CreateInstance();
		if (!scythe::PhysicsController::GetInstance()->Initialize())
			return false;

		// Sphere model
		sphere_mesh_ = new scythe::Mesh(renderer_);
		sphere_mesh_->AddFormat(scythe::VertexAttribute(scythe::VertexAttribute::kVertex, 3));
		sphere_mesh_->AddFormat(scythe::VertexAttribute(scythe::VertexAttribute::kNormal, 3));
		sphere_mesh_->CreateSphere(1.0f, 128, 64);
		if (!sphere_mesh_->MakeRenderable())
			return false;

		// Box model
		box_mesh_ = new scythe::Mesh(renderer_);
		box_mesh_->AddFormat(scythe::VertexAttribute(scythe::VertexAttribute::kVertex, 3));
		box_mesh_->AddFormat(scythe::VertexAttribute(scythe::VertexAttribute::kNormal, 3));
		box_mesh_->CreateCube();
		if (!box_mesh_->MakeRenderable())
			return false;

		// Models
		sphere_model_ = scythe::Model::Create(sphere_mesh_);
		box_model_ = scythe::Model::Create(box_mesh_);

		// Nodes
		CreateBox(scythe::Vector3(0.0f, 0.0f, 0.0f), scythe::Vector3(5.0f, 1.0f, 5.0f), scythe::Vector3(0.1f, 1.0f, 0.2f), 0.0f);
		CreateSphere(scythe::Vector3(0.0f, 3.0f, 0.0f), 2.0f, scythe::Vector3(1.0f, 0.0f, 0.0f), 0.1f);
		CreateSphere(scythe::Vector3(0.5f, 5.0f, 0.5f), 1.0f, scythe::Vector3(0.8f, 0.0f, 0.5f), 0.2f);
		
		// Load shaders
		const char *attribs[] = {"a_position"};
		if (!renderer_->AddShader(object_shader_, "data/shaders/sandbox/object", attribs, _countof(attribs))) return false;
		if (!renderer_->AddShader(text_shader_, "data/shaders/text", attribs, 1)) return false;
		if (!renderer_->AddShader(gui_shader_, "data/shaders/gui_colored", attribs, 1)) return false;

		renderer_->AddFont(font_, "data/fonts/GoodDog.otf");
		if (font_ == nullptr)
			return false;

		fps_text_ = scythe::DynamicText::Create(renderer_, 30);
		if (!fps_text_)
			return false;

		// Create parser
		parser_ = new Parser(this);

		// Create console
		console_ = new Console(renderer_, font_, gui_shader_, text_shader_,
			0.6f, 0.05f, 0.6f, aspect_ratio_);
		console_->set_parser(parser_->object());

		// Matrices setup
		scythe::Matrix4 projection;
		scythe::Matrix4::CreatePerspective(90.0f, aspect_ratio_, 0.1f, 100.0f, &projection);
		renderer_->SetProjectionMatrix(projection);

		scythe::Vector3 eye(10.0f, 5.0f, 0.0f), target(0.0f, 0.0f, 0.0f);
		scythe::Matrix4 view_matrix;
		scythe::Matrix4::CreateLookAt(eye, target, scythe::Vector3::UnitY(), &view_matrix);
		renderer_->SetViewMatrix(view_matrix);

		projection_view_matrix_ = renderer_->projection_matrix() * renderer_->view_matrix();

		// Setup other parameters
		light_position_.Set(100.0f, 100.0f, 100.f);

		// Finally bind constants
		BindShaderConstants();
		
		return true;
	}
	void Unload() final
	{
		if (console_)
			delete console_;
		if (parser_)
			delete parser_;
		if (fps_text_)
			delete fps_text_;
		objects_.clear(); // release nodes
		SC_SAFE_RELEASE(box_model_);
		SC_SAFE_RELEASE(sphere_model_)
		SC_SAFE_RELEASE(box_mesh_);
		SC_SAFE_RELEASE(sphere_mesh_);

		scythe::PhysicsController::GetInstance()->Deinitialize();
		scythe::PhysicsController::DestroyInstance();
	}
	void Update() final
	{
		BindShaderVariables();

		const float kFrameTime = GetFrameTime();
		console_->Update(kFrameTime);
	}
	void UpdatePhysics(float sec) final
	{
		scythe::PhysicsController::GetInstance()->Update(sec);
	}
	void RenderNode(scythe::Node * node, const scythe::Vector3& color)
	{
		renderer_->PushMatrix();
		renderer_->LoadMatrix(node->GetWorldMatrix());

		object_shader_->UniformMatrix4fv("u_model", renderer_->model_matrix());
		object_shader_->Uniform3fv("u_color", color);
		
		node->GetDrawable()->Draw();
		
		renderer_->PopMatrix();
	}
	void RenderObjects()
	{
		object_shader_->Bind();
		object_shader_->UniformMatrix4fv("u_projection_view", projection_view_matrix_);

		for (auto& object : objects_)
		{
			RenderNode(object.node(), object.color());
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

		// Draw console
		console_->Render();
		
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
		if (console_->IsActive())
		{
			console_->ProcessCharInput(code);
		}
	}
	void OnKeyDown(scythe::PublicKey key, int mods) final
	{
		// Console blocks key input
		if (console_->IsActive())
		{
			console_->ProcessKeyInput(key, mods);
		}
		else // process another input
		{
			if (key == scythe::PublicKey::kF)
			{
				ToggleFullscreen();
			}
			else if (key == scythe::PublicKey::kEscape)
			{
				DesktopApplication::Terminate();
			}
			else if ((key == scythe::PublicKey::kGraveAccent) && !(mods & scythe::ModifierKey::kShift))
			{
				console_->Move();
			}
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
	scythe::Mesh * sphere_mesh_;
	scythe::Mesh * box_mesh_;

	scythe::Model * sphere_model_;
	scythe::Model * box_model_;

	scythe::Shader * object_shader_;
	scythe::Shader * text_shader_;
	scythe::Shader * gui_shader_;

	scythe::Font * font_;
	scythe::DynamicText * fps_text_;
	Parser * parser_;
	Console * console_;
	
	scythe::Matrix4 projection_view_matrix_;

	scythe::Vector3 light_position_;

	std::vector<Object> objects_;
};

DECLARE_MAIN(SandboxApp);