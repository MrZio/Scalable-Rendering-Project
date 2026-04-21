#include <Eigen/Dense>
#include <Eigen/SVD>
#ifndef _TRIANGLE_MESH_INCLUDE
#define _TRIANGLE_MESH_INCLUDE


#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "ShaderProgram.h"


using namespace std;


// Class TriangleMesh contains the geometry of a mesh built out of triangles.
// Both the vertices and the triangles are stored in vectors.
// TriangleMesh also manages the ids of the copy in the GPU, so as to 
// be able to render it using OpenGL.


class TriangleMesh
{

public:
	TriangleMesh();
	~TriangleMesh();

	void addVertex(const glm::vec3 &position);
	void addTriangle(int v0, int v1, int v2);

	void initVertices(const vector<float> &newVertices, const vector<float> &newColors);
	void initTriangles(const vector<int> &newTriangles);
	void simplify(int resolution);

	void buildCube();
	
	void sendToOpenGL();
	void render() const;
	void free();

	//create getters for vectors below
	const vector<glm::vec3>& getVertices() const;
	const vector<glm::vec3>& getColors() const;
	const vector<int>& getTriangles() const;

	struct GridIndex {
		int i, j, k;

		bool operator<(const GridIndex& other) const {
			if (i != other.i) return i < other.i;
			if (j != other.j) return j < other.j;
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

	struct CellInfo {
    	Eigen::Matrix4d Q; // La matrice per la QEM
    	int count;         // Numero di vertici (opzionale, ma utile)
    	int newVertexId;   // L'ID del vertice semplificato

    CellInfo() {
        Q.setZero();
        count = 0;
        newVertexId = -1;
    }
};
	

private:
	vector<glm::vec3> vertices;
	vector<glm::vec3> colors;
	vector<int> triangles;
	vector<glm::vec3> originalVertices;
    vector<int> originalTriangles;
	
	GLuint vao;
	GLuint vbo;
	GLint posLocation, normalLocation, colorLocation;

	
	
};


#endif // _TRIANGLE_MESH_INCLUDE

