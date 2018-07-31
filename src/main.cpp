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
float rotateHorizontal = 1.57f;
float rotateVertical = 0.0f;
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
	double coff[5] = {1.0f, 10.0f, 0.5f, 0.3f, 5.0f};//p, q, r, s, lambda
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

	void importanceLength(Mesh &mesh)//line size
	{
		double maxLineSize = 0.0;
		for (int i = 0; i < (int)mesh.lines.size(); ++i)
			maxLineSize = max(maxLineSize, (double)mesh.lines[i].size());
		maxLineSize /= 0.95;
		for (int i = 0; i < (int)mesh.segmentNum; ++i)
			G[i] = (double)mesh.lines[i / mesh.segPerLine].size() / maxLineSize;
		for (int i = 0; i < (int)mesh.segmentNum; ++i)
			if (i < mesh.segmentNum / 2)
				G[i] = .8;
			else
				G[i] = 0.001;
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
GLuint headPointers[SCR_HEIGHT][SCR_WIDTH];
glm::vec4 listBuffer[MAX_FRAGMENT_NUM];

//init functions
void initGlfw();
void glfwWindowCreate(GLFWwindow* window);
void openglConfig();

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
	//Shader matrixShader("getMatrix.vs", "getMatrix.fs");
	Shader displayShader("display.vs", "display.fs");

	// load models
	// -----------
	//Mesh mesh("Data/flow_data/cyclone.obj");
	Mesh mesh("Data/flow_data/test.obj");
	
	//load MATLAB engine
	//-----------------
	cout << "Loading MATLAB engine..." << endl;
	assert((engMx.ep = engOpen("\0")) != nullptr);
	cout << "Finished loading MATLAB engine." << endl;

	//transfer coffients / segment number / segPerLine
	engMx.mxData = mxCreateDoubleMatrix(1, 5, mxREAL);
	engMx.putMxData1D("coff", engCpp.coff, 5);

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
		engCpp.Od[i] = 1.0, engCpp.O[i] = 0.0f;

	//compute matrix G with line length, and transfer it to MATLAB
	engCpp.importanceLength(mesh);
	engMx.mxData = mxCreateDoubleMatrix(1, mesh.segmentNum, mxREAL);
	engMx.putMxData1D("G_diag", engCpp.G, mesh.segmentNum);

	//init MATLAB script
	engEvalString(engMx.ep, "init");
	cout << "Finished init MATLAB data" << endl << endl;
	//cin.get();

	// render loop
	// -----------
	int updateTimes = 0;
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
		model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f)); // translate it down so it's at the center of the scene
		model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));	// it's a bit too big for our scene, so scale it down
		modelViewProjectionMatrix = projection * view * model;
		rotMat2 = glm::rotate(rotMat2, rotateHorizontal, glm::vec3(0.0f, 1.0f, 0.0f));
		rotMat2 = glm::rotate(rotMat2, rotateVertical, glm::vec3(1.0f, 0.0f, 0.0f));
		rotateHorizontal = rotateVertical = 0.0f;
		rotMat = rotMat2 * rotMat;

		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		engMx.mxData = mxCreateLogicalScalar(false);
		engMx.getMxDataBool("flag", &engCpp.flag);
		if (engCpp.flag)//compute new opacity
		{
			cout << "Update times: " << ++updateTimes << endl;
			engMx.mxData = mxCreateLogicalScalar(false);
			engMx.putMxData("flag");

			engMx.getMxData1D("O", engCpp.Od, mesh.segmentNum);
			for (int i = 0; i < (int)mesh.segmentNum; ++i)
				engCpp.O[i] = (float)engCpp.Od[i];

			cout << "Finished get O from MATLAB" << endl;			

			glBindTexture(GL_TEXTURE_2D, mesh.TEX_OPACITY);
			glBindBuffer(GL_TEXTURE_BUFFER, mesh.SBO_OPACITY);
			void *dataOpacity;
			assert(dataOpacity != nullptr);
			dataOpacity = (void *)glMapBuffer(GL_TEXTURE_BUFFER, GL_WRITE_ONLY);
			memcpy(dataOpacity, engCpp.O, sizeof(float) * mesh.segmentNum);
			glUnmapBuffer(GL_TEXTURE_BUFFER);
			glBindTexture(GL_TEXTURE_2D, 0);

			//glFlush();

			//cout << "Opacity has been transfered to GPU" << endl;

			// init work before building the linked list
			// -----------------------------------------
			//glDisable(GL_DEPTH_TEST);
			//glDisable(GL_CULL_FACE);

			//reset list counter
			setAtomicCounter(0, 0);

			//reset head pointer
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, mesh.PBO_SET_HEAD);
			glBindTexture(GL_TEXTURE_2D, mesh.TEX_HEADER);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCR_WIDTH, SCR_HEIGHT, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);
			glBindTexture(GL_TEXTURE_2D, 0);

			//reset visit
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, mesh.PBO_SET_VISIT);
			glBindTexture(GL_TEXTURE_2D, mesh.TEX_VISIT);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCR_WIDTH, SCR_HEIGHT, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);

			//Bind head-pointer image
			glBindImageTexture(0, mesh.TEX_HEADER, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
			//Bind linked-list buffer
			glBindImageTexture(1, mesh.TEX_LIST, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32UI);
			//Bind visit
			glBindImageTexture(2, mesh.TEX_VISIT, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);

			//glFlush();

			//cout << "Finished initializing head and list" << endl;

			GLuint fragmentNum = 0;

			//1. build linked list
			//--------------------
			buildShader.use();

			buildShader.setVec3("viewDirection", camera.Front);
			buildShader.setMat4("modelViewProjectionMatrix", modelViewProjectionMatrix);
			buildShader.setFloat("stripWidth", 2 * 10.0f / SCR_WIDTH);//strip width
			buildShader.setMat4("transform", rotMat);

			mesh.Draw();

			fragmentNum = readAtomicCounter(0);

			cout << "fragment number" << fragmentNum << endl;

			//cout << "done with build shader" << endl;

			////2. compute matrix H
			////-------------------
			//matrixShader.use();

			//matrixShader.setVec3("viewDirection", camera.Front);
			//matrixShader.setMat4("modelViewProjectionMatrix", modelViewProjectionMatrix);
			//matrixShader.setFloat("stripWidth", 2 * 5.0f / SCR_WIDTH);//strip width
			//matrixShader.setMat4("transform", rotMat);

			//matrixShader.setInt("segmentNum", mesh.segmentNum);

			//mesh.Draw();

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
			//	{
			//		out << headPointers[i][j] << '\t';
			//	}
			//	out << endl;
			//}
			glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
			glBindTexture(GL_TEXTURE_2D, 0);

			cout << "Finished reading head pointers" << endl;

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
			//{
			//	memcpy(&tmp, &(listBuffer[i].x), sizeof(int));
			//	out << i << '\t' << listBuffer[i].x << '\t' << listBuffer[i].y << '\t' << listBuffer[i].z << endl;
			//}
			//out.close();
			glUnmapBuffer(GL_TEXTURE_BUFFER);
			glBindTexture(GL_TEXTURE_2D, 0);

			//cout << "Finished reading list buffer" << endl;

			for (int i = 0; i < (int)mesh.segmentNum; ++i)
				for (int j = 0; j < (int)mesh.segmentNum; ++j)
					engCpp.H[i][j] = 0.0f;

			for (int i = 0; i < (int)SCR_HEIGHT; ++i)
			{
				for (int j = 0; j < (int)SCR_WIDTH; ++j)
				{
					int curIndex = headPointers[i][j];
					while (curIndex > EPS)
					{
						// x,y,z,w: next pointer, depth, weight, reserved
						glm::vec4 &curNode = listBuffer[curIndex];
						int segId = curNode.z;
						int backIndex = curNode.x;
						while (backIndex > EPS)
						{
							glm::vec4 &backNode = listBuffer[backIndex];
							int segId2 = backNode.z;
							if (backNode.y > curNode.y)
							{
								double v = backNode.z - segId2;
								engCpp.H[segId][segId2] += 1 - v;
								if(segId2 + 1 < mesh.segmentNum)
									engCpp.H[segId][segId2 + 1] += v;
							}
							else
							{
								double v = curNode.z - segId;
								engCpp.H[segId2][segId] += 1 - v;
								if (segId + 1 < mesh.segmentNum)
									engCpp.H[segId2][segId + 1] += v;
							}
							backIndex = backNode.x;
						}
						curIndex = curNode.x;
					}
				}
			}

			double maxH = 0.0;
			for (int i = 0; i < (int)mesh.segmentNum; ++i)
				for(int j = 0; j < (int)mesh.segmentNum; ++j)
					maxH = max(maxH, engCpp.H[i][j]);
			
			if (maxH > EPS)
			{
				for (int i = 0; i < (int)mesh.segmentNum; ++i)
					for (int j = 0; j < (int)mesh.segmentNum; ++j) engCpp.H[i][j] /= maxH;
			}
		
			engMx.mxData = mxCreateDoubleMatrix(mesh.segmentNum, mesh.segmentNum, mxREAL);
			engMx.putMxData2D("H", engCpp.H, mesh.segmentNum);

			//engEvalString(engMx.ep, "cd 'G:/MatlabWorkSpace/'");
			engEvalString(engMx.ep, "solveOpacity");

			//boost::thread asyncEvalString(&asyncEvalString);
			//boost::this_thread::sleep(boost::posix_time::seconds(1));

			cout << "Update finished" << endl << endl;
		}

		//3. final display
		//glEnable(GL_DEPTH_TEST);
		//glEnable(GL_CULL_FACE);
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glClear(GL_COLOR_BUFFER_BIT);
		displayShader.use();

		displayShader.setVec3("viewDirection", camera.Front);
		displayShader.setMat4("modelViewProjectionMatrix", modelViewProjectionMatrix);
		displayShader.setFloat("stripWidth", 2 * 3.0f / SCR_WIDTH);//strip width
		displayShader.setMat4("transform", rotMat);

		mesh.Draw();

		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		// -------------------------------------------------------------------------------
		glfwSwapBuffers(window);
		glfwPollEvents();
		//break;
	}

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
	//glfwWindowHint(GLFW_SAMPLES, 4);
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
	glDisable(GL_MULTISAMPLE);
	//glEnable(GL_MULTISAMPLE);
	//draw multiple instances using a single call
	glEnable(GL_PRIMITIVE_RESTART);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glPrimitiveRestartIndex(RESTART_NUM);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
}

void asyncEvalString()
{
	Engine* ep;
	if (!(ep = engOpen("\0")))
	{
		fprintf(stderr, "Can't start another MATLAB engine\n");
		cin.get();
	}
	engEvalString(ep, "cd 'G:/MatlabWorkSpace/'");
	engEvalString(ep, "solveOpacity");
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