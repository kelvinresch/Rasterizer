// Rasterizer.cpp : Defines the entry point for the application.

#include "Rasterizer.h"
#include "obj_reader.h"
#include <iostream> // provides std::cout
#include <fstream>
#include <SDL2/SDL.h> // provides the screen and framebuffer
#include <chrono>
#include <numbers>
#include <string>
#include <sstream>
#include <cstdint> // has fixed-width integer types
#include <numeric> // provides std::inner_product
#include <algorithm> // provides std::transform
#include <random> // generates random numbers
#include <cmath> // provides std::sin() and std::cos()
#include <array> // provides std::array
#include <vector> // provides std::vector

// Height and Width of the image in pixels
constexpr std::size_t ImageHeight = 256;
constexpr std::size_t ImageWidth = 256;

// Define a 3xNxM matrix that holds the color values of a single image.
using Color = std::array<uint8_t, 3>;
using Row = std::array<Color, ImageWidth>;
using Image = std::array<Row, ImageHeight>;

// I think this is a depth buffer
using DepthRow = std::array<double, ImageWidth>;
using DepthBuffer = std::array<DepthRow, ImageHeight>;

// A Vector2D exists in 2D, for now.
using Vector2D = std::array<double, 2>;
using Triangle2D = std::array<Vector2D, 3>;

// A vector in 3D has 3 coordinates, and a triangle in 3D has three vectors
using Vector3D = std::array<double, 3>;
using Triangle3D = std::array<Vector3D, 3>;

// A obj face has some number of vertices, and each vertex has a normal for shading.
// the face itself does not store the actual vectors, but rather stores their index.
struct Face3D {
	std::vector<int> vertices;
	std::vector<int> normals;
};

struct Geometry {
	std::string name;
	std::vector<Vector3D> vertices;
	std::vector<Vector3D> normals;
	std::vector<Face3D> faces;
};

using Clock = std::chrono::steady_clock;

