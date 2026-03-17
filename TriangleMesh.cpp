#include <iostream>
#include <vector>
#include <map>
#include "TriangleMesh.h"
#include "Application.h"


using namespace std;


TriangleMesh::TriangleMesh()
{
	vao = -1;
	vbo = -1;
}

TriangleMesh::~TriangleMesh()
{
	free();
}


void TriangleMesh::addVertex(const glm::vec3 &position)
{
	vertices.push_back(position);
	colors.push_back(glm::vec3(1.0f));
}

void TriangleMesh::addTriangle(int v0, int v1, int v2)
{
	triangles.push_back(v0);
	triangles.push_back(v1);
	triangles.push_back(v2);
}

void TriangleMesh::initVertices(const vector<float> &newVertices, const vector<float> &newColors)
{
	vertices.resize(newVertices.size() / 3);
	colors.resize(vertices.size(), glm::vec3(1.0f));
	for(unsigned int i=0; i<vertices.size(); i++)
	{
		vertices[i] = glm::vec3(newVertices[3*i], newVertices[3*i+1], newVertices[3*i+2]);
		if(newColors.size() >= 3 * (i + 1))
			colors[i] = glm::vec3(newColors[3*i], newColors[3*i+1], newColors[3*i+2]);
	}
}

void TriangleMesh::initTriangles(const vector<int> &newTriangles)
{
	triangles = newTriangles;
}

void TriangleMesh::buildCube()
{
	float vertices[] = {-1, -1, -1,
                      1, -1, -1,
                      1,  1, -1,
                      -1,  1, -1,
                      -1, -1,  1,
                      1, -1,  1,
                      1,  1,  1,
                      -1,  1,  1
	};

	int faces[] = {3, 1, 0, 3, 2, 1,
                 5, 6, 7, 4, 5, 7,
                 7, 3, 0, 0, 4, 7,
                 1, 2, 6, 6, 5, 1,
                 0, 1, 4, 5, 4, 1,
                 2, 3, 7, 7, 6, 2
	};

	int i;

	for(i=0; i<8; i+=1)
		addVertex(0.5f * glm::vec3(vertices[3*i], vertices[3*i+1], vertices[3*i+2]));
	for(i=0; i<12; i++)
		addTriangle(faces[3*i], faces[3*i+1], faces[3*i+2]);
}

void TriangleMesh::sendToOpenGL()
{
	vector<float> data;
	data.reserve(triangles.size() * 9);
	
	for(unsigned int tri=0; tri<triangles.size(); tri+=3)
	{
		glm::vec3 normal;
	
		normal = glm::cross(vertices[triangles[tri+1]] - vertices[triangles[tri]], 
	                      vertices[triangles[tri+2]] - vertices[triangles[tri]]);
		normal = glm::normalize(normal);
		for(unsigned int vrtx=0; vrtx<3; vrtx++)
		{
			data.push_back(vertices[triangles[tri + vrtx]].x);
			data.push_back(vertices[triangles[tri + vrtx]].y);
			data.push_back(vertices[triangles[tri + vrtx]].z);

			data.push_back(normal.x);
			data.push_back(normal.y);
			data.push_back(normal.z);

			const glm::vec3 &color = colors[triangles[tri + vrtx]];
			data.push_back(color.r);
			data.push_back(color.g);
			data.push_back(color.b);
		}
	}

	// Send data to OpenGL
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
	posLocation = Application::instance().getShader()->bindVertexAttribute("position", 3, 9*sizeof(float), 0);
	normalLocation = Application::instance().getShader()->bindVertexAttribute("normal", 3, 9*sizeof(float), (void *)(3*sizeof(float)));
	colorLocation = Application::instance().getShader()->bindVertexAttribute("colorAttr", 3, 9*sizeof(float), (void *)(6*sizeof(float)));
}

void TriangleMesh::render() const
{
	Application::instance().getShader()->use();

	glBindVertexArray(vao);
	glEnableVertexAttribArray(posLocation);
	glEnableVertexAttribArray(normalLocation);
	glEnableVertexAttribArray(colorLocation);
	glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(triangles.size()));
}

void TriangleMesh::free()
{
	if(vbo != -1)
		glDeleteBuffers(1, &vbo);
	if(vao != -1)
		glDeleteVertexArrays(1, &vao);
	
	vertices.clear();
	colors.clear();
	triangles.clear();
}

