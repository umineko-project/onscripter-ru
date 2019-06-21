/**
 *  Furu.cpp
 *  ONScripter-RU
 *
 *  Emulation of Takashi Toyama's "snow.dll" and "hana.dll" NScripter plugin filters.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Layers/Furu.hpp"
#include "Engine/Graphics/GPU.hpp"
#include "Engine/Readers/Base.hpp"
#include "Support/FileIO.hpp"

const float FURU_RATE_COEF = 0.2;

static float *base_disp_table = nullptr;
static int furu_count         = 0;
//const float fall_mult[N_FURU_ELEMENTS] = {0.9, 0.7, 0.6};
const float fall_mult[N_FURU_ELEMENTS] = {0.9, 0.7, 0.25};

static void buildBaseDispTable() {
	if (base_disp_table)
		return;

	base_disp_table = new float[FURU_AMP_TABLE_SIZE];
	// a = sin? * Z(cos?)
	// Z(z) = rate_z * z +1
	for (int i = 0; i < FURU_AMP_TABLE_SIZE; ++i) {
		float rad          = static_cast<float>(i) * M_PI * 2 / FURU_AMP_TABLE_SIZE;
		base_disp_table[i] = std::sin(rad) * (FURU_RATE_COEF * std::cos(rad) + 1);
	}
}

FuruLayer::FuruLayer(int w, int h, bool animated, BaseReader **br) {
	width    = w;
	height   = h;
	tumbling = animated;
	reader   = br;

	interval = fall_velocity = wind = amplitude = freq = angle = 0;
	paused = halted = false;
	max_sp_w        = 0;

	sprite = nullptr;

	initialized = false;
}

FuruLayer::~FuruLayer() {
	if (initialized) {
		--furu_count;
		if (furu_count == 0) {
			delete[] base_disp_table;
			base_disp_table = nullptr;
		}
	}
}

void FuruLayer::furu_init() {
	for (auto &element : elements) {
		element.init();
	}
	angle  = 0;
	halted = false;
	paused = false;

	buildBaseDispTable();

	++furu_count;
	initialized = true;
}

bool FuruLayer::update(bool /*old*/) {
	if (initialized && !paused) {
		if (amplitude != 0)
			angle = (angle - freq + FURU_AMP_TABLE_SIZE) % FURU_AMP_TABLE_SIZE;
		int position = 0;
		for (int j = 0; j < (tumbling ? N_FURU_ELEMENTS : N_FURU_SNOW_ELEMENTS); ++j) {
			Element *cur = nullptr;
			if (tumbling) {
				cur = &elements[j];
				position++;
			} else {
				cur = &elements[0];
				for (position = 0; position < N_FURU_ELEMENTS; position++) {
					if (j >= N_FURU_DISTR[position]) {
						cur = &elements[position];
						break;
					}
				}
			}
			const int virt_w = width + max_sp_w;
			//sendToLog(LogLevel::Info, "Furulayer@virt_w %d\n", virt_w);

			if (tumbling || j == N_FURU_DISTR[position]) {
				int i = cur->pstart;
				while (i != cur->pend) {
					cur->points[i].pt.x = (cur->points[i].pt.x + wind + virt_w) % virt_w;
					cur->points[i].pt.y += cur->fall_speed;
					cur->points[i].pt.cell = (1 + cur->points[i].pt.cell) % cur->sprite->num_of_cells;
					i                      = (1 + i) % FURU_ELEMENT_BUFSIZE;
				}
			}

			if (!halted) {
				if (--cur->frame_cnt <= 0) {
					const int tmp = (cur->pend + 1) % FURU_ELEMENT_BUFSIZE;
					cur->frame_cnt += interval;
					if (tmp != cur->pstart) {
						// add a point for this element
						OscPt *item = &cur->points[cur->pend];
						// hana.dll wants grouping
						if (tumbling) {
							if (position == 1) {
								item->pt.x = std::rand() % (virt_w / 3);
							} else if (position == 2) {
								item->pt.x = std::rand() % (virt_w / 3) + (virt_w / 3);
							} else {
								item->pt.x = std::rand() % (virt_w / 3) + (virt_w / 3 * 2);
								position   = 0;
							}
							// snow.dll does not want grouping
						} else {
							item->pt.x = std::rand() % virt_w;
							position   = 0;
						}
						item->pt.y = -(cur->sprite->pos.h);
						//item->pt.type = j;
						item->pt.cell    = 0;
						item->base_angle = std::rand() % FURU_AMP_TABLE_SIZE;
						cur->pend        = tmp;
					}
				}
			}
			while ((cur->pstart != cur->pend) &&
			       (cur->points[cur->pstart].pt.y >= static_cast<int32_t>(height)))
				cur->pstart = (1 + cur->pstart) % FURU_ELEMENT_BUFSIZE;
		}
	}
	return true;
}