double msSince(Clock::time_point t0) {
	return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

// An Image struct exists in pixel space. The View describes from where we want to take an image.
struct View {
	Image& image; // ImageHeight x ImageWidth array that stores colors

	Vector3D position; // position that you're viewing from
	Vector3D direction; // should be a unit vector describing where you're looking
	const Vector3D x; // a vector that defines image height and the latitudinal direction of the image
	const Vector3D y; // a vector that defines image width and the longitudinal direction of the image
	// x and y need to be orthogonal to each other

	double vertical_span; // the arc span of the height of the image in radians
	double horizontal_span; // the arc span of the width of the image in radians
};

// Holds all the SDL stuff required for rendering a window.
struct SDL_Context {
	SDL_Window* window = nullptr;
	SDL_Renderer* renderer = nullptr;
	SDL_Texture* texture = nullptr;
};

// Axis-aligned bounding box
struct AABB {
	double xmin;
	double xmax;
	double ymin;
	double ymax;
};

double dot(const Vector2D& x, const Vector2D& y) {
	return std::inner_product(x.begin(), x.end(), y.begin(), 0.0);
}

double dot(const Vector3D& x, const Vector3D& y) {
	return std::inner_product(x.begin(), x.end(), y.begin(), 0.0);
}

double cross(const Vector2D& x, const Vector2D& y) {
	return (x[0] * y[1]) - (x[1] * y[0]);
}

Vector3D cross(const Vector3D& x, const Vector3D& y) {
	double a = x[1] * y[2] - y[1] * x[2];
	double b = x[2] * y[0] - y[2] * x[0];
	double c = x[0] * y[1] - y[0] * x[1];

	Vector3D product = { a, b, c };
	return product;
}

Vector2D add(const Vector2D& x, const Vector2D& y) {
	Vector2D sum{};
	std::transform(x.begin(), x.end(), y.begin(), sum.begin(),
		[](double xn, double yn) {return xn + yn;});
	return sum;
}

Vector3D add(const Vector3D& x, const Vector3D& y) {
	Vector3D sum{};
	std::transform(x.begin(), x.end(), y.begin(), sum.begin(),
		[](double xn, double yn) {return xn + yn;});
	return sum;
}

Vector2D subtract(const Vector2D& x, const Vector2D& y) {
	Vector2D delta{};
	std::transform(x.begin(), x.end(), y.begin(), delta.begin(),
		[](double xn, double yn) {return xn - yn;});
	return delta;
}

Vector3D subtract(const Vector3D& x, const Vector3D& y) {
	Vector3D delta{};
	std::transform(x.begin(), x.end(), y.begin(), delta.begin(),
		[](double xn, double yn) {return xn - yn;});
	return delta;
}

Vector2D scalar_mult(const Vector2D& x, const double s) {
	Vector2D product{};
	std::transform(x.begin(), x.end(), product.begin(),
		[s](double xn) {return xn * s;});
	return product;
}

Vector3D scalar_mult(const Vector3D& x, const double s) {
	Vector3D product{};
	std::transform(x.begin(), x.end(), product.begin(),
		[s](double xn) {return xn * s;});
	return product;
}

Vector2D rotate90(const Vector2D& x) {
	Vector2D y = { x[1], -x[0] };
	return y;
}

// angle in rad
Vector2D rotate(const Vector2D& x, const double theta) {
	// input [1 0]
	// rotate 10 deg
	// output [x1*cos(theta) x1*sin(theta)]
	// input [0 1]
	// output [-x2*sin(theta) x2*cos(theta)
	Vector2D y = { x[0] * std::cos(theta) - x[1] * std::sin(theta),
				x[0] * std::sin(theta) + x[1] * std::cos(theta) };
	return y;
}

Triangle2D translate_triangle(const Triangle2D& t, const Vector2D& x) {
	Vector2D a = add(x, t[0]);
	Vector2D b = add(x, t[1]);
	Vector2D c = add(x, t[2]);

	Triangle2D y = { a, b, c };
	return y;
}

Triangle3D translate_triangle(const Triangle3D& t, const Vector3D& x) {
	Vector3D a = add(x, t[0]);
	Vector3D b = add(x, t[1]);
	Vector3D c = add(x, t[2]);

	Triangle3D y = { a, b, c };
	return y;
}

Triangle2D scale_triangle(const Triangle2D& t, const Vector2D& x, const double s) {
	Vector2D a = add(x, scalar_mult(subtract(t[0], x), s));
	Vector2D b = add(x, scalar_mult(subtract(t[1], x), s));
	Vector2D c = add(x, scalar_mult(subtract(t[2], x), s));

	Triangle2D y = { a, b, c };
	return y;
}

// Rotate a triangle t about a point x by theta radians
Triangle2D rotate_triangle(const Triangle2D& t, const Vector2D& x, const double theta) {
	Vector2D a = add(x, rotate(subtract(t[0], x), theta));
	Vector2D b = add(x, rotate(subtract(t[1], x), theta));
	Vector2D c = add(x, rotate(subtract(t[2], x), theta));
	
	Triangle2D y = { a, b, c };
	return y;
}

bool in_triangle(const Vector2D& point, const Triangle2D& triangle) {
	// define side n as the side spanning between points n and n+1
	Vector2D side1 = subtract(triangle[1], triangle[0]);
	Vector2D side2 = subtract(triangle[2], triangle[1]);
	Vector2D side3 = subtract(triangle[0], triangle[2]);
	
	Vector2D a_to_point = subtract(point, triangle[0]);
	Vector2D b_to_point = subtract(point, triangle[1]);
	Vector2D c_to_point = subtract(point, triangle[2]);

	double result1 = dot(side1, rotate90(a_to_point));
	double result2 = dot(side2, rotate90(b_to_point));
	double result3 = dot(side3, rotate90(c_to_point));

	if (result1 <= 0 and result2 <= 0 and result3 <= 0) {
		return true;
	}
	else if (result1 >= 0 and result2 >= 0 and result3 >= 0) {
		return true;
	}
	else {
		return false;
	}
}

Color random_color(std::mt19937& gen) {
	std::uniform_int_distribution<> distr(0, 255);

	Color color = { 
		static_cast<uint8_t>(distr(gen)), 
		static_cast<uint8_t>(distr(gen)), 
		static_cast<uint8_t>(distr(gen)) 
	};
	return color;
}

Triangle2D random_triangle(std::mt19937& gen) {
	std::uniform_real_distribution<double> distr_width(0, ImageWidth);
	std::uniform_real_distribution<double> distr_height(0, ImageHeight);

	Vector2D a = { distr_width(gen), distr_height(gen) };
	Vector2D b = { distr_width(gen), distr_height(gen) };
	Vector2D c = { distr_width(gen), distr_height(gen) };

	Triangle2D triangle = { a, b, c };

	return triangle;
}

bool init_sdl(SDL_Context& sdl) {
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		std::cerr << "SDL Initialization Failed: "
			<< SDL_GetError() << "\n";
		return false;
	}

	sdl.window = SDL_CreateWindow(
		"Rasterizer",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		ImageWidth,
		ImageHeight,
		SDL_WINDOW_SHOWN
	);

	if (sdl.window == nullptr) {
		std::cerr << "Window creation failed: "
			<< SDL_GetError() << '\n';

		return false;
	}

	sdl.renderer = SDL_CreateRenderer(
		sdl.window,
		-1,
		SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC
	);

	if (sdl.renderer == nullptr) {
		std::cerr << "Renderer creation failed: "
			<< SDL_GetError() << '\n';

		SDL_DestroyWindow(sdl.window);
		sdl.window = nullptr;
		return false;
	}

	sdl.texture = SDL_CreateTexture(
		sdl.renderer,
		SDL_PIXELFORMAT_BGR24,
		SDL_TEXTUREACCESS_STREAMING,
		static_cast<int>(ImageWidth),
		static_cast<int>(ImageHeight)
	);

	if (sdl.texture == nullptr) {
		std::cerr << "Texture creation failed: "
			<< SDL_GetError() << "\n";

		SDL_DestroyRenderer(sdl.renderer);
		SDL_DestroyWindow(sdl.window);

		sdl.renderer = nullptr;
		sdl.window = nullptr;
		return false;
	}

	return true;
}

