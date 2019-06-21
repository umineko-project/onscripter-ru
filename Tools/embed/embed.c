/**
 *  embed.c
 *  ONScripter-RU
 *
 *  A simple^W program to create Resources.cpp
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

#define GLES_IFDEF "#if defined(IOS) || defined(DROID) || defined(WIN32)"

enum comment_type {
	CStyleComment,
	CXXStyleComment,
	NoComment = -1
};

enum glsl_verify_result {
	OK,
	WARN,
	ERR
};

static enum glsl_verify_result verify_glsl(const char *src) {
	const char *ver = strstr(src, "#version");
	if (!ver) {
		fprintf(stderr, "ERR: No #version directive\n");
		return ERR;
	}
	if (ver + strlen("#version XYZ") > src + strlen(src)) {
		fprintf(stderr, "ERR: past EOF version directive\n");
		return ERR;
	}

	/* Verify tokens before version directive */
	if (ver != src) {
		enum comment_type ctype = NoComment;

		for (const char *c = src; c < ver; c++) {
			/* Terminate comment if open */
			if (*c == '\n' && ctype == CXXStyleComment) {
				ctype = NoComment;
				c++;
				continue;
			}
			if (*c == '*' && c[1] == '/' && ctype == CStyleComment) {
				ctype = NoComment;
				c++;
				continue;
			}

			if (isspace(*c)) {
				continue;
			}

			/* Open comment if not yet */
			if (*c == '/' && c[1] == '/' && ctype == NoComment) {
				ctype = CXXStyleComment;
				c++;
				continue;
			}
			if (*c == '/' && c[1] == '*' && ctype == NoComment) {
				ctype = CStyleComment;
				c++;
				continue;
			}

			/* Any token that doesn't terminate comment */
			if (ctype != NoComment) {
				continue;
			}

			/* Any token not in comment */
			fprintf(stderr, "WARN: Extraneous tokens before #version directive; it will default "
			                "to GLSL 110\n");
			return WARN;
		}
	}

	return OK;
}

/* Preprocess glsl shader for gles compat. */
static char *preprocess_gles(const char *src) {
	const char *specifiers[] = {
	    "#version 100\nprecision mediump float;\nprecision mediump int;",
	    "#version 100\nprecision highp float;\nprecision highp int;"};

	int precision = 0;
	// We use this notation instead of normal #pragma because ANGLE 43 and 44 boil with an error on them :/
	if (strstr(src, "//PRAGMA: ONS_RU highprecision")) {
		precision = 1;
	}

	size_t len = strlen(specifiers[precision]);

	char *dst = (char *)(calloc(1, strlen(src) + strlen(specifiers[precision]) + 1));
	if (!dst) {
		abort();
	}

	const char *ver = strstr(src, "#version ");

	memcpy(dst, src, ver - src);
	memcpy(&dst[ver - src], specifiers[precision], len);

	const char *endver = strchr(ver, '\n');
	strncpy(&dst[ver - src + len], endver, strlen(src) - (endver - src));

	const char *highpfind = "/* PRAGMA: ONS_RU highprecision */";
	const char *highprepl = " highp                            ";
	char *find;
	while ((find = strstr(dst, highpfind)) != NULL)
		memcpy(find, highprepl, strlen(highprepl));

	return dst;
}

static void gen_array(const char *buf, FILE *dst, size_t idx, size_t len, bool gles, bool terminate) {
	if (gles) {
		fprintf(dst, "\n" GLES_IFDEF);
	}
	fprintf(dst, "\nstatic const uint8_t resource_%zu", idx);
	if (gles) {
		fprintf(dst, "_gles");
	}
	fprintf(dst, "_buffer[] = {\n\t");
	for (const char *c = buf; c < buf + len; c++) {
		fprintf(dst, "%3u, ", *(unsigned char *)c);
		if ((c - buf + 1) % 16 == 0) {
			fprintf(dst, "\n\t");
		}
	}
	if (terminate) {
		fprintf(dst, "0");
	}
	fprintf(dst, "};\n");
	if (gles) {
		fprintf(dst, "#endif\n");
	}
}

