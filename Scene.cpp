#include <iostream>
#include <cmath>
#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include "Scene.h"
#include "PLYReader.h"
#include "Application.h"

int gridResolution = 100;
int globalManualLOD = 0; // Parte da 0 (massima qualità)

Scene::Scene()
{
	meshCube = NULL;
	meshFigurine = NULL;
	meshWall = NULL;
	meshBunny = NULL;
	meshDragon = NULL;
	meshHappy = NULL;
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
	if (meshHappy != NULL)
		delete meshHappy;
	for (vector<TriangleMeshInstance *>::iterator it = objects.begin(); it != objects.end(); it++)
		delete *it;
}

// Initialize the scene. This includes the cube we will use to render
// the floor and ceiling, as well as the camera.

void Scene::init()
{
	meshCube = new TriangleMesh();
	meshCube->buildCube();
	meshCube->sendToOpenGL(0, meshCube->getVertices(), meshCube->getTriangles());
	currentTime = 0.0f;

	camera.init(glm::vec3(0.f, 1.0f, 2.f));
}

// Load the map & all its associated models

bool Scene::loadMap(const string &filename)
{
	ifstream fin;
	string model_filename;

	fin.open(filename);
	if (!fin.is_open())
	{
		cout << "ERRORE CRITICO: Impossibile trovare il file della mappa: " << filename << endl;
		return false;
	}

	// 1. Armadillo
	fin >> model_filename;
	if ((meshFigurine = loadMesh(model_filename)) == NULL)
	{
		cout << "ERRORE CRITICO: Impossibile caricare il modello: " << model_filename << endl;
		return false;
	}

	// 2. Muro
	fin >> model_filename;
	if ((meshWall = loadMesh(model_filename)) == NULL)
	{
		cout << "ERRORE CRITICO: Impossibile caricare il modello: " << model_filename << endl;
		return false;
	}

	// 3. Base
	fin >> model_filename;
	if ((meshBase = loadMesh(model_filename)) == NULL)
	{
		cout << "ERRORE CRITICO: Impossibile caricare il modello: " << model_filename << endl;
		return false;
	}

	// 4. Bunny
	fin >> model_filename;
	if ((meshBunny = loadMesh(model_filename)) == NULL)
	{
		cout << "ERRORE CRITICO: Impossibile caricare il modello: " << model_filename << endl;
		return false;
	}

	// Semplifichiamo il Bunny
	// cout << "Semplificando il Bunny..." << endl;
	// meshBunny->simplify(100, SimplifyMode::NORMAL_CLUSTERING);

	// 5. Dragon
	fin >> model_filename;
	if ((meshDragon = loadMesh(model_filename)) == NULL)
	{
		cout << "ERRORE CRITICO: Impossibile caricare il modello: " << model_filename << endl;
		return false;
	}

	// 6. Happy
	fin >> model_filename;
	meshHappy = loadMesh(model_filename);
	if (meshHappy == NULL)
	{
		cout << "ATTENZIONE: Impossibile caricare " << model_filename << ". Il piedistallo sara' vuoto." << endl;
	}

	// Semplifichiamo il Drago (proviamo con cubetti più grandi, 0.05)
	// cout << "Semplificando il Drago..." << endl;
	// meshDragon->simplify(100);
	// meshDragon->simplify(100, SimplifyMode::NORMAL_CLUSTERING);
	cout << "Pre-computing LODs for Armadillo..." << endl;
	meshFigurine->computeAllLODs(SimplifyMode::NORMAL_CLUSTERING);

	cout << "Pre-computing LODs for Bunny..." << endl;
	meshBunny->computeAllLODs(SimplifyMode::NORMAL_CLUSTERING);

	cout << "Pre-computing LODs for Dragon..." << endl;
	meshDragon->computeAllLODs(SimplifyMode::NORMAL_CLUSTERING);

	if (meshHappy != NULL)
	{
		cout << "Pre-computing LODs for Happy..." << endl;
		meshHappy->computeAllLODs(SimplifyMode::NORMAL_CLUSTERING); // Ricorda di usare QEM per l'organico!
	}

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
		mesh->sendToOpenGL(0, mesh->getVertices(), mesh->getTriangles());
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

	// Se vuoi usare i tasti, commenta tutto quello che segue!
	if (bAutoLOD)
	{
		// Altrimenti, la matematica automatica sovrascriverà sempre i tuoi tasti.

		int maxCost = 200000; // Alza un po' il budget per vedere i cambiamenti
		int currentTotalCost = 0;

		// 1. Reset
		for (TriangleMeshInstance *obj : objects)
		{
			obj->setLOD(3); // Tutti partono al minimo
			currentTotalCost += obj->getMesh()->getCost(3);
		}

		// 2. Greedy Loop
		bool canUpgrade = true;
		glm::vec3 camPos = camera.getPosition();

		while (currentTotalCost < maxCost && canUpgrade)
		{
			canUpgrade = false;
			float bestScore = -1.0f;
			TriangleMeshInstance *bestObj = nullptr;

			for (TriangleMeshInstance *obj : objects)
			{
				int L = obj->getLOD();
				if (L == 0)
					continue;

				float D = glm::distance(camPos, obj->getPosition());
				if (D < 0.1f)
					D = 0.1f;

				float d = obj->getMesh()->getDiagonal();

				// Usiamo il calcolo che abbiamo discusso
				float benefitAttuale = d / (D * (1 << L));
				float benefitFuturo = d / (D * (1 << (L - 1)));
				float deltaBenefit = benefitFuturo - benefitAttuale;

				int deltaCost = obj->getMesh()->getCost(L - 1) - obj->getMesh()->getCost(L);

				if (deltaCost > 0)
				{
					float score = deltaBenefit / (float)deltaCost;
					if (score > bestScore)
					{
						bestScore = score;
						bestObj = obj;
					}
				}
			}

			if (bestObj != nullptr)
			{
				int L = bestObj->getLOD();
				int costToUpgrade = bestObj->getMesh()->getCost(L - 1) - bestObj->getMesh()->getCost(L);

				if (currentTotalCost + costToUpgrade <= maxCost)
				{
					bestObj->setLOD(L - 1);
					currentTotalCost += costToUpgrade;
					canUpgrade = true;
				}
			}
		}
	}
	else
	{
		// MODALITÀ MANUALE: applichiamo il valore dei tasti a tutti gli oggetti
		for (TriangleMeshInstance *obj : objects)
		{
			obj->setLOD(globalManualLOD);
		}
	}
}