AABB find_AABB(Triangle2D t) {
	// initialize to the first vertex of the triangle
	double xmin = t[0][0];
	double xmax = t[0][0];
	double ymin = t[0][1];
	double ymax = t[0][1];

	// iterate through the rest of the vertices
	for (size_t i = 1; i < 3; i++) {
		if (t[i][0] < xmin) {
			xmin = t[i][0];
		}
		if (t[i][0] > xmax) {
			xmax = t[i][0];
		}
		if (t[i][1] < ymin) {
			ymin = t[i][1];
		}
		if (t[i][1] > ymax) {
			ymax = t[i][1];
		}
	}

	AABB bounding_box = { xmin, xmax, ymin, ymax };
	return bounding_box;
}

int rasterize_triangles(View& view, std::vector<Triangle3D>& triangles, std::vector<Color>& colors) {
	if (triangles.size() != colors.size()) {
		std::cerr << "Number of triangles and number of colors needs to be the same!";
		return 1;
	}

	for (std::size_t i = 0; i < triangles.size(); i++) {

		//std::cout << i << "\n";

		// First step is to convert the list of triangles given in world space into
		// view space by subtracting the view position from the triangle.
		Triangle3D viewed_triangle = translate_triangle(triangles[i], scalar_mult(view.position, -1.0));

		// Next we need to project the triangle, now in view space, onto the view plane.
		// This is still a mapping from R3 to R3
		Vector3D a3d = scalar_mult(viewed_triangle[0], dot(view.direction, view.direction) / dot(viewed_triangle[0], view.direction));
		Vector3D b3d = scalar_mult(viewed_triangle[1], dot(view.direction, view.direction) / dot(viewed_triangle[1], view.direction));
		Vector3D c3d = scalar_mult(viewed_triangle[2], dot(view.direction, view.direction) / dot(viewed_triangle[2], view.direction));

		// Now that the triangle, defined by a, b, and c, is in the view plane, we need to do a change of basis
		// into the frame coordinates (R3 -> R2)
		double x_len2 = dot(view.x, view.x);
		double y_len2 = dot(view.y, view.y);
		Vector2D a2d = { dot(view.x, a3d) / x_len2, dot(view.y, a3d) / y_len2 };
		Vector2D b2d = { dot(view.x, b3d) / x_len2, dot(view.y, b3d) / y_len2 };
		Vector2D c2d = { dot(view.x, c3d) / x_len2, dot(view.y, c3d) / y_len2 };

		Triangle2D framed_triangle = { a2d, b2d, c2d }; 

		// Now the triangle is in 2D coordinates in frame space. We now need to convert to pixel space
		// which puts the origin at the bottom left corner instead of the center so that we don't
		// ever iterate over negative pixel indices
		Vector2D frame_center = { static_cast<double>(ImageWidth) / 2, static_cast<double>(ImageHeight) / 2 };
		Triangle2D pixel_triangle = translate_triangle(framed_triangle, frame_center);

		//std::cout << "Triangle: \n";

		//std::cout << "vertex[0]: " << triangles[i][0][0] << " " << triangles[i][0][1] << " " << triangles[i][0][2] << "\n";
		//std::cout << "vertex[1]: " << triangles[i][1][0] << " " << triangles[i][1][1] << " " << triangles[i][1][2] << "\n";
		//std::cout << "vertex[2]: " << triangles[i][2][0] << " " << triangles[i][2][1] << " " << triangles[i][2][2] << "\n";

		//std::cout << "viewed_triangle[0]: " << viewed_triangle[0][0] << " " << viewed_triangle[0][1] << " " << viewed_triangle[0][2] << "\n";
		//std::cout << "viewed_triangle[1]: " << viewed_triangle[1][0] << " " << viewed_triangle[1][1] << " " << viewed_triangle[1][2] << "\n";
		//std::cout << "viewed_triangle[2]: " << viewed_triangle[2][0] << " " << viewed_triangle[2][1] << " " << viewed_triangle[2][2] << "\n";

		//std::cout << "a3d: " << a3d[0] << " " << a3d[1] << " " << a3d[2] << "\n";
		//std::cout << "b3d: " << b3d[0] << " " << b3d[1] << " " << b3d[2] << "\n";
		//std::cout << "c3d: " << c3d[0] << " " << c3d[1] << " " << c3d[2] << "\n";

		//std::cout << "a2d: " << a2d[0] << " " << a2d[1] << "\n";
		//std::cout << "b2d: " << b2d[0] << " " << b2d[1] << "\n";
		//std::cout << "c2d: " << c2d[0] << " " << c2d[1] << "\n";

		//std::cout << "view.x: " << view.x[0] << " " << view.x[1] << " " << view.x[2] << "\n";
		//std::cout << "view.y: " << view.y[0] << " " << view.y[1] << " " << view.y[2] << "\n";

		//std::cout << "dot(view.x, a3d) / x_len: " << dot(view.x, a3d) / x_len2 << "\n";
		//std::cout << "dot(view.y, a3d) / ylen: " << dot(view.y, a3d) / y_len2 << "\n";

		//std::cout << "x1: " << framed_triangle[0][0] << "\n";
		//std::cout << "y1: " << framed_triangle[0][1] << "\n";
		//std::cout << "x2: " << framed_triangle[1][0] << "\n";
		//std::cout << "y2: " << framed_triangle[1][1] << "\n";
		//std::cout << "x3: " << framed_triangle[2][0] << "\n";
		//std::cout << "y3: " << framed_triangle[2][1] << "\n";

		// For a given triangle, find the axis-aligned bounding box
		AABB aabb = find_AABB(pixel_triangle);
		// Once I find the bounding box of the triangle in view space, I need to
		// translate that into pixel space

		// Check that none of the box vertices are out of the view space window
		double ImageWidth_d = static_cast<double>(ImageWidth);
		double ImageHeight_d = static_cast<double>(ImageHeight);


		if (aabb.xmin < 0) {
			aabb.xmin = 0;
		}
		else if (aabb.xmin > ImageWidth_d) {
			aabb.xmin = ImageWidth_d;
		}
		if (aabb.xmax < 0) {
			aabb.xmax = 0;
		}
		else if (aabb.xmax >= ImageWidth_d) {
			aabb.xmax = ImageWidth_d;
		}
		if (aabb.ymin < 0) {
			aabb.ymin = 0;
		}
		else if (aabb.ymin >= ImageHeight_d) {
			aabb.ymin = ImageHeight_d;
		}
		if (aabb.ymax < 0) {
			aabb.ymax = 0;
		}
		else if (aabb.ymax >= ImageHeight_d) {
			aabb.ymax = ImageHeight_d;
		}

		//std::cout << "xmin: " << aabb.xmin << "\n";
		//std::cout << "xmax: " << aabb.xmax << "\n";
		//std::cout << "ymin: " << aabb.ymin << "\n";
		//std::cout << "ymax: " << aabb.ymax << "\n";

		// discretize the bounding box to move from view space to pixel space
		int x_min_d = static_cast<int>(aabb.xmin);
		int x_max_d = static_cast<int>(aabb.xmax);
		int y_min_d = static_cast<int>(aabb.ymin);
		int y_max_d = static_cast<int>(aabb.ymax);

		for (int row = y_min_d; row < y_max_d; row++) {
			for (int col = x_min_d; col < x_max_d; col++) {
				Vector2D point = { static_cast<double>(col) + 0.5, static_cast<double>(row) + 0.5 };
				if (in_triangle(point, pixel_triangle)) {
					view.image[row][col] = colors[i];
				}
			}
		}
	}
	return 0;
}

