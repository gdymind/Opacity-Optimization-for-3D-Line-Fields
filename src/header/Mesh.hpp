#ifndef MESH_H
#define MESH_H

#include <glad/glad.h> 

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Shader.h"

#include <string>
#include <cstdio>
#include <map>
#include <vector>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <set>

using namespace std;

const unsigned int RESTART_NUM = 0x5FFFFFu;//primitive restart
// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 800;
const unsigned int TOTAL_PIXELS = SCR_WIDTH * SCR_HEIGHT;

struct Vertex {
	glm::vec3 Position;
	glm::vec3 Direction;
	glm::vec2 TexCoords;
	float weight = 0;//blending weight
	Vertex(float x, float y, float z): Position(glm::vec3(x, y, z)) {}
};

typedef vector<unsigned int> IndexType;

class Mesh
{
public:
	/*  Mesh Data */
	vector<Vertex> vertices;
	vector<IndexType> lines;
	IndexType indices;

	//int fragmentNum = 40000000;
	int fragmentNum = 7000000;
	//int frgmentNum = 640000;

	GLuint VAO, VBO, EBO; //vertex array object, vertex buffer object, element buffer object
	GLuint ABO = 0; //atomic buffer object, the corresponding atomic counter is for the linked list
	GLuint HT;//head pointer texture
	GLuint HI;//head pointer initializer
	GLuint SBO;//fragment storage buffer object
	GLuint LT;//linked list texture

	/*  Functions   */
	// constructor, expects a filepath to a 3D model.
	Mesh(std::string const &path)
	{
		cout << "Reading the model..." << endl;
		loadModel(path);
		cout << "Finished reading the model file." << endl;
		setupMesh();
	}

