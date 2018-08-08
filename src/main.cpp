#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Include/Shader.hpp"
#include "Include/Camera.hpp"
#include "Mesh.hpp"

#include <iostream>

#include <boost/thread.hpp>

//MATLAB
#include "engine.h"
#pragma comment(lib, "libeng.lib")
#pragma comment(lib, "libmx.lib")
#pragma comment(lib, "libmat.lib")

using namespace std;

//constants
const double EPS = 1e-10;

//callbacks
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);

//camera
//-------
Camera camera(glm::vec3(0.0f, 0.0f, 1.5f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
glm::mat4 rotMat = glm::mat4(1.0f);

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

//MATLAB data
struct ENG_MX
{
	Engine *ep;//engine for MATLAB
	mxArray *mxData;

	void getMxData1D(const string name, double *data, int num)
	{
		mxData = engGetVariable(ep, name.c_str());
		memcpy(data, mxGetPr(mxData), num * sizeof(double));
		mxDestroyArray(mxData);
	}

	void putMxData1D(const string name, double *data, int num)
	{
		memcpy(mxGetPr(mxData), data, num * sizeof(double));
		engPutVariable(ep, name.c_str(), mxData);
		mxDestroyArray(mxData);
	}

	//memcpy fails for large data, so specialized functions are designed for 2D array
	void getMxData2D(const string name, double **data, int num)
	{
		mxData = engGetVariable(ep, name.c_str());
		double *mxP = mxGetPr(mxData);
		for (int i = 0; i < num; ++i)
		{
			memcpy(data[i], mxP, num * sizeof(double));
			mxP += num;
		}
		mxDestroyArray(mxData);
	}

	void putMxData2D(const string name, double **data, int num)
	{
		double *mxP = mxGetPr(mxData);
		for (int i = 0; i < num; ++i)
		{
			memcpy(mxP, data[i], num * sizeof(double));
			mxP += num;
		}
		engPutVariable(ep, name.c_str(), mxData);
		mxDestroyArray(mxData);
	}

	void getMxDataBool(const string name, bool *data)
	{
		mxData = engGetVariable(ep, name.c_str());
		memcpy(data, mxGetPr(mxData), sizeof(bool));
		mxDestroyArray(mxData);
	}

	void putMxData(const string name)
	{
		engPutVariable(ep, name.c_str(), mxData);
		mxDestroyArray(mxData);
	}

}engMx;

//C++ data for MATLAB
struct ENG_CPP
{
	double **H = nullptr;
	double *G = nullptr;
	//double **D = nullptr;
	double *Od = nullptr;//double type opacity (for MATLAB)
	float *O = nullptr;//double type opacity (for shader)

	bool flag = false;//to check if the opt solution is available

	double** new2D(int num, double val)
	{
		double **arr = new double*[num];

		for(int i = 0; i < num; ++i)
			arr[i] = new double[num];

		for (int i = 0; i < num; ++i)
			for (int j = 0; j < num; ++j) arr[i][j] = val;

		return arr;
	}

	void del2D(double **arr, int num)
	{
		for (int i = 0; i < num; ++i) delete[] arr[i];
		delete[] arr;
		arr = nullptr;
	}

	void clamp(int &x, int l, int r)
	{
		if (x < l) x = l;
		if (x > r) x = r;
	}

	void clamp(double &x, double l, double r)
	{
		if (x < l) x = l;
		if (x > r) x = r;
	}

	void adjustImportance(Mesh &mesh)
	{
		double *G2 = new double[mesh.segmentNum];
		memcpy(G2, G, sizeof(double) * mesh.segmentNum);
		sort(G2, G2 + mesh.segmentNum);

		int id = mesh.segmentNum * 0.9;
		clamp(id, (int)0, (int)mesh.segmentNum - 1);
		double upper = G2[id];

		if (upper > EPS)
		{
			for (int i = 0; i < (int)mesh.segmentNum; ++i)
			{
				G[i] /= upper;
				clamp(G[i], 0.0, 1.0);
			}
		}
		else if(upper < -EPS)
			cout << "Maximum importance <= 0" << endl;
		delete[] G2;
	}

	void importanceLength(Mesh &mesh)//line size;[0,0.95]
	{
		double maxLineSize = 0.0;
		for (int i = 0; i < (int)mesh.lines.size(); ++i)
			maxLineSize = max(maxLineSize, (double)mesh.lines[i].size());
		maxLineSize /= 0.95;
		for (int i = 0; i < (int)mesh.segmentNum; ++i)
			G[i] = (double)mesh.lines[i / mesh.segPerLine].size() / maxLineSize;

		adjustImportance(mesh);
	}

	void importanceCurvature(Mesh &mesh)
	{
		//ofstream out("lr.txt");
		for (int i = 0; i < (int)mesh.segmentNum; ++i)
		{
			int j = i / mesh.segPerLine;
			int k = i % mesh.segPerLine;

			IndexType &line = mesh.lines[j];

			//attention: line size is doubled
			int realSize = line.size() / 2;
			int l = round((float)k / mesh.segPerLine * realSize);
			int r = round((float)(k + 1) / mesh.segPerLine * realSize);
			clamp(l, 0, realSize - 1);
			clamp(r, 0, realSize - 1);

			G[i] = 0.0;
			for (int ii = l + 1; ii < r; ++ii)
			{
				glm::dvec3 p1 = mesh.vertices[line[2 * (ii - 1)]].Position;
				glm::dvec3 p2 = mesh.vertices[line[2 * ii]].Position;
				glm::dvec3 p3 = mesh.vertices[line[2 * (ii + 1)]].Position;

				//Triangle p1-p2-p3
				//Curvature: 4 times triangle area divided by the product of its three sides
/*				double curv = 2 * glm::length(glm::cross(p2 - p1, p2 - p3));
				double dd = sqrt(glm::length(p1 - p2) * glm::length(p1 - p3) * glm::length(p2 - p3));
				if(dd > EPS) curv /= dd;
				else cout << "same points" << endl;	*/			

				

				p1 = 1e100 * (p2 - p1);
				p3 = 1e100 * (p3 - p2);
				double angle;
				double dd = glm::length(p1) * glm::length(p3);
				angle = acos((double)glm::dot(p1, p3) / dd);

				if (!isnan(angle))
					G[i] += angle;
					//G[i] += curv;
			}
		}

		adjustImportance(mesh);
	}
}engCpp;

//multithread
void asyncEvalString();

//atomic counter
//--------------
//index == 0 -> list couter; index == 1 -> debug out
GLuint readAtomicCounter(int index);
void setAtomicCounter(int index, GLuint val);

//list data
GLuint fragmentNum = 0;
GLuint headPointers[SCR_HEIGHT][SCR_WIDTH];
glm::vec4 listBuffer[MAX_FRAGMENT_NUM];

//init functions
void initGlfw();
void glfwWindowCreate(GLFWwindow* window);
void openglConfig();

//update related
int updateTimes = 0;
bool updateFlag = false;
void updateOpacities(Mesh &mesh);
void resetGpuData(Mesh &mesh);
void readHeadAndList(Mesh &mesh);
void computeH(Mesh &mesh);

//parameters
enum ImportanceType { LENGTH, CURVATURE };
ImportanceType importMode = CURVATURE;
double scaleH = 60;
double coff[5] = { 1.0f, 2.0f, 0.2f, 0.3f, 5.0f };//p, q, r, s, lambda
float rotateHorizontal = 1.2f;
float rotateVertical = 0.0f;

Mesh mesh;

int main()
{
	initGlfw();

	// glfw window creation
	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Lines", NULL, NULL);
	glfwWindowCreate(window);

	// glad: load all OpenGL function pointers
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

	// configure global opengl state
	openglConfig();

	// build and compile shaders
	// -------------------------
	Shader buildShader("build.vs", "build.fs");
	Shader displayShader("resolve.vs", "resolve.fs");

	// load models
	// -----------
	mesh.Init("Data/flow_data/cyclone.obj");
	//Mesh mesh("Data/flow_data/test.obj");
	
	//load MATLAB engine
	//-----------------
	cout << "Loading MATLAB engine..." << endl;
	assert((engMx.ep = engOpen("\0")) != nullptr);
	cout << "Finished loading MATLAB engine." << endl;

	//transfer coffients / segment number / segPerLine
	engMx.mxData = mxCreateDoubleMatrix(1, 5, mxREAL);
	engMx.putMxData1D("coff", coff, 5);

	//transfer segment number
	engMx.mxData = mxCreateDoubleScalar((double)mesh.segmentNum);
	engMx.putMxData("segmentNum");

	engMx.mxData = mxCreateDoubleScalar((double)mesh.segPerLine);
	engMx.putMxData("segPerLine");	

	//initilize matrxies
	engCpp.H = engCpp.new2D(mesh.segmentNum, 0.0);
	engCpp.G = new double[mesh.segmentNum];
	engCpp.Od = new double[mesh.segmentNum];
	engCpp.O = new float[mesh.segmentNum];

	//initilize opacity
	for (int i = 0; i < mesh.segmentNum; ++i) 
		engCpp.Od[i] = 1.0, engCpp.O[i] = 1.0f;

	//compute matrix G with line length, and transfer it to MATLAB
	if (importMode == LENGTH)
		engCpp.importanceLength(mesh);
	else
		engCpp.importanceCurvature(mesh);
	engMx.mxData = mxCreateDoubleMatrix(1, mesh.segmentNum, mxREAL);
	engMx.putMxData1D("G_diag", engCpp.G, mesh.segmentNum);

	//init MATLAB script
	engEvalString(engMx.ep, "init");
	cout << "Finished init MATLAB data" << endl << endl;
	//cin.get();

	// render loop
	// -----------
	while (!glfwWindowShouldClose(window))
	//for(int ti = 0; ti < 2; ++ti)
	{
		// per-frame time logic
		// --------------------
		float currentFrame = (float)glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		processInput(window);

		// view/projection/model/rotate matrix
		glm::mat4 projection, view, model, modelViewProjectionMatrix, rotMat2;
		projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.001f, 5.0f);
		view = camera.GetViewMatrix();
		//model is vec4(1) here
		modelViewProjectionMatrix = projection * view * model;
		rotMat2 = glm::rotate(rotMat2, rotateHorizontal, glm::vec3(0.0f, 1.0f, 0.0f));
		rotMat2 = glm::rotate(rotMat2, rotateVertical, glm::vec3(1.0f, 0.0f, 0.0f));
		rotateHorizontal = rotateVertical = 0.0f;
		rotMat = rotMat2 * rotMat;

		if (updateFlag)
			glDisable(GL_MULTISAMPLE);

		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		resetGpuData(mesh);

		//1. build linked list
		//--------------------
		glDisable(GL_DEPTH_TEST);

		buildShader.use();
		buildShader.setVec3("viewDirection", camera.Front);
		buildShader.setMat4("modelViewProjectionMatrix", modelViewProjectionMatrix);
		buildShader.setFloat("stripWidth", 3.0f / SCR_WIDTH);//strip width
		buildShader.setMat4("transform", rotMat);
		buildShader.setMat4("model", model);
		buildShader.setVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));
		buildShader.setVec3("lightPos", glm::vec3(0.0f, 0.0f, 1.0f));
		buildShader.setVec3("lineColor", glm::vec3(1.0f, 0.63f, 0.0f));
		mesh.Draw();

		if (updateFlag)
		{
			updateOpacities(mesh);
			glfwPollEvents();
			glEnable(GL_MULTISAMPLE);
			continue;
		}

		//2. OIT render
		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		displayShader.use();

		displayShader.setVec3("viewDirection", camera.Front);
		displayShader.setMat4("modelViewProjectionMatrix", modelViewProjectionMatrix);
		displayShader.setFloat("stripWidth", 3.0f / SCR_WIDTH);//strip width
		displayShader.setMat4("transform", rotMat);
		mesh.Draw();

		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		// -------------------------------------------------------------------------------
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	//cin.get();

	// glfw: terminate, clearing all previously allocated GLFW resources.
	// ------------------------------------------------------------------
	glfwTerminate();

	engCpp.del2D(engCpp.H, mesh.segmentNum);
	delete[] engCpp.G;
	delete[] engCpp.Od;
	delete[] engCpp.O;
	//engClose(engMx.ep);

	return 0;
}

