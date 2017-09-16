#include "load_save_png.hpp"
#include "GL.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

static GLuint compile_shader(GLenum type, std::string const &source);
static GLuint link_program(GLuint vertex_shader, GLuint fragment_shader);

int main(int argc, char **argv) {
	//Configuration:
	struct {
		std::string title = "Game1: Cave Explorer";
		glm::uvec2 size = glm::uvec2(640, 480);
	} config;

	//------------  initialization ------------

	//Initialize SDL library:
	SDL_Init(SDL_INIT_VIDEO);

	//Ask for an OpenGL context version 3.3, core profile, enable debug:
	SDL_GL_ResetAttributes();
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

	//create window:
	SDL_Window *window = SDL_CreateWindow(
		config.title.c_str(),
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		config.size.x, config.size.y,
		SDL_WINDOW_OPENGL /*| SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI*/
	);

	if (!window) {
		std::cerr << "Error creating SDL window: " << SDL_GetError() << std::endl;
		return 1;
	}

	//Create OpenGL context:
	SDL_GLContext context = SDL_GL_CreateContext(window);

	if (!context) {
		SDL_DestroyWindow(window);
		std::cerr << "Error creating OpenGL context: " << SDL_GetError() << std::endl;
		return 1;
	}

	#ifdef _WIN32
	//On windows, load OpenGL extensions:
	if (!init_gl_shims()) {
		std::cerr << "ERROR: failed to initialize shims." << std::endl;
		return 1;
	}
	#endif

	//Set VSYNC + Late Swap (prevents crazy FPS):
	if (SDL_GL_SetSwapInterval(-1) != 0) {
		std::cerr << "NOTE: couldn't set vsync + late swap tearing (" << SDL_GetError() << ")." << std::endl;
		if (SDL_GL_SetSwapInterval(1) != 0) {
			std::cerr << "NOTE: couldn't set vsync (" << SDL_GetError() << ")." << std::endl;
		}
	}

	//Hide mouse cursor (note: showing can be useful for debugging):
	//SDL_ShowCursor(SDL_DISABLE);

	//------------ opengl objects / game assets ------------

	//texture:
	GLuint tex = 0;
	glm::uvec2 tex_size = glm::uvec2(0,0);

	{ //load texture 'tex':
		std::vector< uint32_t > data;
		if (!load_png("roguelikeSheet_transparent.png", &tex_size.x, &tex_size.y, &data, LowerLeftOrigin)) {
			std::cerr << "Failed to load texture." << std::endl;
			exit(1);
		}
		//create a texture object:
		glGenTextures(1, &tex);
		//bind texture object to GL_TEXTURE_2D:
		glBindTexture(GL_TEXTURE_2D, tex);
		//upload texture data from data:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_size.x, tex_size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, &data[0]);
		//set texture sampling parameters:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	//shader program:
	GLuint program = 0;
	GLuint program_Position = 0;
	GLuint program_TexCoord = 0;
	GLuint program_Color = 0;
	GLuint program_mvp = 0;
	GLuint program_tex = 0;
	{ //compile shader program:
		GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER,
			"#version 330\n"
			"uniform mat4 mvp;\n"
			"in vec4 Position;\n"
			"in vec2 TexCoord;\n"
			"in vec4 Color;\n"
			"out vec2 texCoord;\n"
			"out vec4 color;\n"
			"void main() {\n"
			"	gl_Position = mvp * Position;\n"
			"	color = Color;\n"
			"	texCoord = TexCoord;\n"
			"}\n"
		);

		GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER,
			"#version 330\n"
			"uniform sampler2D tex;\n"
			"in vec4 color;\n"
			"in vec2 texCoord;\n"
			"out vec4 fragColor;\n"
			"void main() {\n"
			"	fragColor = texture(tex, texCoord) * color;\n"
			"}\n"
		);

		program = link_program(fragment_shader, vertex_shader);

		//look up attribute locations:
		program_Position = glGetAttribLocation(program, "Position");
		if (program_Position == -1) throw std::runtime_error("no attribute named Position");
		program_TexCoord = glGetAttribLocation(program, "TexCoord");
		if (program_TexCoord == -1) throw std::runtime_error("no attribute named TexCoord");
		program_Color = glGetAttribLocation(program, "Color");
		if (program_Color == -1) throw std::runtime_error("no attribute named Color");

		//look up uniform locations:
		program_mvp = glGetUniformLocation(program, "mvp");
		if (program_mvp == -1) throw std::runtime_error("no uniform named mvp");
		program_tex = glGetUniformLocation(program, "tex");
		if (program_tex == -1) throw std::runtime_error("no uniform named tex");
	}

	//vertex buffer:
	GLuint buffer = 0;
	{ //create vertex buffer
		glGenBuffers(1, &buffer);
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
	}

	struct Vertex {
		Vertex(glm::vec2 const &Position_, glm::vec2 const &TexCoord_, glm::u8vec4 const &Color_) :
			Position(Position_), TexCoord(TexCoord_), Color(Color_) { }
		glm::vec2 Position;
		glm::vec2 TexCoord;
		glm::u8vec4 Color;
	};
	static_assert(sizeof(Vertex) == 20, "Vertex is nicely packed.");

	//vertex array object:
	GLuint vao = 0;
	{ //create vao and set up binding:
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glVertexAttribPointer(program_Position, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLbyte *)0);
		glVertexAttribPointer(program_TexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLbyte *)0 + sizeof(glm::vec2));
		glVertexAttribPointer(program_Color, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (GLbyte *)0 + sizeof(glm::vec2) + sizeof(glm::vec2));
		glEnableVertexAttribArray(program_Position);
		glEnableVertexAttribArray(program_TexCoord);
		glEnableVertexAttribArray(program_Color);
	}

	//------------ sprite info ------------
	struct SpriteInfo {
		glm::vec2 min_uv = glm::vec2(0.0f);
		glm::vec2 max_uv = glm::vec2(1.0f);
		glm::vec2 rad = glm::vec2(0.5f);
	};

	float coord[] = {
		17.0f, 17.0f, // dirt tile - 0
		731.0f, 187, // track - 1
		731.0f, 204.0f, // side track - 2
		731.0f, 340.0f, // gold bars - 3
		221.0f, 17.0f, // dirt wall - 4
		833.0f, 204.0f, // side cart - 5
		867.0f, 204.0f, // cart - 6
		697.0f, 340.0f, // logs - 7
		833.0f, 153.0f, // side cart w/ gold - 8
		867.0f, 153.0f // cart w/ gold 9
	};

	SpriteInfo sprites[10];
	for (int i = 0; i < 10; i++) {
		sprites[i].min_uv.x = coord[2 * i] / 968.0f;
		sprites[i].min_uv.y = coord[2 * i + 1] / 526.0f;
		sprites[i].max_uv.x = (coord[2 * i] + 16.0f) / 968.0f;
		sprites[i].max_uv.y = (coord[2 * i + 1] + 16.0f) / 526.0f;
	}

	//------------ game state ------------
	const int length = 11;
	const int width = 11;
	int grid[length][width] = {
		{ 0, 2, 0, 2, 2, 0, 2, 2, 0, 4, 3 },
		{ 4, 4, 1, 4, 4, 1, 4, 4, 1, 4, 0 },
		{ 3, 4, 0, 4, 4, 1, 4, 4, 1, 4, 1 },
		{ 0, 4, 4, 4, 4, 0, 2, 2, 0, 4, 1 },
		{ 1, 4, 4, 4, 4, 1, 4, 4, 1, 4, 1 },
		{ 0, 2, 2, 2, 2, 0, 4, 4, 0, 2, 0 },
		{ 4, 4, 4, 4, 4, 1, 4, 4, 4, 4, 4 },
		{ 3, 4, 0, 2, 2, 0, 2, 2, 0, 2, 0 },
		{ 0, 4, 1, 4, 4, 4, 4, 4, 1, 4, 3 },
		{ 1, 4, 1, 4, 4, 4, 3, 4, 1, 4, 4 },
		{ 0, 2, 0, 2, 2, 2, 0, 2, 0, 2, 0 }
	};
	bool lit[length][width] = {};
	const float speed = 5.0f;
	glm::uvec2 pos = glm::uvec2(5, 5);
	glm::vec2 cart = glm::vec2(0, 0);
	float loc = 0.0f;
	int state = 0; // 0 - horizontal, 1 - left, 2 - right, 3 - vertical, 4 - up, 5 - down, +6 - gold and win
	int num_gold_left = 5;
	lit[pos.y][pos.x] = true;

	struct {
		glm::vec2 at = glm::vec2(0.0f, 0.0f);
		glm::vec2 radius = glm::vec2(5.5f);
	} camera;
	//correct radius for aspect ratio:
	camera.radius.x = camera.radius.y * (float(config.size.x) / float(config.size.y));

	//------------ game loop ------------

	bool should_quit = false;
	while (true) {
		static SDL_Event evt;
		while (SDL_PollEvent(&evt) == 1) {
			//handle input:
			if (evt.type == SDL_KEYDOWN) {
				if (evt.key.keysym.sym == SDLK_ESCAPE) {
					should_quit = true;
				}
				if (state == 0 || state == 3) {
					if (evt.key.keysym.sym == SDLK_LEFT) {
						state = 0;
						if (pos.x > 0 && grid[pos.y][pos.x - 1] == 2) {
							pos.x--;
							loc = cart.x - 1.0f;
							state = 1;
							lit[pos.y][pos.x] = true;
						}
					} else if (evt.key.keysym.sym == SDLK_RIGHT) {
						state = 0;
						if (pos.x < width - 1 && grid[pos.y][pos.x + 1] == 2) {
							pos.x++;
							loc = cart.x + 1.0f;
							state = 2;
							lit[pos.y][pos.x] = true;
						}
					} else if (evt.key.keysym.sym == SDLK_UP) {
						state = 3;
						if (pos.y > 0 && grid[pos.y - 1][pos.x] == 1) {
							pos.y--;
							loc = cart.y + 1.0f;
							state = 4;
							lit[pos.y][pos.x] = true;
						}
					} else if (evt.key.keysym.sym == SDLK_DOWN) {
						state = 3;
						if (pos.y < length - 1 && grid[pos.y + 1][pos.x] == 1) {
							pos.y++;
							loc = cart.y - 1.0f;
							state = 5;
							lit[pos.y][pos.x] = true;
						}
					} else if (evt.key.keysym.sym == SDLK_SPACE) {
						if (pos.x > 0 && grid[pos.y][pos.x - 1] == 3) {
							if (std::chrono::high_resolution_clock::now().time_since_epoch().count() % num_gold_left == 0) {
								std::cout << "You win!" << std::endl;
								state += 6;
							} else {
								grid[pos.y][pos.x - 1] = 7;
								num_gold_left--;
							}
						} else if (pos.x < width - 1 && grid[pos.y][pos.x + 1] == 3) {
							if (std::chrono::high_resolution_clock::now().time_since_epoch().count() % num_gold_left == 0) {
								std::cout << "You win!" << std::endl;
								state += 6;
							} else {
								grid[pos.y][pos.x + 1] = 7;
								num_gold_left--;
							}
						} else if (pos.y > 0 && grid[pos.y - 1][pos.x] == 3) {
							if (std::chrono::high_resolution_clock::now().time_since_epoch().count() % num_gold_left == 0) {
								std::cout << "You win!" << std::endl;
								state += 6;
							} else {
								grid[pos.y - 1][pos.x] = 7;
								num_gold_left--;
							}
						} else if (pos.y < length - 1 && grid[pos.y + 1][pos.x] == 3) {
							if (std::chrono::high_resolution_clock::now().time_since_epoch().count() % num_gold_left == 0) {
								std::cout << "You win!" << std::endl;
								state += 6;
							} else {
								grid[pos.y + 1][pos.x] = 7;
								num_gold_left--;
							}
						}
					}
				}
			} else if (evt.type == SDL_QUIT) {
				should_quit = true;
				break;
			}
		}
		if (should_quit) break;

		auto current_time = std::chrono::high_resolution_clock::now();
		static auto previous_time = current_time;
		float elapsed = std::chrono::duration< float >(current_time - previous_time).count();
		previous_time = current_time;

		{ //update game state:
			if (state == 1) {
				cart.x -= elapsed * speed;
				while (cart.x < loc) {
					if (grid[pos.y][pos.x] == 0) {
						cart.x = loc;
						state = 0;
						if (pos.x > 0 && grid[pos.y][pos.x - 1] == 3) {
							lit[pos.y][pos.x - 1] = true;
						} else if (pos.x < width - 1 && grid[pos.y][pos.x + 1] == 3) {
							lit[pos.y][pos.x + 1] = true;
						} else if (pos.y > 0 && grid[pos.y - 1][pos.x] == 3) {
							lit[pos.y - 1][pos.x] = true;
						} else if (pos.y < length - 1 && grid[pos.y + 1][pos.x] == 3) {
							lit[pos.y + 1][pos.x] = true;
						}
					} else {
						pos.x--;
						loc -= 1.0f;
						lit[pos.y][pos.x] = true;
					}
				}
			} else if (state == 2) {
				cart.x += elapsed * speed;
				while (cart.x > loc) {
					if (grid[pos.y][pos.x] == 0) {
						cart.x = loc;
						state = 0;
						if (pos.x > 0 && grid[pos.y][pos.x - 1] == 3) {
							lit[pos.y][pos.x - 1] = true;
						} else if (pos.x < width - 1 && grid[pos.y][pos.x + 1] == 3) {
							lit[pos.y][pos.x + 1] = true;
						} else if (pos.y > 0 && grid[pos.y - 1][pos.x] == 3) {
							lit[pos.y - 1][pos.x] = true;
						} else if (pos.y < length - 1 && grid[pos.y + 1][pos.x] == 3) {
							lit[pos.y + 1][pos.x] = true;
						}
					} else {
						pos.x++;
						loc += 1.0f;
						lit[pos.y][pos.x] = true;
					}
				}
			} else if (state == 4) {
				cart.y += elapsed * speed;
				while (cart.y > loc) {
					if (grid[pos.y][pos.x] == 0) {
						cart.y = loc;
						state = 3;
						if (pos.x > 0 && grid[pos.y][pos.x - 1] == 3) {
							lit[pos.y][pos.x - 1] = true;
						} else if (pos.x < width - 1 && grid[pos.y][pos.x + 1] == 3) {
							lit[pos.y][pos.x + 1] = true;
						} else if (pos.y > 0 && grid[pos.y - 1][pos.x] == 3) {
							lit[pos.y - 1][pos.x] = true;
						} else if (pos.y < length - 1 && grid[pos.y + 1][pos.x] == 3) {
							lit[pos.y + 1][pos.x] = true;
						}
					} else {
						pos.y--;
						loc += 1.0f;
						lit[pos.y][pos.x] = true;
					}
				}
			} else if (state == 5) {
				cart.y -= elapsed * speed;
				while (cart.y < loc) {
					if (grid[pos.y][pos.x] == 0) {
						cart.y = loc;
						state = 3;
						if (pos.x > 0 && grid[pos.y][pos.x - 1] == 3) {
							lit[pos.y][pos.x - 1] = true;
						} else if (pos.x < width - 1 && grid[pos.y][pos.x + 1] == 3) {
							lit[pos.y][pos.x + 1] = true;
						} else if (pos.y > 0 && grid[pos.y - 1][pos.x] == 3) {
							lit[pos.y - 1][pos.x] = true;
						} else if (pos.y < length - 1 && grid[pos.y + 1][pos.x] == 3) {
							lit[pos.y + 1][pos.x] = true;
						}
					} else {
						pos.y++;
						loc -= 1.0f;
						lit[pos.y][pos.x] = true;
					}
				}
			}
		}

		//draw output:
		glClearColor(78.0f / 255.0f, 46 / 255.0f, 40.0f / 255.0f, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


		{ //draw game state:
			std::vector< Vertex > verts;

			//helper: add rectangle to verts:
			auto rect = [&verts](glm::vec2 const &at, glm::vec2 const &rad, glm::u8vec4 const &tint) {
				verts.emplace_back(at + glm::vec2(-rad.x,-rad.y), glm::vec2(0.0f, 0.0f), tint);
				verts.emplace_back(verts.back());
				verts.emplace_back(at + glm::vec2(-rad.x, rad.y), glm::vec2(0.0f, 1.0f), tint);
				verts.emplace_back(at + glm::vec2( rad.x,-rad.y), glm::vec2(1.0f, 0.0f), tint);
				verts.emplace_back(at + glm::vec2( rad.x, rad.y), glm::vec2(1.0f, 1.0f), tint);
				verts.emplace_back(verts.back());
			};

			auto draw_sprite = [&verts](SpriteInfo const &sprite, glm::vec2 const &at, float angle = 0.0f) {
				glm::vec2 min_uv = sprite.min_uv;
				glm::vec2 max_uv = sprite.max_uv;
				glm::vec2 rad = sprite.rad;
				glm::u8vec4 tint = glm::u8vec4(0xff, 0xff, 0xff, 0xff);
				glm::vec2 right = glm::vec2(std::cos(angle), std::sin(angle));
				glm::vec2 up = glm::vec2(-right.y, right.x);

				verts.emplace_back(at + right * -rad.x + up * -rad.y, glm::vec2(min_uv.x, min_uv.y), tint);
				verts.push_back(verts.back());
				verts.emplace_back(at + right * -rad.x + up * rad.y, glm::vec2(min_uv.x, max_uv.y), tint);
				verts.emplace_back(at + right *  rad.x + up * -rad.y, glm::vec2(max_uv.x, min_uv.y), tint);
				verts.emplace_back(at + right *  rad.x + up *  rad.y, glm::vec2(max_uv.x, max_uv.y), tint);
				verts.push_back(verts.back());
			};

			// draw background
			glm::vec2 at;
			at.y = 0.5f * (length - 1);
			for (int i = 0; i < length; i++) {
				at.x = -0.5f * (width - 1);
				for (int j = 0; j < width; j++) {
					if (state >= 6 || lit[i][j]) {
						draw_sprite(sprites[0], at);
						if (grid[i][j] != 0)
							draw_sprite(sprites[grid[i][j]], at);
					} else {
						draw_sprite(sprites[4], at);
					}
					at.x += 1.0f;
				}
				at.y -= 1.0f;
			}

			// draw player
			if (state <= 2)
				draw_sprite(sprites[5], cart);
			else if (state <= 5)
				draw_sprite(sprites[6], cart);
			else if (state <= 8)
				draw_sprite(sprites[8], cart);
			else
				draw_sprite(sprites[9], cart);

			glBindBuffer(GL_ARRAY_BUFFER, buffer);
			glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * verts.size(), &verts[0], GL_STREAM_DRAW);

			glUseProgram(program);
			glUniform1i(program_tex, 0);
			glm::vec2 scale = 1.0f / camera.radius;
			glm::vec2 offset = scale * -camera.at;
			glm::mat4 mvp = glm::mat4(
				glm::vec4(scale.x, 0.0f, 0.0f, 0.0f),
				glm::vec4(0.0f, scale.y, 0.0f, 0.0f),
				glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
				glm::vec4(offset.x, offset.y, 0.0f, 1.0f)
			);
			glUniformMatrix4fv(program_mvp, 1, GL_FALSE, glm::value_ptr(mvp));

			glBindTexture(GL_TEXTURE_2D, tex);
			glBindVertexArray(vao);

			glDrawArrays(GL_TRIANGLE_STRIP, 0, verts.size());
		}


		SDL_GL_SwapWindow(window);
	}


	//------------  teardown ------------

	SDL_GL_DeleteContext(context);
	context = 0;

	SDL_DestroyWindow(window);
	window = NULL;

	return 0;
}



static GLuint compile_shader(GLenum type, std::string const &source) {
	GLuint shader = glCreateShader(type);
	GLchar const *str = source.c_str();
	GLint length = source.size();
	glShaderSource(shader, 1, &str, &length);
	glCompileShader(shader);
	GLint compile_status = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
	if (compile_status != GL_TRUE) {
		std::cerr << "Failed to compile shader." << std::endl;
		GLint info_log_length = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);
		std::vector< GLchar > info_log(info_log_length, 0);
		GLsizei length = 0;
		glGetShaderInfoLog(shader, info_log.size(), &length, &info_log[0]);
		std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
		glDeleteShader(shader);
		throw std::runtime_error("Failed to compile shader.");
	}
	return shader;
}

static GLuint link_program(GLuint fragment_shader, GLuint vertex_shader) {
	GLuint program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);
	GLint link_status = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &link_status);
	if (link_status != GL_TRUE) {
		std::cerr << "Failed to link shader program." << std::endl;
		GLint info_log_length = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);
		std::vector< GLchar > info_log(info_log_length, 0);
		GLsizei length = 0;
		glGetProgramInfoLog(program, info_log.size(), &length, &info_log[0]);
		std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
		throw std::runtime_error("Failed to link program");
	}
	return program;
}
