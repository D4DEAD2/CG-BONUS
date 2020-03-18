#include "SceneBuildLayer.h"
#include "florp/game/SceneManager.h"
#include "florp/game/RenderableComponent.h"
#include <florp\graphics\MeshData.h>
#include <florp\graphics\MeshBuilder.h>
#include <florp\graphics\ObjLoader.h>

#include <glm/gtc/matrix_transform.hpp>
#include <florp\game\Transform.h>
#include "RotateBehaviour.h"
#include "CameraComponent.h"
#include "florp/app/Application.h"
#include <ControlBehaviour.h>
#include <ShadowLight.h>
#include "PointLightComponent.h"

/*
 * Helper function for creating a shadow casting light
 * @param scene The scene to create the light in
 * @param entityOut An optional pointer to receive the ENTT entity that was created (set to nullptr if you don't care)
 * @param pos The position of the light in world space
 * @param target The point for the light to look at, in world space
 * @param up A unit vector indicating what axis is considered 'up'
 * @param distance The far clipping plane of the light
 * @param fov The field of view of the light, in degrees
 * @param bufferSize The size of the buffer to create for the light (default 1024x1024)
 * @param name The name to associate with the light's buffer
 */
ShadowLight& CreateShadowCaster(florp::game::Scene* scene, entt::entity* entityOut, glm::vec3 pos, glm::vec3 target, glm::vec3 up, float distance = 10.0f, float fov = 60.0f, glm::ivec2 bufferSize = { 1024, 1024 }, const char* name = nullptr)
{
	// The depth attachment is a texture, with 32 bits for depth
	RenderBufferDesc depth = RenderBufferDesc();
	depth.ShaderReadable = true;
	depth.Attachment = RenderTargetAttachment::Depth;
	depth.Format = RenderTargetType::Depth32;

	// Our shadow buffer is depth-only
	FrameBuffer::Sptr shadowBuffer = std::make_shared<FrameBuffer>(bufferSize.x, bufferSize.y);
	shadowBuffer->AddAttachment(depth);
	shadowBuffer->Validate();
	if (name != nullptr)
		shadowBuffer->SetDebugName(name);

	// Create a new entity
	entt::entity entity = scene->CreateEntity();

	// Assign and initialize a shadow light component
	ShadowLight& light = scene->Registry().assign<ShadowLight>(entity);
	light.ShadowBuffer = shadowBuffer;
	light.Projection = glm::perspective(glm::radians(fov), (float)bufferSize.x / (float)bufferSize.y, 0.25f, distance);
	light.Attenuation = 1.0f / distance;
	light.Color = glm::vec3(1.0f);

	// Assign and initialize the transformation
	florp::game::Transform& t = scene->Registry().get<florp::game::Transform>(entity);
	t.SetPosition(pos);
	t.LookAt(target, up);

	// Send out the entity ID if we passed in a place to store it
	if (entityOut != nullptr)
		*entityOut = entity;

	return light;
}

