#include "framework.h"

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

//Konstansok
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
const int NODES = 50;															//
const float TELITETTSEG = 0.05f;	//Number of real edges						//
const int CIRCLE_RESOLUTION = 16;	//											//
const float RADIUS = 0.03f;			//Circle radius								//
const size_t EDGES = 61;			//(((NODES - 1)* NODES) / 2)* TELITETTSEG;	//
const float dist = 0.06f;			//Prefered distance							//	
const float SURLODAS = 0.1f;													//
const float DT = 0.0001f;
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

vec3 trf(vec2 inp, float nagyitas = 1.8f) {
	vec3 ret;
	ret.z = 1.0f + inp.x * inp.x + inp.y * inp.y;
	ret.x = inp.x / ret.z;
	ret.y = inp.y / ret.z;
	return ret * nagyitas;
}


float lorenz(vec3 a, vec3 b) {
	return a.x * b.x + a.y * b.y - a.z * b.z;
}
float d(const vec3& a, const vec3& b) {
	return acoshf(-lorenz(a, b)); //Ahol a pos egy vec3 típusú tagváltozó. (x,y,w)
}

float normTav(const vec3& a) {
	return sqrtf(a.x * a.x + a.y * a.y);
}

struct grafPont {
	vec3 hip;
	vec3 pos;
	vec3 eroIranya;
	vec3 v;
	grafPont():v(0.0f) {
		pos.x = ((float)(rand() % 2000) - 1000.0f) / 1000.0f;
		pos.y = ((float)(rand() % 2000) - 1000.0f) / 1000.0f;
		float w = 1.0f + pos.x * pos.x + pos.y * pos.y;
		pos.z = sqrtf(w);
		hip = trf(vec2(pos.x, pos.y));
		eroIranya = pos;
	}
	void repos() {
		hip = trf(vec2(pos.x, pos.y)); 
	}
	grafPont& operator=(const grafPont& rhs) {
		if (this == &rhs) return *this;
		this->hip = rhs.hip;
		this->pos = rhs.pos;
		this->eroIranya = rhs.eroIranya;
		this->v = rhs.v;
		return *this;
	}
};


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
				if (szMtx[min(n1, n2)][max(n1, n2)] == false) {
					szMtx[min(n1, n2)][max(n1, n2)] = true;
					--szukseges_el;
				}
			}
		}
	}
	grafPont& operator[](size_t idx) {
		if (idx >= NODES) throw "Tulindexeles";
		return nodes[idx];
	}
	grafPont* edgeAt(size_t idx) {
		if (idx >= EDGES) throw "Sok lesz az az el!";
		size_t cnt = 0;
		for (size_t x = 0; x < NODES - 1; ++x) {
			for (size_t y = x + 1; y < NODES; ++y) {
				if (szMtx[x][y] == true) ++cnt;
				if (cnt == idx) {
					grafPont* ret = new grafPont[2];
					ret[0] = nodes[x];
					ret[1] = nodes[y];
					return ret;
				}
			}
		}
		return nullptr;
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
			//Az alábbi forciklus forrása: https://vik.wiki/Sz%C3%A1m%C3%ADt%C3%B3g%C3%A9pes_grafika_h%C3%A1zi_feladat_tutorial
			for (size_t j = 0; j < CIRCLE_RESOLUTION; j++) {
				float angle = float(j) / float(CIRCLE_RESOLUTION) * 2.0f * float(M_PI);
				vec2 p(x + RADIUS * cosf(angle), y + RADIUS * sinf(angle));
				vec3 t = trf(p);
				vertices[(i * 32) + (j * 2)] = t.x;
				vertices[(i * 32) + (j * 2) + 1] = t.y;
			}
		}
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);  // AttribArray 0
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
		gpuProgram.create(vertexSource, fragmentSource, "outColor");	
	}
	void drawCircle() {
		int location = glGetUniformLocation(gpuProgram.getId(), "color");
		glUniform3f(location, 0.0f, 1.0f, 0.0f); // 3 floats
		float MVPtransf[4][4] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
		location = glGetUniformLocation(gpuProgram.getId(), "MVP");
		glUniformMatrix4fv(location, 1, GL_TRUE, &MVPtransf[0][0]);
		glBindVertexArray(nodeVao);  // Draw call
		for (size_t i = 0; i < NODES; ++i) {
			glDrawArrays(GL_TRIANGLE_FAN, i * CIRCLE_RESOLUTION /*startIdx*/, CIRCLE_RESOLUTION /*# Elements*/);
		}
	}
	void prepareEdges() {
		if (edgeVao == 0) glGenVertexArrays(1, &edgeVao);
		glBindVertexArray(edgeVao);
		if(edgeVbo==0) glGenBuffers(1, &edgeVbo);
		glBindBuffer(GL_ARRAY_BUFFER, edgeVbo);
		float vertices[EDGES * 4];
		for (size_t i = 0; i < EDGES; ++i) {
			grafPont* tmp = edgeAt(i);
			if (tmp == nullptr) continue;
			vertices[i * 4] = tmp[0].hip.x;
			vertices[i * 4 + 1] = tmp[0].hip.y;
			vertices[i * 4 + 2] = tmp[1].hip.x;
			vertices[i * 4 + 3] = tmp[1].hip.y;
			delete[] tmp;
		}
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);  // AttribArray 0
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
		gpuProgram.create(vertexSource, fragmentSource, "outColor");
	}
	void drawEdges() {
		int location = glGetUniformLocation(gpuProgram.getId(), "color");
		glUniform3f(location, 0.0f, 0.0f, 1.0f); // 3 floats
		float MVPtransf[4][4] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
		location = glGetUniformLocation(gpuProgram.getId(), "MVP");
		glUniformMatrix4fv(location, 1, GL_TRUE, &MVPtransf[0][0]);
		glBindVertexArray(edgeVao);  // Draw call
		glDrawArrays(GL_LINES, 0, EDGES * 2);///startIdx///# Elements/
	}
	void magic() {
		int legjobb = elmetszetek();
		grafPont* gp = nodes;
		for (int i = 0; i < 300; ++i) {
			nodes = new grafPont[NODES];
			if (elmetszetek() < legjobb) {
				delete[] gp;
				legjobb = elmetszetek();
				gp = nodes;
			}
			else delete[] nodes;
		}
		nodes = gp;
	}
	int elmetszetek() {
		int sum = 0;
		for (size_t i = 0; i < EDGES - 1; ++i) {
			for (size_t j = i + 1; j < EDGES; ++j) {
				grafPont* egyik = edgeAt(i);
				grafPont* masik = edgeAt(j);
				if (metszikEgymast(egyik, masik)) ++sum;
				delete[] egyik;
				delete[] masik;
			}
		}
		return sum;
	}
	/*
	* A kód alapja a http://flassari.is/2008/11/line-line-intersection-in-cplusplus/ oldalról származik
	* melyet késõbb kissé módosítottam.
	*/
	bool metszikEgymast(grafPont* a, grafPont* b) {
		if (a == nullptr || b == nullptr) return false;
		float x1 = a[0].pos.x, x2 = a[1].pos.x, x3 = b[0].pos.x, x4 = b[1].pos.x;
		float y1 = a[0].pos.y, y2 = a[1].pos.y, y3 = b[0].pos.y, y4 = b[1].pos.y;
		float d = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
		if (d == 0) return false;
		float pre = (x1 * y2 - y1 * x2), post = (x3 * y4 - y3 * x4);
		float x = (pre * (x3 - x4) - (x1 - x2) * post) / d;
		float y = (pre * (y3 - y4) - (y1 - y2) * post) / d;
		if (x < min(x1, x2) || x > max(x1, x2) || x < min(x3, x4) || x > max(x3, x4)) return false;
		if (y < min(y1, y2) || y > max(y1, y2) || y < min(y3, y4) || y > max(y3, y4)) return false;
		return true;
	}

	void calcNode(const size_t idx) {
		if (idx >= NODES) throw "tul lett indexelve";
		grafPont& p = nodes[idx];
		for (size_t i = 0; i < NODES; ++i) 
		{
			if (i != idx ) {
				grafPont& tmp = nodes[i];
				vec3 irany(tmp.pos.x - p.pos.x, tmp.pos.y - p.pos.y, 0.0f);
				if (szMtx[(idx > i) ? i : idx][(idx > i) ? idx : i]) {
					if (normTav(irany) < dist) {
						p.eroIranya = p.eroIranya + (irany * (1 / normTav(irany))) - p.v * SURLODAS; //Taszít
					}
					else {
						p.eroIranya = p.eroIranya - (irany * (1 / normTav(irany))) - p.v * SURLODAS; //Vonz
					}
				}
				else {
					p.eroIranya = p.eroIranya + (irany * (1 / normTav(irany))) - p.v * SURLODAS; //Taszít
				}
			}
			p.v = p.v + (p.eroIranya * DT);
			p.pos = p.pos + (p.v * DT);

		}
		p.repos();

	}

	~Graf() {
		delete[] nodes;
	}
};

