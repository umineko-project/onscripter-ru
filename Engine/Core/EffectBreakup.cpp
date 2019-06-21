/**
 *  EffectBreakup.cpp
 *  ONScripter-RU
 *
 *  Emulation of Takashi Toyama's "breakup.dll" NScripter plugin effect.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Core/ONScripter.hpp"
#include "Engine/Graphics/Common.hpp"
#include "Engine/Components/Window.hpp"

constexpr int BREAKUP_DIRECTIONS = 8;

// Breakup is divided into a number of "frames".
// Breakup is controlled by the user through breakupFactor which ALWAYS runs between 0 and 1000 (no unit), so changing the following
// values has no effect on the overall "duration" of breakup -- that is user-controlled. What these values do control is the
// relative duration spent in each part of the overall breakup animation. Therefore the concept of "frames" here is very relative.
// You could think of them as "intervals" or "steps" perhaps.

// BREAKUP_DISSOLVE_FRAMES : The number of frames taken for a single tile/circle to go from maximum radius to minimum radius.
constexpr int BREAKUP_DISSOLVE_FRAMES = 1000;
// BREAKUP_WIPE_FRAMES: The number of frames that elapse between the first tile/circle starting to vanish and the last one starting.
// (The time for the diagonal wipe to cross the screen.)
// ex: If it's 0 everything will go flying at once.
// ex: If it's at least DISSOLVE, then some tiles will still be in place after the first one is completely gone.
// ex: If it's many times DISSOLVE, many tiles will still be in place after the first tiles are completely gone.
constexpr int BREAKUP_WIPE_FRAMES = 3000;
// (The total number of frames squished into the 1000 breakupFactor units will be these two added together --
// first we have to reach the last tile [WIPE] to begin its animation, then it has to go through it and complete it [DISSOLVE].)

// BREAKUP_MOVE_FRAMES: The number of frames within the animation of a single tile for which the tile is moving.
// (Should be less than or equal to BREAKUP_DISSOLVE_FRAMES.)
constexpr int BREAKUP_MOVE_FRAMES = 850;

const int breakup_disp_x[BREAKUP_DIRECTIONS]{-7, -7, -5, -4, -2, 1, 3, 5};
const int breakup_disp_y[BREAKUP_DIRECTIONS]{0, 2, 4, 6, 7, 7, 6, 5};

void ONScripter::buildBreakupCellforms() {
	// New method: just load as a file
	if (breakup_cellforms_gpu)
		return;
	breakup_cellforms_gpu = loadGpuImage("breakup-cellforms.png");
}

bool ONScripter::breakupInitRequired(BreakupID id) {
	return breakupData.count(id) == 0;
}

void ONScripter::initBreakup(BreakupID id, GPU_Image *src, GPU_Rect *src_rect) {
	//sendToLog(LogLevel::Info,"breakup called with breakup factor %u and canvas_w/h %u %u\n", breakupFactor, ons.canvas_width, ons.canvas_height);
	int cellFactor = ons.new_breakup_implementation ? BREAKUP_CELLSEPARATION : BREAKUP_CELLWIDTH;
	int w{0}, h{0};
	if (id.type == BreakupType::SPRITE_TIGHTFIT && ons.new_breakup_implementation) {
		if (src_rect) {
			w = src_rect->w;
			h = src_rect->h;
		} else {
			w = src->w;
			h = src->h;
		}
	} else {
		w = window.canvas_width;
		h = window.canvas_height;
	}

	int numCellsX = ((w + cellFactor - 1) / cellFactor) + 1;
	int numCellsY = ((h + cellFactor - 1) / cellFactor) + 1;

	BreakupData &data = breakupData[id];
	data.breakup_cells.resize(numCellsX * numCellsY);
	data.diagonals.resize(numCellsX + numCellsY - 1);
	data.wInCellsFloat = (static_cast<float>(w) / (1.0f * cellFactor));
	data.hInCellsFloat = (static_cast<float>(h) / (1.0f * cellFactor));
	data.cellFactor    = cellFactor;
	data.numCellsX     = numCellsX;
	data.numCellsY     = numCellsY;

	if (!ons.new_breakup_implementation) {
		buildBreakupCellforms();
		if (!breakup_cellform_index_grid) {
			breakup_cellform_index_grid    = gpu.createImage(numCellsX, numCellsY, 4);
			breakup_cellform_index_surface = SDL_CreateRGBSurface(SDL_SWSURFACE, numCellsX, numCellsY, 32, 0, 0, 0, 0);
		}
	}
}

void ONScripter::oncePerBreakupEffectBreakupSetup(BreakupID id, int breakupDirectionFlagset, int numCellsX,
                                                  int numCellsY) {
	if (!ons.new_breakup_implementation)
		return;
	BreakupData &data = breakupData[id];
	if (data.breakup_mode.has() && data.breakup_mode.get() == breakupDirectionFlagset) {
		// nothing to do, we're all set up
		return;
	}

	std::srand(id.hash);

	data.breakup_mode.set(breakupDirectionFlagset);
	data.n_cells    = numCellsX * numCellsY;
	data.tot_frames = BREAKUP_DISSOLVE_FRAMES + BREAKUP_WIPE_FRAMES;

	int totalDiagCount      = numCellsX + numCellsY - 1;
	int n                   = 0;
	BreakupCell *cells      = breakupData[id].breakup_cells.data();
	BreakupCell **diagonals = breakupData[id].diagonals.data();
	for (int thisDiagNo = 0; thisDiagNo < totalDiagCount; thisDiagNo++) {
		diagonals[thisDiagNo] = &cells[n];
		for (int x = thisDiagNo, y = 0; (x >= 0) && (y < numCellsY); x--, y++) {
			if (x >= numCellsX)
				continue; // until it gets back into range -- this removes the need for two loops
			// calculate initial state
			int state = BREAKUP_DISSOLVE_FRAMES;
			if (totalDiagCount > 1) { // prevent divide by zero
				//TODO: this gives uneven distribution, 20 should most likely depend on thisDiagNo/totalDiagCount.
				int fakeDiagNo = thisDiagNo - (std::rand() % 20);
				if (fakeDiagNo < 0)
					fakeDiagNo = 0;
				state += (fakeDiagNo * BREAKUP_WIPE_FRAMES / (totalDiagCount - 1));
			}
			// First cells iterated have state of only BREAKUP_DISSOLVE_FRAMES, so they will be first to disappear or last to appear.
			// If breakup mode is LEFT, then they should be at x=0.
			// If breakup mode is LOWER, then they should be at y=maxCell-1, because top-left is 0,0 for textures.
			cells[n].cell_x   = (breakupDirectionFlagset & BREAKUP_MODE_LEFT) ? x : numCellsX - x - 1;
			cells[n].cell_y   = (breakupDirectionFlagset & BREAKUP_MODE_LOWER) ? numCellsY - y - 1 : y;
			cells[n].diagonal = thisDiagNo;
			cells[n].state    = state;

			int x_dir = 1;
			int y_dir = -1; // flip the y-axis here so that we can express below angles between 0~90 for the top right quadrant, as is normal in math
			if (breakupDirectionFlagset & BREAKUP_MODE_JUMBLE) {
				x_dir = -x_dir;
				y_dir = -y_dir;
			}
			if (breakupDirectionFlagset & BREAKUP_MODE_LEFT) {
				x_dir = -x_dir;
			}
			if (breakupDirectionFlagset & BREAKUP_MODE_LOWER) {
				y_dir = -y_dir;
			}

			int ax                    = (thisDiagNo - (numCellsY - 1));
			ax                        = ax > 0 ? x - ax : x;
			int ay                    = (thisDiagNo - (numCellsX - 1));
			ay                        = ay > 0 ? y - ay : y;
			double angle              = ax == 0 ? M_PI / 2.0 : std::atan2(ay, ax);
			int plusminus50           = (std::rand() % 101) - 50;
			double plusminus45degrees = M_PI / 4.0 * plusminus50 / 50.0;
			angle += plusminus45degrees;

			cells[n].xMovement = x_dir * std::cos(angle);
			cells[n].yMovement = y_dir * std::sin(angle);

			++n;
		}
	}
}

void ONScripter::deinitBreakup(BreakupID id) {
	if (breakupInitRequired(id)) {
		return;
	}
	breakupData.erase(id);
}

void ONScripter::effectBreakupNew(BreakupID id, int breakupFactor) {
	BreakupData &data    = breakupData[id];
	BreakupCell *myCells = data.breakup_cells.data();

	int duration = 1000;
	int frame    = data.tot_frames * breakupFactor / duration;

	int maximumDiagonal = 0;
	int state           = 0;
	bool touched        = false;

	for (int n = 0; n < data.n_cells; ++n) {
		BreakupCell &cell = myCells[n];
		state             = cell.state - frame;
		cell.disp_x       = 0;
		cell.disp_y       = 0;
		touched           = false;
		cell.resizeFactor = 1.0f; // If we haven't started the animation yet
		if (state < BREAKUP_DISSOLVE_FRAMES) {
			// We started the animation, so now the radius should reduce to zero according to state
			cell.resizeFactor = state <= 0 ? 0.0f : 1.0f * state / BREAKUP_DISSOLVE_FRAMES;
			touched           = true;
		}
		if (state < BREAKUP_MOVE_FRAMES && state > 0) {
			// If we've started moving the position
			cell.disp_x = cell.xMovement * (BREAKUP_MOVE_FRAMES - state);
			cell.disp_y = cell.yMovement * (BREAKUP_MOVE_FRAMES - state);
			touched     = true;
		}
		if (touched && cell.diagonal > maximumDiagonal) {
			maximumDiagonal = cell.diagonal;
		}
	}
	data.maxDiagonalToContainBrokenCells = maximumDiagonal;
}

void ONScripter::oncePerFrameBreakupSetup(BreakupID id, int breakupDirectionFlagset, int numCellsX, int numCellsY) {
	auto &data = breakupData[id];

	if (ons.new_breakup_implementation) {
		std::srand(static_cast<unsigned int>(id.hash));
	}

	data.breakup_mode.set(breakupDirectionFlagset);

	int totalDiagCount = numCellsX + numCellsY - 1;
	data.n_cells       = numCellsX * numCellsY;
	data.tot_frames    = BREAKUP_DISSOLVE_FRAMES + BREAKUP_WIPE_FRAMES;
	data.prev_frame    = 0;

	int n = 0, dir = 1;
	auto myCells = data.breakup_cells.data();
	for (int thisDiagNo = 0; thisDiagNo < totalDiagCount; thisDiagNo++) {
		int state = BREAKUP_DISSOLVE_FRAMES;
		if (!ons.new_breakup_implementation && totalDiagCount > 1) //prevent divide by zero
			state += (thisDiagNo * BREAKUP_WIPE_FRAMES / (totalDiagCount - 1));
		for (int x = thisDiagNo, y = 0; (x >= 0) && (y < numCellsY); x--, y++) {
			if (x >= numCellsX)
				continue; // until it gets back into range -- this removes the need for two loops

			if (ons.new_breakup_implementation) {
				state = BREAKUP_DISSOLVE_FRAMES;
				if (totalDiagCount > 1) { //prevent divide by zero
					int fakeDiagNo = thisDiagNo - (std::rand() % 20);
					if (fakeDiagNo < 0)
						fakeDiagNo = 0;
					state += (fakeDiagNo * BREAKUP_WIPE_FRAMES / (totalDiagCount - 1));
				}
			}

			myCells[n].cell_x = x;
			myCells[n].cell_y = y;
			if (!(breakupDirectionFlagset & BREAKUP_MODE_LEFT))
				myCells[n].cell_x = numCellsX - x - 1;
			if (breakupDirectionFlagset & BREAKUP_MODE_LOWER)
				myCells[n].cell_y = numCellsY - y - 1;
			myCells[n].dir    = dir;
			myCells[n].state  = state;
			myCells[n].radius = 0;
			dir               = (dir + 1) & (BREAKUP_DIRECTIONS - 1);
			++n;
		}
	}
}

void ONScripter::effectBreakupOld(BreakupID id, int breakupFactor) {
	auto &data   = breakupData[id];
	auto myCells = data.breakup_cells.data();
	int duration = 1000;

	int x_dir = -1;
	int y_dir = -1;

	int frame      = data.tot_frames * breakupFactor / duration;
	int frame_diff = frame - data.prev_frame;
	if (frame_diff == 0)
		return;

	data.prev_frame += frame_diff;
	frame_diff = -frame_diff;

	int breakupDirectionFlagset = data.breakup_mode.get();
	if (breakupDirectionFlagset & BREAKUP_MODE_JUMBLE) {
		x_dir = -x_dir;
		y_dir = -y_dir;
	}
	if (!(breakupDirectionFlagset & BREAKUP_MODE_LEFT)) {
		x_dir = -x_dir;
	}
	if (breakupDirectionFlagset & BREAKUP_MODE_LOWER) {
		y_dir = -y_dir;
	}

	int state{0};
	for (int n = 0; n < data.n_cells; ++n) {
		myCells[n].state += frame_diff;
		state             = myCells[n].state;
		myCells[n].disp_x = 0;
		myCells[n].disp_y = 0;
		myCells[n].radius = 0;
		// If we haven't started the animation yet
		myCells[n].radius = BREAKUP_CELLFORMS; // greater than the maximum index, indicating "do not apply mask"
		if (state < BREAKUP_DISSOLVE_FRAMES) {
			// We started the animation, so now the radius should reduce to zero according to state
			myCells[n].radius = state <= 0 ? 0 : BREAKUP_CELLFORMS * state / BREAKUP_DISSOLVE_FRAMES;
		}
		if (state < BREAKUP_MOVE_FRAMES && state > 0) {
			// If we've started moving the position
			myCells[n].disp_x = x_dir * breakup_disp_x[myCells[n].dir] * (state - BREAKUP_MOVE_FRAMES) / 10;
			myCells[n].disp_y = y_dir * breakup_disp_y[myCells[n].dir] * (BREAKUP_MOVE_FRAMES - state) / 10;
			// ehhhhhhhh don't really like this method of calculation... the divisor is pretty arbitrary
		}
		int c = (myCells[n].radius * 255) / BREAKUP_CELLFORMS;
		setSurfacePixel(breakup_cellform_index_surface, myCells[n].cell_x, myCells[n].cell_y,
		                SDL_MapRGBA(breakup_cellform_index_surface->format, c, c, c, 255));
	}
	GPU_GetTarget(breakup_cellform_index_grid);
	gpu.updateImage(breakup_cellform_index_grid, nullptr, breakup_cellform_index_surface, nullptr, false);
}
