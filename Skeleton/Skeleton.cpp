#include "framework.h"

// NYILATKOZAT
// ---------------------------------------------------------------------------------------------
// Nev    : Turai Botond
// Neptun : SR9IVY
// ---------------------------------------------------------------------------------------------
// ezennel kijelentem, hogy a feladatot magam keszitettem, es ha barmilyen segitseget igenybe vettem vagy
// mas szellemi termeket felhasznaltam, akkor a forrast es az atvett reszt kommentekben egyertelmuen jeloltem.
// A forrasmegjeloles kotelme vonatkozik az eloadas foliakat es a targy oktatoi, illetve a
// grafhazi doktor tanacsait kiveve barmilyen csatornan (szoban, irasban, Interneten, stb.) erkezo minden egyeb
// informaciora (keplet, program, algoritmus, stb.). Kijelentem, hogy a forrasmegjelolessel atvett reszeket is ertem,
// azok helyessegere matematikai bizonyitast tudok adni. Tisztaban vagyok azzal, hogy az atvett reszek nem szamitanak
// a sajat kontribucioba, igy a feladat elfogadasarol a tobbi resz mennyisege es minosege alapjan szuletik dontes.
// Tudomasul veszem, hogy a forrasmegjeloles kotelmenek megsertese eseten a hazifeladatra adhato pontokat
// negativ elojellel szamoljak el es ezzel parhuzamosan eljaras is indul velem szemben.
//=============================================================================================

const char* const vertexSource = R"(
	#version 330				// Shader 3.3
	precision highp float;		// normal floats, makes no difference on desktop computers
	uniform mat4 MVP;			// uniform variable, the Model-View-Projection transformation matrix
	layout(location = 0) in vec2 vp;	// Varying input: vp = vertex position is expected in attrib array 0
	void main() {
		gl_Position = vec4(vp.x, vp.y, 0, 1) * MVP;		// transform vp from modeling space to normalized device space
	}
)";

const char* const fragmentSource = R"(
	#version 330			// Shader 3.3
	precision highp float;	// normal floats, makes no difference on desktop computer
	uniform vec3 color;		// uniform variable, the color of the primitive
	out vec4 outColor;		// computed color of the current pixel
	void main() {
		outColor = vec4(color, 1);	// computed color is the color of the primitive
	}
)";

GPUProgram gpuProgram;

const int NODES = 50;
const float TELITETTSEG = 0.05f;
const int CIRCLE_RESOLUTION = 16;
const float RADIUS = 0.03f;
const size_t EDGES = 61;
const float DIST = 0.4f;
const float SURLODAS = 0.01f;
const float DT = 0.000008f;
const float HIBAHATAR = 0.02f;
bool dinSim = false;

vec3 trf(vec2 inp, float nagyitas = 1.0f) {
	vec3 ret;
	ret.z = (1.0f + inp.x * inp.x + inp.y * inp.y);
	ret.x = inp.x / ret.z;
	ret.y = inp.y / ret.z;
	return ret * nagyitas;
}
vec3 hip(const vec2 inp) {
	vec3 ret;
	ret.x = inp.x;
	ret.y = inp.y;
	ret.z = sqrtf(1.0f + inp.x * inp.x + inp.y * inp.y);
	return ret;
}
void sikra(vec3& inp) {
	inp.x = inp.x / inp.z;
	inp.y = inp.y = inp.z;
}
vec3 Eukl(vec3 hip) {
	vec3 ret;
	ret.x = hip.x * hip.z;
	ret.y = hip.y * hip.z;
	return ret ;
}
float lorenz(const vec3& a, const vec3& b) {
	return (a.x * b.x) + (a.y * b.y) - (a.z * b.z);
}
float dd(const vec3& a, const vec3& b) {
	return acoshf(-lorenz(a, b));
}


struct grafPont {
	vec3 hip;
	vec3 pos;
	vec3 ujpos;
	vec3 ero;
	vec3 v;
	grafPont() {
		pos.x = ((float)(rand() % 2000) - 1000.0f) / 1000.0f;
		pos.y = ((float)(rand() % 2000) - 1000.0f) / 1000.0f;
		hip = trf(vec2(pos.x, pos.y));
	}
	void repos() {
		pos = ujpos;
		hip = trf(vec2(pos.x, pos.y)); 
	}
	grafPont& operator=(const grafPont& rhs) {
		if (this == &rhs) return *this;
		this->hip = rhs.hip;
		this->pos = rhs.pos;
		this->ero = rhs.ero;
		this->v = rhs.v;
		return *this;
	}
};
float normTav(const grafPont& p, const grafPont& q) {
	float a = abs(p.pos.x - q.pos.x);
	float b = abs(p.pos.y - q.pos.y);
	return sqrtf(a * a + b * b);
}

