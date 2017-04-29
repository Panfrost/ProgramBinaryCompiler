#include <array>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <EGL/egl.h>
#define GL_GLES_PROTOTYPES
#include <GLES3/gl3.h>
#include <GLES3/gl31.h>

EGLDisplay dpy;
EGLContext ctx;
enum ShaderType
{
	TYPE_VERTEX,
	TYPE_FRAGMENT,
	TYPE_COMPUTE,
	TYPE_MERGED,
};

GLuint compileShader(GLuint type, const std::string& source)
{
	GLuint result = glCreateShader(type);

	const char* src[] = { source.c_str() };

	glShaderSource(result, 1, src, nullptr);
	glCompileShader(result);
	GLint compileStatus;
	glGetShaderiv(result, GL_COMPILE_STATUS, &compileStatus);
	GLsizei length = 0;
	glGetShaderiv(result, GL_INFO_LOG_LENGTH, &length);

	if (compileStatus != GL_TRUE)
	{
		std::string info_log;
		info_log.resize(length);
		glGetShaderInfoLog(result, length, &length, &info_log[0]);

		const char* prefix = "";
		switch (type)
		{
		case GL_VERTEX_SHADER:
			prefix = "vs";
			break;
		case GL_FRAGMENT_SHADER:
			prefix = "ps";
			break;
		case GL_COMPUTE_SHADER:
			prefix = "cs";
			break;

		}

		printf("%s Shader info log:\n%s\n", prefix, info_log.c_str());
	}
	return result;
}

void outputBinary(std::vector<uint8_t>& binary, char* output)
{
	FILE* fp;
	fp = fopen(output, "wb");
	for (auto& it : binary)
	{
		fputc(it, fp);
	}
	fflush(fp);
	fclose(fp);
}

void readShader(std::string& output, char* input)
{
	std::fstream fs;

	fs.open(input);

	if (!fs.is_open())
	{
		printf("Couldn't open file '%s'\n", input);
		return;
	}

	fs >> output;
	fs.seekg(0, std::ios::end);
	output.reserve(fs.tellg());
	fs.seekg(0, std::ios::beg);

	output.assign(std::istreambuf_iterator<char>(fs),
	              std::istreambuf_iterator<char>());

	fs.close();
}

