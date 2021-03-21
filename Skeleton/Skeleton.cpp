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
	precision highp float;	// normal floats, makes no difference on desktop computers
	
	uniform vec3 color;		// uniform variable, the color of the primitive
	out vec4 outColor;		// computed color of the current pixel

	void main() {
		outColor = vec4(color, 1);	// computed color is the color of the primitive
	}
)";

GPUProgram gpuProgram;
#define NODES 50
#define TELITETTSEG 0.05

const size_t EDGES = (((NODES - 1) * NODES) / 2)* TELITETTSEG;

struct grafPont {
	vec2 koord;
	vec3 pos; 
	unsigned int vao;
	grafPont(): vao(0){
		pos.x = ((float)(rand() % 2000) - 1000.0f) / 1000.0f;
		pos.y = ((float)(rand() % 2000) - 1000.0f) / 1000.0f;
		float w = 1.0f + pos.x*pos.x + pos.y*pos.y;
		pos.z = sqrtf(w);
		koord.x = pos.x / w;
		koord.y = pos.y / w;
	}
	void transform(vec2 v) {
		pos = v;
		float w = 1.0f + pos.x * pos.x + pos.y * pos.y;
		pos.z = sqrtf(w);
		koord.x = pos.x / w;
		koord.y = pos.y / w;
	}

	grafPont& operator=(const grafPont& rhs) {
		if (this == &rhs) return *this;
		this->koord= rhs.koord;
		this->vao = rhs.vao;
		return *this;
	}
};

float lorenz(vec3 a, vec3 b) {
	return a.x * b.x + a.y * b.y - a.z * b.z;
}

float d(const grafPont& a, const grafPont& b) {
	return acosh(-lorenz(a.pos, b.pos)); //Ahol a pos egy vec3 típusú tagváltozó. (x,y,w)
}

class Graf {
	grafPont* nodes;
	bool szMtx[NODES][NODES];
	unsigned int edgeVao;
	unsigned int nodeVao;
public:
	Graf() {
		nodes = new grafPont[NODES];
		int szukseges_el = EDGES;

		for (size_t x = 0; x < NODES -1 ; ++x) {
			for (size_t y = x + 1; y < NODES; ++y) {
				szMtx[x][y] = false;
			}
		}

		while (szukseges_el != 0) {
			int n1 = rand() % NODES;
			int n2 = rand() % NODES;
			if (n1 != n2) {
				if (szMtx[min(n1,n2)][max(n1,n2)] == false) {
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
		for (size_t x = 0; x < NODES -1; ++x) {
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
	void prepareNodes() {
		glGenVertexArrays(1, &nodeVao);
		glBindVertexArray(nodeVao);	
		unsigned int vbo;
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);		
		float vertices[(NODES * 2)];
		for (size_t i = 0; i < NODES ; ++i) {
			vertices[i*2] = nodes[i].koord.x;
			vertices[i*2 +1] = nodes[i].koord.y;
		}
		glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0,2, GL_FLOAT, GL_FALSE,0, NULL);
		gpuProgram.create(vertexSource, fragmentSource, "outColor");
	}
	void drawNodes() {
		int location = glGetUniformLocation(gpuProgram.getId(), "color");
		glUniform3f(location, 0.0f, 1.0f, 0.0f); // 3 floats
		float MVPtransf[4][4] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
		location = glGetUniformLocation(gpuProgram.getId(), "MVP");
		glUniformMatrix4fv(location, 1, GL_TRUE, &MVPtransf[0][0]);
		glBindVertexArray(nodeVao);  // Draw call
		glDrawArrays(GL_POINTS, 0 /*startIdx*/, NODES /*# Elements*/);

	}
	void prepareEdges() {
		glGenVertexArrays(1, &edgeVao);
		glBindVertexArray(edgeVao);
		unsigned int vbo;
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		float vertices[EDGES*4];
		for (size_t i = 0; i < EDGES; ++i) {
			grafPont* tmp = edgeAt(i);
			if (tmp == nullptr) continue;
			vertices[i * 4] = tmp[0].koord.x;
			vertices[i * 4 + 1] = tmp[0].koord.y;
			vertices[i * 4 + 2] = tmp[1].koord.x;
			vertices[i * 4 + 3] = tmp[1].koord.y;
			delete[] tmp;
		}
		glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);  // AttribArray 0
		glVertexAttribPointer(0,2, GL_FLOAT, GL_FALSE,0, NULL);
		gpuProgram.create(vertexSource, fragmentSource, "outColor");
	}
	void drawEdges() {
		int location = glGetUniformLocation(gpuProgram.getId(), "color");
		glUniform3f(location, 0.0f, 0.0f, 1.0f); // 3 floats
		float MVPtransf[4][4] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
		location = glGetUniformLocation(gpuProgram.getId(), "MVP");
		glUniformMatrix4fv(location, 1, GL_TRUE, &MVPtransf[0][0]);
		glBindVertexArray(edgeVao);  // Draw call
		glDrawArrays(GL_LINES, 0, EDGES*2);///startIdx///# Elements/
	}
	void magic() {
		for (size_t n1 = 0; n1 < NODES - 1; n1+=2) {
			float osszsuly = 0.0f;
			vec2 tkp;
			for (size_t n2 = n1+1; n2 < NODES-1; n2+=2) {
				++osszsuly;
				if (szMtx[min(n1, n2)][max(n1, n2)]) {
					tkp.x += nodes[n2].pos.x;
					tkp.y += nodes[n2].pos.y;
				}
				else {
					tkp.x -= nodes[n2].pos.x;
					tkp.y -= nodes[n2].pos.y;
				}
			}

			tkp.x /= osszsuly;
			tkp.y /= osszsuly;
			nodes[n1].transform(tkp);
		}
	}

	~Graf() {
		delete[] nodes;
	}
};

Graf g;


void onInitialization() {
	printf("tavolsag a [0] es [1] csucs kozott = %.3f", d(g[0], g[1]));
	glViewport(0, 0, windowWidth, windowHeight);
	g.magic(); //heurisztika
    g.prepareNodes();
	g.prepareEdges();
}

// Window has become invalid: Redraw
void onDisplay() {
	glClearColor(0, 0, 0, 0);     // background color
	glClear(GL_COLOR_BUFFER_BIT); // clear frame buffer
	g.drawEdges();
	g.drawNodes();
	glutSwapBuffers(); // exchange buffers for double buffering
}

// Key of ASCII code pressed
void onKeyboard(unsigned char key, int pX, int pY) {
	if (key == 'd') glutPostRedisplay();         // if d, invalidate display, i.e. redraw
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
}