	// draws the model, and thus all its meshes
	void Draw()
	{
		// draw mesh
		glBindVertexArray(VAO);
		//glDrawElements(GL_LINE_STRIP, indices.size(), GL_UNSIGNED_INT, 0);
		glDrawElements(GL_TRIANGLE_STRIP, indices.size(), GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}


private:
	/*  Functions   */
	void loadModel(std::string const &path)
	{
		//modify each points' coordinates to fit the window
		float maxCoord[3] = { -1e9f, -1e9f, -1e9f };// maximum original coordinate
		float minCoord[3] = { 1e9f, 1e9f, 1e9f };// minimum original coordinate
		float fillratio = 0.6f;//content fill ratio of the whole window

		//initialize
		ios::sync_with_stdio(false);
		vertices.clear();
		lines.clear();
		indices.clear();

		//read the model file
		string buf;
		ifstream fin(path);
		//ofstream out("out1.txt");
		while (getline(fin, buf))
		{
			stringstream ss(buf);//line string stream
			string type;//v vt g l
			getline(ss, type, ' ');

			if (type == "v")//a vertex
			{
				float x[3];
				ss >> x[0] >> x[1] >> x[2];
				for (int i = 0; i < 3; ++i)
				{
					maxCoord[i] = std::max(maxCoord[i], x[i]);
					minCoord[i] = std::min(minCoord[i], x[i]);
				}
				//split each vertex into 2 vertices (for segment width and halo effect)
				vertices.push_back(Vertex(x[0], x[1], x[2]));
				//vertices.back().TexCoords = glm::vec2(1.0f, 1.0f);
				vertices.push_back(Vertex(x[0], x[1], x[2]));
				//vertices.back().TexCoords = glm::vec2(1.0f, 0.0f);
			}
			else if (type == "l")// a line
			{
				//get the line vertices' indices
				IndexType line;
				unsigned int idx;
				while (ss >> idx)
				{
					--idx;
					line.push_back(idx * 2u);
					line.push_back(idx * 2u + 1u);
				}
				lines.push_back(line);

				indices.insert(indices.end(), line.begin(), line.end());
				indices.push_back(RESTART_NUM);
			}
		}

		float maxLength;
		maxLength = max(maxCoord[0] - minCoord[0], maxCoord[1] - minCoord[1]);
		maxLength = max(maxLength, maxCoord[2] - minCoord[2]);
		maxLength *= 0.5f;
		for (auto it = vertices.begin(); it != vertices.end(); ++it, ++it)
		{
			Vertex &v = *it;
			glm::vec3 &Position = v.Position;
			Position.x -= (maxCoord[0] + minCoord[0]) * 0.5f;
			Position.y -= (maxCoord[1] + minCoord[1]) * 0.5f;
			Position.z -= (maxCoord[2] + minCoord[2]) * 0.5f;
			Position /= maxLength;
			Position = Position * fillratio;
		}

		float curWeight = 0.5f;
		for (auto it = lines.begin(); it != lines.end(); ++it)
		{
			vector<unsigned int> &line = *it;
		
			//set vertex attributes: Direction, TexCoords
			for (int i = 0; i < (int)line.size(); i += 2)//set a pair vertices a time
			{
				Vertex &v = vertices[line[i]];
				bool noLeft = (i - 2 < 0);
				bool noRight = (i + 2 >= (int)line.size());
				glm::vec3 dir;
				if (noLeft && noRight)
				{
					cout << "no left and no right" << endl;
					dir = glm::vec3(0.0f);
				}
				else if (noLeft)
				{
					dir = vertices[line[i + 2]].Position - vertices[line[i]].Position;
				}
				else if (noRight)
				{
					dir = vertices[line[i]].Position - vertices[line[i - 2]].Position;
				}
				else
					dir = vertices[line[i + 2]].Position - vertices[line[i - 2]].Position;

				if (glm::length(dir) < 1e-6f)
					v.Direction = glm::vec3(0.0f);
				else
					v.Direction = glm::normalize(dir);

				Vertex &v2 = vertices[line[i + 1]];
				v2 = v;
				v.TexCoords = glm::vec2(1.0f, 1.0f);
				v2.TexCoords = glm::vec2(1.0f, 0.0f);
			}

			//set vertex attribute: weight
			/*
			weight example(one line -> three segments):
				line one:
					0.5		3.5//raw weight
					1		3//real weight
					0.5~1.0 map to 1
					1.0~3.0 unchanged
					3.0~3.5 map to 3
				line two:
					3.5		6.5
					4		6
			*/
			float diff = 8.0f / line.size();			float startWeight = curWeight + 0.5f;
			float endWeight = startWeight + 7.0f;
			if (line.size() < 8)//no segmentation for short lines
			{
				diff = 1.0f / line.size();
				endWeight = startWeight;
			}
			for (auto it2 = line.begin(); it2 != line.end(); ++it2)
			{
				float &w = vertices[*it2].weight;
				w = max(curWeight, startWeight);
				w = min(w, endWeight);
				curWeight += diff;
			}
		}
	}

	void setupMesh()
	{
		// create buffers/arrays
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);
		glGenBuffers(1, &ABO);
		glGenTextures(1, &HT);
		glGenBuffers(1, &HI);
		glGenBuffers(1, &SBO);
		glGenTextures(1, &LT);

		//bind VAO
		glBindVertexArray(VAO);

		//set VBO
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);
		// vertex Positions
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
		glEnableVertexAttribArray(0);
		// vertex Directions
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Direction));
		glEnableVertexAttribArray(1);
		// vertex texture coords
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));
		glEnableVertexAttribArray(2);
		// vertex weight
		glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, weight));
		glEnableVertexAttribArray(3);

		//set EBO
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

		//set ABO: atomic counter buffer object
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, ABO);
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint), NULL, GL_DYNAMIC_COPY);
		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, ABO);

		//set HT: head pointer texture
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, HT);
		//2D texture, level 0, 32-bit GLuint per texel, width, height, no border, single channel, GLuint, no data yet
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, SCR_WIDTH, SCR_HEIGHT, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);
		glBindTexture(GL_TEXTURE_2D, 0);

		glBindImageTexture(0, HT, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);

		//set HI: head pointer initializer
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, HI);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, TOTAL_PIXELS * sizeof(GLuint), NULL, GL_STATIC_DRAW);
		GLuint *data;//for clear head pointers
		data = (GLuint *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
		memset(data, 0xff, TOTAL_PIXELS * sizeof(GLuint));//0xff->list end
		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

		//set SBO: fragment storage buffer object
		glBindBuffer(GL_TEXTURE_BUFFER, SBO);
		glBufferData(GL_TEXTURE_BUFFER, fragmentNum * sizeof(glm::vec4), NULL, GL_DYNAMIC_COPY);
		glBindBuffer(GL_TEXTURE_BUFFER, 0);

		//set LT: linked list texture
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_BUFFER, LT);
		glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32UI, SBO);
		glBindTexture(GL_TEXTURE_BUFFER, 0);

		glBindImageTexture(1, LT, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32UI);

		//unbind VAO
		glBindVertexArray(0);
	}
};
#endif