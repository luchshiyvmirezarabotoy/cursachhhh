#include "Render.h"

#include "Camera.h"
#include "MyOGL.h"

#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace
{
	Camera camera;

	struct Point
	{
		double x;
		double y;
		double z;

		const double* p() const
		{
			return &x;
		}
	};

	GLuint topTextureId = 0;

	const std::vector<std::array<double, 3>> sideColors = {
		{ 0.86, 0.25, 0.21 },
		{ 0.22, 0.58, 0.90 },
		{ 0.18, 0.67, 0.41 },
		{ 0.95, 0.70, 0.18 },
		{ 0.62, 0.36, 0.71 },
		{ 0.18, 0.73, 0.73 },
		{ 0.82, 0.43, 0.15 },
		{ 0.55, 0.49, 0.18 }
	};

	const double prismHeight = 3.5;

	const std::vector<Point> bottom = {
		{ 0, 8, 0 },
		{ 1, 1, 0 },
		{ 7, 0, 0 },
		{ 3, -5, 0 },
		{ 0, -2, 0.0 },
		{ -6, -7, 0.0 },
		{ -9, -2, 0.0 },
		{ -1, -0, 0.0 }
	};

	std::vector<Point> build_top()
	{
		std::vector<Point> top = bottom;
		for (Point& p : top)
		{
			p.z = prismHeight;
		}
		return top;
	}

	double polygon_area_xy(const std::vector<Point>& polygon)
	{
		double area = 0.0;
		for (std::size_t i = 0; i < polygon.size(); ++i)
		{
			const Point& a = polygon[i];
			const Point& b = polygon[(i + 1) % polygon.size()];
			area += a.x * b.y - b.x * a.y;
		}
		return area * 0.5;
	}

	double cross_xy(const Point& a, const Point& b, const Point& c)
	{
		return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
	}

	bool point_in_triangle_xy(const Point& p, const Point& a, const Point& b, const Point& c)
	{
		const double ab = cross_xy(a, b, p);
		const double bc = cross_xy(b, c, p);
		const double ca = cross_xy(c, a, p);

		const bool hasNegative = (ab < 0.0) || (bc < 0.0) || (ca < 0.0);
		const bool hasPositive = (ab > 0.0) || (bc > 0.0) || (ca > 0.0);

		return !(hasNegative && hasPositive);
	}

	std::vector<std::array<int, 3>> triangulate_polygon(const std::vector<Point>& polygon)
	{
		std::vector<std::array<int, 3>> triangles;
		std::vector<int> indices;
		indices.reserve(static_cast<int>(polygon.size()));

		for (int i = 0; i < static_cast<int>(polygon.size()); ++i)
		{
			indices.push_back(i);
		}

		if (polygon_area_xy(polygon) < 0.0)
		{
			std::reverse(indices.begin(), indices.end());
		}

		while (indices.size() > 3)
		{
			bool earFound = false;

			for (std::size_t i = 0; i < indices.size(); ++i)
			{
				const int prevIndex = indices[(i + indices.size() - 1) % indices.size()];
				const int currIndex = indices[i];
				const int nextIndex = indices[(i + 1) % indices.size()];

				const Point& prev = polygon[prevIndex];
				const Point& curr = polygon[currIndex];
				const Point& next = polygon[nextIndex];

				if (cross_xy(prev, curr, next) <= 0.0)
				{
					continue;
				}

				bool containsPoint = false;
				for (std::size_t j = 0; j < indices.size(); ++j)
				{
					const int testIndex = indices[j];
					if (testIndex == prevIndex || testIndex == currIndex || testIndex == nextIndex)
					{
						continue;
					}

					if (point_in_triangle_xy(polygon[testIndex], prev, curr, next))
					{
						containsPoint = true;
						break;
					}
				}

				if (containsPoint)
				{
					continue;
				}

				triangles.push_back({ prevIndex, currIndex, nextIndex });
				indices.erase(indices.begin() + static_cast<std::ptrdiff_t>(i));
				earFound = true;
				break;
			}

			if (!earFound)
			{
				return triangles;
			}
		}

		if (indices.size() == 3)
		{
			triangles.push_back({ indices[0], indices[1], indices[2] });
		}

		return triangles;
	}

	Point normalize(const Point& v)
	{
		const double len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
		if (len == 0.0)
		{
			return { 0.0, 0.0, 0.0 };
		}
		return { v.x / len, v.y / len, v.z / len };
	}

	Point side_normal(const Point& a, const Point& b)
	{
		const double dx = b.x - a.x;
		const double dy = b.y - a.y;
		return normalize({ -dy, dx, 0.0 });
	}

	void top_uv(const Point& p, double& u, double& v)
	{
		const double minX = -9.0;
		const double maxX = 7.0;
		const double minY = -7.0;
		const double maxY = 8.0;

		u = (p.x - minX) / (maxX - minX);
		v = (p.y - minY) / (maxY - minY);
	}

	void create_top_texture()
	{
		const int width = 256;
		const int height = 256;
		std::vector<unsigned char> pixels(width * height * 4);

		for (int y = 0; y < height; ++y)
		{
			for (int x = 0; x < width; ++x)
			{
				const int index = (y * width + x) * 4;
				const bool cell = ((x / 32) + (y / 32)) % 2 == 0;
				const unsigned char r = cell ? 236 : 84;
				const unsigned char g = cell ? 231 : 133;
				const unsigned char b = cell ? 198 : 191;

				pixels[index + 0] = static_cast<unsigned char>(r + (x / 8) % 20);
				pixels[index + 1] = static_cast<unsigned char>(g + (y / 12) % 20);
				pixels[index + 2] = b;
				pixels[index + 3] = 255;
			}
		}

		glGenTextures(1, &topTextureId);
		glBindTexture(GL_TEXTURE_2D, topTextureId);

		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_RGBA,
			width,
			height,
			0,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			pixels.data());

		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void setup_lighting()
	{
		float lamb[] = { 0.25f, 0.25f, 0.25f, 0.0f };
		float ldif[] = { 0.85f, 0.85f, 0.85f, 0.0f };
		float lspec[] = { 1.0f, 1.0f, 1.0f, 0.0f };
		float lposition[] = { 10.0f, 12.0f, 14.0f, 1.0f };

		glLightfv(GL_LIGHT0, GL_POSITION, lposition);
		glLightfv(GL_LIGHT0, GL_AMBIENT, lamb);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, ldif);
		glLightfv(GL_LIGHT0, GL_SPECULAR, lspec);
		glEnable(GL_LIGHT0);
		glEnable(GL_LIGHTING);
		glEnable(GL_NORMALIZE);
	}

	void setup_material()
	{
		float amb[] = { 0.35f, 0.35f, 0.35f, 1.0f };
		float dif[] = { 0.85f, 0.85f, 0.85f, 1.0f };
		float spec[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		float sh = 0.18f * 256.0f;

		glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, dif);
		glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
		glMaterialf(GL_FRONT, GL_SHININESS, sh);
		glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
		glEnable(GL_COLOR_MATERIAL);
	}

	void draw_side_faces(const std::vector<Point>& top)
	{
		const std::size_t count = bottom.size();

		glDisable(GL_TEXTURE_2D);
		glBegin(GL_QUADS);
		for (std::size_t i = 0; i < count; ++i)
		{
			const std::size_t next = (i + 1) % count;
			const Point normal = side_normal(bottom[i], bottom[next]);
			const std::array<double, 3>& color = sideColors[i % sideColors.size()];

			glColor3d(color[0], color[1], color[2]);
			glNormal3dv(normal.p());

			glVertex3dv(bottom[i].p());
			glVertex3dv(top[i].p());
			glVertex3dv(top[next].p());
			glVertex3dv(bottom[next].p());
		}
		glEnd();
	}

	void draw_bottom_face(const std::vector<std::array<int, 3>>& triangles)
	{
		glDisable(GL_TEXTURE_2D);
		glColor3d(0.92, 0.38, 0.49);
		glNormal3d(0.0, 0.0, -1.0);
		glBegin(GL_TRIANGLES);
		for (std::size_t i = 0; i < triangles.size(); ++i)
		{
			const std::array<int, 3>& t = triangles[i];
			glVertex3dv(bottom[t[2]].p());
			glVertex3dv(bottom[t[1]].p());
			glVertex3dv(bottom[t[0]].p());
		}
		glEnd();
	}

	void draw_top_face(const std::vector<Point>& top, const std::vector<std::array<int, 3>>& triangles)
	{
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, topTextureId);
		glColor3d(1.0, 1.0, 1.0);
		glNormal3d(0.0, 0.0, 1.0);

		glBegin(GL_TRIANGLES);
		for (std::size_t i = 0; i < triangles.size(); ++i)
		{
			const std::array<int, 3>& t = triangles[i];

			double u = 0.0;
			double v = 0.0;

			top_uv(top[t[0]], u, v);
			glTexCoord2d(u, v);
			glVertex3dv(top[t[0]].p());

			top_uv(top[t[1]], u, v);
			glTexCoord2d(u, v);
			glVertex3dv(top[t[1]].p());

			top_uv(top[t[2]], u, v);
			glTexCoord2d(u, v);
			glVertex3dv(top[t[2]].p());
		}
		glEnd();

		glBindTexture(GL_TEXTURE_2D, 0);
		glDisable(GL_TEXTURE_2D);
	}
}

