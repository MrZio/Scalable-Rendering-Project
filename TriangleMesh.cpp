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
    originalVertices = vertices;
}

void TriangleMesh::initTriangles(const vector<int> &newTriangles)
{
	triangles = newTriangles;
    originalTriangles = triangles;
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

void TriangleMesh::simplify(int resolution)
{
    // Sicurezza: se non c'è il backup, fermiamo tutto!
    if (originalVertices.empty()) return;

    // 1. CALCOLO DEL BOUNDING BOX (Scansionando il BACKUP)
    glm::vec3 minBox = originalVertices[0];
    glm::vec3 maxBox = originalVertices[0];
    
    for (int i = 0; i < originalVertices.size(); i++) {
        minBox.x = min(minBox.x, originalVertices[i].x);
        minBox.y = min(minBox.y, originalVertices[i].y);
        minBox.z = min(minBox.z, originalVertices[i].z);
        
        maxBox.x = max(maxBox.x, originalVertices[i].x);
        maxBox.y = max(maxBox.y, originalVertices[i].y);
        maxBox.z = max(maxBox.z, originalVertices[i].z);
    }

    // 2. CALCOLO DELLA CELL SIZE
    float maxAxis = max(maxBox.x - minBox.x, max(maxBox.y - minBox.y, maxBox.z - minBox.z));
    float cellSize = maxAxis / (float)resolution;

    // CREIAMO UNA MAPPA PULITA PER QUESTO CICLO
    std::map<GridIndex, CellInfo> grid; 

    // ==========================================
    // PARTE 1: RAGGRUPPAMENTO (Leggendo dal BACKUP)
    // ==========================================
    for (int v = 0; v < originalVertices.size(); v++) 
    {
        glm::vec3 vertex = originalVertices[v]; // <-- Modificato

        GridIndex index;
        index.i = floor((vertex.x - minBox.x) / cellSize);
        index.j = floor((vertex.y - minBox.y) / cellSize);
        index.k = floor((vertex.z - minBox.z) / cellSize);

        grid[index].sum += vertex;
        grid[index].count += 1;
    } 

    // ==========================================
    // PARTE 2: CREAZIONE NUOVI VERTICI
    // ==========================================
    vector<glm::vec3> newVertices;

    for (auto it = grid.begin(); it != grid.end(); it++)
    {
        glm::vec3 verticeMedio = it->second.sum / (float)it->second.count;
        newVertices.push_back(verticeMedio);
        it->second.newVertexId = newVertices.size() - 1;
    }

    // ==========================================
    // PARTE 3: RICOSTRUZIONE TRIANGOLI (Leggendo dal BACKUP)
    // ==========================================
    vector<int> newTriangles;
    
    for (size_t t = 0; t < originalTriangles.size(); t += 3) // <-- Modificato
    {
        int v1 = originalTriangles[t];       // <-- Modificato
        int v2 = originalTriangles[t + 1];   // <-- Modificato
        int v3 = originalTriangles[t + 2];   // <-- Modificato

        glm::vec3 pos1 = originalVertices[v1]; // <-- Modificato
        glm::vec3 pos2 = originalVertices[v2]; // <-- Modificato
        glm::vec3 pos3 = originalVertices[v3]; // <-- Modificato

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
    }

    // ==========================================
    // SALVATAGGIO FINALE
    // ==========================================
    vertices = newVertices;
    triangles = newTriangles;

    cout << "Risoluzione " << resolution << " -> Dopo: " << vertices.size() << " vertici, " << triangles.size() / 3 << " triangoli." << endl;
    
    sendToOpenGL(); 
}
