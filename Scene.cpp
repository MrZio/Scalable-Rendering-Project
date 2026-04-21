#include <iostream>
#include <cmath>
#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include "Scene.h"
#include "PLYReader.h"
#include "Application.h"

int gridResolution = 100; 

Scene::Scene()
{
	meshCube = NULL;
	meshFigurine = NULL;
	meshWall = NULL;
	meshBunny = NULL;
	meshDragon = NULL;
}

Scene::~Scene()
{
	if (meshCube != NULL)
		delete meshCube;
	if (meshFigurine != NULL)
		delete meshFigurine;
	if (meshWall != NULL)
		delete meshWall;
	if (meshBase != NULL)
		delete meshBase;
	if (meshBunny != NULL)
		delete meshBunny;
	if (meshDragon != NULL)
		delete meshDragon;
	for (vector<TriangleMeshInstance *>::iterator it = objects.begin(); it != objects.end(); it++)
		delete *it;
}

// Initialize the scene. This includes the cube we will use to render
// the floor and ceiling, as well as the camera.

void Scene::init()
{
	meshCube = new TriangleMesh();
	meshCube->buildCube();
	meshCube->sendToOpenGL();
	currentTime = 0.0f;

	camera.init(glm::vec3(0.f, 1.0f, 2.f));
}

// Load the map & all its associated models

bool Scene::loadMap(const string &filename)
{
	ifstream fin;
	string model_filename;

	fin.open(filename);
	if (!fin.is_open()) {
        cout << "ERRORE CRITICO: Impossibile trovare il file della mappa: " << filename << endl;
		return false;
    }

	// 1. Armadillo
	fin >> model_filename;
	if ((meshFigurine = loadMesh(model_filename)) == NULL) {
        cout << "ERRORE CRITICO: Impossibile caricare il modello: " << model_filename << endl;
		return false;
    }

	// 2. Muro
	fin >> model_filename;
	if ((meshWall = loadMesh(model_filename)) == NULL) {
        cout << "ERRORE CRITICO: Impossibile caricare il modello: " << model_filename << endl;
		return false;
    }

	// 3. Base
	fin >> model_filename;
	if ((meshBase = loadMesh(model_filename)) == NULL) {
        cout << "ERRORE CRITICO: Impossibile caricare il modello: " << model_filename << endl;
		return false;
    }

    // 4. Bunny
	fin >> model_filename;
	if ((meshBunny = loadMesh(model_filename)) == NULL) {
        cout << "ERRORE CRITICO: Impossibile caricare il modello: " << model_filename << endl;
		return false;
    }

    
	// Semplifichiamo il Bunny 
    cout << "Semplificando il Bunny..." << endl;
    meshBunny->simplify(gridResolution); 

	fin >> model_filename;
	if ((meshDragon = loadMesh(model_filename)) == NULL)
		return false;


	// 5. Dragon
	fin >> model_filename;
	if ((meshDragon = loadMesh(model_filename)) == NULL) {
        cout << "ERRORE CRITICO: Impossibile caricare il modello: " << model_filename << endl;
		return false;
    }

    // Semplifichiamo il Drago (proviamo con cubetti più grandi, 0.05)
    cout << "Semplificando il Drago..." << endl;
    meshDragon->simplify(100); 

	buildRoom();

	return true;
}

// Loads the mesh into CPU memory and sends it to GPU memory (using GL)

TriangleMesh *Scene::loadMesh(const string &filename) const
{
	TriangleMesh *mesh;
#pragma warning(push)
#pragma warning(disable : 4101)
	PLYReader reader;
#pragma warning(pop)

	mesh = new TriangleMesh();
	bool bSuccess = reader.readMesh(filename, *mesh);
	if (bSuccess)
		mesh->sendToOpenGL();
	else
	{
		delete mesh;
		mesh = NULL;
	}

	return mesh;
}

void Scene::update(int deltaTime)
{
	currentTime += deltaTime;
}

// Render the scene. First the room, then the mesh it there is one loaded.

void Scene::render()
{
	Application::instance().getShader()->use();
	camera.render();
	for (vector<TriangleMeshInstance *>::iterator it = objects.begin(); it != objects.end(); it++)
		(*it)->render();
}

VectorCamera &Scene::getCamera()
{
	return camera;
}

int Scene::getGridResolution() {
	return gridResolution;
}

void Scene::setGridResolution(int newResolution) {
	gridResolution = newResolution;
}

void Scene::changeLevelDetail(int delta) {
    gridResolution += delta;
    if(gridResolution < 1) gridResolution = 1;

    cout << "Semplificando..." << endl;
    if(meshBunny != NULL) meshBunny->simplify(gridResolution);
    if(meshDragon != NULL) meshDragon->simplify(gridResolution);
}
// Init & render the room. Both the floor and the walls are instances of the
// same initial cube scaled and translated to build the room.