void compileShaderProgram(ShaderType type, char* input, char* output)
{
	std::string defaultcode =
	R"(
		#version 320 es
		void main()
		{
		}
	)";

	std::string vcode;
	std::string pcode;
	std::string bulk;
	std::string *realshader;

	switch (type)
	{
	case ShaderType::TYPE_VERTEX:
		pcode = defaultcode;
		realshader = &vcode;
		break;
	case ShaderType::TYPE_FRAGMENT:
		vcode = defaultcode;
		realshader = &pcode;
		break;
	case ShaderType::TYPE_COMPUTE:
		realshader = &pcode;
		break;
	case ShaderType::TYPE_MERGED:
		realshader = &bulk;
		break;
	}

	readShader(*realshader, input);

	GLuint pid = glCreateProgram();
	GLuint vsid = 0, psid = 0, csid = 0;

	if (type == ShaderType::TYPE_COMPUTE)
	{
		csid = compileShader(GL_COMPUTE_SHADER, pcode);
		glAttachShader(pid, csid);
	}
	else
	{
		if (type == ShaderType::TYPE_MERGED)
		{
			uint32_t i = 0;
			std::string::size_type prev_pos = 0, pos = 0;
			std::array<std::string*, 2> str_ptrs =
			{
				&vcode, &pcode
			};
			std::string token { "<<<<<" };

			while((pos = bulk.find(token, pos)))
			{
				std::string substring( bulk.substr(prev_pos, pos-prev_pos) );
				str_ptrs[i]->append(substring);

				i++;
				pos += token.size();
				prev_pos = pos;
				if (i >= str_ptrs.size())
					break;
			}
		}

		vsid = compileShader(GL_VERTEX_SHADER, vcode);
		psid = compileShader(GL_FRAGMENT_SHADER, pcode);

		glAttachShader(pid, vsid);
		glAttachShader(pid, psid);
	}
	glProgramParameteri(pid, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);

	glLinkProgram(pid);

	// original shaders aren't needed any more
	glDeleteShader(vsid);
	glDeleteShader(psid);
	glDeleteShader(csid);

	GLint linkStatus;
	glGetProgramiv(pid, GL_LINK_STATUS, &linkStatus);
	GLsizei length = 0;
	glGetProgramiv(pid, GL_INFO_LOG_LENGTH, &length);
	if (linkStatus != GL_TRUE)
	{
		std::string info_log;
		info_log.resize(length);
		glGetProgramInfoLog(pid, length, &length, &info_log[0]);
		printf("Program info log:\n%s\n", info_log.c_str());
	}

      GLint link_status = GL_FALSE, delete_status = GL_TRUE, binary_size = 0;
      glGetProgramiv(pid, GL_PROGRAM_BINARY_LENGTH, &binary_size);
	printf("Ended up with program binary length of %d\n", binary_size);
	GLenum err;
      if ((err = glGetError()) != GL_NO_ERROR || !binary_size)
      {
		printf("ERROR! 0x%04x\n", err);
		return;
      }

	std::vector<uint8_t> data(binary_size);
	GLenum format;
	glGetProgramBinary(pid, binary_size, nullptr, &format, &data[0]);
	if (glGetError() != GL_NO_ERROR)
	{
		printf("Failed to get program binary!\n");
		return;
	}

	outputBinary(data, output);
}

int main(int argc, char** argv)
{
	if (argc < 4)
	{
		printf("Usage: %s {-v, -f, -c, -fv} <input.txt> <output.bin>\n", argv[0]);
		return 0;
	}
	EGLint egl_major, egl_minor;

	dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	eglInitialize(dpy, &egl_major, &egl_minor);

	/* Detection code */
	EGLint num_configs;
	EGLConfig config;

	int attribs[] =
	{
		EGL_RENDERABLE_TYPE,
		(1 << 6), /* ES3 bit */
		EGL_NONE
	};

	if (!eglChooseConfig(dpy, attribs, &config, 1, &num_configs))
	{
		printf("Error: couldn't get an EGL visual config\n");
		return 0;
	}


	eglBindAPI(EGL_OPENGL_ES_API);

	EGLint ctx_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};

	ctx = eglCreateContext(dpy, config, nullptr, ctx_attribs);
	if (!eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx))
	{
		printf("ERROR: Couldn't make current\n");
	}

	printf("EGL_VENDOR '%s'\n", eglQueryString(dpy, EGL_VENDOR));
	printf("GL_VENDOR '%s'\n", glGetString(GL_VENDOR));
	printf("GL_VERSION '%s'\n", glGetString(GL_VERSION));
	printf("GL_RENDERER '%s'\n", glGetString(GL_RENDERER));

	std::string type_string = argv[1];
	ShaderType type = ShaderType::TYPE_COMPUTE;

	if (!strncmp(argv[1], "-v", 2))
	{
		type = ShaderType::TYPE_VERTEX;
	}
	else if (!strncmp(argv[1], "-fv", 3))
	{
		type = ShaderType::TYPE_MERGED;
	}
	else if (!strncmp(argv[1], "-f", 2))

	{
		type = ShaderType::TYPE_FRAGMENT;
	}
	else if (!strncmp(argv[1], "-c", 2))
	{
		type = ShaderType::TYPE_COMPUTE;
	}
	else
	{
		printf("Unknown shader type. Falling back to compute!\n");
		type = ShaderType::TYPE_COMPUTE;
	}

	compileShaderProgram(type, argv[2], argv[3]);

	eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglTerminate(dpy);
	return 0;
}