// Render the scene. First the room, then the mesh it there is one loaded.

void Scene::render()
{
	Application::instance().getShader()->use();
	camera.render();
	for (vector<TriangleMeshInstance *>::iterator it = objects.begin(); it != objects.end(); it++)
	{
		// Supponendo che TriangleMeshInstance abbia un metodo per impostare il LOD
		// (Se non lo ha, dovrai aggiungerlo nella classe TriangleMeshInstance)

		(*it)->render();
	}
}

void Scene::toggleAutoLOD()
{
	bAutoLOD = !bAutoLOD;
	cout << "Auto LOD is now " << (bAutoLOD ? "ON" : "OFF") << endl;
}

VectorCamera &Scene::getCamera()
{
	return camera;
}

int Scene::getGridResolution()
{
	return gridResolution;
}

void Scene::setGridResolution(int newResolution)
{
	gridResolution = newResolution;
}

void Scene::changeLevelDetail(int delta)
{
	// Spegniamo l'automatico non appena l'utente tocca un tasto manuale
	bAutoLOD = false;

	globalManualLOD += delta;

	// Assicuriamoci che l'indice rimanga tra 0 e 3 (NUM_LODS - 1)
	if (globalManualLOD < 0)
		globalManualLOD = 0;
	if (globalManualLOD > 3)
		globalManualLOD = 3;

	cout << "Auto LOD OFF - Switched to Manual LOD Level: " << globalManualLOD << endl;
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
			if (tileType == '0' || tileType == '1' || tileType == '2' || tileType == '3' || tileType == '4' || tileType == '5')
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
			if (tileType == '5')
			{
				// 1. Piedistallo
				transform = glm::mat4(1.0f);
				transform = glm::translate(transform, glm::vec3(realX, 0.0f, realZ));
				transform = glm::scale(transform, glm::vec3(0.5f, 0.75f, 0.5f));
				instance = new TriangleMeshInstance();
				instance->init(meshBase, glm::vec4(1.0f), transform, 0.15f, 0.75f);
				objects.push_back(instance);

				// 2. Happy sopra al piedistallo
				if (meshHappy != NULL)
				{
					transform = glm::mat4(1.0f);
					transform = glm::translate(transform, glm::vec3(realX, 0.75f, realZ));
					transform = glm::scale(transform, glm::vec3(0.5f, 0.5f, 0.5f));
					instance = new TriangleMeshInstance();
					instance->init(meshHappy, glm::vec4(1.0f), transform, 0.15f, 0.4f);
					objects.push_back(instance);
				}
			}
		}
		z++; // Finito di leggere la riga, incrementiamo l'asse Z
	}
}