void Scene::buildRoom()
{
	glm::mat4 transform;
	TriangleMeshInstance *instance;

	// 1. Apriamo il file del livello appena creato
	ifstream fin("../level.txt");
	if (!fin.is_open())
	{
		cout << "ERRORE: Impossibile trovare level.txt" << endl;
		return; // Interrompe se non trova il file
	}

	string line;
	int z = 0;			   // Indice della riga (Asse Z nel 3D)
	float tileSize = 2.0f; // La grandezza fisica di ogni cella

	// 2. Leggiamo il file riga per riga
	while (fin >> line)
	{

		// 3. Scansioniamo ogni singolo carattere della riga
		for (int x = 0; x < line.length(); x++)
		{ // Indice della colonna (Asse X)
			char tileType = line[x];

			// Calcoliamo la coordinata matematica reale.
			// Sottraiamo 10.0f per far "centrare" la stanza attorno alla telecamera
			float realX = (x * tileSize) - 10.0f;
			float realZ = (z * tileSize) - 10.0f;

			// CASO A: Pavimento (Lo creiamo sempre, per qualsiasi blocco valido)
			if (tileType == '0' || tileType == '1' || tileType == '2' || tileType == '3' || tileType == '4')
			{
				transform = glm::mat4(1.0f);
				transform = glm::translate(transform, glm::vec3(realX, -0.05f, realZ));
				// Scaliamo un cubo in modo che sia largo "tileSize" ma piatto (0.1f)
				transform = glm::scale(transform, glm::vec3(tileSize, 0.1f, tileSize));
				instance = new TriangleMeshInstance();
				instance->init(meshCube, glm::vec4(0.137f, 0.094f, 0.074f, 1.0f), transform, 0.1f, 0.85f);
				objects.push_back(instance);
			}

			// CASO B: Muro
			if (tileType == '1')
			{
				transform = glm::mat4(1.0f);
				// Alziamo il muro di 1.0 sull'asse Y per poggiarlo sopra al pavimento
				transform = glm::translate(transform, glm::vec3(realX, 1.0f, realZ));
				// Le slide di lab1.pdf dicono: "Empty cells & walls = Scaled cubes"
				transform = glm::scale(transform, glm::vec3(tileSize, 2.0f, tileSize));
				instance = new TriangleMeshInstance();
				// Grigio chiaro per i muri
				instance->init(meshCube, glm::vec4(0.6f, 0.6f, 0.6f, 1.0f), transform, 0.1f, 0.85f);
				objects.push_back(instance);
			}

			// CASO C: Piedistallo + Armadillo (il nostro nuovo codice '2')
			if (tileType == '2')
			{
				// Piedistallo
				transform = glm::mat4(1.0f);
				transform = glm::translate(transform, glm::vec3(realX, 0.0f, realZ));
				transform = glm::scale(transform, glm::vec3(0.5f, 0.75f, 0.5f));
				instance = new TriangleMeshInstance();
				instance->init(meshBase, glm::vec4(1.0f), transform, 0.15f, 0.75f);
				objects.push_back(instance);

				// Armadillo (meshFigurine)
				transform = glm::mat4(1.0f);
				transform = glm::translate(transform, glm::vec3(realX, 0.75f, realZ));
				transform = glm::scale(transform, glm::vec3(0.5f, 0.5f, 0.5f));
				instance = new TriangleMeshInstance();
				instance->init(meshFigurine, glm::vec4(1.0f), transform, 0.15f, 0.4f);
				objects.push_back(instance);
			}

			if (tileType == '3')
			{
				// 1. Prima creo il Piedistallo
				transform = glm::mat4(1.0f);
				transform = glm::translate(transform, glm::vec3(realX, 0.0f, realZ));
				transform = glm::scale(transform, glm::vec3(0.5f, 0.75f, 0.5f));
				instance = new TriangleMeshInstance();
				instance->init(meshBase, glm::vec4(1.0f), transform, 0.15f, 0.75f);
				objects.push_back(instance);

				// 2. Poi creo il Bunny sopra al piedistallo (Y a 0.75)
				transform = glm::mat4(1.0f);
				transform = glm::translate(transform, glm::vec3(realX, 0.75f, realZ));
				transform = glm::scale(transform, glm::vec3(0.5f, 0.5f, 0.5f)); // Se il bunny ti sembra piccolo, puoi provare 0.8f invece di 0.5f
				instance = new TriangleMeshInstance();
				instance->init(meshBunny, glm::vec4(1.0f), transform, 0.15f, 0.4f);
				objects.push_back(instance);
			}

			if (tileType == '4')
			{
				// 1. Prima creo il Piedistallo
				transform = glm::mat4(1.0f);
				transform = glm::translate(transform, glm::vec3(realX, 0.0f, realZ));
				transform = glm::scale(transform, glm::vec3(0.5f, 0.75f, 0.5f));
				instance = new TriangleMeshInstance();
				instance->init(meshBase, glm::vec4(1.0f), transform, 0.15f, 0.75f);
				objects.push_back(instance);

				// 2. Poi creo il Dragon sopra al piedistallo (Y a 0.75)
				transform = glm::mat4(1.0f);
				transform = glm::translate(transform, glm::vec3(realX, 0.75f, realZ));
				transform = glm::scale(transform, glm::vec3(0.5f, 0.5f, 0.5f));
				instance = new TriangleMeshInstance();
				instance->init(meshDragon, glm::vec4(1.0f), transform, 0.15f, 0.75f);
				objects.push_back(instance);
			}
		}
		z++; // Finito di leggere la riga, incrementiamo l'asse Z
	}
}
