#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Include/Shader.hpp"
#include "Include/Camera.hpp"
#include "Mesh.hpp"

#include <iostream>

//#include <boost/thread.hpp>

//MATLAB
//------
#include "engine.h"
#pragma comment(lib, "libeng.lib")
#pragma comment(lib, "libmx.lib")
#pragma comment(lib, "libmat.lib")

//MATLAB data
//-----------
struct ENG_MX
{
	Engine *ep;//engine for MATLAB
	Engine *ep2;//another engine for asynchronous call
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
		mxDestroyArray(mxData);
	}

	void getMxDataBool(const string name, bool *data)
	{
		mxData = engGetVariable(ep, name.c_str());
		memcpy(data, mxGetPr(mxData), sizeof(bool));
		mxDestroyArray(mxData);
	}

	void putMxDataBool(const string name)
	{
		engPutVariable(ep, name.c_str(), mxData);
		mxDestroyArray(mxData);
	}

}engMx;

//C++ data for MATLAB
//-----------
struct ENG_CPP
{
	double coff[5] = {1.0f, 1.2f, 0.12f, 0.3f, 1.0f};//p, q, r, s, lambda
	double **H = nullptr;
	double **G = nullptr;
	double **D = nullptr;
	double *Od = nullptr;//double type opacity (for MATLAB)
	float *O = nullptr;//double type opacity (for shader)

	bool flag = false;//to check if the opt solution is available

	double** new2D(int num)
	{
		double **arr = new double*[num];

		arr[0] = new double[num * num];
		for (int i = 1; i < num; ++i)
			arr[i] = arr[i - 1] + num;

		for (int i = 0; i < num; ++i)
			for (int j = 0; j < num; ++j) arr[i][j] = 0.0;

		return arr;
	}

	void del2D(double **arr, int num)
	{
		//for (int i = 0; i < num; ++i) delete[] arr[i];
		delete arr[0];
		delete[] arr;
		arr = nullptr;
	}
}engCpp;

//callbacks
//---------
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
//void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);

