#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Shader.h"
#include "Camera.h"
#include "Mesh.h"
#include <iostream>

//MATLAB header files
#include "engine.h"

#pragma comment(lib, "libeng.lib")
#pragma comment(lib, "libmx.lib")
#pragma comment(lib, "libmat.lib")


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


int main()
{
	// glfw: initialize and configure
	// ------------------------------
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

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

	// tell GLFW to capture our mouse
	//glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	//glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	// glad: load all OpenGL function pointers
	// ---------------------------------------
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}


	// configure global opengl state
	// -----------------------------
	glEnable(GL_DEPTH_TEST);
	// draw in wireframe
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	glEnable(GL_PRIMITIVE_RESTART);
	glPrimitiveRestartIndex(RESTART_NUM);

	//glEnable(GL_POLYGON_SMOOTH);

	//glEnable(GL_LINE_SMOOTH);
	//glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);//GL_ONE_MINUS_SRC_ALPHA
	//glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

	//glEnable(GL_POLYGON_SMOOTH);
	//glEnable(GL_BLEND);
	////glBlendFunc(GL_SRC_ALPHA_SATURATE, GL_ONE_MINUS_SRC_ALPHA);
	//glBlendFunc(GL_SRC_ALPHA_SATURATE, GL_ONE_MINUS_SRC_ALPHA);
	//glHint(GL_POLYGON_SMOOTH_HINT, GL_DONT_CARE);


	Engine *ep;
	ep = engOpen("\0");
	//// open engine
	//if (!(ep = engOpen("\0")))
	//{
	//	fprintf(stderr, "\nCan't start MATLAB engine\n");
	//	return EXIT_FAILURE;
	//}


	// build and compile shaders
	// -------------------------
	Shader buildShader("build.vs", "build.fs");
	Shader resolveShader("resolve.vs", "resolve.fs");

	// load models
	// -----------
	Mesh mesh("res/flow_data/cyclone.obj");
	//Mesh mesh("res/flow_data/test.obj");

	// render loop
	// -----------
	bool show = true;
	while (!glfwWindowShouldClose(window))
	for(int bb = 0; bb < 10; ++bb)
	{
		// per-frame time logic
		// --------------------
		float currentFrame = (float)glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		// input
		// -----
		processInput(window);

		// render
		// ------
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// enable build shader
		buildShader.use();

		//clear head pointer
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, mesh.HI);
		glBindTexture(GL_TEXTURE_2D, mesh.HT);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCR_WIDTH, SCR_HEIGHT, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);
		glBindTexture(GL_TEXTURE_2D, 0);

		// Bind head-pointer image for read-write
		glBindImageTexture(0, mesh.HT, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);

		// Bind linked-list buffer for write
		glBindImageTexture(1, mesh.LT, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32UI);

		setAtomicCounter(0);

		//enable resolve shader before setting uniforms
		resolveShader.use();

		// view/projection/model matrix
		glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.001f, 5.0f);
		glm::mat4 view = camera.GetViewMatrix();
		glm::mat4 model;
		model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f)); // translate it down so it's at the center of the scene
		model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));	// it's a bit too big for our scene, so scale it down
		glm::mat4 modelViewProjectionMatrix = projection * view * model;
		resolveShader.setVec3("viewDirection", camera.Front);
		resolveShader.setMat4("modelViewProjectionMatrix", modelViewProjectionMatrix);

		//strip width
		resolveShader.setFloat("stripWidth", 2 * 2.0f / SCR_WIDTH);

		//rotate
		glm::mat4 rotMat2;
		rotMat2 = glm::rotate(rotMat2, rotateHorizontal, glm::vec3(0.0f, 1.0f, 0.0f));
		rotMat2 = glm::rotate(rotMat2, rotateVertical, glm::vec3(1.0f, 0.0f, 0.0f));
		rotateHorizontal = rotateVertical = 0.0f;
		rotMat = rotMat2 * rotMat;
		resolveShader.setMat4("transform", rotMat);

		mesh.Draw();

		//cout << "fragments number: " << readAtomicCounter() << endl;

		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		// -------------------------------------------------------------------------------
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

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