extern OpenGL gl;

void initRender()
{
	camera.caclulateCameraPos();

	gl.WheelEvent.reaction(&camera, &Camera::Zoom);
	gl.MouseMovieEvent.reaction(&camera, &Camera::MouseMovie);
	gl.MouseLeaveEvent.reaction(&camera, &Camera::MouseLeave);
	gl.MouseLdownEvent.reaction(&camera, &Camera::MouseStartDrag);
	gl.MouseLupEvent.reaction(&camera, &Camera::MouseStopDrag);

	camera.setPosition(0.0, -26.0, 16.0);

	create_top_texture();
}

void Render(double delta_time)
{
	(void)delta_time;

	camera.SetUpCamera();
	gl.DrawAxes();

	setup_lighting();
	setup_material();

	glDisable(GL_TEXTURE_2D);

	glPointSize(8.0f);
	glDisable(GL_LIGHTING);
	glBegin(GL_POINTS);
	glColor3d(1.0, 0.85, 0.15);
	glVertex3d(10.0, 12.0, 14.0);
	glEnd();
	glEnable(GL_LIGHTING);

	const std::vector<Point> top = build_top();
	const std::vector<std::array<int, 3>> triangles = triangulate_polygon(bottom);

	draw_side_faces(top);
	draw_bottom_face(triangles);
	draw_top_face(top, triangles);
}