static void setStr(char **dst, const char *src, long num = -1) {
	delete[] * dst;
	*dst = nullptr;

	if (src) {
		if (num >= 0) {
			*dst = new char[num + 1];
			std::memcpy(*dst, src, num);
			(*dst)[num] = '\0';
		} else {
			*dst = copystr(src);
		}
	}
}

static SDL_Surface *loadImage(char *file_name, bool *has_alpha, SDL_Surface *surface, BaseReader *br) {
	if (!file_name)
		return nullptr;
	size_t length;
	uint8_t *buffer;
	translatePathSlashes(file_name);
	if (!br->getFile(file_name, length, &buffer))
		return nullptr;

	SDL_Surface *tmp = IMG_Load_RW(SDL_RWFromMem(buffer, static_cast<int>(length)), 1);

	char *ext = std::strrchr(file_name, '.');
	if (!tmp && ext && (equalstr(ext + 1, "JPG") || equalstr(ext + 1, "jpg"))) {
		sendToLog(LogLevel::Warn, " *** force-loading a JPG image [%s]\n", file_name);
		SDL_RWops *src = SDL_RWFromMem(buffer, static_cast<int>(length));
		tmp            = IMG_LoadJPG_RW(src);
		SDL_RWclose(src);
	}
	if (tmp && has_alpha)
		*has_alpha = tmp->format->Amask;

	freearr(&buffer);
	if (!tmp) {
		sendToLog(LogLevel::Error, " *** can't load file [%s] ***\n", file_name);
		return nullptr;
	}

	SDL_Surface *ret = SDL_ConvertSurface(tmp, surface->format, SDL_SWSURFACE);
	SDL_FreeSurface(tmp);
	return ret;
}

void FuruLayer::buildAmpTables() {
	float amp[N_FURU_ELEMENTS];
	amp[0] = static_cast<float>(amplitude);
	for (int i = 1; i < N_FURU_ELEMENTS; ++i)
		amp[i] = amp[i - 1] * 0.8;

	for (int i = 0; i < N_FURU_ELEMENTS; ++i) {
		Element *cur = &elements[i];
		if (!cur->amp_table)
			cur->amp_table = new int[FURU_AMP_TABLE_SIZE];
		for (int j = 0; j < FURU_AMP_TABLE_SIZE; ++j)
			cur->amp_table[j] = static_cast<int>(amp[i] * base_disp_table[j]);
	}
}

void FuruLayer::validate_params() {
	const int half_wx = width / 2;

	if (interval < 1)
		interval = 1;
	else if (interval > 10000)
		interval = 10000;
	if (fall_velocity < 1)
		fall_velocity = 1;
	else if (fall_velocity > static_cast<int32_t>(height))
		fall_velocity = height;
	for (int i = 0; i < N_FURU_ELEMENTS; i++)
		elements[i].fall_speed = static_cast<int>(fall_mult[i] * (fall_velocity + 1));
	if (wind < -half_wx)
		wind = -half_wx;
	else if (wind > half_wx)
		wind = half_wx;
	if (amplitude < 0)
		amplitude = 0;
	else if (amplitude > half_wx)
		amplitude = half_wx;
	if (amplitude != 0)
		buildAmpTables();
	if (freq < 0)
		freq = 0;
	else if (freq > 359)
		freq = 359;
	//adjust the freq to range 0-FURU_AMP_TABLE_SIZE-1
	freq = freq * FURU_AMP_TABLE_SIZE / 360;
}

