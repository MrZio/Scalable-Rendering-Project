// =============================================================================
//  PVS.cpp  -  Calcolo OFFLINE del Potentially Visible Set (from-cell)
//  Progetto SRGGE - Scalable Rendering
//
//  IDEA:
//   - La mappa (level.txt) e' una griglia 2D di celle. '1' = muro (occlusore).
//     Tutto il resto ('0','2','3','4','5') = cella navigabile / vuota.
//   - Per ogni cella NON-muro calcoliamo quali altre celle NON-muro sono
//     potenzialmente visibili: esiste ALMENO una linea di vista che non
//     attraversa un muro.
//   - Il test di occlusione usa un attraversamento di griglia "supercover"
//     (Amanatides & Woo + gestione degli spigoli): sappiamo TUTTE le celle che
//     un raggio tocca, inclusi i tocchi d'angolo. Se ne tocca una di muro,
//     quel raggio e' BLOCCATO.
//   - Visibilita' = OR su piu' raggi campionati tra le due celle (centro +
//     4 angoli rientrati). Bastando un solo raggio libero, il PVS resta
//     CONSERVATIVO (non cancella mai cio' che e' davvero visibile).
//   - La visibilita' e' simmetrica: se A vede B, allora B vede A.
//
//  OUTPUT (pvs.txt):
//     riga 1:          W H
//     righe seguenti:  per ogni cella, indice = z*W + x, in ordine:
//                      k  v1 v2 ... vk        (k = numero di celle visibili)
//     Le celle-muro hanno k = 0.
//
//  COMPILAZIONE (standalone):
//     g++ -O2 -o pvs PVS.cpp
//  ESECUZIONE:
//     ./pvs level.txt pvs.txt              (calcola e salva)
//     ./pvs level.txt pvs.txt 5 6          (+ stampa debug dalla cella x=5 z=6)
// =============================================================================

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

using namespace std;

// -----------------------------------------------------------------------------
//  Griglia
// -----------------------------------------------------------------------------
struct Grid
{
    int W = 0, H = 0;
    vector<string> rows; // rows[z][x]

    // Fuori dai bordi consideriamo "solido" (muro): i raggi non escono dalla mappa.
    bool isWall(int x, int z) const
    {
        if (x < 0 || x >= W || z < 0 || z >= H)
            return true;
        return rows[z][x] == '1';
    }
};

bool loadGrid(const string &path, Grid &g)
{
    ifstream fin(path);
    if (!fin.is_open())
        return false;

    string line;
    while (fin >> line) // legge una riga (token senza spazi) alla volta
    {
        g.rows.push_back(line);
        g.W = max(g.W, (int)line.size());
    }
    g.H = (int)g.rows.size();
    if (g.H == 0)
        return false;

    // Uniformiamo la larghezza: se una riga e' piu' corta, la riempiamo di muri.
    for (auto &r : g.rows)
        r.resize(g.W, '1');

    return true;
}