int read_geometry(const std::string filename, Geometry& geometry) {
	std::ifstream file(filename);

	if (!file.is_open()) {
		std::cerr << "Error opening file" << std::endl;
		return 1;
	}

	std::string line{};
	while (std::getline(file, line)) {
		//std::cout << line << "\n";

		std::stringstream ss(line);
		// the linetype stores v, vn, f, g, or whatever else.
		std::string linetype;
		// the token holds the "words" after the linetype
		std::string token;

		std::getline(ss, linetype, ' ');

		if (linetype == "g") {
			ss >> token;
			// assign the geometry's name
			geometry.name = token;
		}
		else if (linetype == "v") {
			// a v will always be 3 numbers (unless the obj has "weight" factors but idc rn)
			double a;
			double b;
			double c;

			ss >> token;
			a = std::stod(token);
			ss >> token;
			b = std::stod(token);
			ss >> token;
			c = std::stod(token);

			Vector3D v = { a, b, c };
			geometry.vertices.push_back(v);
		}
		else if (linetype == "vn") {
			// a vn will always be 3 numbers
			double a;
			double b;
			double c;

			ss >> token;
			a = std::stod(token);
			ss >> token;
			b = std::stod(token);
			ss >> token;
			c = std::stod(token);

			Vector3D vn = { a, b, c };
			geometry.normals.push_back(vn);
		}
		else if (linetype == "f") {
			// a face can have any number of vertices
			Face3D face{};
			std::string face_vertex;
			while (ss >> face_vertex) {
				std::stringstream face_vertex_stream(face_vertex);

				std::getline(face_vertex_stream, token, '/');
				face.vertices.push_back(std::stod(token));
				std::getline(face_vertex_stream, token, '/');
				std::getline(face_vertex_stream, token, '/');
				face.normals.push_back(std::stod(token));
			}

			geometry.faces.push_back(face);
		}
	}

	return 0;
}

