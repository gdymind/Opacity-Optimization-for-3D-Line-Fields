#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Shader.hpp"
#include "Camera.hpp"
#include "Mesh.hpp"

#include <iostream>

//MATLAB related
#include "engine.h"
#pragma comment(lib, "libeng.lib")
#pragma comment(lib, "libmx.lib")
#pragma comment(lib, "libmat.lib")

//callbacks
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
//void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);

// camera
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

GLuint readAtomicCounter()
{
	GLuint *cnt;
	cnt = (GLuint*)glMapBuffer(GL_ATOMIC_COUNTER_BUFFER, GL_READ_ONLY);
	glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
	return cnt[0];
}

void setAtomicCounter(GLuint val)
{
	GLuint *cnt;
	cnt = (GLuint*)glMapBuffer(GL_ATOMIC_COUNTER_BUFFER, GL_WRITE_ONLY);
	cnt[0] = val;
	glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
}

const int MAX_SEGMENTS_NUM = 5000;
float H[MAX_SEGMENTS_NUM][MAX_SEGMENTS_NUM];

int main()
{
	// glfw: initialize and configure
	// ------------------------------
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
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
	glEnable(GL_DEPTH_TEST);
	//glEnable(GL_MULTISAMPLE);
	//draw multiple instances using a single call
	glEnable(GL_PRIMITIVE_RESTART);
	glPrimitiveRestartIndex(RESTART_NUM);

	//Engine *ep;//engine for MATLAB
	//// open engine
	//if (!(ep = engOpen("\0")))
	//{
	//	fprintf(stderr, "Can't start MATLAB engine\n");
	//	cin.get();
	//	return EXIT_FAILURE;
	//}

	// build and compile shaders
	// -------------------------
	Shader buildShader("build.vs", "build.fs");
	//Shader resolveShader("resolve.vs", "resolve.fs");
	//Shader shader("ori.vs", "ori.fs");

	// load models
	// -----------
	//Mesh mesh("res/flow_data/cyclone.obj");
	Mesh mesh("res/flow_data/test.obj");

	// render loop
	// -----------
	bool show = true;
	while (!glfwWindowShouldClose(window))
	{
		// per-frame time logic
		// --------------------
		float currentFrame = (float)glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		// input
		// -----
		processInput(window);

		// build linked list
		// ----------------

		//glDisable(GL_DEPTH_TEST);
		//glDisable(GL_CULL_FACE);
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

									 //reset atomic counter
		setAtomicCounter(0);

		//clear head pointer
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, mesh.PBO_CLR_HEADER);
		glBindTexture(GL_TEXTURE_2D, mesh.TEX_HEADER);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCR_WIDTH, SCR_HEIGHT, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);
		glBindTexture(GL_TEXTURE_2D, 0);
		// Bind head-pointer image for read-write
		glBindImageTexture(0, mesh.TEX_HEADER, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
		// Bind linked-list buffer for write
		//glBindImageTexture(1, mesh.LT, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32UI);

		buildShader.use();

		// view/projection/model matrix
		glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.001f, 5.0f);
		glm::mat4 view = camera.GetViewMatrix();
		glm::mat4 model;
		model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f)); // translate it down so it's at the center of the scene
		model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));	// it's a bit too big for our scene, so scale it down
		glm::mat4 modelViewProjectionMatrix = projection * view * model;
		buildShader.setVec3("viewDirection", camera.Front);
		buildShader.setMat4("modelViewProjectionMatrix", modelViewProjectionMatrix);
		//buildShader.setFloat("stripWidth", 2 * 2.0f / SCR_WIDTH);//strip width
		buildShader.setFloat("stripWidth", 2 * 10.0f / SCR_WIDTH);//strip width

		//rotate
		glm::mat4 rotMat2;
		rotMat2 = glm::rotate(rotMat2, rotateHorizontal, glm::vec3(0.0f, 1.0f, 0.0f));
		rotMat2 = glm::rotate(rotMat2, rotateVertical, glm::vec3(1.0f, 0.0f, 0.0f));
		rotateHorizontal = rotateVertical = 0.0f;
		rotMat = rotMat2 * rotMat;
		buildShader.setMat4("transform", rotMat);

		mesh.Draw();

		glBindTexture(GL_TEXTURE_2D, mesh.TEX_HEADER);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, mesh.PBO_CLR_HEADER);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, (GLvoid*)0);
		GLuint *heads;//for clear head pointers
		heads = (GLuint *)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
		if (heads == nullptr)
		{
			cout << "null heads pointer" << endl;
			//cin.get();
		}
		else
		{
			ofstream out("out.txt");
			for (int i = 0; i < SCR_HEIGHT; ++i)
			{
				for (int j = 0; j < SCR_WIDTH; ++j)
				{
					out << heads[i * SCR_HEIGHT + j] << '\t';
					//out << heads[0] << ' ' << endl;
				}
				out << endl;
			}
		}
		glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
		glBindTexture(GL_TEXTURE_2D, 0);

		cout << "list counter: " << readAtomicCounter() << endl;

		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		// -------------------------------------------------------------------------------
		glfwSwapBuffers(window);
		glfwPollEvents();
		break;
	}
	cin.get();
	// glfw: terminate, clearing all previously allocated GLFW resources.
	// ------------------------------------------------------------------
	glfwTerminate();
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