// -----------------------------------------------------------------------------
//  Test di occlusione: il segmento (x0,z0)->(x1,z1), in coordinate di griglia
//  (lato cella = 1), attraversa qualche cella di MURO?
//
//  Attraversamento "supercover" basato su Amanatides & Woo:
//   - avanziamo di cella in cella seguendo il raggio;
//   - sul bordo che incontriamo prima (tMax minore) facciamo lo step;
//   - quando i due bordi coincidono (raggio che passa per uno SPIGOLO),
//     controlliamo ANCHE le due celle ortogonali adiacenti -> supercover.
//
//  La cella di partenza non viene testata (e' non-muro per costruzione).
//  La cella di arrivo non blocca (e' non-muro): hit() su di essa torna false.
// -----------------------------------------------------------------------------
bool segmentBlocked(const Grid &g, double x0, double z0, double x1, double z1)
{
    int cx = (int)floor(x0), cz = (int)floor(z0);
    const int ex = (int)floor(x1), ez = (int)floor(z1);

    const double dx = x1 - x0;
    const double dz = z1 - z0;

    const int stepX = (dx > 0) - (dx < 0); // -1, 0 oppure +1
    const int stepZ = (dz > 0) - (dz < 0);

    const double INF = 1e30;
    const double tDeltaX = (dx != 0.0) ? fabs(1.0 / dx) : INF;
    const double tDeltaZ = (dz != 0.0) ? fabs(1.0 / dz) : INF;

    // Distanza (parametro t) fino al primo bordo di cella su ciascun asse.
    double tMaxX, tMaxZ;
    if (dx > 0)      tMaxX = (floor(x0) + 1.0 - x0) / dx;
    else if (dx < 0) tMaxX = (floor(x0) - x0) / dx; // numeratore <= 0, dx < 0 -> >= 0
    else             tMaxX = INF;
    if (dz > 0)      tMaxZ = (floor(z0) + 1.0 - z0) / dz;
    else if (dz < 0) tMaxZ = (floor(z0) - z0) / dz;
    else             tMaxZ = INF;

    // Guardia anti-loop in caso di errori di virgola mobile.
    const int maxSteps = (g.W + g.H) * 2 + 4;

    for (int it = 0; it < maxSteps; ++it)
    {
        if (cx == ex && cz == ez)
            return false; // arrivati a destinazione senza incontrare muri

        if (fabs(tMaxX - tMaxZ) < 1e-9)
        {
            // Passaggio esatto per uno SPIGOLO: comportamento supercover.
            // Controlliamo le due celle ortogonali che condividono lo spigolo.
            if (g.isWall(cx + stepX, cz)) return true;
            if (g.isWall(cx, cz + stepZ)) return true;
            cx += stepX; cz += stepZ;
            tMaxX += tDeltaX; tMaxZ += tDeltaZ;
        }
        else if (tMaxX < tMaxZ)
        {
            cx += stepX;
            tMaxX += tDeltaX;
        }
        else
        {
            cz += stepZ;
            tMaxZ += tDeltaZ;
        }

        if (cx == ex && cz == ez)
            return false; // entrati nella cella di arrivo: ok
        if (g.isWall(cx, cz))
            return true; // muro lungo il cammino -> bloccato
    }
    return false; // per sicurezza
}

// -----------------------------------------------------------------------------
//  Punti di campionamento per una cella: centro + 4 angoli rientrati.
//  Il centro fa scattare il supercover sui passaggi diagonali; gli angoli
//  rientrati trovano i varchi reali.
// -----------------------------------------------------------------------------
static const double INSET = 0.05;

void samplePoints(int cx, int cz, double px[5], double pz[5])
{
    px[0] = cx + 0.5;          pz[0] = cz + 0.5;          // centro
    px[1] = cx + INSET;        pz[1] = cz + INSET;        // angoli
    px[2] = cx + 1.0 - INSET;  pz[2] = cz + INSET;
    px[3] = cx + INSET;        pz[3] = cz + 1.0 - INSET;
    px[4] = cx + 1.0 - INSET;  pz[4] = cz + 1.0 - INSET;
}

// Cella (ax,az) e (bx,bz) reciprocamente visibili?
// Visibili se ALMENO un raggio tra i punti campionati e' libero.
bool cellsVisible(const Grid &g, int ax, int az, int bx, int bz)
{
    if (ax == bx && az == bz)
        return true;

    double apx[5], apz[5], bpx[5], bpz[5];
    samplePoints(ax, az, apx, apz);
    samplePoints(bx, bz, bpx, bpz);

    for (int i = 0; i < 5; ++i)
        for (int j = 0; j < 5; ++j)
            if (!segmentBlocked(g, apx[i], apz[i], bpx[j], bpz[j]))
                return true; // trovato un varco: basta cosi'

    return false;
}

// -----------------------------------------------------------------------------
//  Stampa di debug: mappa ASCII della visibilita' da una cella.
// -----------------------------------------------------------------------------
void printPVSDebug(const Grid &g, const vector<vector<int>> &pvs, int camX, int camZ)
{
    const int N = g.W * g.H;
    vector<char> vis(N, 0);
    const int cam = camZ * g.W + camX;
    for (int t : pvs[cam])
        vis[t] = 1;

    cout << "\nVisibilita' dalla cella (" << camX << "," << camZ << "):\n";
    cout << "  C = camera   V = visibile   . = NON visibile   # = muro\n\n";
    for (int z = 0; z < g.H; ++z)
    {
        cout << "  ";
        for (int x = 0; x < g.W; ++x)
        {
            if (x == camX && z == camZ)      cout << 'C';
            else if (g.isWall(x, z))         cout << '#';
            else if (vis[z * g.W + x])       cout << 'V';
            else                             cout << '.';
        }
        cout << "\n";
    }
    cout << "\n  Celle visibili da (" << camX << "," << camZ
         << "): " << pvs[cam].size() << "\n";
}

