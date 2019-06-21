/**
 *  Animation.hpp
 *  ONScripter-RU
 *
 *  General image storage class.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Engine/Graphics/Common.hpp"
#include "Support/Camera.hpp"
#include "Support/Clock.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_gpu.h>

#include <utility>
#include <map>

#include <cstring>

const int TRANSBTN_CUTOFF = 1; //alpha threshold for ignoring transparent areas

enum {
	SPRITE_NONE = 0,
	SPRITE_LSP  = 0x1,
	SPRITE_LSP2 = 0x2,
	//SPRITE_SYSMENUTITLE = 0x4,
	SPRITE_BAR           = 0x8,
	SPRITE_PRNUM         = 0x10,
	SPRITE_BG            = 0x20,
	SPRITE_SENTENCE_FONT = 0x40,
	SPRITE_CURSOR        = 0x80,
	SPRITE_TACHI         = 0x100,
	//SPRITE_LOOKBACK = 0x200,
	SPRITE_BUTTONS = 0x400,
	SPRITE_ALL     = 0xFFF
};

struct SpriteIdentifier {
	SpriteIdentifier(int _no = -1, bool _lsp2 = false)
	    : no(_no), lsp2(_lsp2) {}
	int no;
	bool lsp2;
};

enum {
	FLIP_NONE         = 0,
	FLIP_HORIZONTALLY = 1,
	FLIP_VERTICALLY   = 2
};

struct FIT_MODE {
	enum {
		FIT_NONE   = 0,
		FIT_BOTTOM = 1,
		FIT_TOP    = 2,
		FIT_BOTH   = FIT_BOTTOM | FIT_TOP
	};
};

class GPUBigImage;

class AnimationInfo {
public:
	/* identification variables */
	int type{SPRITE_NONE};
	int id{0};
	std::map<int, SpriteIdentifier> childImages; //ordered by z-order
	bool exists{false};                          // this is here temporarily

	/* never copied */
	bool distinguish_from_old_ai{true};
	AnimationInfo *old_ai{nullptr};

	/* internal types */

	using ONSBuf = uint32_t;

	enum {
		TRANS_INVALID,
		TRANS_ALPHA,
		TRANS_TOPLEFT,
		TRANS_COPY,
		TRANS_STRING,
		TRANS_DIRECT,
		TRANS_PALETTE,
		TRANS_TOPRIGHT,
		TRANS_MASK,
		TRANS_LAYER
	};

	enum class ScrollSnap {
		NONE,
		TOP,
		BOTTOM
	};

	struct ScrollableInfo {
		bool isSpecialScrollable = false;
		bool respondsToMouseOver = true;
		bool respondsToClick     = true;
		int tightlyFit           = FIT_MODE::FIT_BOTH;
		bool hoverGradients      = true;
		bool normalGradients     = true;
		uchar3 hoverMultiplier{0xFF, 0xFF, 0xFF};
		uchar3 normalMultipler{0xFF, 0xFF, 0xFF};
		AnimationInfo *elementBackground = nullptr;
		AnimationInfo *divider           = nullptr;
		AnimationInfo *scrollbar         = nullptr;
		int scrollbarTop                 = 0;
		int scrollbarHeight              = 0;
		int elementTreeIndex             = 0;
		int totalHeight                  = 0;
		int firstMargin                  = 0;
		int lastMargin                   = 0;
		int columns                      = 1;
		int columnGap                    = 0;
		int elementWidth                 = 0;
		int elementHeight                = 0;
		int textMarginLeft               = 0;
		int textMarginRight              = 0;
		int textMarginTop                = 0;

		long layoutedElements = 0;
		long hoveredElement   = 0;
		long snappedElement   = 0;
		ScrollSnap snapType   = ScrollSnap::NONE;
		// always identical in the case of gamepad, not so for mouse
		// (mouse changes hoveredElement based on its position, scrolledToElement based on its wheel)
		bool mouseCursorIsOverHoveredElement = false; // when we move cursor outside an element, hoveredElement remains intact for gamepad's sake. This becomes false.
	};

	struct SpriteTransforms {
		bool sepia{false};
		bool negative1{false};
		bool negative2{false};
		bool greyscale{false};
		int blurFactor{0};
		int breakupFactor{0};
		int breakupDirectionFlagset{0};
		Clock warpClock;
		float warpSpeed{0};
		float warpWaveLength{1000};
		float warpAmplitude{0};
		bool hasNoneExceptMaybeBreakup() { // the name is ugly, but so is the change in behavior ;p
			return blurFactor == 0 &&      /*breakupFactor==0 &&*/
			       !greyscale && !sepia && !negative1 && !negative2 &&
			       !warpAmplitude && warpWaveLength == 1000;
		}
	};

	/* variables set from the image tag */
	int trans_mode{TRANS_COPY};
	uchar3 direct_color{0, 0, 0};
	uchar3 color{0, 0, 0};
	int num_of_cells{0};
	int current_cell{0};
	int direction{1};
	int *duration_list{nullptr};
	uchar3 *color_list{nullptr};
	int loop_mode{0};
	bool vertical_cells{false};
	bool is_animatable{false};
	bool skip_whitespace{false};
	int layer_no{-1}; //Mion: for Layer effects
	char *file_name{nullptr};
	char *lips_name{nullptr};
	char *mask_file_name{nullptr};
	BlendModeId blending_mode{BlendModeId::NORMAL};
	int font_size_xy[2]{0, 0}; // used by prnum and lsp string

	/* Variables from AnimationInfo */
	bool deferredLoading{false};
	bool stale_image{true}; //set to true when the image needs to be created/redone
	bool visible{false};
	bool abs_flag{true};

	bool has_z_order_override{false};
	int z_order_override;

	GPU_Rect orig_pos{0, 0, 0, 0}; //Mion: position and size of the image before resizing
	GPU_Rect pos{0, 0, 0, 0};      // position and size of the current cell
	GPU_Rect scrollable{0, 0, 0, 0};

	SpriteIdentifier parentImage;

	int trans{255};
	SDL_Color darkenHue{255, 255, 255, 255};
	int flip{FLIP_NONE};
	char *image_name{nullptr};

	// Normal sprite
	SDL_Surface *image_surface{nullptr};
	GPU_Image *gpu_image{nullptr};
	SpriteTransforms spriteTransforms;

	// Scrollable
	ScrollableInfo scrollableInfo;

	// BigImage
	bool is_big_image{false};
	std::shared_ptr<GPUBigImage> big_image;

	Clock clock;
	Camera camera;

	/* Variables for extended sprite (lsp2, drawsp2, etc.) */
	float scale_x{0}, scale_y{0};
	float rot{0};
	int mat[2][2]{{1024, 0}, {0, 1024}};
	float corner_xy[4][2]{};
	GPU_Rect bounding_rect{};

	bool has_hotspot{false};
	bool has_scale_center{false};
	float2 hotspot;          // The point within the image to be placed at (middle of script, bottom of script).
	float2 scale_center;     // The offset from the center of the image (or from the hotspot, if provided) to use as the center for scaling and rotate operations.
	float2 rendering_center; // (Computed) The location of the new image-center for images that have a rotation applied.

	int param;     // used by prnum and bar
	int max_param; // used by bar
	int max_width; // used by bar

	AnimationInfo() = default;
	AnimationInfo(const AnimationInfo &o);
	~AnimationInfo();

	AnimationInfo &operator=(const AnimationInfo &anim) = delete;
	void performCopyID(const AnimationInfo &o);             // id-dependent fields
	void performCopyNonImageFields(const AnimationInfo &o); // everything else (but images)
	void deepcopyNonImageFields(const AnimationInfo &o);
	void deepcopy(const AnimationInfo &o);

	void reset();

	void setImageName(const char *name);
	void deleteImage();
	void remove();
	void removeTag();
	void removeNonImageFields();

	bool proceedAnimation();
	uint64_t getDurationNanos(int i);
	int getDuration(int i);

	void setCell(int cell);
	float2 findOpaquePoint(GPU_Rect *clip = nullptr);
	int getPixelAlpha(int x, int y);

	void calcAffineMatrix(int script_width, int script_height);

	void calculateImage(int w, int h);
	void copySurface(SDL_Surface *surface, GPU_Rect *src_rect = nullptr, GPU_Rect *dst_rect = nullptr);
	void fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
	SDL_Surface *setupImageAlpha(SDL_Surface *surface, SDL_Surface *surface_m, bool has_alpha);
	void setImage(GPU_Image *image);
	void setSurface(SDL_Surface *surface);
	void setBigImage(GPUBigImage *image);

	/* For effects etc */
	void backupState();
	void commitState();
	AnimationInfo *oldNew(int refresh_mode);
};