char *FuruLayer::message(const char *message, int &ret_int) {
	int num_cells[3], tmp[5];
	char buf[3][128];

	char *ret_str = nullptr;
	ret_int       = 0;

	if (!sprite)
		return nullptr;

	//sendToLog(LogLevel::Info, "FuruLayer: got message '%s'\n", message);
	//Image loading
	if (!std::strncmp(message, "i|", 2)) {
		max_sp_w                 = 0;
		SDL_Surface *ref_surface = SDL_CreateRGBSurface(SDL_SWSURFACE, 1, 1, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
		if (tumbling) {
			// "Hana"
			if (std::sscanf(message, "i|%d,%d,%d,%d,%d,%d",
			                &tmp[0], &num_cells[0],
			                &tmp[1], &num_cells[1],
			                &tmp[2], &num_cells[2])) {
				for (int i = 0; i < 3; i++) {
					elements[i].setSprite(new AnimationInfo(sprite_info[tmp[i]]));
					elements[i].sprite->num_of_cells = num_cells[i];
					if (elements[i].sprite->pos.w > max_sp_w)
						max_sp_w = elements[i].sprite->pos.w;
				}
			} else if (std::sscanf(message, "i|%120[^,],%d,%120[^,],%d,%120[^,],%d",
			                       &buf[0][0], &num_cells[0],
			                       &buf[1][0], &num_cells[1],
			                       &buf[2][0], &num_cells[2])) {
				for (int i = 0; i < 3; i++) {
					bool has_alpha = false;
					translatePathSlashes(&buf[i][0]);
					SDL_Surface *img    = loadImage(&buf[i][0], &has_alpha, ref_surface, *reader);
					AnimationInfo *anim = new AnimationInfo();
					anim->num_of_cells  = num_cells[i];
					anim->duration_list = new int[anim->num_of_cells];
					for (int j = anim->num_of_cells - 1; j >= 0; --j)
						anim->duration_list[j] = 0;
					anim->loop_mode  = 3; // not animatable
					anim->trans_mode = AnimationInfo::TRANS_TOPLEFT;
					setStr(&anim->file_name, &buf[i][0]);
					anim->image_surface = anim->setupImageAlpha(img, nullptr, has_alpha);
					if (anim->image_surface != nullptr) {
						anim->gpu_image = gpu.copyImageFromSurface(anim->image_surface);
						anim->setImage(anim->gpu_image);
					} else
						sendToLog(LogLevel::Error, "Failed to load %s\n", buf[i]);
					elements[i].setSprite(anim);
					if (anim->pos.w > max_sp_w)
						max_sp_w = anim->pos.w;
				}
			}
		} else {
			// "Snow"
			if (std::sscanf(message, "i|%d,%d,%d",
			                &tmp[0], &tmp[1], &tmp[2])) {
				for (int i = 0; i < 3; i++) {
					elements[i].setSprite(new AnimationInfo(sprite_info[tmp[i]]));
					if (elements[i].sprite->pos.w > max_sp_w)
						max_sp_w = elements[i].sprite->pos.w;
				}
			} else if (std::sscanf(message, "i|%120[^,],%120[^,],%120[^,]",
			                       &buf[0][0], &buf[1][0], &buf[2][0])) {
				for (int i = 0; i < 3; i++) {
					uint32_t firstpix   = 0;
					bool has_alpha      = false;
					SDL_Surface *img    = loadImage(&buf[i][0], &has_alpha, ref_surface, *reader);
					AnimationInfo *anim = new AnimationInfo();
					anim->num_of_cells  = 1;
					firstpix            = *(static_cast<uint32_t *>(img->pixels)) & ~(img->format->Amask);
					if (firstpix > 0) {
						anim->trans_mode = AnimationInfo::TRANS_TOPLEFT;
					} else {
						// if first pix is black, this is an "additive" sprite
						anim->trans_mode    = AnimationInfo::TRANS_COPY;
						anim->blending_mode = BlendModeId::ADD;
					}
					setStr(&anim->file_name, &buf[i][0]);
					anim->image_surface = anim->setupImageAlpha(img, nullptr, has_alpha);
					if (anim->image_surface != nullptr) {
						anim->gpu_image = gpu.copyImageFromSurface(anim->image_surface);
						anim->setImage(anim->gpu_image);
					} else
						sendToLog(LogLevel::Error, "Failed to load %s\n", buf[i]);
					elements[i].setSprite(anim);
					if (anim->pos.w > max_sp_w)
						max_sp_w = anim->pos.w;
				}
			}
		}
		SDL_FreeSurface(ref_surface);
		//Set Parameters
	} else if (std::sscanf(message, "s|%d,%d,%d,%d,%d",
	                       &interval, &fall_velocity, &wind,
	                       &amplitude, &freq)) {
		furu_init();
		validate_params();
		//Transition (adjust) Parameters
	} else if (std::sscanf(message, "t|%d,%d,%d,%d,%d",
	                       &tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4])) {
		interval += tmp[0];
		fall_velocity += tmp[1];
		wind += tmp[2];
		amplitude += tmp[3];
		freq += tmp[4];
		validate_params();
		//Fill Screen w/Elements
	} else if (!std::strcmp(message, "f")) {
		if (initialized) {
			if (sprite->old_ai)
				sprite->old_ai->blending_mode = BlendModeId::ADD;
			else
				sprite->blending_mode = BlendModeId::ADD;

			for (int j = 0; j < (tumbling ? N_FURU_ELEMENTS : N_FURU_SNOW_ELEMENTS); ++j) {
				Element *cur = nullptr;
				if (tumbling) {
					cur = &elements[j];
				} else {
					cur = &elements[0];
					for (int k = 0; k < N_FURU_ELEMENTS; k++) {
						if (j >= N_FURU_DISTR[k]) {
							cur = &elements[k];
							break;
						}
					}
				}
				for (int y = 0; y < static_cast<int32_t>(height); y += interval * cur->fall_speed) {
					const int tmp = (cur->pend + 1) % FURU_ELEMENT_BUFSIZE;
					if (tmp != cur->pstart) {
						// add a point for each element
						OscPt *item = &cur->points[cur->pend];
						item->pt.x  = std::rand() % (width + max_sp_w);
						item->pt.y  = y;
						//item->pt.type = j;
						item->pt.cell    = std::rand() % cur->sprite->num_of_cells;
						item->base_angle = std::rand() % FURU_AMP_TABLE_SIZE;
						cur->pend        = tmp;
					}
				}
			}
		}
		//Get Parameters
	} else if (!std::strcmp(message, "g")) {
		ret_int = paused ? 1 : 0;
		std::sprintf(&buf[0][0], "s|%d,%d,%d,%d,%d", interval, fall_velocity,
		             wind, amplitude, (freq * 360 / FURU_AMP_TABLE_SIZE));
		setStr(&ret_str, &buf[0][0]);
		//Halt adding new elements
	} else if (!std::strcmp(message, "h")) {
		halted = true;
		//Get number of elements displayed
	} else if (!std::strcmp(message, "n")) {
		for (auto &element : elements)
			ret_int += (element.pend - element.pstart + FURU_ELEMENT_BUFSIZE) % FURU_ELEMENT_BUFSIZE;
		//Pause
	} else if (!std::strcmp(message, "p")) {
		paused = true;
		//Restart
	} else if (!std::strcmp(message, "r")) {
		paused = false;
		//eXtinguish
	} else if (!std::strcmp(message, "x")) {
		for (auto &element : elements)
			element.clear();
		initialized = false;
	}
	return ret_str;
}