class Graf {
	grafPont* nodes;
	bool szMtx[NODES][NODES];
	unsigned int edgeVao;
	unsigned int nodeVao;
	unsigned int edgeVbo;
	unsigned int nodeVbo;
public:
	Graf() : edgeVao(0), edgeVbo(0), nodeVao(0), nodeVbo(0) {
		nodes = new grafPont[NODES];
		int szukseges_el = EDGES;
		for (size_t x = 0; x < NODES - 1; ++x) {
			for (size_t y = x + 1; y < NODES; ++y) {
				szMtx[x][y] = false;
			}
		}
		while (szukseges_el != 0) {
			int n1 = rand() % NODES;
			int n2 = rand() % NODES;
			if (n1 != n2) {
				
				if (szMtx[(n1 < n2) ? n1 : n2][(n1 < n2) ? n2 : n1] == false) {
					szMtx[(n1 < n2) ? n1 : n2][(n1 < n2) ? n2 : n1] = true;
					--szukseges_el;
				}
			}
		}
	}
	grafPont& operator[](size_t idx) {
		if (idx >= NODES) throw "Tulindexeles";
		return nodes[idx];
	}
	bool edgeAt(const size_t idx, size_t& a, size_t& b) {
		if (idx >= EDGES) throw "Sok lesz az az el!";
		size_t cnt = 0;
		for (size_t x = 0; x < NODES - 1; ++x) {
			for (size_t y = x + 1; y < NODES; ++y) {
				if (cnt == idx && szMtx[x][y] == true) {
					a = x;
					b = y;
					return true;
				}
				if (szMtx[x][y] == true) { ++cnt; }
			}
		}
		return false;
	}
	void prepareCircle() {
		if(nodeVao==0)
			glGenVertexArrays(1, &nodeVao);
		glBindVertexArray(nodeVao);
		if(nodeVbo==0)
			glGenBuffers(1, &nodeVbo);
		glBindBuffer(GL_ARRAY_BUFFER, nodeVbo);
		float vertices[NODES * CIRCLE_RESOLUTION * 2];
		for (size_t i = 0; i < NODES; ++i) {
			float x = nodes[i].pos.x, y = nodes[i].pos.y;
			//A forciklus forrasa: https://vik.wiki/Sz%C3%A1m%C3%ADt%C3%B3g%C3%A9pes_grafika_h%C3%A1zi_feladat_tutorial
			for (size_t j = 0; j < CIRCLE_RESOLUTION; j++) {
				float angle = float(j) / float(CIRCLE_RESOLUTION) * 2.0f * float(M_PI);
				vec2 p(x + RADIUS * cosf(angle), y + RADIUS * sinf(angle));
				vec3 t = trf(p);
				vertices[(i * 32) + (j * 2)] = t.x;
				vertices[(i * 32) + (j * 2) + 1] = t.y;
			}
		}
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
		glEnableVertexAttribArray(0);  // AttribArray 0
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	}
	void drawCircle() {
		int location = glGetUniformLocation(gpuProgram.getId(), "color");
		glUniform3f(location, 0.0f, 1.0f, 0.0f);
		float MVPtransf[4][4] = { 1.8f, 0, 0, 0, 0, 1.8f, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
		location = glGetUniformLocation(gpuProgram.getId(), "MVP");
		glUniformMatrix4fv(location, 1, GL_TRUE, &MVPtransf[0][0]);
		glBindVertexArray(nodeVao); 
		for (size_t i = 0; i < NODES; ++i) {
			glDrawArrays(GL_TRIANGLE_FAN, i * CIRCLE_RESOLUTION, CIRCLE_RESOLUTION);
		}
	}
	void prepareEdges() {
		if (edgeVao == 0) glGenVertexArrays(1, &edgeVao);
		glBindVertexArray(edgeVao);
		if(edgeVbo==0) glGenBuffers(1, &edgeVbo);
		glBindBuffer(GL_ARRAY_BUFFER, edgeVbo);
		float vertices[EDGES * 4];
		for (size_t i = 0; i < EDGES; ++i) {
			size_t idxA;
			size_t idxB;
			if (edgeAt(i, idxA, idxB)) {
				vertices[i * 4] = nodes[idxA].hip.x;
				vertices[i * 4 + 1] = nodes[idxA].hip.y;
				vertices[i * 4 + 2] = nodes[idxB].hip.x;
				vertices[i * 4 + 3] = nodes[idxB].hip.y;
			}
		}
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	}
	void drawEdges() {
		int location = glGetUniformLocation(gpuProgram.getId(), "color");
		glUniform3f(location, 0.0f, 0.0f, 1.0f);
		float MVPtransf[4][4] = { 1.8f, 0, 0, 0, 0, 1.8f, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
		location = glGetUniformLocation(gpuProgram.getId(), "MVP");
		glUniformMatrix4fv(location, 1, GL_TRUE, &MVPtransf[0][0]);
		glBindVertexArray(edgeVao);
		glDrawArrays(GL_LINES, 0, EDGES * 2);
	}
	void magic() {
		int legjobb = elmetszetek();
		grafPont* gp = nodes;
		int ig = legjobb *0.7;
		int cnt = 300;
		while(legjobb > ig && cnt >0) {
			--cnt;
			nodes = new grafPont[NODES];
			if (elmetszetek() < legjobb) {
				delete[] gp;
				legjobb = elmetszetek();
				gp = nodes;
				break;
			}
			else delete[] nodes;
		}
		nodes = gp;
	}
	int elmetszetek() {
		int sum = 0;
		for (size_t i = 0; i < EDGES - 1; ++i) {
			for (size_t j = i + 1; j < EDGES; ++j) {
				size_t idxA, idxB, idxC, idxD;
				if(edgeAt(i,idxA, idxB) && edgeAt(j, idxC, idxD)){
					if (metszikEgymast(nodes[idxA], nodes[idxB], nodes[idxC], nodes[idxD])) 
						++sum;
				}
			}
		}
		return sum;
	}
	/*
	* A kod alapja a http://flassari.is/2008/11/line-line-intersection-in-cplusplus/ oldalrol szarmazik
	* melyet kesobb kisse modositottam.
	*/
	bool metszikEgymast(const grafPont& pa, const grafPont& pb, const grafPont& pc, const grafPont& pd ) {
		float x1 = pa.pos.x, x2 = pb.pos.x, x3 = pc.pos.x, x4 = pd.pos.x;
		float y1 = pa.pos.y, y2 = pb.pos.y, y3 = pc.pos.y, y4 = pd.pos.y;
		float d = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
		if (d == 0) return false;
		float pre = (x1 * y2 - y1 * x2), post = (x3 * y4 - y3 * x4);
		float x = (pre * (x3 - x4) - (x1 - x2) * post) / d;
		float y = (pre * (y3 - y4) - (y1 - y2) * post) / d;
		if (x < ((x1<x2)?x1:x2) || x > ((x1>x2)?x1:x2) || x < ((x3<x4)?x3:x4) || x > ((x3>x4)?x3:x4)) return false;
		if (y < ((y1<y2)?y1:y2) || y > ((y1>y2)?y1:y2) || y < ((y3<y4)?y3:y4) || y > ((y3>y4)?y3:y4)) return false;
		return true;
	}
	float calcNode(const size_t idx) {
		if (idx >= NODES) throw "tul lett indexelve";
		grafPont& p = nodes[idx];
		p.ujpos = p.pos;
		p.ero = p.ero * 0;
		p.v = p.v * 0;
		for (size_t i = 0; i < NODES; ++i) {
			if (i == idx) continue;
			grafPont& q = nodes[i];
			F(p, q, szMtx[(idx > i) ? i : idx][(idx > i) ? idx : i]);
		}
		grafPont kozepe;
		kozepe.pos.x = 0;
		kozepe.pos.y = 0;
		for (int i = 0; i < 5; ++i) {
			vec3 kul(kozepe.pos - p.pos);
			kul = kul * 0.001f;
			p.ero = p.ero + kul;
		}

		p.ero = p.ero - (p.v * SURLODAS);
		p.v = p.v + p.ero / DT;
		p.ujpos = p.ujpos + (p.v * DT);
		return sqrtf(p.v.x * p.v.x + p.v.y * p.v.y);
	}
	void F(grafPont& a, grafPont& b, const bool szomszedos) {
		const float csillapitas = 0.0001f;
		if (szomszedos) {
			if (normTav(a, b) < (DIST - HIBAHATAR)) {
				vec3 kul(a.pos - b.pos);
				kul = kul * csillapitas;
				a.ero = a.ero + kul;
			}
			else if (normTav(a, b) > (DIST + HIBAHATAR)) {
				vec3 kul(b.pos - a.pos);
				kul = kul * csillapitas;
				a.ero = a.ero + kul;
			}
		}
		else {
			vec3 kul(a.pos - b.pos);
			kul = kul * csillapitas * (1 / (normTav(a, b)));
			a.ero = a.ero + kul;
		}
	}
	~Graf() {
		delete[] nodes;
	}
};

Graf g;

struct Mozgas {
	vec2 kezdopont;
	bool kezd;
	Mozgas():kezd(false) {}
	void onPress(int px, int py) {
		kezd = false;
	}
	void onMove(int px, int py) {
		if (!kezd) {
			kezd = true;
			kezdopont.x = (float(px) - 300.0f) / 300.0f;
			kezdopont.y = (float(py) - 300.0f) / 300.0f;
			return;
		}
		kezdopont.x = 0.0f;
		kezdopont.y = 0.0f;
		vec3 p = hip(kezdopont);
		vec2 vegpont = vec2(float(px), float(py));
		vegpont.x = 0.004f;// (vegpont.x - 300.0f) / 300.0f;
		vegpont.y = 0.000f;// (vegpont.y - 300.0f) / 300.0f;
		vec3 q = hip(vegpont);

		const float hipTav = dd(p, q);
		if (hipTav > 0.001f) {
			vec3 v = (q - (p * coshf(hipTav))) / (sinhf(hipTav));
			float tized = hipTav / 10.0f;
			vec3 m1 = (p * coshf(tized)) + (v * sinhf(tized));
			float hatod = (hipTav*6.0f) / 10.0f;
			vec3 m2 = (p * coshf(hatod)) + (v * sinhf(hatod));

			printf("PQ: %.5f \t\t  M1M2: %.5f\n", dd(p, q), dd(m1, m2));

			for (size_t i = 0; i < NODES; ++i) {
				tukrozes(g[i], m1);
				tukrozes(g[i], m2);
				g[i].pos = Eukl(g[i].hip);
			}
			kezdopont = vegpont;
			g.prepareCircle();
			glutPostRedisplay();
		}
	}
	void tukrozes(grafPont& gp, const vec3& m1) {
		vec3 p = hip(vec2(gp.pos.x, gp.pos.y));
		float pm = dd(p, m1);
		vec3 v = (m1 - p * coshf(pm)) / sinh(pm);
		vec3 pvesszo = p * coshf(2.0f * pm) + v * sinhf(2.0f * pm);
		sikra(pvesszo);
		gp.hip = pvesszo;
	}
		
	void printVec3(const vec3& out, const char* name ="")const {
		printf(">%s\t(%.4f ; %.4f ; %.4f)\n",name, out.x, out.y, out.z);
	}

};
Mozgas mo;

void onInitialization() {
	glViewport(0, 0, windowWidth, windowHeight);
	g.prepareCircle();
	g.prepareEdges();
	gpuProgram.create(vertexSource, fragmentSource, "outColor");
}
void onDisplay() {
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	g.drawEdges();
	g.drawCircle();
	glutSwapBuffers();
}
void onKeyboard(unsigned char key, int pX, int pY) {
	if (key == ' ') {
		g.magic();
		g.prepareCircle();
		g.prepareEdges();
		glutPostRedisplay();
		dinSim =!dinSim;
	}
}
void onKeyboardUp(unsigned char key, int pX, int pY) {
}
void onMouseMotion(int pX, int pY) {
	mo.onMove(pX, pY );
}
void onMouse(int button, int state, int pX, int pY) {
	if (state == 1 && button == 0) {
		mo.onPress(pX, pY);
	}
	else if (state == 0 && button == 0) {
		mo.onMove(pX, pY);
	}
}
void onIdle() {
	if (dinSim) {
		float sum = 0.0f;
		for (size_t i = 0; i < NODES; ++i)
			sum+=g.calcNode(i);
		for (size_t i = 0; i < NODES; ++i)
			g[i].repos();
			
		g.prepareCircle();
		g.prepareEdges();
		glutPostRedisplay();
	}
}