Graf g;
bool dinSim = false;

void onInitialization() {
	glViewport(0, 0, windowWidth, windowHeight);
	g.prepareCircle();
	g.prepareCircle();
	g.prepareEdges();
	g.prepareCircle();
}

// Window has become invalid: Redraw
void onDisplay() {
	glClearColor(0, 0, 0, 0);     // background color
	glClear(GL_COLOR_BUFFER_BIT); // clear frame buffer
	g.drawEdges();
	g.drawCircle();
	glutSwapBuffers(); // exchange buffers for double buffering
}

// Key of ASCII code pressed
void onKeyboard(unsigned char key, int pX, int pY) {
	if (key == ' ') {
		g.magic(); //heurisztika
		g.prepareCircle();
		g.prepareEdges();
		glutPostRedisplay();
		dinSim =!dinSim;
	}
}

// Key of ASCII code released
void onKeyboardUp(unsigned char key, int pX, int pY) {
}

// Move mouse with key pressed
void onMouseMotion(int pX, int pY) {
}

// Mouse click event
void onMouse(int button, int state, int pX, int pY) {
}

// Idle event indicating that some time elapsed: do animation here
void onIdle() {
	long time = glutGet(GLUT_ELAPSED_TIME); // elapsed time since the start of the program
	if (dinSim) {
		for (size_t i = 0; i < NODES; ++i)
			g.calcNode(i);


		g.prepareCircle();
		g.prepareEdges();
		glutPostRedisplay();
	}
}