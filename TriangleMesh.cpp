#include <iostream>
#include <vector>
#include <map>
#include "TriangleMesh.h"
#include "Application.h"


using namespace std;



TriangleMesh::TriangleMesh()
{
    for(int i = 0; i < NUM_LODS; i++) {
        vao[i] = -1;
        vbo[i] = -1;
        numTrianglesLOD[i] = 0;
    }
	
}

TriangleMesh::~TriangleMesh()
{
	free();
}

const vector<glm::vec3> &TriangleMesh::getVertices() const
{
    return vertices;
}

const vector<glm::vec3> &TriangleMesh::getColors() const
{
    return colors;
}

const vector<int> &TriangleMesh::getTriangles() const
{
    return triangles;
}

void TriangleMesh::render() const
{
    // Se non viene specificato il LOD, deleghiamo la logica al LOD massimo (0)
    render(0);
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

void TriangleMesh::sendToOpenGL(int lvl, const vector<glm::vec3>& currentVerts, const vector<int>& currentTris)
{
	vector<float> data;
	data.reserve(currentTris.size() * 9);
	
	for(unsigned int tri=0; tri<currentTris.size(); tri+=3)
	{
		glm::vec3 normal;
	
		normal = glm::cross(currentVerts[currentTris[tri+1]] - currentVerts[currentTris[tri]], 
	                      currentVerts[currentTris[tri+2]] - currentVerts[currentTris[tri]]);
		normal = glm::normalize(normal);
		for(unsigned int vrtx=0; vrtx<3; vrtx++)
        {
            glm::vec3 p = currentVerts[currentTris[tri + vrtx]];
            data.push_back(p.x); data.push_back(p.y); data.push_back(p.z);
            data.push_back(normal.x); data.push_back(normal.y); data.push_back(normal.z);

            // Per i colori, se non ne hai di specifici per il LOD, usa il bianco o quelli originali
            data.push_back(1.0f); data.push_back(1.0f); data.push_back(1.0f);
        }
	}

	// Send data to OpenGL
    if(vao[lvl] == -1) glGenVertexArrays(1, &vao[lvl]);
    glBindVertexArray(vao[lvl]);
    if(vbo[lvl] == -1) glGenBuffers(1, &vbo[lvl]);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[lvl]);
	
	glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
	posLocation = Application::instance().getShader()->bindVertexAttribute("position", 3, 9*sizeof(float), 0);
	normalLocation = Application::instance().getShader()->bindVertexAttribute("normal", 3, 9*sizeof(float), (void *)(3*sizeof(float)));
	colorLocation = Application::instance().getShader()->bindVertexAttribute("colorAttr", 3, 9*sizeof(float), (void *)(6*sizeof(float)));
    numTrianglesLOD[lvl] = currentTris.size() / 3;

}

void TriangleMesh::render(int lodLevel) const
{
    // FALLBACK: Se ci chiedono un livello che non esiste (come per i muri), usiamo l'originale (0)
    if (lodLevel < 0 || lodLevel >= NUM_LODS || vao[lodLevel] == (GLuint)-1) {
        lodLevel = 0; 
    }

    // Se perfino lo zero è vuoto, c'è un problema, interrompiamo.
    if (vao[lodLevel] == (GLuint)-1) return;

    Application::instance().getShader()->use();
    glBindVertexArray(vao[lodLevel]); 
    
    glEnableVertexAttribArray(posLocation);
    glEnableVertexAttribArray(normalLocation);
    glEnableVertexAttribArray(colorLocation);
    
    glDrawArrays(GL_TRIANGLES, 0, numTrianglesLOD[lodLevel] * 3);
}

void TriangleMesh::free()
{
    for(int i = 0; i < NUM_LODS; i++) {
        if(vbo[i] != -1) glDeleteBuffers(1, &vbo[i]);
        if(vao[i] != -1) glDeleteVertexArrays(1, &vao[i]);
    }
    
    vertices.clear();
    colors.clear();
    triangles.clear();
}

