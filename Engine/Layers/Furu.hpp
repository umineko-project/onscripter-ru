/**
 *  Furu.hpp
 *  ONScripter-RU
 *
 *  Emulation of Takashi Toyama's "snow.dll" and "hana.dll" NScripter plugin filters.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Engine/Layers/Layer.hpp"

const int N_FURU_ELEMENTS{3};
const int N_FURU_SNOW_ELEMENTS{11};
const int N_FURU_DISTR[N_FURU_ELEMENTS]{10, 8, 0};

const int FURU_RU_WINTER_FACTOR{64};
const int FURU_ELEMENT_BUFSIZE{512 * FURU_RU_WINTER_FACTOR}; // should be a power of 2
const int FURU_AMP_TABLE_SIZE{256 * FURU_RU_WINTER_FACTOR};  // should also be power of 2, it helps

class FuruLayer : public Layer {
public:
	FuruLayer(int w, int h, bool animated, BaseReader **br = nullptr);
	~FuruLayer() override;
	bool update(bool /*old*/) override;
	char *message(const char *message, int &ret_int) override;
	void refresh(GPU_Target *target, GPU_Rect &clip, float x, float y, bool centre_coordinates, int /*rm*/, float scalex = 1.0, float scaley = 1.0) override;
	BlendModeId blendingMode(int /*rm*/) override {
		return BlendModeId::ADD;
	}

private:
	bool tumbling; // true (hana) or false (snow)

	// message parameters
	int interval;      // 1 ~ 10000; # frames between a new element release
	int fall_velocity; // 1 ~ screen_height; pix/frame
	int wind;          // -script_width/2 ~ script_width/2; pix/frame
	int amplitude;     // 0 ~ script_width/2; pix/frame
	int freq;          // 0 ~ 359; degree/frame
	int angle;
	bool paused, halted;

	struct OscPt { // point plus base oscillation angle
		int base_angle;
		Pt pt;
	};
	struct Element {
		AnimationInfo *sprite;
		int *amp_table;
		// rolling buffer
		OscPt *points;
		int pstart, pend, frame_cnt, fall_speed;
		Element() {
			sprite    = nullptr;
			amp_table = nullptr;
			points    = nullptr;
			pstart = pend = frame_cnt = fall_speed = 0;
		}
		~Element() {
			delete sprite;
			delete[] amp_table;
			delete[] points;
		}
		void init() {
			if (!points)
				points = new OscPt[FURU_ELEMENT_BUFSIZE];
			pstart = pend = frame_cnt = 0;
		}
		void clear() {
			freevar(&sprite);
			freearr(&amp_table);
			freearr(&points);
			pstart = pend = frame_cnt = 0;
		}
		void setSprite(AnimationInfo *anim) {
			delete sprite;
			sprite = anim;
		}
	} elements[N_FURU_ELEMENTS];
	int max_sp_w;

	bool initialized;

	void furu_init();
	void validate_params();
	void buildAmpTables();
};