// -----------------------------------------------------------------------------
//  main
// -----------------------------------------------------------------------------
int main(int argc, char **argv)
{
    string inPath  = (argc > 1) ? argv[1] : "level.txt";
    string outPath = (argc > 2) ? argv[2] : "pvs.txt";

    Grid g;
    if (!loadGrid(inPath, g) && !loadGrid("../" + inPath, g))
    {
        cerr << "ERRORE: impossibile aprire " << inPath << endl;
        return 1;
    }
    cout << "Griglia caricata: " << g.W << " x " << g.H
         << " (" << g.W * g.H << " celle)\n";

    const int N = g.W * g.H;
    vector<vector<int>> pvs(N);
    auto idx = [&](int x, int z) { return z * g.W + x; };

    long long pairsTested = 0, pairsVisible = 0;

    // Ogni cella vede se stessa.
    for (int z = 0; z < g.H; ++z)
        for (int x = 0; x < g.W; ++x)
            if (!g.isWall(x, z))
                pvs[idx(x, z)].push_back(idx(x, z));

    // Calcoliamo solo le coppie con b > a e poi specchiamo (visibilita' simmetrica).
    for (int z0 = 0; z0 < g.H; ++z0)
    for (int x0 = 0; x0 < g.W; ++x0)
    {
        if (g.isWall(x0, z0)) continue;
        const int a = idx(x0, z0);

        for (int z1 = 0; z1 < g.H; ++z1)
        for (int x1 = 0; x1 < g.W; ++x1)
        {
            if (g.isWall(x1, z1)) continue;
            const int b = idx(x1, z1);
            if (b <= a) continue;

            pairsTested++;
            if (cellsVisible(g, x0, z0, x1, z1))
            {
                pvs[a].push_back(b);
                pvs[b].push_back(a);
                pairsVisible++;
            }
        }
    }

    // Ordiniamo gli indici per leggibilita' (opzionale).
    for (auto &row : pvs)
        sort(row.begin(), row.end());

    // --- Scrittura su file ---
    ofstream fout(outPath);
    if (!fout.is_open())
    {
        cerr << "ERRORE: impossibile scrivere " << outPath << endl;
        return 1;
    }
    fout << g.W << " " << g.H << "\n";
    for (int i = 0; i < N; ++i)
    {
        fout << pvs[i].size();
        for (int v : pvs[i])
            fout << " " << v;
        fout << "\n";
    }
    fout.close();

    // --- Statistiche ---
    int navigable = 0;
    long long totalVisible = 0;
    for (int z = 0; z < g.H; ++z)
        for (int x = 0; x < g.W; ++x)
            if (!g.isWall(x, z))
            {
                navigable++;
                totalVisible += (long long)pvs[idx(x, z)].size();
            }

    cout << "Coppie testate: " << pairsTested
         << "  |  visibili: " << pairsVisible << "\n";
    cout << "Celle navigabili: " << navigable << "\n";
    if (navigable > 0)
        cout << "Media celle visibili per cella: "
             << (double)totalVisible / navigable << "\n";
    cout << "PVS salvato in: " << outPath << "\n";

    // --- Debug opzionale ---
    if (argc > 4)
    {
        int dx = atoi(argv[3]);
        int dz = atoi(argv[4]);
        if (dx >= 0 && dx < g.W && dz >= 0 && dz < g.H && !g.isWall(dx, dz))
            printPVSDebug(g, pvs, dx, dz);
        else
            cout << "(cella di debug non valida o e' un muro)\n";
    }
    else
    {
        // Default: una cella centrale navigabile, giusto per vedere il risultato.
        int dx = g.W / 2, dz = g.H / 2;
        for (int r = 0; r < g.W + g.H && g.isWall(dx, dz); ++r)
        {
            dx = (dx + 1) % g.W;
            if (dx == 0) dz = (dz + 1) % g.H;
        }
        if (!g.isWall(dx, dz))
            printPVSDebug(g, pvs, dx, dz);
    }

    return 0;
}
