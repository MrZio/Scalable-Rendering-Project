#ifndef _TRIANGLE_MESH_INCLUDE
#define _TRIANGLE_MESH_INCLUDE

#include <Eigen/Dense>
#include <Eigen/SVD>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "ShaderProgram.h"



using namespace std;

// Class TriangleMesh contains the geometry of a mesh built out of triangles.
// Both the vertices and the triangles are stored in vectors.
// TriangleMesh also manages the ids of the copy in the GPU, so as to
// be able to render it using OpenGL.

enum class SimplifyMode
{
	QEM_STANDARD,
	NORMAL_CLUSTERING
};

class TriangleMesh
{

	public:
	TriangleMesh();
	~TriangleMesh();

	void addVertex(const glm::vec3 &position);
	void addTriangle(int v0, int v1, int v2);

	void initVertices(const vector<float> &newVertices, const vector<float> &newColors);
	void initTriangles(const vector<int> &newTriangles);
	void simplify(int resolution, SimplifyMode mode = SimplifyMode::QEM_STANDARD);
	void computeAllLODs(SimplifyMode mode = SimplifyMode::NORMAL_CLUSTERING);

	void buildCube();

	void sendToOpenGL(int lvl, const vector<glm::vec3>& currentVerts, const vector<int>& currentTris);
	void render() const;
	void free();

	float getDiagonal() const;
	// Restituisce il numero di triangoli per un certo livello
	int getCost(int lodLevel) const { return numTrianglesLOD[lodLevel]; }
	// Modifichiamo il render affinché accetti il livello da disegnare
	void render(int lodLevel) const;

	// create getters for vectors below
	const vector<glm::vec3> &getVertices() const;
	const vector<glm::vec3> &getColors() const;
	const vector<int> &getTriangles() const;

	struct GridIndex
	{
		int i, j, k;

		bool operator<(const GridIndex &other) const
		{
			if (i != other.i)
				return i < other.i;
			if (j != other.j)
				return j < other.j;
			return k < other.k;
		}
	};

	// struct CellInfo
	// {
	// 	glm::vec3 sum;	 // Somma delle posizioni dei vertici nella cella
	// 	int count;		 // Numero di vertici ci sono finiti dentro
	// 	int newVertexId; // Id del nuovo vertice creato per questa cella

	// 	// Costruttore: all'inizio il cestino è vuoto
	// 	CellInfo()
	// 	{
	// 		sum = glm::vec3(0.0f);
	// 		count = 0;
	// 		newVertexId = -1;
	// 	}
	// };

	struct CellInfo
	{
		Eigen::Matrix4d Q[8];
		int count[8];
		int newVertexId[8];

		CellInfo()
		{
			for (int i = 0; i < 8; i++)
			{
				Q[i].setZero();
				count[i] = 0;
				newVertexId[i] = -1;
			}
		}
	};


private:

static const int NUM_LODS = 4;
int numTrianglesLOD[NUM_LODS];
// I dati originali ci servono ancora intatti in RAM per poter calcolare i LOD peggiori
vector<glm::vec3> originalVertices;
vector<int> originalTriangles;

vector<glm::vec3> vertices;
vector<glm::vec3> colors;
vector<int> triangles;
//vector<glm::vec3> originalVertices;
//vector<int> originalTriangles;

GLuint vao[NUM_LODS];
GLuint vbo[NUM_LODS];
GLint posLocation, normalLocation, colorLocation;
};

#endif // _TRIANGLE_MESH_INCLUDE
