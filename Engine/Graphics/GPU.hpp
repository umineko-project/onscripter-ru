/**
 *  GPU.hpp
 *  ONScripter-RU
 *
 *  Contains higher level GPU abstraction.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Engine/Graphics/Common.hpp"
#include "Engine/Components/Base.hpp"
#include "Engine/Entities/Breakup.hpp"
#include "Support/Cache.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_gpu.h>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <stack>
#include <vector>
#include <iostream>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Leave these constants as they are for now
#ifndef GL_RGBA
#define GL_RGBA 0x1908
#endif

#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif

void dbgSaveImg(void *ptr);
void dbgSaveTgt(void *ptr);
void dbgSaveImgR(GPU_Image *ptr);
void dbgSaveTgtR(GPU_Target *ptr);

struct GPUImageDiff {
	int w{0}, h{0};
	GPU_FormatEnum format{GPU_FORMAT_RGBA};
	bool operator==(const GPUImageDiff &two) const;
};

namespace std {
template <>
struct hash<SDL_Point> {
	size_t operator()(const SDL_Point &x) const {
		return static_cast<size_t>(x.x) + (static_cast<size_t>(x.y) << 16);
	}
};
template <>
struct equal_to<SDL_Point> {
	bool operator()(const SDL_Point &one, const SDL_Point &two) const {
		return one.x == two.x && one.y == two.y;
	}
};
template <>
struct hash<GPUImageDiff> {
	size_t operator()(const GPUImageDiff &x) const {
		return static_cast<size_t>(x.w) + (static_cast<size_t>(x.h) << 16) + static_cast<size_t>(x.format);
	}
};
template <>
struct equal_to<GPUImageDiff> {
	bool operator()(const GPUImageDiff &one, const GPUImageDiff &two) const {
		return one == two;
	}
};
} // namespace std

class CombinedImagePool {
public:
	GPU_Image *get(int w, int h, int channels, bool store);
	CombinedImagePool(int size)
	    : existent(size) {}
	LRUCachedSet<Wrapped_GPU_Image, GPUImageDiff> existent;
	void init() {}
	void clear() {
		auto it = requested.begin();
		while (it != requested.end()) {
			GPU_FreeImage(*it);
			it = requested.erase(it);
		}

		existent.clear();
		if (access) {
			SDL_AtomicLock(&access);
			toDo.clear();
			SDL_AtomicUnlock(&access);
		}
	}
	void push(GPU_Rect &&rect) {
		SDL_AtomicLock(&access);
		toDo.push_back(rect);
		SDL_AtomicUnlock(&access);
	}
	bool generate();

private:
	SDL_SpinLock access{0};
	std::vector<GPU_Rect> toDo;
	std::vector<GPU_Image *> requested;
};

const std::array<GPU_BlendMode, static_cast<size_t>(BlendModeId::TOTAL)> BLEND_MODES{{//{GPU_FUNC_SRC_ALPHA, GPU_FUNC_ONE_MINUS_SRC_ALPHA, GPU_FUNC_SRC_ALPHA, GPU_FUNC_DST_ALPHA, GPU_EQ_ADD, GPU_EQ_ADD},
                                                                                      {GPU_FUNC_ONE, GPU_FUNC_ONE_MINUS_SRC_ALPHA, GPU_FUNC_ONE, GPU_FUNC_ONE_MINUS_SRC_ALPHA, GPU_EQ_ADD, GPU_EQ_ADD},
                                                                                      //Take care of alpha values, we need them for rain
                                                                                      {GPU_FUNC_SRC_ALPHA, GPU_FUNC_ONE, GPU_FUNC_SRC_ALPHA, GPU_FUNC_DST_ALPHA, GPU_EQ_ADD, GPU_EQ_ADD},
                                                                                      {GPU_FUNC_ONE, GPU_FUNC_ONE, GPU_FUNC_ONE, GPU_FUNC_ONE, GPU_EQ_SUBTRACT, GPU_EQ_SUBTRACT},
                                                                                      {GPU_FUNC_DST_COLOR, GPU_FUNC_ZERO, GPU_FUNC_SRC_ALPHA, GPU_FUNC_ONE_MINUS_SRC_ALPHA, GPU_EQ_ADD, GPU_EQ_ADD},
                                                                                      {GPU_FUNC_ZERO, GPU_FUNC_SRC_ALPHA, GPU_FUNC_ZERO, GPU_FUNC_SRC_ALPHA, GPU_EQ_ADD, GPU_EQ_ADD}}};

struct PooledGPUImage;
class TempGPUImagePool {
	std::unordered_map<GPU_Image *, bool> pool; // boolean = is this GPU_Image* "checked-out"?
public:
	SDL_Point size;
	GPU_Image *getImage();         // get a fresh temporary image
	void giveImage(GPU_Image *im); // return a temporary image to the pool for reuse
	void addImages(int n);         // pre-create some blank temporary images to avoid delays later
	void clearUnused(bool require_empty = false);
};

struct PooledGPUImage {
	GPU_Image *image{nullptr};
	TempGPUImagePool *pool{nullptr};
	PooledGPUImage() = default;
	PooledGPUImage(TempGPUImagePool *_pool)
	    : pool(_pool) {
		image = pool->getImage();
	}
	~PooledGPUImage() {
		if (image) {
			pool->giveImage(image);
		}
	}
	// Can't copy pooled gpu image containers
	PooledGPUImage(const PooledGPUImage &) = delete; // no copy
	PooledGPUImage &operator=(const PooledGPUImage &) = delete; // no assign
	// But you can move them
	PooledGPUImage(PooledGPUImage &&src) noexcept
	    : image(src.image), pool(src.pool) {
		src.image = nullptr;
		src.pool  = nullptr;
	}
	PooledGPUImage &operator=(PooledGPUImage &&src) noexcept {
		if (image && pool) {
			pool->giveImage(image);
		}
		image     = src.image;
		pool      = src.pool;
		src.image = nullptr;
		src.pool  = nullptr;
		return *this;
	}
};

class GPUTransformableCanvasImage {
	friend class GPUController;
	std::unordered_map<SDL_Point, PooledGPUImage> pooledDownscaledImages;

public:
	GPU_Image *image{nullptr};
	void setImage(GPU_Image *_canvas);
	void clearImage();
	GPUTransformableCanvasImage() {}
	GPUTransformableCanvasImage(GPU_Image *canvas)
	    : image(canvas) {}
};

class GPUImageChunkLoader {
	friend class GPUController;
	int x{0}, y{0}; // next-to-load chunk position in units of chunk size dimensions
public:
	SDL_Surface *src{nullptr};
	GPU_Rect *src_area{nullptr};
	GPU_Image *dst{nullptr};
	constexpr static uint32_t MinimumChunkDim{128};
	uint32_t chunkWidth{0};
	uint32_t chunkHeight{0};
	bool isLoaded{false};
	bool isActive{false};
	void loadChunk(bool finish);
};

class GPUBigImage {
	// Contains GPU_Images and their positions in an abstract image from top left to bottom right
	std::vector<Wrapped_GPU_Image> images;
	void create(SDL_Surface *surface = nullptr);

public:
	uint16_t w{0}, h{0};
	int channels{0};
	// Returns a vector of GPU_Images with their dst coordinates
	std::vector<std::pair<GPU_Image *, GPU_Rect>> getImagesForArea(GPU_Rect &area);
	bool has() {
		return w > 0 && h > 0;
	}
	GPUBigImage(SDL_Surface *surface);
	GPUBigImage(uint16_t w_, uint16_t h_, int channels_);
	GPUBigImage() = default;
};

// Generalized struct for easier batch blitting of triangles.
// This is NOT breakup specific -- don't put any breakup methods or data in here.
// Glass smash may also use it later.
struct TriangleBlitter {
	std::vector<float> vertices;
	std::vector<uint16_t> indices;
	GPU_Image *image{nullptr};
	GPU_Target *target{nullptr};
	int elementsPerVertex{4};
	GPU_BatchFlagEnum dataStructure{GPU_BATCH_XY_ST};
	static constexpr int maxVertices{60000};
	static constexpr int maxIndices{200000};
	uint16_t verticesInVertexBuffer{0};
	int verticesInIndexBuffer{0};
	bool fewerTriangles{false};

private:
	FORCE_INLINE void setTexturedVertex(float *vertices, uint16_t *indices, float s, float t, float x, float y, float z = 0) {
		float *ptr = vertices + verticesInVertexBuffer * elementsPerVertex;
		ptr[0]     = x;
		ptr[1]     = y;
		if (dataStructure == GPU_BATCH_XYZ_ST) {
			ptr[2] = z;
			ptr[3] = s;
			ptr[4] = t;
		} else {
			ptr[2] = s;
			ptr[3] = t;
		}
		if (indices) {
			indices[verticesInIndexBuffer++] = verticesInVertexBuffer;
		}
		verticesInVertexBuffer++;
	}

	FORCE_INLINE void setIndexedVertex(uint16_t *indices, uint16_t index) {
		indices[verticesInIndexBuffer] = index;
		verticesInIndexBuffer++;
	}

	// In these functions, s/t are the source texture coordinates and are always normalized to the size of the texture.
	void addEllipse(
	    float s, float t, float radius_s,
	    float radius_t, float x, float y, float radius_x,
	    float radius_y);

	void addTriangle(
	    float s1, float t1, float s2, float t2, float s3, float t3,
	    float x1, float y1, float z1, float x2, float y2, float z2,
	    float x3, float y3, float z3);

	void rotateCoordinates(
	    float coords[3][3],
	    float centerX, float centerY, float centerZ,
	    float yaw, float pitch, float roll);

public:
	// In these functions, xSrc/ySrc are normal pixel coordinates and are not normalized to the size of the texture.
	FORCE_INLINE void copyTriangle(
	    float xSrc1, float ySrc1, float xSrc2, float ySrc2, float xSrc3, float ySrc3,
	    float xDst, float yDst, float zDst = 0, float yaw = 0, float pitch = 0, float roll = 0) {
		float coords[3][3];

		coords[0][0] = xSrc1 + xDst;
		coords[0][1] = ySrc1 + yDst;
		coords[0][2] = zDst;

		coords[1][0] = xSrc2 + xDst;
		coords[1][1] = ySrc2 + yDst;
		coords[1][2] = zDst;

		coords[2][0] = xSrc3 + xDst;
		coords[2][1] = ySrc3 + yDst;
		coords[2][2] = zDst;

		if (yaw != 0 || pitch != 0 || roll != 0) {
			float centerX = (coords[0][0] + coords[1][0] + coords[2][0]) / 3.0;
			float centerY = (coords[0][1] + coords[1][1] + coords[2][1]) / 3.0;
			rotateCoordinates(coords, centerX, centerY, zDst, yaw, pitch, roll);
		}

		addTriangle(
		    xSrc1 / image->w, ySrc1 / image->h,
		    xSrc2 / image->w, ySrc2 / image->h,
		    xSrc3 / image->w, ySrc3 / image->h,
		    coords[0][0], coords[0][1], coords[0][2],
		    coords[1][0], coords[1][1], coords[1][2],
		    coords[2][0], coords[2][1], coords[2][2]);
	}

	FORCE_INLINE void copyCircle(
	    float xSrc, float ySrc, float radius, float xDst, float yDst, float resizeFactor = 1.0) {
		addEllipse(
		    xSrc / image->w, ySrc / image->h,
		    radius / image->w, radius / image->h,
		    xDst, yDst, radius * resizeFactor, radius * resizeFactor);
	}

	FORCE_INLINE void updateTargets(GPU_Image *src, GPU_Target *dst) {
		//Note, that we do not check vector sizes here
		image  = src;
		target = dst;
	}

	FORCE_INLINE void useFewerTriangles(bool arg = true) {
		fewerTriangles = arg;
	}

	void finish();
};

class ONScripter;
class GPUController : public BaseController {
private:
	TempGPUImagePool canvasImagePool, scriptImagePool;
	std::unordered_map<SDL_Point, TempGPUImagePool> typedImagePools;
	CombinedImagePool globalImagePool;

	/* {program: {uniform name: location}} */
	std::unordered_map<uint32_t, std::unordered_map<std::string, int>> uniformLocations;