void initGlfw()
{
	// glfw: initialize and configure
	// ------------------------------
	glfwInit();
	//OpenGL version 4.6
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	//glfwWindowHint(GLFW_DECORATED, false);//remove title bar for debugging(otherwise the minimum width is too big)
	glfwWindowHint(GLFW_SAMPLES, 4);
}

void glfwWindowCreate(GLFWwindow* window)
{
	assert(window != nullptr);
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetScrollCallback(window, scroll_callback);
}

void openglConfig()
{
	// configure global opengl state
	// -----------------------------
	//glDisable(GL_MULTISAMPLE);
	glEnable(GL_MULTISAMPLE);
	//draw multiple instances using a single call
	glEnable(GL_PRIMITIVE_RESTART);
	glPrimitiveRestartIndex(RESTART_NUM);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
}

void asyncEvalString()
{
	Engine* ep;
	assert((ep = engOpen("\0")) != nullptr);
	engEvalString(ep, "solveOpacity");
	cout << "opt finished" << endl << endl;
}

GLuint readAtomicCounter(int index)
{
	GLuint *cnt;
	cnt = (GLuint*)glMapBuffer(GL_ATOMIC_COUNTER_BUFFER, GL_READ_ONLY);
	glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
	return cnt[index];
}

void setAtomicCounter(int index, GLuint val)
{
	GLuint *cnt;
	cnt = (GLuint*)glMapBuffer(GL_ATOMIC_COUNTER_BUFFER, GL_WRITE_ONLY);
	cnt[index] = val;
	glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
}

