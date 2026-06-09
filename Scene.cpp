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

	loadPVS("../pvs.txt"); 
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

    if (bAutoLOD)
    {
        // --- Budget FISSO Hardware-Calibrated ---
        const long long TPS = 40000000; // Abbassato per evitare l'overdraw sul laptop
        const float targetFPS = 60.0f;
        const int maxCost = (int)(TPS / targetFPS); 
        int currentTotalCost = 0;

        int camCell = worldToCell(camera.getPosition());
        bool camValid = bPVSCulling && pvsLoaded && camCell != -1 && pvsVisible[camCell][camCell];
        glm::vec3 camPos = camera.getPosition();

        vector<float> objDistances(objects.size(), 0.1f);
        vector<bool> objVisible(objects.size(), false);
        vector<int> startLOD(objects.size(), 3);

        // 1. Fase di Reset e Setup
        for (size_t i = 0; i < objects.size(); ++i)
        {
            TriangleMeshInstance *obj = objects[i];
            
            startLOD[i] = obj->getLOD();
            obj->updateCooldown(); 

            int cell = (i < objectCell.size()) ? objectCell[i] : -1;
            objVisible[i] = (!camValid || cell == -1) ? true : (pvsVisible[camCell][cell] != 0);

            if (objVisible[i]) {
                objDistances[i] = glm::distance(camPos, obj->getPosition());
                if (objDistances[i] < 0.1f) objDistances[i] = 0.1f;

                if (obj->getCooldown() > 0) {
                    currentTotalCost += obj->getMesh()->getCost(obj->getLOD());
                } else {
                    obj->setLOD(3);
                    currentTotalCost += obj->getMesh()->getCost(3);
                }
            } else {
                obj->setLOD(3); 
            }
        }

        // 2. Greedy Loop ad altissime prestazioni
        bool canUpgrade = true;
        while (currentTotalCost < maxCost && canUpgrade)
        {
            canUpgrade = false;
            float bestScore = -1.0f;
            int bestObjIdx = -1;
            int bestCostDiff = 0;

            for (size_t i = 0; i < objects.size(); ++i)
            {
                if (!objVisible[i]) continue;

                TriangleMeshInstance *obj = objects[i];
                int L = obj->getLOD();

                if (L == 0 || obj->getCooldown() > 0) continue;

                float D = objDistances[i]; 
                
                // =======================================================
                // NOVITÀ: IL CUTOFF SPAZIALE (Fix per l'anomalia visiva)
                // Impedisce di sprecare il budget avanzato per oggetti lontani
                // =======================================================
                int nextLOD = L - 1;
                if (nextLOD == 0 && D > 6.0f) continue;  // Niente Bianco oltre le 6 celle di distanza
                if (nextLOD == 1 && D > 12.0f) continue; // Niente Blu oltre le 12 celle
                if (nextLOD == 2 && D > 18.0f) continue; // Niente Giallo oltre le 18 celle

                float d = obj->getMesh()->getDiagonal();
                float benefitAttuale = d / (D * (1 << L));
                float benefitFuturo  = d / (D * (1 << nextLOD));
                float deltaBenefit   = benefitFuturo - benefitAttuale;

                int costNow  = obj->getMesh()->getCost(L);
                int costNext = obj->getMesh()->getCost(nextLOD);
                int deltaCost = costNext - costNow;

                if (deltaCost > 0)
                {
                    if (currentTotalCost + deltaCost <= maxCost) 
                    {
                        float score = deltaBenefit / (float)deltaCost;
                        if (score > bestScore)
                        {
                            bestScore = score;
                            bestObjIdx = i;
                            bestCostDiff = deltaCost;
                        }
                    }
                }
            }

            // Promozione 
            if (bestObjIdx != -1)
            {
                TriangleMeshInstance* bestObj = objects[bestObjIdx];
                bestObj->setLOD(bestObj->getLOD() - 1);
                currentTotalCost += bestCostDiff;
                canUpgrade = true;
            }
        }

        // 3. Fase Finale: Isteresi inter-frame
        for (size_t i = 0; i < objects.size(); ++i)
        {
            if (objects[i]->getCooldown() == 0 && objects[i]->getLOD() != startLOD[i]) {
                objects[i]->setCooldown(20);
            }
        }
    }
    else
    {
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

	int camCell = worldToCell(camera.getPosition());
	bool camValid = bPVSCulling && pvsLoaded && camCell != -1 &&
	                pvsVisible[camCell][camCell];

	glm::vec3 camPos = camera.getPosition();

	// Raccogliamo gli oggetti da disegnare con la loro distanza dalla camera.
	static vector<pair<float, int>> drawList; // static: evita riallocazioni ogni frame
	drawList.clear();

	int culled = 0;
	for (size_t i = 0; i < objects.size(); ++i)
	{
		int cell = (i < objectCell.size()) ? objectCell[i] : -1;
		if (camValid && cell != -1 && pvsVisible[camCell][cell] == 0)
		{
			culled++;
			continue;
		}
		glm::vec3 d = objects[i]->getPosition() - camPos;
		drawList.push_back({glm::dot(d, d), (int)i}); // distanza^2, basta per ordinare
	}

	// FRONT-TO-BACK: i piu' vicini per primi -> early-Z scarta i frammenti coperti.
	sort(drawList.begin(), drawList.end());

	long long tris = 0;
	for (auto &e : drawList)
	{
		objects[e.second]->render();
		tris += objects[e.second]->getMesh()->getCost(objects[e.second]->getLOD());
	}

	static int f = 0;
	if (++f % 60 == 0)
		cout << "Disegnati: " << drawList.size() << " | cullati: " << culled
		     << " | triangoli: " << tris << endl;
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

	// Helper: aggiunge un oggetto E la sua cella, mantenendo i due vector allineati.
	// cell = -1  -> sempre disegnato (pavimento, muri)
	// cell >= 0  -> soggetto a culling PVS (modelli, piedistalli)
	auto addObject = [&](TriangleMeshInstance *inst, int cell)
	{
		objects.push_back(inst);
		objectCell.push_back(cell);
	};

	ifstream fin("../level.txt");
	if (!fin.is_open())
	{
		cout << "ERRORE: Impossibile trovare level.txt" << endl;
		return;
	}

	string line;
	int z = 0;
	float tileSize = 2.0f;

	while (fin >> line)
	{
		for (int x = 0; x < (int)line.length(); x++)
		{
			char tileType = line[x];

			float realX = (x * tileSize) - 10.0f;
			float realZ = (z * tileSize) - 10.0f;

			// Indice di cella per questo blocco (deve combaciare con il PVS: z*W + x)
			int thisCell = (pvsLoaded && pvsW > 0) ? (z * pvsW + x) : -1;

			// CASO A: Pavimento (sempre disegnato)
			if (tileType == '0' || tileType == '1' || tileType == '2' ||
			    tileType == '3' || tileType == '4' || tileType == '5')
			{
				transform = glm::mat4(1.0f);
				transform = glm::translate(transform, glm::vec3(realX, -0.05f, realZ));
				transform = glm::scale(transform, glm::vec3(tileSize, 0.1f, tileSize));
				instance = new TriangleMeshInstance();
				instance->init(meshCube, glm::vec4(0.137f, 0.094f, 0.074f, 1.0f), transform, 0.1f, 0.85f);
				addObject(instance, thisCell);
			}

			// CASO B: Muro (sempre disegnato)
			if (tileType == '1')
			{
				transform = glm::mat4(1.0f);
				transform = glm::translate(transform, glm::vec3(realX, 1.0f, realZ));
				transform = glm::scale(transform, glm::vec3(tileSize, 2.0f, tileSize));
				instance = new TriangleMeshInstance();
				instance->init(meshCube, glm::vec4(0.6f, 0.6f, 0.6f, 1.0f), transform, 0.1f, 0.85f);
				addObject(instance, -1);
			}

			// CASO C: Piedistallo + Armadillo
			if (tileType == '2')
			{
				transform = glm::mat4(1.0f);
				transform = glm::translate(transform, glm::vec3(realX, 0.0f, realZ));
				transform = glm::scale(transform, glm::vec3(0.5f, 0.75f, 0.5f));
				instance = new TriangleMeshInstance();
				instance->init(meshBase, glm::vec4(1.0f), transform, 0.15f, 0.75f);
				addObject(instance, thisCell);

				transform = glm::mat4(1.0f);
				transform = glm::translate(transform, glm::vec3(realX, 0.75f, realZ));
				transform = glm::scale(transform, glm::vec3(0.5f, 0.5f, 0.5f));
				instance = new TriangleMeshInstance();
				instance->init(meshFigurine, glm::vec4(1.0f), transform, 0.15f, 0.4f);
				addObject(instance, thisCell);
			}

			// CASO D: Piedistallo + Bunny
			if (tileType == '3')
			{
				transform = glm::mat4(1.0f);
				transform = glm::translate(transform, glm::vec3(realX, 0.0f, realZ));
				transform = glm::scale(transform, glm::vec3(0.5f, 0.75f, 0.5f));
				instance = new TriangleMeshInstance();
				instance->init(meshBase, glm::vec4(1.0f), transform, 0.15f, 0.75f);
				addObject(instance, thisCell);

				transform = glm::mat4(1.0f);
				transform = glm::translate(transform, glm::vec3(realX, 0.75f, realZ));
				transform = glm::scale(transform, glm::vec3(0.5f, 0.5f, 0.5f));
				instance = new TriangleMeshInstance();
				instance->init(meshBunny, glm::vec4(1.0f), transform, 0.15f, 0.4f);
				addObject(instance, thisCell);
			}

			// CASO E: Piedistallo + Dragon
			if (tileType == '4')
			{
				transform = glm::mat4(1.0f);
				transform = glm::translate(transform, glm::vec3(realX, 0.0f, realZ));
				transform = glm::scale(transform, glm::vec3(0.5f, 0.75f, 0.5f));
				instance = new TriangleMeshInstance();
				instance->init(meshBase, glm::vec4(1.0f), transform, 0.15f, 0.75f);
				addObject(instance, thisCell);

				transform = glm::mat4(1.0f);
				transform = glm::translate(transform, glm::vec3(realX, 0.75f, realZ));
				transform = glm::scale(transform, glm::vec3(0.5f, 0.5f, 0.5f));
				instance = new TriangleMeshInstance();
				instance->init(meshDragon, glm::vec4(1.0f), transform, 0.15f, 0.75f);
				addObject(instance, thisCell);
			}

			// CASO F: Piedistallo + Happy
			if (tileType == '5')
			{
				transform = glm::mat4(1.0f);
				transform = glm::translate(transform, glm::vec3(realX, 0.0f, realZ));
				transform = glm::scale(transform, glm::vec3(0.5f, 0.75f, 0.5f));
				instance = new TriangleMeshInstance();
				instance->init(meshBase, glm::vec4(1.0f), transform, 0.15f, 0.75f);
				addObject(instance, thisCell);

				if (meshHappy != NULL)
				{
					transform = glm::mat4(1.0f);
					transform = glm::translate(transform, glm::vec3(realX, 0.75f, realZ));
					transform = glm::scale(transform, glm::vec3(0.5f, 0.5f, 0.5f));
					instance = new TriangleMeshInstance();
					instance->init(meshHappy, glm::vec4(1.0f), transform, 0.15f, 0.4f);
					addObject(instance, thisCell);
				}
			}
		}
		z++;
	}
}