public:
	int ownInit() override;
	int ownDeinit() override;

	std::unordered_map<std::string, uint32_t> shaders;
	std::unordered_map<std::string, uint32_t> programs;
	uint32_t currentProgram{0};
	std::stack<BlendModeId> blend_mode;
	bool texture_reuse{true};
	// Provides a way (if false) to reuse textures on some Intel machines
	bool use_glclear{true};
	// Provides a way to use texture atlas with VMware
	bool simulate_reads{false};
	// Provides a way to use new breakup / glass smash with added flushes on some Intel machines
	bool triangle_blit_flush{false};
	// Implements a speedhack by rendering to self at alpha multiplication and similar.
	// Violates GL/GLES standard but appears to work everywhere.
	// The only exception is ANGLE 33+: https://bugs.chromium.org/p/angleproject/issues/detail?id=496
	int render_to_self{-1};
	// Upper texture dimension limit (in pixels)
	int max_texture{0};
	// Upper chunk size limit (in bytes)
	int max_chunk{896 * 896 * 4};

	GPU_Image *loadGPUImageByChunks(SDL_Surface *s, GPU_Rect *r = nullptr);

	void setVirtualResolution(unsigned int width, unsigned int height);
	void setBlendMode(GPU_Image *image);
	void pushBlendMode(BlendModeId mode);
	void popBlendMode();

	void createShadersFromResources();
	bool isStandaloneShader(const char *text);
	std::vector<uint32_t> findAllLinkTargets(const char *text, size_t len);
	void createShader(const char *filename);
	void createProgramFromShaders(const char *programAlias, const char *frag, const char *vert);
	void createProgramFromShaders(const char *programAlias, std::vector<uint32_t> &targets);
	void linkProgram(const char *programAlias, uint32_t prog);

	//We are in need of a proper image loading that disables SDL_gpu blending...
	GPU_Image *createImage(uint16_t w, uint16_t h, uint8_t channels, bool store = false) {
		GPU_Image *image = globalImagePool.get(w, h, channels, store);
		//GPU_SetBlendMode(image, GPU_BLEND_OVERRIDE);
		if (image->snap_mode != GPU_SNAP_NONE)
			GPU_SetSnapMode(image, GPU_SNAP_NONE);
		return image;
	}

	GPU_Image *copyImage(GPU_Image *image) {
		GPU_SetSnapMode(image, GPU_SNAP_DIMENSIONS);
		GPU_Image *newImage = GPU_CopyImage(image);
		GPU_SetSnapMode(newImage, GPU_SNAP_NONE);
		GPU_SetSnapMode(image, GPU_SNAP_NONE);
		return newImage;
	}

	GPU_Image *copyImageFromTarget(GPU_Target *target) {
		GPU_Image *image = GPU_CopyImageFromTarget(target);
		GPU_SetSnapMode(image, GPU_SNAP_NONE);
		return image;
	}

	GPU_Image *copyImageFromSurface(SDL_Surface *surface) {
		GPU_Image *image = createImage(surface->w, surface->h, surface->format->BytesPerPixel == 4 ? 4 : 3);
		updateImage(image, nullptr, surface, nullptr);
		//GPU_SetBlendMode(image, GPU_BLEND_OVERRIDE);
		if (image->snap_mode != GPU_SNAP_NONE)
			GPU_SetSnapMode(image, GPU_SNAP_NONE);
		return image;
	}

	void clear(GPU_Target *target, uint8_t r = 0, uint8_t g = 0, uint8_t b = 0, uint8_t a = 0) {
		if (use_glclear) {
			GPU_ClearRGBA(target, r, g, b, a);
		} else {
			// Dodges a strange bug on certain hardware
			GPU_SetShapeBlending(false);
			SDL_Color color{r, g, b, a};
			GPU_Rect full{0, 0, static_cast<float>(target->w), static_cast<float>(target->h)};
			GPU_RectangleFilled2(target, full, color);
		}
	}

	void freeImage(GPU_Image *image) {
		if (!texture_reuse || !initialised() || image->refcount > 1 || (image->format != GPU_FORMAT_RGB && image->format != GPU_FORMAT_RGBA)) {
			GPU_FreeImage(image);
		} else {
			GPUImageDiff diff;
			diff.w      = image->w;
			diff.h      = image->h;
			diff.format = image->format;
			globalImagePool.existent.add(diff, std::make_shared<Wrapped_GPU_Image>(image));
		}
	}

	GPU_ShaderEnum getShaderTypeByExtension(const char *filename);
	void bindImageToSlot(GPU_Image *image, int slot_number);
	void multiplyAlpha(GPU_Image *image, GPU_Rect *dst_clip = nullptr);
	void mergeAlpha(GPU_Image *image, GPU_Rect *imageRect, GPU_Image *mask, GPU_Rect *maskRect, SDL_Surface *src);
	void enter3dMode();
	void exit3dMode();
	void setShaderProgram(const char *programAlias);
	void unsetShaderProgram();
	int32_t getUniformLoc(const char *name);
	void setShaderVar(const char *name, int value);
	void setShaderVar(const char *name, float value);
	void setShaderVar(const char *name, float value1, float value2);
	void setShaderVar(const char *name, const SDL_Color &color);
	void clearWholeTarget(GPU_Target *target, uint8_t r = 0, uint8_t g = 0, uint8_t b = 0, uint8_t a = 0);
	void copyGPUImage(GPU_Image *img, GPU_Rect *src_rect, GPU_Rect *clip_rect, GPU_Target *target, float x = 0, float y = 0, float ratio_x = 1, float ratio_y = 1, float angle = 0, bool centre_coordinates = false);
	void copyGPUImage(GPU_Image *img, GPU_Rect *src_rect, GPU_Rect *clip_rect, GPUBigImage *bigImage, float x = 0, float y = 0);
	void updateImage(GPU_Image *image, const GPU_Rect *image_rect, SDL_Surface *surface, const GPU_Rect *surface_rect, bool finish = true);
	void convertNV12ToRGB(GPU_Image *image, GPU_Image **imgs, GPU_Rect &rect, uint8_t *planes[4], int *linesizes, bool masked);
	void convertYUVToRGB(GPU_Image *image, GPU_Image **imgs, GPU_Rect &rect, uint8_t *planes[4], int *linesizes, bool masked);
	void simulateRead(GPU_Image *img);

	struct GPURendererInfo {
		const char *name{nullptr};
		GPU_RendererID (GPUController::*makeRendererId)(){nullptr};
		void (GPUController::*initRendererFlags)(){nullptr};
		int (GPUController::*getImageFormat)(GPU_Image *image){nullptr};
		void (GPUController::*printBlitBufferState)(){nullptr};
		void (GPUController::*syncRendererState)(){nullptr};
		int (GPUController::*getMaxTextureSize)(){nullptr};
		bool mobile{false};
		int formatRGBA{GL_RGBA};
		int formatBGRA{GL_BGRA};
	};

	GPU_Target *rendererInit(GPU_WindowFlagEnum SDL_flags);
	GPU_Target *rendererInitWithInfo(GPURendererInfo &info, uint16_t w, uint16_t h, GPU_WindowFlagEnum SDL_flags);

	GPU_RendererID makeRendererIdGLES2();
	void initRendererFlagsGLES2();
	int getImageFormatGLES2(GPU_Image *image);
	void printBlitBufferStateGLES2();
	void syncRendererStateGLES2();
	int getMaxTextureSizeGLES2();

	GPU_RendererID makeRendererIdGLES3();
	void initRendererFlagsGLES3();
	int getImageFormatGLES3(GPU_Image *image);
	void printBlitBufferStateGLES3();
	void syncRendererStateGLES3();
	int getMaxTextureSizeGLES3();

	GPU_RendererID makeRendererIdANGLE2();
	void initRendererFlagsANGLE2();
	int getImageFormatANGLE2(GPU_Image *image);
	void printBlitBufferStateANGLE2();
	void syncRendererStateANGLE2();
	int getMaxTextureSizeANGLE2();

	GPU_RendererID makeRendererIdANGLE3();
	void initRendererFlagsANGLE3();
	int getImageFormatANGLE3(GPU_Image *image);
	void printBlitBufferStateANGLE3();
	void syncRendererStateANGLE3();
	int getMaxTextureSizeANGLE3();

	GPU_RendererID makeRendererIdGL2();
	void initRendererFlagsGL2();
	int getImageFormatGL2(GPU_Image *image);
	void printBlitBufferStateGL2();
	void syncRendererStateGL2();
	int getMaxTextureSizeGL2();