void TriangleMesh::simplify(int resolution, SimplifyMode mode)
{
    // Sicurezza: se non c'è il backup, ci fermiamo 
    if (originalVertices.empty()) return;

    // 1. CALCOLO DEL BOUNDING BOX 
    glm::vec3 minBox = originalVertices[0];
    glm::vec3 maxBox = originalVertices[0];
    
    for (size_t i = 0; i < originalVertices.size(); i++) {
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

    // Creiamo la mappa GLOBALE della griglia
    std::map<GridIndex, CellInfo> grid; 

    // ==========================================
    // PASSAGGIO 0: Calcolo Normali dei Vertici (Solo se in modalità Normal Clustering)
    // ==========================================
    vector<glm::vec3> vertNormals(originalVertices.size(), glm::vec3(0.0f));
    if (mode == SimplifyMode::NORMAL_CLUSTERING) {
        for (size_t t = 0; t < originalTriangles.size(); t += 3) {
            glm::vec3 v0 = originalVertices[originalTriangles[t]];
            glm::vec3 v1 = originalVertices[originalTriangles[t+1]];
            glm::vec3 v2 = originalVertices[originalTriangles[t+2]];

            glm::vec3 crossP = glm::cross(v1 - v0, v2 - v0);
            // Non normalizziamo qui: i triangoli più grandi influenzeranno di più la normale
            vertNormals[originalTriangles[t]] += crossP;
            vertNormals[originalTriangles[t+1]] += crossP;
            vertNormals[originalTriangles[t+2]] += crossP;
        }
        // Normalizzazione finale
        for (size_t i = 0; i < vertNormals.size(); i++) {
            // Tolleranza bassissima per i modelli giganti come il Drago
            if (glm::length(vertNormals[i]) > 1e-12f) {
                vertNormals[i] = glm::normalize(vertNormals[i]);
            }
        }
    }

    // ==========================================
    // PARTE 1: Quadric construction & Normal Clustering
    // ==========================================
    for (size_t t = 0; t < originalTriangles.size(); t += 3) 
    {
        int vIdx[3] = { originalTriangles[t], originalTriangles[t + 1], originalTriangles[t + 2] };
        glm::vec3 verts[3] = { originalVertices[vIdx[0]], originalVertices[vIdx[1]], originalVertices[vIdx[2]] };

        // Calcoliamo la normale della faccia (ci serve per il piano della Quadrica)
        glm::vec3 crossP = glm::cross(verts[1] - verts[0], verts[2] - verts[0]);
        float area = glm::length(crossP);
        
        // LA FIX DEL DRAGO: 1e-12f per non scartare i micro-triangoli
        if (area < 1e-12f) continue; 
        glm::vec3 faceNormal = crossP / area; 

        for(int i = 0; i < 3; i++) {
            GridIndex idx;
            idx.i = floor((verts[i].x - minBox.x) / cellSize);
            idx.j = floor((verts[i].y - minBox.y) / cellSize);
            idx.k = floor((verts[i].z - minBox.z) / cellSize);

            // 1. Calcoliamo il centro esatto di QUESTA cella
            glm::vec3 cellCenter;
            cellCenter.x = minBox.x + (idx.i + 0.5f) * cellSize;
            cellCenter.y = minBox.y + (idx.j + 0.5f) * cellSize;
            cellCenter.z = minBox.z + (idx.k + 0.5f) * cellSize;

            // 2. Calcoliamo 'd' RELATIVO al centro della cella (usando verts[i] per precisione)
            float d_relativo = -glm::dot(faceNormal, verts[i] - cellCenter);

            Eigen::Vector4d plane(faceNormal.x, faceNormal.y, faceNormal.z, d_relativo);
            Eigen::Matrix4d Q_triangle = plane * plane.transpose();

            int subNodeIndex = 0;
            if (mode == SimplifyMode::NORMAL_CLUSTERING) {
                //Usiamo la normale del SINGOLO VERTICE per non strappare la mesh!
                glm::vec3 vNorm = vertNormals[vIdx[i]];
                subNodeIndex = (vNorm.x > 0.0f ? 1 : 0) | 
                               (vNorm.y > 0.0f ? 2 : 0) | 
                               (vNorm.z > 0.0f ? 4 : 0);
            }

            grid[idx].Q[subNodeIndex] += Q_triangle; 
            grid[idx].count[subNodeIndex] += 1;
        }
    }

    // ==========================================
    // PARTE 2: Creazione Nuovi Vertici (Multi-SVD)
    // ==========================================
    vector<glm::vec3> newVertices;
    for (auto it = grid.begin(); it != grid.end(); it++)
    {
        GridIndex idx = it->first;
        
        // Ricalcoliamo il centro della cella per riportare il vertice nel mondo reale
        glm::vec3 cellCenter;
        cellCenter.x = minBox.x + (idx.i + 0.5f) * cellSize;
        cellCenter.y = minBox.y + (idx.j + 0.5f) * cellSize;
        cellCenter.z = minBox.z + (idx.k + 0.5f) * cellSize;

        for(int sub = 0; sub < 8; sub++) 
        {
            if (it->second.count[sub] == 0) continue;

            Eigen::Matrix4d Q = it->second.Q[sub];
            Q(3, 0) = 0; Q(3, 1) = 0; Q(3, 2) = 0; Q(3, 3) = 1;
            Eigen::Vector4d b(0, 0, 0, 1);

            // SVD ora troverà il punto a norma minima (ovvero quello più vicino a 0,0,0 locale)
            Eigen::Vector4d pStar = Q.jacobiSvd(Eigen::ComputeFullU | Eigen::ComputeFullV).solve(b);
            
            // Sommiamo cellCenter per "traslare" il punto dalla cella al mondo intero
            newVertices.push_back(glm::vec3(pStar.x(), pStar.y(), pStar.z()) + cellCenter);
            it->second.newVertexId[sub] = (int)newVertices.size() - 1;
        }
    }

    // ==========================================
    // PARTE 3: RICOSTRUZIONE TRIANGOLI (Leggendo dal BACKUP)
    // ==========================================
    vector<int> newTriangles;
    
    for (size_t t = 0; t < originalTriangles.size(); t += 3)
    {
        int vIdx[3] = { originalTriangles[t], originalTriangles[t + 1], originalTriangles[t + 2] };
        glm::vec3 verts[3] = { originalVertices[vIdx[0]], originalVertices[vIdx[1]], originalVertices[vIdx[2]] };

        // 1. Calcoliamo i cestini (subNodes) in modo INDIPENDENTE usando le vertNormals!
        int subNode[3] = {0, 0, 0};
        if (mode == SimplifyMode::NORMAL_CLUSTERING) {
            for(int i = 0; i < 3; i++) {
                glm::vec3 vNorm = vertNormals[vIdx[i]];
                subNode[i] = (vNorm.x > 0.0f ? 1 : 0) | 
                             (vNorm.y > 0.0f ? 2 : 0) | 
                             (vNorm.z > 0.0f ? 4 : 0);
            }
        }

        // 2. Calcoliamo gli indici della griglia per i 3 vertici
        GridIndex idx[3];
        for(int i = 0; i < 3; i++) {
            idx[i].i = floor((verts[i].x - minBox.x) / cellSize);
            idx[i].j = floor((verts[i].y - minBox.y) / cellSize);
            idx[i].k = floor((verts[i].z - minBox.z) / cellSize);
        }

        // 3. Peschiamo i vertici dai cestini CORRETTI
        int newV1 = grid[idx[0]].newVertexId[subNode[0]];
        int newV2 = grid[idx[1]].newVertexId[subNode[1]];
        int newV3 = grid[idx[2]].newVertexId[subNode[2]];
    
        // 4. Assicuriamoci che nessuno dei 3 vertici sia "vuoto" (-1)
        if(newV1 != -1 && newV2 != -1 && newV3 != -1) 
        {
            if(newV1 != newV2 && newV2 != newV3 && newV1 != newV3) 
            {
                newTriangles.push_back(newV1);
                newTriangles.push_back(newV2);
                newTriangles.push_back(newV3);
            }
        }
    }

    // ==========================================
    // SALVATAGGIO FINALE
    // ==========================================
    vertices = newVertices;
    triangles = newTriangles;

    cout << "Risoluzione " << resolution << " -> Dopo: " << vertices.size() << " vertici, " << triangles.size() / 3 << " triangoli." << endl;
}

void TriangleMesh::computeAllLODs(SimplifyMode mode)
{
    // LOD 0: Originale
    cout << "Generando LOD 0 (Originale)..." << endl;
    sendToOpenGL(0, originalVertices, originalTriangles);

    int resolutions[4] = {0, 100, 50, 25};

    for(int lvl = 1; lvl < NUM_LODS; lvl++) {
        cout << "Generando LOD " << lvl << " (Res: " << resolutions[lvl] << ")..." << endl;

        // Reset ai dati originali prima di ogni semplificazione[cite: 9]
        //vertices = originalVertices;
        //triangles = originalTriangles;

        vertices.clear();
        triangles.clear();

        // Esegui la semplificazione (che aggiorna 'vertices' e 'triangles' della classe)
        simplify(resolutions[lvl], mode);

        // Ora passiamo i risultati (contenuti in 'vertices' e 'triangles') alla GPU
        sendToOpenGL(lvl, vertices, triangles);
    }
}

float TriangleMesh::getDiagonal() const
{
    if (originalVertices.empty()) return 0.0f;
    glm::vec3 minB = originalVertices[0], maxB = originalVertices[0];
    for( const auto& v : originalVertices) {
        minB = glm::min(minB,v);
        maxB = glm::max(maxB,v);
    }

    return glm::distance(minB, maxB); //la 'd' della formula

    
}