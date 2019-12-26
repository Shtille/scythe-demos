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

class SandboxApp : public scythe::OpenGlApplication
				 , public scythe::DesktopInputListener
{
public:
	SandboxApp()
	: sphere_mesh_(nullptr)
	, box_mesh_(nullptr)
	, sphere_model_(nullptr)
	, box_model_(nullptr)
	, sphere_node_(nullptr)
	, box_node_(nullptr)
	, font_(nullptr)
	, fps_text_(nullptr)
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
	bool Load() final
	{
		// Constants
		const float kSphereRadius(1.0f);
		const scythe::Vector3 kSpherePosition(0.0f, 3.0f, 0.0f);

		const scythe::Vector3 kBoxExtents(5.0f, 1.0f, 5.0f);
		const scythe::Vector3 kBoxPosition(0.0f, 0.0f, 0.0f);

		scythe::PhysicsController::CreateInstance();
		if (!scythe::PhysicsController::GetInstance()->Initialize())
			return false;

		// Sphere model
		sphere_mesh_ = new scythe::Mesh(renderer_);
		sphere_mesh_->AddFormat(scythe::VertexAttribute(scythe::VertexAttribute::kVertex, 3));
		sphere_mesh_->AddFormat(scythe::VertexAttribute(scythe::VertexAttribute::kNormal, 3));
		sphere_mesh_->CreateSphere(kSphereRadius, 128, 64);
		if (!sphere_mesh_->MakeRenderable())
			return false;

		// Box model
		box_mesh_ = new scythe::Mesh(renderer_);
		box_mesh_->AddFormat(scythe::VertexAttribute(scythe::VertexAttribute::kVertex, 3));
		box_mesh_->AddFormat(scythe::VertexAttribute(scythe::VertexAttribute::kNormal, 3));
		box_mesh_->CreateBox(kBoxExtents);
		if (!box_mesh_->MakeRenderable())
			return false;

		// Models
		sphere_model_ = scythe::Model::Create(sphere_mesh_);
		box_model_ = scythe::Model::Create(box_mesh_);

		// Nodes
		{
			scythe::PhysicsRigidBody::Parameters params(0.1f);
			sphere_node_ = scythe::Node::Create("sphere");
			sphere_node_->SetTranslation(kSpherePosition);
			sphere_node_->SetDrawable(sphere_model_);
			sphere_node_->SetCollisionObject(scythe::PhysicsCollisionObject::kRigidBody,
				scythe::PhysicsCollisionShape::DefineSphere(kSphereRadius),
				&params);
		}
		{
			scythe::PhysicsRigidBody::Parameters params(0.0f);
			box_node_ = scythe::Node::Create("box");
			box_node_->SetTranslation(kBoxPosition);
			box_node_->SetDrawable(box_model_);
			box_node_->SetCollisionObject(scythe::PhysicsCollisionObject::kRigidBody,
				scythe::PhysicsCollisionShape::DefineBox(kBoxExtents),
				&params);
		}
		
		// Load shaders
		const char *attribs[] = {"a_position"};
		if (!renderer_->AddShader(object_shader_, "data/shaders/sandbox/object", attribs, _countof(attribs))) return false;
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
		if (fps_text_)
			delete fps_text_;
		box_node_->SetDrawable(nullptr);
		SC_SAFE_RELEASE(box_node_);
		sphere_node_->SetDrawable(nullptr);
		SC_SAFE_RELEASE(sphere_node_);
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

		RenderNode(sphere_node_, scythe::Vector3(1.0f, 1.0f, 0.5f));
		RenderNode(box_node_, scythe::Vector3(0.1f, 1.0f, 0.2f));

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
	scythe::Mesh * sphere_mesh_;
	scythe::Mesh * box_mesh_;

	scythe::Model * sphere_model_;
	scythe::Model * box_model_;

	scythe::Node * sphere_node_;
	scythe::Node * box_node_;

	scythe::Shader * object_shader_;
	scythe::Shader * text_shader_;

	scythe::Font * font_;
	scythe::DynamicText * fps_text_;
	
	scythe::Matrix4 projection_view_matrix_;

	scythe::Vector3 light_position_;
};

DECLARE_MAIN(SandboxApp);