#if defined(DROID) || defined(IOS)
	// This is generally too harsh on iOS
	static constexpr size_t GlobalImagePoolSize{10};
#else
	static constexpr size_t GlobalImagePoolSize{20};
#endif

#if defined(DROID) || defined(IOS)
	GPURendererInfo renderers[2]{
	    {"GLES2",
	     &GPUController::makeRendererIdGLES2,
	     &GPUController::initRendererFlagsGLES2,
	     &GPUController::getImageFormatGLES2,
	     &GPUController::printBlitBufferStateGLES2,
	     &GPUController::syncRendererStateGLES2,
	     &GPUController::getMaxTextureSizeGLES2,
	     true},
	    {
		    "GLES3",
		    &GPUController::makeRendererIdGLES3,
		    &GPUController::initRendererFlagsGLES3,
		    &GPUController::getImageFormatGLES3,
		    &GPUController::printBlitBufferStateGLES3,
		    &GPUController::syncRendererStateGLES3,
		    &GPUController::getMaxTextureSizeGLES3,
		    true
	    }};
#elif defined(WIN32)
	GPURendererInfo renderers[3]{
	    {"GL2",
	     &GPUController::makeRendererIdGL2,
	     &GPUController::initRendererFlagsGL2,
	     &GPUController::getImageFormatGL2,
	     &GPUController::printBlitBufferStateGL2,
	     &GPUController::syncRendererStateGL2,
	     &GPUController::getMaxTextureSizeGL2,
	     false},
	    {"ANGLE2",
	     &GPUController::makeRendererIdANGLE2,
	     &GPUController::initRendererFlagsANGLE2,
	     &GPUController::getImageFormatANGLE2,
	     &GPUController::printBlitBufferStateANGLE2,
	     &GPUController::syncRendererStateANGLE2,
	     &GPUController::getMaxTextureSizeANGLE2,
	     true},
	    {
		    "ANGLE3",
		    &GPUController::makeRendererIdANGLE3,
		    &GPUController::initRendererFlagsANGLE3,
		    &GPUController::getImageFormatANGLE3,
		    &GPUController::printBlitBufferStateANGLE3,
		    &GPUController::syncRendererStateANGLE3,
		    &GPUController::getMaxTextureSizeANGLE3,
		    true
	    }};