void resetGpuData(Mesh &mesh)
{
	// init work before building the linked list
	// -----------------------------------------
	//reset list counter
	setAtomicCounter(0, 0);

	//reset head pointer
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, mesh.PBO_SET_HEAD);
	glBindTexture(GL_TEXTURE_2D, mesh.TEX_HEADER);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCR_WIDTH, SCR_HEIGHT, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);
	glBindTexture(GL_TEXTURE_2D, 0);

	////reset visit
	//glBindBuffer(GL_PIXEL_UNPACK_BUFFER, mesh.PBO_SET_VISIT);
	//glBindTexture(GL_TEXTURE_2D, mesh.TEX_VISIT);
	//glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCR_WIDTH, SCR_HEIGHT, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);

	//Bind head-pointer image
	glBindImageTexture(0, mesh.TEX_HEADER, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
	//Bind linked-list buffer
	glBindImageTexture(1, mesh.TEX_LIST, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32UI);
	////Bind visit
	//glBindImageTexture(2, mesh.TEX_VISIT, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);

	glFlush();

	//cout << "Finished initializing GPU data" << endl;
}

void readHeadAndList(Mesh &mesh)
{
	fragmentNum = readAtomicCounter(0);
	cout << fragmentNum << " fragments." << endl;
	//read head pointers
	glBindTexture(GL_TEXTURE_2D, mesh.TEX_HEADER);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, mesh.PBO_READ_HEAD);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, (GLvoid*)0);
	GLuint *dataHead;//for clear head pointers
	dataHead = (GLuint *)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
	assert(dataHead != nullptr);
	memcpy(&headPointers[0][0], dataHead, sizeof(GLuint) * TOTAL_PIXELS);
	//ofstream out("head.txt");
	//for (int i = 0; i < SCR_HEIGHT; ++i)
	//{
	//	for (int j = 0; j < SCR_WIDTH; ++j)
	//		out << headPointers[i][j] << '\t';
	//	out << endl;
	//}
	//out.close();
	glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
	glBindTexture(GL_TEXTURE_2D, 0);

	//read list buffer
	glBindTexture(GL_TEXTURE_2D, mesh.TEX_LIST);
	glBindBuffer(GL_TEXTURE_BUFFER, mesh.SBO_LIST);
	GLfloat *dataList;
	dataList = (GLfloat *)glMapBuffer(GL_TEXTURE_BUFFER, GL_READ_ONLY);
	assert(dataList != nullptr);
	memcpy(listBuffer, dataList, fragmentNum * sizeof(glm::vec4));
	int tmpNumber;
	for (int i = 0; i < (int)fragmentNum; ++i)
	{
		memcpy(&tmpNumber, &(listBuffer[i].x), sizeof(int));
		listBuffer[i].x = (float)tmpNumber;
	}
	//ofstream out("listbuffer.txt");
	//for (int i = 0; i < (int)fragmentNum; ++i)
	//	out << i << '\t' << listBuffer[i].x << '\t' << listBuffer[i].y << '\t' << listBuffer[i].z << endl;
	//out.close();
	glUnmapBuffer(GL_TEXTURE_BUFFER);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void computeH(Mesh &mesh)
{
	for (int i = 0; i < (int)mesh.segmentNum; ++i)
		for (int j = 0; j < (int)mesh.segmentNum; ++j)
			engCpp.H[i][j] = 0.0f;

	for (int i = 0; i < (int)SCR_HEIGHT; ++i)
	{
		for (int j = 0; j < (int)SCR_WIDTH; ++j)
		{
			int segId1, segId2;
			double v;

			int curIndex = headPointers[i][j];
			while (curIndex > EPS)
			{
				// x,y,z,w: next pointer, depth, weight, color
				glm::vec4 &curNode = listBuffer[curIndex];
				int backIndex = curNode.x;
				while (backIndex > EPS)
				{
					glm::vec4 &backNode = listBuffer[backIndex];
					if (curNode.y > backNode.y)//curNode is in front of backNode
					{
						segId1 = round(curNode.z);
						segId2 = (int)backNode.z;
						v = backNode.z - segId2;
					}
					else
					{
						segId1 = round(backNode.z);
						segId2 = (int)curNode.z;
						v = curNode.z - segId2;
					}

					engCpp.H[segId1][segId2] += 1 - v;
					if (segId2 + 1 < mesh.segmentNum)
						engCpp.H[segId1][segId2 + 1] += v;

					backIndex = backNode.x;
				}
				curIndex = curNode.x;
			}
		}
	}

	double maxH = 0.0;
	for (int i = 0; i < (int)mesh.segmentNum; ++i)
		for (int j = 0; j < (int)mesh.segmentNum; ++j)
			maxH = max(maxH, engCpp.H[i][j]);

	if (maxH > EPS)
	{
		for (int i = 0; i < (int)mesh.segmentNum; ++i)
			for (int j = 0; j < (int)mesh.segmentNum; ++j) engCpp.H[i][j] /= maxH;
	}
	else
		cout << "maximum h <= 0" << endl;

	for (int i = 0; i < (int)mesh.segmentNum; ++i)
		for (int j = 0; j < (int)mesh.segmentNum; ++j)
		{
			engCpp.H[i][j] *= scaleH;
			if (engCpp.H[i][j] > 1.0)
				engCpp.H[i][j] = 1.0;
		}
}