void SceneBuilder::Initialize()
{
	florp::app::Application* app = florp::app::Application::Get();
	
	using namespace florp::game;
	using namespace florp::graphics;
	
	auto* scene = SceneManager::RegisterScene("main");
	SceneManager::SetCurrentScene("main");

	// We'll load in a monkey head to render something interesting
	MeshData data = ObjLoader::LoadObj("monkey.obj", glm::vec4(1.0f));

	Shader::Sptr shader = std::make_shared<Shader>();
	shader->LoadPart(ShaderStageType::VertexShader, "shaders/lighting.vs.glsl");
	shader->LoadPart(ShaderStageType::FragmentShader, "shaders/forward.fs.glsl"); 
	shader->Link();

	// Load and set up our simple test material
	Material::Sptr mat = std::make_shared<Material>(shader); 
	mat->Set("s_Albedo", Texture2D::LoadFromFile("marble.png", false, true, true));

	Material::Sptr mat2 = mat->Clone(); 
	mat2->Set("s_Albedo", Texture2D::LoadFromFile("polka.png", false, true, true));

	// We'll use a constant to tell us how many monkeys to use
	const int numMonkeys = 6;
	const float step = glm::two_pi<float>() / numMonkeys; // Determine the angle between monkeys in radians

	// We'll create a ring of monkeys
	for (int ix = 0; ix < numMonkeys; ix++) {
		entt::entity test = scene->CreateEntity();
		RenderableComponent& renderable = scene->Registry().assign<RenderableComponent>(test);
		renderable.Mesh = MeshBuilder::Bake(data);
		renderable.Material = mat;
		Transform& t = scene->Registry().get<Transform>(test);
		t.SetPosition(glm::vec3(glm::cos(step * ix) * 5.0f, 0.0f, glm::sin(step * ix) * 5.0f));
		t.SetEulerAngles(glm::vec3(-90.0f, glm::degrees(-step * ix), 0.0f)); 
	}
	
	// We'll create a ring of point lights behind each monkey
	for (int ix = 0; ix < numMonkeys; ix++) {
		// We'll attach an indicator cube to all the lights, and align it with the light's facing
		entt::entity entity = scene->CreateEntity();
		PointLightComponent& light = scene->Registry().assign<PointLightComponent>(entity);
		light.Color = glm::vec3(
			glm::sin(-ix * step) + 1.0f, 
			glm::cos(-ix * step) + 1.0f, 
			glm::sin((-ix * step) + glm::pi<float>()) + 1.0f) / 2.0f * 0.7f;
		light.Attenuation = 1.0f / 40.0f;
		Transform& t = scene->Registry().get<Transform>(entity);
		t.SetPosition(glm::vec3(glm::cos(step * ix) * 20.0f, 2.0f, glm::sin(step * ix) * 20.0f));
	}

	// The central monkey
	{
		entt::entity test = scene->CreateEntity();
		RenderableComponent& renderable = scene->Registry().assign<RenderableComponent>(test);
		renderable.Mesh = MeshBuilder::Bake(data);
		renderable.Material = mat;
		Transform& t = scene->Registry().get<Transform>(test);
		// Make our monkeys spin around the center
		scene->AddBehaviour<RotateBehaviour>(test, glm::vec3(45.0f, 45.0f, 45.0f));
	}
	
	// The box with the polka pattern
	{
		MeshData indicatorCube = MeshBuilder::Begin();
		MeshBuilder::AddAlignedCube(indicatorCube, glm::vec3(0.0f, 0, 0.0), glm::vec3(2.0f, 2.0f, 2.0f));
		Mesh::Sptr indicatorMesh = MeshBuilder::Bake(indicatorCube);
		
		entt::entity test = scene->CreateEntity();
		RenderableComponent& renderable = scene->Registry().assign<RenderableComponent>(test);
		renderable.Mesh = indicatorMesh;
		renderable.Material = mat2;
		Transform& t = scene->Registry().get<Transform>(test);
		t.SetPosition({ 20.0f, 0.0f, 0.0f });
	}

	// We'll use a tiny cube to cast a shadow from our camera, and to indicate where the light sources are
	MeshData indicatorCube = MeshBuilder::Begin();
	MeshBuilder::AddAlignedCube(indicatorCube, glm::vec3(0.0f, 0, 0.0), glm::vec3(0.1f, 0.1f, 0.1f));
	Mesh::Sptr indicatorMesh = MeshBuilder::Bake(indicatorCube);
	
	
	// Creates our main camera
	{
		// The color buffer should be marked as shader readable, so that we generate a texture for it
		RenderBufferDesc mainColor = RenderBufferDesc();
		mainColor.ShaderReadable = true;
		mainColor.Attachment = RenderTargetAttachment::Color0;
		mainColor.Format = RenderTargetType::ColorRgb8;

		// The normal buffer
		RenderBufferDesc normalBuffer = RenderBufferDesc(); // NEW
		normalBuffer.ShaderReadable = true;
		normalBuffer.Attachment = RenderTargetAttachment::Color1;
		normalBuffer.Format = RenderTargetType::ColorRgb10; // Note: this format is 10 bits per component
		
		// The depth attachment does not need to be a texture (and would cause issues since the format is DepthStencil)
		RenderBufferDesc depth = RenderBufferDesc();
		depth.ShaderReadable = true;
		depth.Attachment = RenderTargetAttachment::Depth;
		depth.Format = RenderTargetType::Depth32;

		// Our main frame buffer needs a color output, and a depth output
		FrameBuffer::Sptr buffer = std::make_shared<FrameBuffer>(app->GetWindow()->GetWidth(), app->GetWindow()->GetHeight(), 4);
		buffer->AddAttachment(mainColor);
		buffer->AddAttachment(normalBuffer);
		buffer->AddAttachment(depth);
		buffer->Validate();
		buffer->SetDebugName("MainBuffer");

		// We'll create an entity, and attach a camera component to it
		entt::entity camera = scene->CreateEntity();
		CameraComponent& cam = scene->Registry().assign<CameraComponent>(camera);
		cam.BackBuffer = buffer;
		cam.FrontBuffer = buffer->Clone();
		cam.IsMainCamera = true;
		cam.Projection = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 1000.0f);

		// We'll add our control behaviour so that we can fly the camera around
		scene->AddBehaviour<ControlBehaviour>(camera, glm::vec3(1.0f));

		// We'll attach a cube to the camera so that it casts shadows
		RenderableComponent& renderable = scene->Registry().assign<RenderableComponent>(camera);
		renderable.Mesh = indicatorMesh;
		renderable.Material = mat;
	}

	// We'll create a projector to cast our smile on the floor
	auto& light = CreateShadowCaster(scene, nullptr, glm::vec3(0.0f, 5.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), 25.0f);
	light.Color = glm::vec3(1.0f, 1.0f, 1.0f);
	light.Attenuation = 1.0f / 15.0f; 
	light.ProjectorImage = Texture2D::LoadFromFile("light_projection.png", false, false, true); 

	
	// We'll create a ring of shadow casting lights, one for each monkey
	for (int ix = 0; ix < numMonkeys; ix++) {
		auto& light = CreateShadowCaster(
			scene, nullptr, 
			glm::vec3(glm::cos(step * ix) * 9.0f, 3.5f, glm::sin(step * ix) * 9.0f), // Each light will be behind the monkey
			glm::vec3(0.0f),                                                         // Look at the center
			glm::vec3(0.0f, 1.0f, 0.0f),                                             // Y is up
			25.0f,                                                                   // The far plane is 25 units away
			75.0f);                                                                  // We'll use a 75 degree field of view
		// We'll generate a color for the light
		light.Color = glm::vec3(glm::sin(ix * step) + 1.0f, glm::cos(ix * step) + 1.0f, glm::sin((ix * step) + glm::pi<float>()) + 1.0f) / 2.0f * 0.1f; 
		light.Attenuation = 1.0f / 20.0f;

		// We'll attach an indicator cube to all the lights, and align it with the light's facing
		entt::entity entity = scene->CreateEntity();
		RenderableComponent& renderable = scene->Registry().assign<RenderableComponent>(entity);
		renderable.Mesh = indicatorMesh;
		renderable.Material = mat;
		Transform& t = scene->Registry().get<Transform>(entity);
		t.SetPosition(glm::vec3(glm::cos(step * ix) * 9.0f, 2.0f, glm::sin(step * ix) * 9.0f));
	}
			
	// Our floor plane
	{
		// Building the mesh
		MeshData data = MeshBuilder::Begin();
		MeshBuilder::AddAlignedCube(data, glm::vec3(0.0f, -1.0f, 0.0), glm::vec3(100.0f, 0.1f, 100.0f));
		Mesh::Sptr mesh = MeshBuilder::Bake(data);

		// Creating the entity and attaching the renderable
		entt::entity entity = scene->CreateEntity();
		RenderableComponent& renderable = scene->Registry().assign<RenderableComponent>(entity);
		renderable.Mesh = MeshBuilder::Bake(data);
		renderable.Material = mat;
	}
}