int main(int argc, const char *const argv[]) {
	if (argc == 2 && !strcmp(argv[1], "--version")) {
		printf("Resource embed v0.1\n");
		return 0;
	}

	if (argc < 3) {
		fprintf(stderr, "Usage: embed input1 internalname1 [input2...] [outfile]\n"
		                "Shaders are assumed to be in OpenGL GLSL format\n");
		return -1;
	}

	FILE *fout;
	if (argc % 2 == 0) { /* Odd number of effective args: path -> name ... outfile */
		fout = fopen(argv[argc - 1], "wb");
		if (!fout) {
			fprintf(stderr, "Failed to open %s for writing\n", argv[argc - 1]);
			return -1;
		}
		argc--;
	} else {
		fout = stdout;
	}

	fprintf(fout, "/**\n"
	              " *  Resources.cpp\n"
	              " *  ONScripter-RU\n"
	              " *\n"
	              " *  Generated file - do not edit!!!\n"
	              " *\n"
	              " *  Consult LICENSE file for licensing terms and copyright holders.\n"
	              " */\n\n"
	              "#include \"Resources/Support/Resources.hpp\"\n\n"
	              "#include <cstring>\n"
	              "#include <cstdint>\n");

	size_t last = (argc % 2 == 0 ? argc - 2 : argc - 1);

	for (size_t i = 1; i <= last; i += 2) {
		printf("Embedding: %s -> %s\n", argv[i], argv[i + 1]);
		char path[FILENAME_MAX];
		snprintf(path, FILENAME_MAX, "%s", argv[i]);
		FILE *fres            = fopen(path, "rb");
		bool isSeparate       = false;
		const char *shaderExt = strstr(argv[i], ".frag");
		if (!shaderExt) {
			shaderExt = strstr(argv[i], ".vert");
		}

		if (!fres && shaderExt) {
			snprintf(path, FILENAME_MAX, "%.*s.gl%s", (int)(shaderExt - argv[i]), argv[i], shaderExt);
			fres       = fopen(path, "rb");
			isSeparate = true;
		}

		if (!fres) {
			fprintf(stderr, "Failed to open %s for embedding\n", path);
			return -1;
		}

		char *resBuf;
		fseek(fres, 0, SEEK_END);
		long sz = ftell(fres);

		fseek(fres, 0, SEEK_SET);
		resBuf = (char *)(calloc(1, sz + 1));
		if (!resBuf) {
			abort();
		}
		fread(resBuf, sz, 1, fres);
		fclose(fres);
		resBuf[sz] = 0;

		char *glesBuf = NULL;
		if (shaderExt) {
			enum glsl_verify_result res = verify_glsl(resBuf);
			if (res != OK) {
				fprintf(stderr, "\tin shader %s\n", argv[i]);
				if (res == ERR) {
					return -1;
				}
			}
			if (isSeparate) {
				char path[FILENAME_MAX];
				snprintf(path, FILENAME_MAX, "%.*s.gles%s", (int)(shaderExt - argv[i]), argv[i], shaderExt); //-V755
				fres = fopen(path, "rb");
				if (!fres) {
					fprintf(stderr, "Failed to open %s for embedding\n", path);
					return -1;
				}
				fseek(fres, 0, SEEK_END);
				long sz = ftell(fres);

				fseek(fres, 0, SEEK_SET);
				glesBuf = (char *)(calloc(1, sz + 1));
				if (!glesBuf) {
					abort();
				}
				fread(glesBuf, sz, 1, fres);
				fclose(fres);
				glesBuf[sz] = 0;
			}

			if (!glesBuf) {
				glesBuf = preprocess_gles(resBuf);
			}
		}
		size_t idx = (i - 1) / 2;
		gen_array(resBuf, fout, idx, sz, false, true);
		if (glesBuf) {
			gen_array(glesBuf, fout, idx, strlen(glesBuf), true, true);
		}
		fprintf(fout, "static const char filename_%zu[] = \"%s\";\n", idx, argv[i + 1]);
		if (glesBuf) {
			fprintf(fout, "\n" GLES_IFDEF "\n");
			fprintf(fout, "static struct InternalResource resource_%zu_gles = ", idx);
			fprintf(fout, "{filename_%zu, resource_%zu_gles_buffer, %zu, NULL};\n", idx, idx, strlen(glesBuf));
			fprintf(fout, "#endif\n");
		}
		fprintf(fout, "static struct InternalResource resource_%zu = ", idx);
		fprintf(fout, "{filename_%zu, resource_%zu_buffer, %ld, ", idx, idx, sz);
		if (glesBuf) {
			fprintf(fout, "\n" GLES_IFDEF "\n"
			              "&resource_%zu_gles\n"
			              "#else\n"
			              "NULL\n"
			              "#endif\n"
			              "};\n",
			        idx);
		} else {
			fprintf(fout, "NULL};\n");
		}
		free(glesBuf);
		free(resBuf);
	}

	/* gles InternalResource's are not on the list */
	fprintf(fout, "static const InternalResource resources[] = {");
	for (size_t i = 1; i <= last; i += 2) {
		fprintf(fout, "resource_%zu, ", (i - 1) / 2);
		if (i % 8 == 0) {
			fprintf(fout, "\n\t");
		}
	}
	fprintf(fout, "{NULL, NULL, 0, NULL}};\n");

	fprintf(fout, "const InternalResource *getResourceList() {\n"
	              "\treturn resources;\n"
	              "}\n");
	fprintf(fout, "const InternalResource *getResource(const char *filename, bool mobile) {\n"
	              "\t(void)mobile;\n"
	              "\tfor (size_t i = 0; i < sizeof(resources) / sizeof(*resources) - 1; i++)\n"
	              "\t\tif (!std::strcmp(resources[i].filename, filename))\n" GLES_IFDEF "\n"
	              "\t\t\treturn mobile && resources[i].glesVariant ? resources[i].glesVariant : &resources[i];\n"
	              "#else\n"
	              "\t\t\treturn &resources[i];\n"
	              "#endif\n"
	              "\treturn NULL;\n"
	              "}\n");
	if (fout != stdout) {
		fclose(fout);
	}
	return 0;
}