bool Scene::loadPVS(const string &filename)
{
	ifstream fin(filename);
	if (!fin.is_open())
	{
		cout << "ATTENZIONE: impossibile aprire " << filename
		     << ". Culling PVS disattivato." << endl;
		pvsLoaded = false;
		return false;
	}

	fin >> pvsW >> pvsH;
	if (pvsW <= 0 || pvsH <= 0)
	{
		cout << "ATTENZIONE: header PVS non valido." << endl;
		pvsLoaded = false;
		return false;
	}

	int N = pvsW * pvsH;
	pvsVisible.assign(N, vector<char>(N, 0));

	for (int i = 0; i < N; ++i)
	{
		int k;
		if (!(fin >> k))
			break;
		for (int j = 0; j < k; ++j)
		{
			int v;
			fin >> v;
			if (v >= 0 && v < N)
				pvsVisible[i][v] = 1;
		}
	}

	pvsLoaded = true;
	cout << "PVS caricato: " << pvsW << " x " << pvsH << " (" << N << " celle)" << endl;
	return true;
}

// Converte una posizione del mondo nell'indice di cella (stessa convenzione di buildRoom:
// realX = x*tileSize - 10, tileSize = 2). Ritorna -1 se fuori griglia.
int Scene::worldToCell(const glm::vec3 &pos) const
{
	if (pvsW == 0 || pvsH == 0)
		return -1;
	const float tileSize = 2.0f;
	int x = (int)floor((pos.x + 10.0f) / tileSize + 0.5f);
	int z = (int)floor((pos.z + 10.0f) / tileSize + 0.5f);
	if (x < 0 || x >= pvsW || z < 0 || z >= pvsH)
		return -1;
	return z * pvsW + x;
}

void Scene::togglePVSCulling()
{
	bPVSCulling = !bPVSCulling;
	cout << "PVS culling is now " << (bPVSCulling ? "ON" : "OFF") << endl;
}
