/*
 * =====================================================================
 *        Version:  1.0
 *        Created:  24.10.2012 11:39:21
 *         Author:  Miroslav Bendík
 *        Company:  LinuxOS.sk
 * =====================================================================
 */

#include <GL/glut.h>
#include <stdint.h>
#include <iostream>
#include <fstream>
#include <vector>

#define DEVICE_FILE "/home/mirec/serial"

#define COORD_X 0
#define COORD_Y 1
#define COORD_Z 2

using namespace std;

struct GLVector3D {
	GLfloat x;
	GLfloat y;
	GLfloat z;
};

struct Vector3DData {
	float data[3];
	uint32_t ecc;
};

struct VectorLengthData {
	uint16_t data[6];
	uint32_t ecc;
	uint16_t length() {
		return data[0];
	}
	bool isCamera() {
		if (data[0] == 0x0080 & data[1] == 0 && data[2] == 0 && data[3] == 0 && data[4] == 0 && data[5] == 0 && ecc == 0) {
			return true;
		}
		else {
			return false;
		}
	}
};

class Scene {
public:
	typedef vector<GLVector3D> VectorList;

	GLfloat *projectionMatrix()
	{
		return m_projectionMatrix;
	}

	VectorList::const_iterator begin() const
	{
		return m_lines.begin();
	}

	VectorList::const_iterator end() const
	{
		return m_lines.end();
	}

	void setProjectionMatrix(GLfloat *projectionMatrix)
	{
		for (size_t i = 0; i < 16; ++i) {
			m_projectionMatrix[i] = projectionMatrix[i];
		}
	}

	void setLines(vector<GLVector3D> lines)
	{
		m_lines = lines;
	}

private:
	GLfloat m_projectionMatrix[16];
	vector<GLVector3D> m_lines;
};

class StreamReader
{
public:
	StreamReader(): state(ErrorState), inputStream(DEVICE_FILE) {};

	Scene readFrame()
	{
		unsigned char byte;
		Vector3DData vector3d;
		VectorLengthData length;
		size_t streamPos= 0;
		GLfloat projectionMatrix[16];


		while (!inputStream.bad()) {
			if (state == CameraDataState || state == VectorDataState) {
				inputStream.read((char *)&vector3d, sizeof(vector3d));
			}
			else if (state == VectorLengthState) {
				inputStream.read((char *)&length, sizeof(length));
			}
			else {
				inputStream.read((char *)&byte, sizeof(byte));
			}
			if (inputStream.eof()) {
				state = ErrorState;
				break;
			}

			switch (state) {
				case ErrorState:
					streamPos= 0;
					if (byte == 0x80) {
						state = CameraHeaderState;
						streamPos= 1;
					}
					break;
				case CameraHeaderState:
					if (byte == 0x00) {
						streamPos++;
						if (streamPos== 16) {
							state = CameraDataState;
							streamPos= 0;
						}
					}
					else {
						streamPos= 0;
						state = ErrorState;
					}
					break;
				case CameraDataState:
					projectionMatrix[streamPos * 2] = vector3d.data[0];
					projectionMatrix[streamPos * 2 + 1] = vector3d.data[1];
					streamPos++;
					if (streamPos == 8) {
						state = VectorLengthState;
						streamPos = 0;
					}
					break;
				case VectorLengthState:
					// Namiesto vektorov nasleduje zase kamera
					if (length.isCamera()) {
						state = CameraDataState;
						streamPos= 0;
						Scene scene;
						scene.setLines(lines);
						scene.setProjectionMatrix(projectionMatrix);
						return scene;
					}
					else {
						lines.resize(length.length() * 2);
						if (length.length() == 0) {
							state = FinishedState;
						}
						else {
							state = VectorDataState;
						}
						streamPos = 0;
					}
					break;
				case VectorDataState:
					lines[streamPos].x = vector3d.data[0];
					lines[streamPos].y = vector3d.data[1];
					lines[streamPos].z = vector3d.data[2];
					streamPos++;
					if (streamPos == length.length() * 2) {
						state = FinishedState;
					}
					break;
				default:
					state = ErrorState;
					streamPos= 0;
			}

			if (state == FinishedState) {
				break;
			}
		}
		state = ErrorState;

		Scene scene;
		scene.setLines(lines);
		scene.setProjectionMatrix(projectionMatrix);
		return scene;
	}

private:
	enum State {
		ErrorState,
		CameraHeaderState,
		CameraDataState,
		VectorLengthState,
		VectorDataState,
		FinishedState
	};
	State state;
	ifstream inputStream;
	vector<GLVector3D> lines;
};

StreamReader reader;
Scene scene;

void onResize(int w, int h)
{
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 1920, 0, 1080, -1, 1);
	glScalef(1, -1, 1);
	glTranslatef(0, -1080, 0);
}

void onDisplay()
{
	Scene scene = reader.readFrame();
	GLfloat *matrix = scene.projectionMatrix();
	GLfloat vec[4];
	GLfloat trans[4];
	GLfloat transBegin[4];
	GLfloat transEnd[4];

	bool beginLine = true;

	glClear(GL_COLOR_BUFFER_BIT);
	glColor3f(1.0, 1.0, 1.0);
	glBegin(GL_LINES);
	for (Scene::VectorList::const_iterator it = scene.begin(); it != scene.end(); ++it) {
		vec[0] = it->x;
		vec[1] = it->y;
		vec[2] = it->z;
		vec[3] = 1;
		for (int col = 0; col < 4; ++col) {
			trans[col] = 0;
		}
		for (int row = 0; row < 4; ++row) {
			int offset = row << 2;
			for (int col = 0; col < 4; ++col) {
				trans[row] += matrix[offset + col] * vec[col];
			}
		}
		for (int row = 0; row < 4; ++row) {
			float transformed = 1.0/trans[3] * trans[row];
			if (beginLine) {
				transBegin[row] = transformed;
			}
			else {
				transEnd[row] = transformed;
			}
		}

		if (!beginLine) {
			if (transBegin[2] < 0 || transEnd[2] < 0) {
				// Začiatok je vždy ďalej na z-osi
				if (transBegin[2] > transEnd[2]) {
					for (int col = 0; col < 4; ++col) {
						trans[col] = transBegin[col];
						transBegin[col] = transEnd[col];
						transEnd[col] = trans[col];
					}
				}
				// Z za kamerou
				if (transEnd[COORD_Z] > 0) {
					float factor = 1.0 - (transBegin[COORD_Z] / (transBegin[COORD_Z] - transEnd[COORD_Z]));
					transEnd[COORD_X] = transBegin[COORD_X] + (transEnd[COORD_X] - transBegin[COORD_X]) * factor;
					transEnd[COORD_Y] = transBegin[COORD_Y] + (transEnd[COORD_Y] - transBegin[COORD_Y]) * factor;
					transEnd[COORD_Z] = 0;
				}

				glVertex2i(transBegin[COORD_X], transBegin[COORD_Y]);
				glVertex2i(transEnd[COORD_X], transEnd[COORD_Y]);
			}
		}
		beginLine = !beginLine;
	}
	glEnd();
	glFlush();
}

int main(int argc, char *argv[])
{
	scene = reader.readFrame();
	glutInitWindowSize(480, 270);
	glutInit(&argc, argv);
	glutCreateWindow("Render");
	glutDisplayFunc(onDisplay);
	glutIdleFunc(onDisplay);
	glutReshapeFunc(onResize);
	glutMainLoop();
}