void updateOpacities(Mesh &mesh)
{
	//engMx.mxData = mxCreateLogicalScalar(false);
	//engMx.getMxDataBool("flag", &engCpp.flag);
	//engMx.mxData = mxCreateLogicalScalar(false);
	//engMx.putMxData("flag");
	updateFlag = false;

	cout << "Update times: " << ++updateTimes << endl;

	readHeadAndList(mesh);

	computeH(mesh);
	engMx.mxData = mxCreateDoubleMatrix(mesh.segmentNum, mesh.segmentNum, mxREAL);
	engMx.putMxData2D("H", engCpp.H, mesh.segmentNum);
	cout << "Finished computed H" << endl;

	cout << "Start to opt..." << endl;
	engEvalString(engMx.ep, "solveOpacity");
	//boost::thread threadAsy(&asyncEvalString);
	cout << "Update finished" << endl;

	engMx.getMxData1D("O", engCpp.Od, mesh.segmentNum);
	for (int i = 0; i < (int)mesh.segmentNum; ++i)
		engCpp.O[i] = (float)engCpp.Od[i];
	glBindTexture(GL_TEXTURE_2D, mesh.TEX_OPACITY);
	glBindBuffer(GL_TEXTURE_BUFFER, mesh.SBO_OPACITY);
	void *dataOpacity;
	assert(dataOpacity != nullptr);
	dataOpacity = (void *)glMapBuffer(GL_TEXTURE_BUFFER, GL_WRITE_ONLY);
	memcpy(dataOpacity, engCpp.O, sizeof(float) * mesh.segmentNum);
	glUnmapBuffer(GL_TEXTURE_BUFFER);
	glBindTexture(GL_TEXTURE_2D, 0);
	glFlush();

	cout << "Opacities are submitted." << endl << endl;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera.ProcessKeyboard(FORWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera.ProcessKeyboard(BACKWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera.ProcessKeyboard(LEFT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		camera.ProcessKeyboard(RIGHT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS)
		updateFlag = true;
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	// make sure the viewport matches the new window dimensions; note that width and 
	// height will be significantly larger than specified on retina displays.
	glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	bool mousePressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
	if (mousePressed)
	{
		if (firstMouse)
		{
			lastX = (float)xpos;
			lastY = (float)ypos;
			firstMouse = false;
		}

		float xoffset = (float)xpos - lastX;
		float yoffset = lastY - (float)ypos; // reversed since y-coordinates go from bottom to top

		lastX = (float)xpos;
		lastY = (float)ypos;

		bool altPressed = (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS);
		if (altPressed)
		{
			rotateHorizontal += xoffset * 0.01f;
			rotateVertical -= yoffset * 0.01f;
			//camera.ProcessMouseMovement(-2 * xoffset / SCR_WIDTH, -2 * yoffset / SCR_HEIGHT, true);
		}
		else
		{
			camera.ProcessMouseMovement(-2 * xoffset / SCR_WIDTH, -2 * yoffset / SCR_HEIGHT, false);
		}
	}
	else
	{
		firstMouse = true;
	}
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	camera.ProcessMouseScroll((float)yoffset);
}