//camera
//-------
Camera camera(glm::vec3(0.0f, 0.0f, 1.5f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
glm::mat4 rotMat = glm::mat4(1.0f);
float rotateHorizontal = 0.0f;
float rotateVertical = 0.0f;
// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

//atomic counter
//--------------
//index == 0 -> list couter; index == 1 -> debug out
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

//list data
//---------
GLuint headPointers[SCR_HEIGHT][SCR_WIDTH];
glm::vec4 listBuffer[MAX_FRAGMENT_NUM];

//constants
//---------
const double EPS = 1e-10;

int main()
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

	// glfw window creation
	// --------------------
	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Lines", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetScrollCallback(window, scroll_callback);

	// glad: load all OpenGL function pointers
	// ---------------------------------------
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		cin.get();
		return -1;
	}

	// configure global opengl state
	// -----------------------------
	glDisable(GL_MULTISAMPLE);
	//glEnable(GL_MULTISAMPLE);
	//draw multiple instances using a single call
	glEnable(GL_PRIMITIVE_RESTART);
	glPrimitiveRestartIndex(RESTART_NUM);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	// build and compile shaders
	// -------------------------
	Shader buildShader("build.vs", "build.fs");
	//Shader matrixShader("getMatrix.vs", "getMatrix.fs");
	Shader displayShader("display.vs", "display.fs");

	// load models
	// -----------
	Mesh mesh("Data/flow_data/cyclone.obj");
	//Mesh mesh("Data/flow_data/test.obj");

	//GLint max_buffer_size;
	//glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &max_buffer_size);
	//cout << "The maximum texture buffer size is " << max_buffer_size << " bytes." << endl;

	//GLint max_texture_number;
	//glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &max_texture_number);
	//cout << "The maximum texture number is " << max_texture_number << endl;
	
	// open engine
	cout << "Loading MATLAB engine..." << endl;
	if (!(engMx.ep = engOpen("\0")))
	{
		fprintf(stderr, "Can't start MATLAB engine\n");
		cin.get();
		return EXIT_FAILURE;
	}
	else
	{
		cout << "Finished loading MATLAB engine." << endl << endl;
	}

	//flag to check if the opt solution is available
	engMx.mxData = mxCreateLogicalScalar(true);
	engMx.putMxDataBool("flag");

	//transform coffients
	engMx.mxData = mxCreateDoubleMatrix(1, 5, mxREAL);
	engMx.putMxData1D("coff", engCpp.coff, 5);

	//initilize matrxies
	engCpp.H = engCpp.new2D(mesh.segmentNum);
	engCpp.G = engCpp.new2D(mesh.segmentNum);
	engCpp.D = engCpp.new2D(mesh.segmentNum);
	//engCpp.H = new double[mesh.segmentNum * mesh.segmentNum];
	//engCpp.G = new double[mesh.segmentNum * mesh.segmentNum];
	//engCpp.D = new double[mesh.segmentNum * mesh.segmentNum];
	//memset(engCpp.H, 0, sizeof(double) * mesh.segmentNum * mesh.segmentNum);
	//memset(engCpp.G, 0, sizeof(double) * mesh.segmentNum * mesh.segmentNum);
	//memset(engCpp.D, 0, sizeof(double) * mesh.segmentNum * mesh.segmentNum);

	//initilize opacity
	engCpp.Od = new double[mesh.segmentNum];
	engCpp.O = new float[mesh.segmentNum];
	for (int i = 0; i < mesh.segmentNum; ++i)
	{
		engCpp.Od[i] = 1.0;
		engCpp.O[i] = 1.0f;
	}

	//initilize opacity in shader (all 1.0f)
	glBindTexture(GL_TEXTURE_2D, mesh.TEX_OPACITY);
	glBindBuffer(GL_TEXTURE_BUFFER, mesh.SBO_OPACITY);
	void *dataOpacity;
	dataOpacity = (void *)glMapBuffer(GL_TEXTURE_BUFFER, GL_WRITE_ONLY);
	if (dataOpacity == nullptr)
	{
		cout << "Init: null dataOpacity pointer" << endl;
		cin.get();
		return -1;
	}
	else
	{
		memcpy(dataOpacity, engCpp.O, sizeof(float) * mesh.segmentNum);
	}
	glUnmapBuffer(GL_TEXTURE_BUFFER);
	glBindTexture(GL_TEXTURE_2D, 0);
	glFlush();

	//transfer opacity to MATLAB
	engMx.mxData = mxCreateDoubleMatrix(1, mesh.segmentNum, mxREAL);
	engMx.putMxData1D("O", engCpp.Od, mesh.segmentNum);

	//compute matrix D and transfer it to MATLAB
	int curSeg = 0;
	for (int i = 0; i < (int)mesh.lines.size(); ++i)
	{
		++curSeg;
		for (int j = 1; j < mesh.segPerLine; ++j, ++curSeg)
		{
			engCpp.D[i][curSeg - 1] += 1.0;
			engCpp.D[i][curSeg] += -1.0;
		}
	}
	engMx.mxData = mxCreateDoubleMatrix(mesh.segmentNum, mesh.segmentNum, mxREAL);
	engMx.putMxData2D("D", engCpp.D, mesh.segmentNum);

	//compute matrix G with line length, and transfer it to MATLAB
	double maxLineSize = 0.0;
	for (int i = 0; i < (int)mesh.lines.size(); ++i)
		maxLineSize = max(maxLineSize, (double)mesh.lines[i].size());
	for (int i = 0; i < (int)mesh.segmentNum; ++i)
		engCpp.G[i][i] = (double)mesh.lines[i / 8].size() / maxLineSize;
	engMx.mxData = mxCreateDoubleMatrix(mesh.segmentNum, mesh.segmentNum, mxREAL);
	engMx.putMxData2D("G", engCpp.G, mesh.segmentNum);

	// render loop
	// -----------
	int updateTimes = 0;
	while (!glfwWindowShouldClose(window))
	//for(int ti = 0; ti < 5; ++ti)
	{
		// per-frame time logic
		// --------------------
		float currentFrame = (float)glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		// input
		// -----
		processInput(window);

		// view/projection/model matrix
		glm::mat4 projection, view, model, modelViewProjectionMatrix;
		projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.001f, 5.0f);
		view = camera.GetViewMatrix();
		model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f)); // translate it down so it's at the center of the scene
		model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));	// it's a bit too big for our scene, so scale it down
		modelViewProjectionMatrix = projection * view * model;
		//rotate
		glm::mat4 rotMat2;
		rotMat2 = glm::rotate(rotMat2, rotateHorizontal, glm::vec3(0.0f, 1.0f, 0.0f));
		rotMat2 = glm::rotate(rotMat2, rotateVertical, glm::vec3(1.0f, 0.0f, 0.0f));
		rotateHorizontal = rotateVertical = 0.0f;
		rotMat = rotMat2 * rotMat;

		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		//engMx.mxData = mxCreateLogicalScalar(false);
		engMx.getMxDataBool("flag", &engCpp.flag);
		if (engCpp.flag)//compute new opacity
		{
			cout << "Update times: " << ++updateTimes << endl;
			engMx.mxData = mxCreateLogicalScalar(false);
			engMx.putMxDataBool("flag");

			//engMx.mxData = mxCreateDoubleMatrix(1, mesh.segmentNum, mxREAL);
			engMx.getMxData1D("O", engCpp.Od, mesh.segmentNum);
			for (int i = 0; i < (int)mesh.segmentNum; ++i)
				engCpp.O[i] = (float)engCpp.Od[i];

			//cout << "Finished get O from MATLAB" << endl;

			void *dataOpacity;
			dataOpacity = (void *)glMapBuffer(GL_TEXTURE_BUFFER, GL_WRITE_ONLY);
			if (dataOpacity == nullptr)
			{
				cout << "null dataOpacity pointer" << endl;
				cin.get();
				return -1;
			}
			else
			{
				memcpy(dataOpacity, engCpp.O, sizeof(float) * mesh.segmentNum);
			}
			glUnmapBuffer(GL_TEXTURE_BUFFER);
			glBindTexture(GL_TEXTURE_2D, 0);

			glFlush();

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

			glFlush();

			//cout << "Finished initializing head and list" << endl;

			GLuint fragmentNum = 0;

			//1. build linked list
			//--------------------
			buildShader.use();

			buildShader.setVec3("viewDirection", camera.Front);
			buildShader.setMat4("modelViewProjectionMatrix", modelViewProjectionMatrix);
			buildShader.setFloat("stripWidth", 2 * 2.0f / SCR_WIDTH);//strip width
			buildShader.setMat4("transform", rotMat);

			mesh.Draw();

			fragmentNum = readAtomicCounter(0);

			//cout << "done with build shader" << endl;

			////2. compute matrix H
			////-------------------
			//matrixShader.use();

			//matrixShader.setVec3("viewDirection", camera.Front);
			//matrixShader.setMat4("modelViewProjectionMatrix", modelViewProjectionMatrix);
			//matrixShader.setFloat("stripWidth", 2 * 2.0f / SCR_WIDTH);//strip width
			//matrixShader.setMat4("transform", rotMat);

			//matrixShader.setInt("segmentNum", mesh.segmentNum);

			//mesh.Draw();

			//read head pointers
			glBindTexture(GL_TEXTURE_2D, mesh.TEX_HEADER);
			glBindBuffer(GL_PIXEL_PACK_BUFFER, mesh.PBO_READ_HEAD);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, (GLvoid*)0);
			GLuint *dataHead;//for clear head pointers
			dataHead = (GLuint *)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
			if (dataHead == nullptr)
			{
				cout << "null heads pointer" << endl;
				cin.get();
			}
			else
			{
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
			}
			glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
			glBindTexture(GL_TEXTURE_2D, 0);

			//cout << "Finished reading head pointers" << endl;

			//read list buffer
			glBindTexture(GL_TEXTURE_2D, mesh.TEX_LIST);
			glBindBuffer(GL_TEXTURE_BUFFER, mesh.SBO_LIST);
			GLfloat *dataList;
			dataList = (GLfloat *)glMapBuffer(GL_TEXTURE_BUFFER, GL_READ_ONLY);
			if (dataList == nullptr)
			{
				cout << "null list buffer" << endl;
				cin.get();
			}
			else
			{
				memcpy(listBuffer, dataList, fragmentNum * sizeof(glm::vec4));
				int tmp;
				for (int i = 0; i < (int)fragmentNum; ++i)
				{
					memcpy(&tmp, &(listBuffer[i].x), sizeof(int));
					listBuffer[i].x = (float)tmp;
				}
				//ofstream out("listbuffer.txt");
				//for (int i = 0; i < (int)fragmentNum; ++i)
				//{
				//	memcpy(&tmp, &(listBuffer[i].x), sizeof(int));
				//	listBuffer[i].x = (float)tmp;
				//	out << i << '\t' << listBuffer[i].x << '\t' << listBuffer[i].y << '\t' << listBuffer[i].z << endl;
				//}
			}
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
							if (curNode.y < backNode.y)
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

			cout << "Finished computing H phase 1" << endl;

			double maxH = -1.0;
			for (int i = 0; i < (int)mesh.segmentNum; ++i)
				for(int j = 0; j < (int)mesh.segmentNum; ++j)
					maxH = max(maxH, engCpp.H[i][j]);

			if (maxH < EPS)
			{
				cout << "maximum element in matrix H <= 0" << endl;
				cin.get();
				return -1;
			}

			for (int i = 0; i < (int)mesh.segmentNum; ++i)
				for (int j = 0; j < (int)mesh.segmentNum; ++j) engCpp.H[i][j] /= maxH;
					
			engMx.mxData = mxCreateDoubleMatrix(mesh.segmentNum, mesh.segmentNum, mxREAL);
			engMx.putMxData2D("H", engCpp.H, mesh.segmentNum);

			cout << "opt..." << endl;

			engEvalString(engMx.ep, "cd \'C:\\Users\\gg\\Documents\\Visual Studio 2015\\Projects\\Lines2015\\Lines2015\'");
			engEvalString(engMx.ep, "solveOpacity");

			cout << "opt finished" << endl;
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
		displayShader.setFloat("stripWidth", 2 * 2.0f / SCR_WIDTH);//strip width
		displayShader.setMat4("transform", rotMat);

		mesh.Draw();

		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		// -------------------------------------------------------------------------------
		glfwSwapBuffers(window);
		glfwPollEvents();
		//break;
	}
	//std::cin.get();
	// glfw: terminate, clearing all previously allocated GLFW resources.
	// ------------------------------------------------------------------
	glfwTerminate();

	delete[] engCpp.H;
	delete[] engCpp.G;
	delete[] engCpp.D;
	delete[] engCpp.Od;
	delete[] engCpp.O;

	engClose(engMx.ep);
	return 0;
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