void TriangleMesh::simplify(float cellSize)
{
    cout << "Prima: " << vertices.size() << " vertici, " << triangles.size() / 3 << " triangoli." << endl;
	std::map<GridIndex, CellInfo> grid;
    
    // Se la mesh non ha vertici, interrompiamo per evitare crash
    if(vertices.empty()) return;

    // Calcoliamo il minBox esplorando tutti i vertici (troviamo le X, Y e Z minime assolute)
    glm::vec3 minBox = vertices[0];
    for (int i = 0; i < vertices.size(); i++) {
        if (vertices[i].x < minBox.x) minBox.x = vertices[i].x;
        if (vertices[i].y < minBox.y) minBox.y = vertices[i].y;
        if (vertices[i].z < minBox.z) minBox.z = vertices[i].z;
    }

    // ==========================================
    // PARTE 1: RAGGRUPPAMENTO
    // ==========================================
    for (int v = 0; v < vertices.size(); v++)
    {
        glm::vec3 vertex = vertices[v];

        GridIndex index;
        index.i = floor((vertex.x - minBox.x) / cellSize);
        index.j = floor((vertex.y - minBox.y) / cellSize);
        index.k = floor((vertex.z - minBox.z) / cellSize);

        grid[index].sum += vertex;
        grid[index].count += 1;
    } // <-- CRITICO: Il ciclo della Parte 1 DEVE finire qui!

    // ==========================================
    // PARTE 2: CREAZIONE NUOVI VERTICI
    // ==========================================
    vector<glm::vec3> newVertices;
    // vector<glm::vec3> newColors; // Da implementare in futuro se vuoi i colori

    for (auto it = grid.begin(); it != grid.end(); it++)
    {
        glm::vec3 verticeMedio = it->second.sum / (float)it->second.count;
        newVertices.push_back(verticeMedio);
        it->second.newVertexId = newVertices.size() - 1;
    } // <-- Il ciclo della Parte 2 finisce qui!

    // ==========================================
    // PARTE 3: RICOSTRUZIONE TRIANGOLI
    // ==========================================
    vector<int> newTriangles;
    
    for (size_t t = 0; t < triangles.size(); t += 3) 
    {
        int v1 = triangles[t];
        int v2 = triangles[t + 1];
        int v3 = triangles[t + 2];

        glm::vec3 pos1 = vertices[v1];
        glm::vec3 pos2 = vertices[v2];
        glm::vec3 pos3 = vertices[v3];

        GridIndex idx1 = {
            (int)floor((pos1.x - minBox.x) / cellSize),
            (int)floor((pos1.y - minBox.y) / cellSize),
            (int)floor((pos1.z - minBox.z) / cellSize)
        };
        GridIndex idx2 = {
            (int)floor((pos2.x - minBox.x) / cellSize),
            (int)floor((pos2.y - minBox.y) / cellSize),
            (int)floor((pos2.z - minBox.z) / cellSize)
        };
        GridIndex idx3 = {
            (int)floor((pos3.x - minBox.x) / cellSize),
            (int)floor((pos3.y - minBox.y) / cellSize),
            (int)floor((pos3.z - minBox.z) / cellSize)
        };

        int newV1 = grid[idx1].newVertexId;
        int newV2 = grid[idx2].newVertexId;
        int newV3 = grid[idx3].newVertexId;
    
        if(newV1 != newV2 && newV2 != newV3 && newV1 != newV3) 
        {
            newTriangles.push_back(newV1);
            newTriangles.push_back(newV2);
            newTriangles.push_back(newV3);
        }
    } // <-- Il ciclo della Parte 3 finisce qui!

    // ==========================================
    // SALVATAGGIO FINALE
    // ==========================================
    // Sostituiamo i vecchi array giganti con quelli nuovi, piccoli e semplificati!
    vertices = newVertices;
    triangles = newTriangles;

	cout << "Dopo: " << vertices.size() << " vertici, " << triangles.size() / 3 << " triangoli." << endl;
    
    // IMPORTANTE: Dobbiamo dire a OpenGL di ricaricare il modello nella scheda video!
    sendToOpenGL(); 
}