#else
	GPURendererInfo renderers[1]{
	    {"GL2",
	     &GPUController::makeRendererIdGL2,
	     &GPUController::initRendererFlagsGL2,
	     &GPUController::getImageFormatGL2,
	     &GPUController::printBlitBufferStateGL2,
	     &GPUController::syncRendererStateGL2,
	     &GPUController::getMaxTextureSizeGL2,
	     false}};
#endif

	GPURendererInfo *current_renderer{nullptr};

	PooledGPUImage getBlurredImage(GPUTransformableCanvasImage &im, int blurFactor);
	PooledGPUImage getMaskedImage(GPUTransformableCanvasImage &im, GPU_Image *mask);

	void breakUpImage(BreakupID id, GPU_Image *src, GPU_Rect *src_rect, GPU_Target *target, int breakupFactor,
	                  int breakupDirectionFlagset, const char *params, float dstX, float dstY);
	PooledGPUImage getBrokenUpImage(GPUTransformableCanvasImage &im, BreakupID id,
	                                int breakupFactor, int breakupDirectionFlagset,
	                                const char *params);
	void drawUnbrokenBreakupRegions(BreakupID id, float dstX, float dstY);

	void glassSmashImage(GPU_Image *src, GPU_Target *dst, int smashFactor);
	PooledGPUImage getGlassSmashedImage(GPUTransformableCanvasImage &im, int smashFactor);

	PooledGPUImage getWarpedImage(GPUTransformableCanvasImage &im, float animationClock, float amplitude, float waveLength, float speed);
	PooledGPUImage getGreyscaleImage(GPUTransformableCanvasImage &im, const SDL_Color &color);
	PooledGPUImage getSepiaImage(GPUTransformableCanvasImage &im);
	PooledGPUImage getNegativeImage(GPUTransformableCanvasImage &im);
	PooledGPUImage getPixelatedImage(GPUTransformableCanvasImage &im, int factor);

	FORCE_INLINE TriangleBlitter createTriangleBlitter(GPU_Image *image, GPU_Target *target) {
		TriangleBlitter res;
		res.image  = image;
		res.target = target;
		//res.elementsPerVertex = 4;
		//res.dataStructure = GPU_BATCH_XY_ST;
		res.elementsPerVertex = 5;
		res.dataStructure     = GPU_BATCH_XYZ_ST;
		res.vertices.resize(res.elementsPerVertex * res.maxVertices);
		res.indices.resize(res.maxIndices);
		return res;
	}

	void clearImage(GPUTransformableCanvasImage *im);

	PooledGPUImage getPooledImage(int w = -1, int h = -1);

	void scheduleLoadImage(int width, int height) {
		// Do not load large images, as they are to be loaded via GPUBigImage.
		if (width <= max_texture && height <= max_texture)
			globalImagePool.push({0, 0, static_cast<float>(width), static_cast<float>(height)});
	}

	bool handleScheduledJobs() {
		return globalImagePool.generate();
	}

	void clearImagePools(bool require_empty=false) {
		scriptImagePool.clearUnused(require_empty);
		canvasImagePool.clearUnused(require_empty);
		typedImagePools.clear();
		globalImagePool.clear();
	}

	GPU_Image *getCanvasImage() { return canvasImagePool.getImage(); }
	void giveCanvasImage(GPU_Image *im) { canvasImagePool.giveImage(im); }
	GPU_Image *getScriptImage() { return scriptImagePool.getImage(); }
	void giveScriptImage(GPU_Image *im) { scriptImagePool.giveImage(im); }

	GPUController()
	    : BaseController(this), globalImagePool(GlobalImagePoolSize) {}
};

extern GPUController gpu;