void FuruLayer::refresh(GPU_Target *target, GPU_Rect &clip, float x, float y, bool /*centre_coordinates*/, int /*rm*/, float /*scalex*/, float /*scaley*/) {
	if (initialized) {
		const int virt_w = width + max_sp_w;
		for (auto &element : elements) {
			Element *cur = &element;
			if (cur->sprite) {
				cur->sprite->visible = true;
				const int n          = (cur->pend - cur->pstart + FURU_ELEMENT_BUFSIZE) % FURU_ELEMENT_BUFSIZE;
				int p                = cur->pstart;
				if (amplitude == 0) {
					//no need to mess with angles if no displacement
					for (int i = n; i > 0; i--) {
						OscPt *curpt              = &cur->points[p];
						p                         = (1 + p) % FURU_ELEMENT_BUFSIZE;
						cur->sprite->current_cell = curpt->pt.cell;
						cur->sprite->pos.x        = ((curpt->pt.x + virt_w) % virt_w) - max_sp_w;
						cur->sprite->pos.y        = curpt->pt.y;
						drawLayerToGPUTarget(target, cur->sprite, clip, x, y);
					}
				} else {
					for (int i = n; i > 0; i--) {
						OscPt *curpt              = &cur->points[p];
						p                         = (1 + p) % FURU_ELEMENT_BUFSIZE;
						const int disp_angle      = (angle + curpt->base_angle + FURU_AMP_TABLE_SIZE) % FURU_AMP_TABLE_SIZE;
						cur->sprite->current_cell = curpt->pt.cell;
						cur->sprite->pos.x        = ((curpt->pt.x + cur->amp_table[disp_angle] + virt_w) % virt_w) - max_sp_w;
						cur->sprite->pos.y        = curpt->pt.y;
						drawLayerToGPUTarget(target, cur->sprite, clip, x, y);
					}
				}
			}
		}
	}
}
