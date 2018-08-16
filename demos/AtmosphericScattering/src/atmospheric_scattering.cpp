#include "../../sht/include/sht.h"
#include "../../sht/graphics/include/model/sphere_model.h"
#include "../../sht/graphics/include/renderer/text.h"
#include "../../sht/utility/include/console.h"
#include "../../sht/utility/include/camera.h"
#include "../../sht/geo/include/constants.h"
#include "../../sht/geo/include/planet_navigation.h"
#include <cmath>

namespace {
    const float kCameraDistance = sht::geo::kEarthRadius * 5.0f;
	const float kInnerRadius = sht::geo::kEarthRadius;
	const float kOuterRadius = sht::geo::kEarthAtmosphereRadius;
    const float kCloudsRadius = sht::geo::kEarthCloudsRadius;
    const sht::math::Vector3 kEarthPosition(0.0f, 0.0f, 0.0f);
    /*
     The distance from Earth to Sun is 1.52*10^11 meters, so practically we dont need to compute
     vector to Sun for each vertex. Thus we just use sun direction vector.
     */
    const sht::math::Vector3 kSunDirection(1.0f, 0.0f, 0.0f);
}

class AtmosphericScatteringApp : public sht::OpenGlApplication
{
public:
    AtmosphericScatteringApp()
    : sphere_(nullptr)
	, font_(nullptr)
	, fps_text_(nullptr)
	, console_(nullptr)
    , camera_manager_(nullptr)
    , planet_navigation_(nullptr)
    , angle_(0.0f)
    , need_update_projection_matrix_(true)
    , camera_animation_stopped_(false)
    {
    }
    const char* GetTitle() final
    {
        return "Shtille Earth";
    }
    const bool IsMultisample() final
    {
        return true;
    }
	void BindShaderConstants()
	{
		const float Kr = 0.0030f;
		const float Km = 0.0015f;
		const float ESun = 16.0f;
		const float g = -0.75f;
		const float scale = 1.0f / (kOuterRadius - kInnerRadius);
		const float scale_depth = 0.25f;
		const float scale_over_scale_depth = scale / scale_depth;

        ground_shader_->Bind();
        ground_shader_->Uniform3fv("u_to_light", kSunDirection);
        ground_shader_->Uniform3f("u_inv_wave_length", 1.0f / powf(0.650f, 4.0f), 1.0f / powf(0.570f, 4.0f), 1.0f / powf(0.475f, 4.0f));
        ground_shader_->Uniform1f("u_inner_radius", kInnerRadius);
        ground_shader_->Uniform1f("u_outer_radius", kOuterRadius);
        ground_shader_->Uniform1f("u_outer_radius2", kOuterRadius * kOuterRadius);
        ground_shader_->Uniform1f("u_kr_esun", Kr * ESun);
        ground_shader_->Uniform1f("u_km_esun", Km * ESun);
        ground_shader_->Uniform1f("u_kr_4_pi", Kr * 4.0f * sht::math::kPi);
        ground_shader_->Uniform1f("u_km_4_pi", Km * 4.0f * sht::math::kPi);
        ground_shader_->Uniform1f("u_scale", scale);
        ground_shader_->Uniform1f("u_scale_depth", scale_depth);
        ground_shader_->Uniform1f("u_scale_over_scale_depth", scale_over_scale_depth);
        ground_shader_->Uniform1i("u_samples", 4);
        ground_shader_->Uniform1i("u_earth_texture", 0);
        ground_shader_->Unbind();
        
        clouds_shader_->Bind();
        clouds_shader_->Uniform3fv("u_to_light", kSunDirection);
        clouds_shader_->Uniform3f("u_inv_wave_length", 1.0f / powf(0.650f, 4.0f), 1.0f / powf(0.570f, 4.0f), 1.0f / powf(0.475f, 4.0f));
        clouds_shader_->Uniform1f("u_inner_radius", kCloudsRadius);
        clouds_shader_->Uniform1f("u_outer_radius", kOuterRadius);
        clouds_shader_->Uniform1f("u_outer_radius2", kOuterRadius * kOuterRadius);
        clouds_shader_->Uniform1f("u_kr_esun", Kr * ESun);
        clouds_shader_->Uniform1f("u_km_esun", Km * ESun);
        clouds_shader_->Uniform1f("u_kr_4_pi", Kr * 4.0f * sht::math::kPi);
        clouds_shader_->Uniform1f("u_km_4_pi", Km * 4.0f * sht::math::kPi);
        clouds_shader_->Uniform1f("u_scale", 1.0f / (kOuterRadius - kCloudsRadius));
        clouds_shader_->Uniform1f("u_scale_depth", scale_depth);
        clouds_shader_->Uniform1f("u_scale_over_scale_depth", 1.0f / (kOuterRadius - kCloudsRadius) / scale_depth);
        clouds_shader_->Uniform1i("u_samples", 4);
        clouds_shader_->Uniform1i("u_clouds_texture", 0);
        clouds_shader_->Unbind();
        
        sky_shader_->Bind();
        sky_shader_->Uniform3fv("u_to_light", kSunDirection);
        sky_shader_->Uniform3f("u_inv_wave_length", 1.0f / powf(0.650f, 4.0f), 1.0f / powf(0.570f, 4.0f), 1.0f / powf(0.475f, 4.0f));
        sky_shader_->Uniform1f("u_inner_radius", kInnerRadius);
        sky_shader_->Uniform1f("u_outer_radius", kOuterRadius);
        sky_shader_->Uniform1f("u_outer_radius2", kOuterRadius * kOuterRadius);
        sky_shader_->Uniform1f("u_kr_esun", Kr * ESun);
        sky_shader_->Uniform1f("u_km_esun", Km * ESun);
        sky_shader_->Uniform1f("u_kr_4_pi", Kr * 4.0f * sht::math::kPi);
        sky_shader_->Uniform1f("u_km_4_pi", Km * 4.0f * sht::math::kPi);
        sky_shader_->Uniform1f("u_scale", scale);
        sky_shader_->Uniform1f("u_scale_depth", scale_depth);
        sky_shader_->Uniform1f("u_scale_over_scale_depth", scale_over_scale_depth);
        sky_shader_->Uniform1i("u_samples", 4);
        sky_shader_->Uniform1f("u_g", g);
        sky_shader_->Uniform1f("u_g2", g * g);
        sky_shader_->Unbind();
	}
	void BindShaderVariables()
	{
		float distance_to_earth = camera_manager_->position()->Distance(kEarthPosition);
        int from_space = (distance_to_earth > kOuterRadius) ? 1 : 0;

		ground_shader_->Bind();
		ground_shader_->Uniform3fv("u_camera_pos", *camera_manager_->position());
		ground_shader_->Uniform1f("u_camera_height", distance_to_earth);
		ground_shader_->Uniform1f("u_camera_height2", distance_to_earth * distance_to_earth);
        ground_shader_->Uniform1i("u_from_space", from_space);
		ground_shader_->Unbind();
        
        clouds_shader_->Bind();
        clouds_shader_->Uniform3fv("u_camera_pos", *camera_manager_->position());
        clouds_shader_->Uniform1f("u_camera_height", distance_to_earth);
        clouds_shader_->Uniform1f("u_camera_height2", distance_to_earth * distance_to_earth);
        clouds_shader_->Uniform1i("u_from_space", from_space);
        clouds_shader_->Unbind();

		sky_shader_->Bind();
		sky_shader_->Uniform3fv("u_camera_pos", *camera_manager_->position());
		sky_shader_->Uniform1f("u_camera_height", distance_to_earth);
		sky_shader_->Uniform1f("u_camera_height2", distance_to_earth * distance_to_earth);
        sky_shader_->Uniform1i("u_from_space", from_space);
		sky_shader_->Unbind();
	}
    bool Load() final
    {
        // Sphere model
		sphere_ = new sht::graphics::SphereModel(renderer_, 128, 64);
		sphere_->AddFormat(sht::graphics::VertexAttribute(sht::graphics::VertexAttribute::kVertex, 3));
		sphere_->AddFormat(sht::graphics::VertexAttribute(sht::graphics::VertexAttribute::kNormal, 3));
		sphere_->AddFormat(sht::graphics::VertexAttribute(sht::graphics::VertexAttribute::kTexcoord, 2));
		sphere_->Create();
        if (!sphere_->MakeRenderable())
            return false;
        
		// Load shaders
        const char *attribs[] = {"a_position", "a_normal", "a_texcoord"};
        if (!renderer_->AddShader(ground_shader_, "data/shaders/atmosphere/ground", attribs, _countof(attribs))) return false;
        if (!renderer_->AddShader(clouds_shader_, "data/shaders/atmosphere/clouds", attribs, _countof(attribs))) return false;
		if (!renderer_->AddShader(sky_shader_, "data/shaders/atmosphere/sky", attribs, _countof(attribs))) return false;
        if (!renderer_->AddShader(text_shader_, "data/shaders/text", attribs, 1)) return false;
        if (!renderer_->AddShader(gui_shader_, "data/shaders/gui_colored", attribs, 1)) return false;
        
		// Load textures
        if (!renderer_->AddTexture(earth_texture_, "data/textures/earth.jpg",
                                   sht::graphics::Texture::Wrap::kClampToEdge,
                                   sht::graphics::Texture::Filter::kTrilinearAniso)) return false;
        if (!renderer_->AddTexture(clouds_texture_, "data/textures/clouds.jpg",
                                   sht::graphics::Texture::Wrap::kClampToEdge,
                                   sht::graphics::Texture::Filter::kTrilinearAniso)) return false;
		if (!renderer_->AddTexture(lights_texture_, "data/textures/lights.jpg")) return false;

        renderer_->AddFont(font_, "data/fonts/GoodDog.otf");
		if (font_ == nullptr)
			return false;

		fps_text_ = sht::graphics::DynamicText::Create(renderer_, 30);
        if (!fps_text_)
            return false;
        
        console_ = new sht::utility::Console(renderer_, font_, gui_shader_, text_shader_, 0.7f, 0.1f, 0.8f, aspect_ratio_);

        camera_manager_ = new sht::utility::CameraManager();
        
        planet_navigation_ = new sht::geo::PlanetNavigation(camera_manager_, sht::geo::kEarthRadius, kCameraDistance, 100.0f);

		// Finally bind constants
		BindShaderConstants();
        
        return true;
    }
    void Unload() final
    {
        if (planet_navigation_)
            delete planet_navigation_;
        if (camera_manager_)
            delete camera_manager_;
		if (console_)
			delete console_;
		if (fps_text_)
			delete fps_text_;
		if (sphere_)
			delete sphere_;
    }
    void Update() final
    {
        const float kFrameTime = GetFrameTime();

        angle_ += 0.005f * kFrameTime;
        rotate_matrix = sht::math::Rotate4(cos(angle_), sin(angle_), 0.0f, 1.0f, 0.0f);
        
        console_->Update(kFrameTime);

        if (!camera_animation_stopped_)
            camera_manager_->Update(kFrameTime);

        // Update matrices
        renderer_->SetViewMatrix(camera_manager_->view_matrix());
        UpdateProjectionMatrix();
        projection_view_matrix_ = renderer_->projection_matrix() * renderer_->view_matrix();

		BindShaderVariables();
    }
    void RenderGround()
    {
        renderer_->PushMatrix();
        renderer_->Translate(kEarthPosition);
        renderer_->Scale(kInnerRadius);
        
        ground_shader_->Bind();
        ground_shader_->UniformMatrix4fv("u_projection_view", projection_view_matrix_);
        ground_shader_->UniformMatrix4fv("u_model", renderer_->model_matrix());
        
        renderer_->ChangeTexture(earth_texture_, 0);
        
        sphere_->Render();
        
        renderer_->ChangeTexture(nullptr, 0);
        
        ground_shader_->Unbind();
        
        renderer_->PopMatrix();
    }
    void RenderClouds()
    {
        renderer_->PushMatrix();
        renderer_->Translate(kEarthPosition);
        renderer_->Scale(kCloudsRadius);
        renderer_->MultMatrix(rotate_matrix);
        
        clouds_shader_->Bind();
        clouds_shader_->UniformMatrix4fv("u_projection_view", projection_view_matrix_);
        clouds_shader_->UniformMatrix4fv("u_model", renderer_->model_matrix());
        
        renderer_->ChangeTexture(clouds_texture_, 0);
        
        sphere_->Render();
        
        renderer_->ChangeTexture(nullptr, 0);
        
        clouds_shader_->Unbind();
        
        renderer_->PopMatrix();
    }
    void RenderSky()
    {
        renderer_->CullFace(sht::graphics::CullFaceType::kFront);
        
        renderer_->PushMatrix();
        renderer_->Translate(kEarthPosition);
        renderer_->Scale(kOuterRadius);
        renderer_->MultMatrix(rotate_matrix);
        
        sky_shader_->Bind();
        sky_shader_->UniformMatrix4fv("u_projection_view", projection_view_matrix_);
        sky_shader_->UniformMatrix4fv("u_model", renderer_->model_matrix());
        
        sphere_->Render();
        
        sky_shader_->Unbind();
        
        renderer_->PopMatrix();
        
        renderer_->CullFace(sht::graphics::CullFaceType::kBack);
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
        
        RenderGround();
        RenderSky();
        RenderClouds();

        RenderInterface();
    }
    void OnChar(unsigned short code)
    {
        if (console_->IsActive())
        {
            console_->ProcessCharInput(code);
        }
    }
    void OnKeyDown(sht::PublicKey key, int mods) final
    {
        // Console blocks key input
        if (console_->IsActive())
        {
            console_->ProcessKeyInput(key, mods);
        }
        else // process another input
        {
            if (key == sht::PublicKey::kF)
            {
                ToggleFullscreen();
            }
            else if (key == sht::PublicKey::kEscape)
            {
                Application::Terminate();
            }
            else if (key == sht::PublicKey::kEqual)
            {
                planet_navigation_->SmoothZoomIn();
            }
            else if (key == sht::PublicKey::kMinus)
            {
                planet_navigation_->SmoothZoomOut();
            }
            else if ((key == sht::PublicKey::kGraveAccent) && !(mods & sht::ModifierKey::kShift))
            {
                console_->Move();
            }
            else if (key == sht::PublicKey::kSpace)
            {
                camera_animation_stopped_ = !camera_animation_stopped_;
            }
            else if (key == sht::PublicKey::kR)
            {
                bool shift_presseed = (mods & sht::ModifierKey::kShift) == sht::ModifierKey::kShift;
                float angle_x = 0.25f * sht::math::kPi; // rotation by Pi/4
                if (shift_presseed)
                    angle_x = -angle_x; // opposite direction
                planet_navigation_->SmoothRotation(angle_x);
            }
        }
    }
    void OnMouseDown(sht::MouseButton button, int modifiers) final
    {
        if (mouse_.button_down(sht::MouseButton::kLeft))
        {
            const sht::math::Vector4& viewport = renderer_->viewport();
            const sht::math::Matrix4& proj = renderer_->projection_matrix();
            const sht::math::Matrix4& view = renderer_->view_matrix();
            planet_navigation_->PanBegin(mouse_.x(), mouse_.y(), viewport, proj, view);
        }
    }
    void OnMouseUp(sht::MouseButton button, int modifiers) final
    {
        if (mouse_.button_down(sht::MouseButton::kLeft))
        {
            planet_navigation_->PanEnd();
        }
    }
    void OnMouseMove() final
    {
        if (mouse_.button_down(sht::MouseButton::kLeft))
        {
            const sht::math::Vector4& viewport = renderer_->viewport();
            const sht::math::Matrix4& proj = renderer_->projection_matrix();
            const sht::math::Matrix4& view = renderer_->view_matrix();
            planet_navigation_->PanMove(mouse_.x(), mouse_.y(), viewport, proj, view);
        }
    }
    void OnSize(int w, int h) final
    {
        Application::OnSize(w, h);
        // To have correct perspective when resizing
        need_update_projection_matrix_ = true;
    }
    void UpdateProjectionMatrix()
    {
        if (need_update_projection_matrix_ || camera_manager_->animated())
        {
            need_update_projection_matrix_ = false;
            
            float znear, zfar;
            planet_navigation_->ObtainZNearZFar(&znear, &zfar);
            renderer_->SetProjectionMatrix(sht::math::PerspectiveMatrix(45.0f, width(), height(), znear, zfar));
        }
    }
    
private:
    sht::graphics::Model * sphere_;
    sht::graphics::Shader * ground_shader_;
    sht::graphics::Shader * clouds_shader_;
	sht::graphics::Shader * sky_shader_;
    sht::graphics::Shader * gui_shader_;
    sht::graphics::Shader * text_shader_;
    sht::graphics::Texture * earth_texture_;
	sht::graphics::Texture * clouds_texture_;
	sht::graphics::Texture * lights_texture_;
    sht::graphics::Font * font_;
    sht::graphics::DynamicText * fps_text_;
    sht::utility::Console * console_;
    sht::utility::CameraManager * camera_manager_;
    sht::geo::PlanetNavigation * planet_navigation_;
    
    sht::math::Matrix4 projection_view_matrix_;
    
    sht::math::Matrix4 rotate_matrix;
    sht::math::Matrix3 normal_matrix;
    
    float angle_; //!< rotation angle of earth
    
    bool need_update_projection_matrix_;
    bool camera_animation_stopped_;
};

DECLARE_MAIN(AtmosphericScatteringApp);