int print_geometry(Geometry geometry) {
	// Print the geometry as an obj text
	std::cout << "g" << geometry.name << "\n";
	for (const Vector3D v : geometry.vertices) {
		std::cout << "v  ";
		for (const auto& x : v) {
			std::cout << x << "  ";
		}
		std::cout << std::endl;
	}
	std::cout << std::endl;
	for (const Vector3D vn : geometry.normals) {
		std::cout << "vn  ";
		for (const auto& x : vn) {
			std::cout << x << "  ";
		}
		std::cout << std::endl;
	}
	std::cout << std::endl;
	for (const Face3D f : geometry.faces) {
		std::cout << "f  ";
		for (int i = 0; i < f.vertices.size(); i++) {
			std::cout << f.vertices[i] << "//" << f.normals[i] << "  ";
		}
		std::cout << std::endl;
	}
	return 0;
}

int main(int argc, char* argv[])
{
	SDL_Context sdl{};

	if (!init_sdl(sdl)) {
		SDL_Quit();
		return 1;
	}
	
	// create a discrete grid in pixel-space
	Image image{};
	Vector3D position = { 0, 0, 300 };
	Vector3D direction = { 0, 0, -1 };
	Vector3D x = { 0, 1, 0 };
	Vector3D y = { 1, 0, 0 };
	double vertical_span = 1;
	double horizontal_span = vertical_span / ImageHeight * ImageWidth;
	x = scalar_mult(x, std::tan(vertical_span / 2) / ImageHeight);
	y = scalar_mult(y, std::tan(horizontal_span / 2) / ImageWidth);
	View view = {
		image = image,
		position = position,
		direction = direction,
		x = x,
		y = y,
		vertical_span = vertical_span,
		horizontal_span = horizontal_span
		};

	std::random_device rd;
	std::mt19937 gen(rd());

	std::vector<Triangle3D> triangles{};
	std::vector<Color> colors{};

	bool running = true;

	// rotate 2 pi radians per second
	double omega = 2 * std::numbers::pi;

	auto t0 = Clock::now();
	double ms = msSince(t0);

	Geometry geometry{};
	read_geometry("assets/teapot.obj", geometry);
	//print_geometry(geometry);

	// Turn the geometry into a list of triangles.
	for (const Face3D f : geometry.faces) {
		Triangle3D triangle3d = { geometry.vertices[f.vertices[0] - 1],
							geometry.vertices[f.vertices[1] - 1],
							geometry.vertices[f.vertices[2] - 1]
		};
		
		triangles.push_back(triangle3d);
		colors.push_back(random_color(gen));
	}

	

	while (running) {
		SDL_Event event;

		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				running = false;
			}
		}

		// Generate some number of random triangles.
		//for (int i = 0; i < num_triangles; i++) {
		//	Vector2D midpoint = { 128, 128 };
		//	triangles[i] = rotate_triangle(triangles[i], midpoint, omega * msSince(t0) / 1000);
		//}

		// update the image from the list of triangles
		t0 = Clock::now();
		// I think rasterize triangles should take in a 2d triangle, but each point in 2d should also
		// hold its distance from the viewer.
		// In addition, the view should also hold not just the color, but also how far away the
		// object that gave that pixel its color was.
		rasterize_triangles(view, triangles, colors);
		ms = msSince(t0);
		std::cout << "Time to rasterize:" << ms << "\n";

		SDL_UpdateTexture(
			sdl.texture,
			nullptr,
			image[0][0].data(),
			static_cast<int>(ImageWidth * sizeof(Color))
		);

		// Clear the image
		view.image = {};

		SDL_RenderClear(sdl.renderer);
		SDL_RenderCopyEx(sdl.renderer, sdl.texture, nullptr, nullptr, 0.0, NULL, SDL_FLIP_VERTICAL);
		SDL_RenderPresent(sdl.renderer);
	}

	SDL_DestroyTexture(sdl.texture);
	SDL_DestroyRenderer(sdl.renderer);
	SDL_DestroyWindow(sdl.window);
	SDL_Quit();

	return 0;
}