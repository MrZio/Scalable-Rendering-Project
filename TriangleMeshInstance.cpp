#include "Application.h"
#include "TriangleMeshInstance.h"
#include <glm/gtc/matrix_transform.hpp>

// Flag globale: attiva/disattiva il LOD coloring (toggle col tasto C)
bool gLODColoring = true;


TriangleMeshInstance::TriangleMeshInstance()
{
	mesh = NULL;
	metallic = 0.0f;
	roughness = 1.0f;
	cooldownFrames = 0;
}

TriangleMeshInstance::~TriangleMeshInstance()
{
}


void TriangleMeshInstance::init(TriangleMesh *mesh, const glm::vec4 &color, const glm::mat4 &transform, float metallic, float roughness)
{
	this->mesh = mesh;
	this->color = color;
	this->transform = transform;
	this->metallic = metallic;
	this->roughness = roughness;
}

void TriangleMeshInstance::render()
{
	if(mesh != NULL)
	{
		Application::instance().getShader()->use();
        
        // --- INIZIO LOD COLORING ---
        glm::vec4 renderColor = color; // colore base
        
        // Coloriamo solo se il toggle e' attivo e solo i modelli con LOD (costo > 0),
        // cosi' muri e pavimento restano del loro colore base.
        if (gLODColoring && mesh->getCost(1) > 0) 
        {
            if (lodLevel == 0) renderColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);      
            else if (lodLevel == 1) renderColor = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f); 
            else if (lodLevel == 2) renderColor = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f); 
            else if (lodLevel == 3) renderColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); 
        }
        // --- FINE LOD COLORING ---

 		Application::instance().getShader()->setUniform4f("color", renderColor.r, renderColor.g, renderColor.b, renderColor.a);
		Application::instance().getShader()->setUniform1f("metallic", metallic);
		Application::instance().getShader()->setUniform1f("roughness", roughness);
		Application::instance().getShader()->setUniformMatrix4f("model", transform);
        
		mesh->render(lodLevel);
	}
}



void TriangleMeshInstance::setTransform(const glm::mat4 &transform)
{
	this->transform = transform;
}

const glm::mat4 &TriangleMeshInstance::getTransform() const
{
	return transform;
}

void TriangleMeshInstance::addTransform(const glm::mat4 &addedTransform)
{
	transform = addedTransform * transform;
}

void TriangleMeshInstance::resetTransform()
{
	transform = glm::mat4(1.0f);
}

void TriangleMeshInstance::translate(const glm::vec3 &move)
{
	transform = glm::translate(glm::mat4(1.0f), move) * transform;
}

void TriangleMeshInstance::rotate(const glm::vec3 &axis, float angleDegrees)
{
	transform = glm::rotate(glm::mat4(1.0f), angleDegrees * 3.14159f / 180.f, axis) * transform;
}

void TriangleMeshInstance::scale(const glm::vec3 &factor)
{
	transform = glm::scale(glm::mat4(1.0f), factor) * transform;
}

void TriangleMeshInstance::setColor(const glm::vec4 &color)
{
	this->color = color;
}

const glm::vec4 &TriangleMeshInstance::getColor() const
{
	return color;
}

void TriangleMeshInstance::setMaterial(float metallic, float roughness)
{
	this->metallic = metallic;
	this->roughness = roughness;
}

float TriangleMeshInstance::getMetallic() const
{
	return metallic;
}

float TriangleMeshInstance::getRoughness() const
{
	return roughness;
}