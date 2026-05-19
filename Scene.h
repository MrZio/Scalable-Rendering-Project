#ifndef _SCENE_INCLUDE
#define _SCENE_INCLUDE


#include <glm/glm.hpp>
#include <vector>
#include <map>
#include <utility>
#include "VectorCamera.h"
#include "TriangleMesh.h"
#include "TriangleMeshInstance.h"


using namespace std;


// Scene contains all the entities of our game.
// It is responsible for updating and render them.


class Scene
{

public:
	Scene();
	~Scene();

	void init();
	bool loadMap(const string &filename);
	TriangleMesh *loadMesh(const string &filename) const;
	void update(int deltaTime);
	void render();
	void changeLevelDetail(int delta);
    int getGridResolution();
    void setGridResolution(int newResolution);
	std::map<std::pair<int, int>, char> grid; // Mappa che associa a ogni cella (x, z) il suo tipo di blocco (0, 1, 2, 3 o 4)

	void toggleAutoLOD();
	VectorCamera &getCamera();

private:
	void computeModelViewMatrix();
	
	void buildRoom();

private:
	VectorCamera camera;
	TriangleMesh *meshCube, *meshFigurine, *meshWall, *meshBase, *meshBunny, *meshDragon, *meshHappy;
	vector<TriangleMeshInstance *> objects;
	float currentTime;
	bool bAutoLOD = true; // Se vero usa il Greedy, se falso usa i tasti

};


#endif // _SCENE_INCLUDE

