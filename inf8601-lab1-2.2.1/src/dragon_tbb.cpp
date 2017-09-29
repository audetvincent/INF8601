/*
 * dragon_tbb.c
 *
 *  Created on: 2011-08-17
 *      Author: Francis Giraldeau <francis.giraldeau@gmail.com>
 */

#include <iostream>

extern "C" {
#include "dragon.h"
#include "color.h"
#include "utils.h"
}
#include "dragon_tbb.h"
#include "tbb/tbb.h"
#include "TidMap.h"

using namespace std;
using namespace tbb;

class DragonLimits {
	piece_t value;

	public:
		DragonLimits(){
			piece_init(&value); //par reference comme demande
		}
		DragonLimits(const DragonLimits& dL, split){
			piece_init(&value);
		}
		void ~DragonLimits(){
			delete value;
		}
		void DragonLimits::operator()(const blocked_range<uint64_t>& r){
			piece_limit(r.begin(), r.end(), &value);				
		}
		void join(DragonLimits& dL){
			piece_merge(&value, dL.value);
		}
};

class DragonDraw {
	public:
		int _i;
		struct draw_data* _data;

		DragonDraw(struct draw_data* data) : _i(0), _data(data) {}
		DragonDraw(const DragonDraw& dD) : _i(dC._i), _data(dC._data){}
		~DragonDraw(){
			delete _data;
		}
		void operator()(const blocked_range<uint64_t>& r) const {
			dragon_draw_raw(r.begin() , r.end(), _data->dragon, _data->dragon_width, _data->dragon_height, _data->limits, _i);
		}
};

class DragonRender {
	public:
		struct draw_data* _data;

		DragonDraw(struct draw_data* data) : _data(data) {}
		DragonDraw(const DragonDraw& dD) : _data(dC._data){}
		~DragonDraw(){
			delete _data;
		}
		void operator()(const blocked_range<uint64_t>& r) const {
			scale_dragon(r.begin(), r.end(), data->image, data->image_width, data->image_height, data->dragon, data->dragon_width, data->dragon_height, data->palette);
		}
};

class DragonClear {
	public:
		char* _value, _canvas;

		DragonClear(char* canvas) : &_value(-1), _canvas(canvas) {}
		DragonClear(const DragonClear& dC) : _value(dC._value), _canvas(dC._canvas){}
		~DragonClear(){
			delete _value;
			delete _canvas;
		}
		void operator()(const blocked_range<uint64_t>& r) const {
			init_canvas(r.begin(), r.end(), _canvas, -1);
		}
};

int dragon_draw_tbb(char **canvas, struct rgb *image, int width, int height, uint64_t size, int nb_thread)
{
	TODO("dragon_draw_tbb");
	struct draw_data data;
	limits_t limits;
	char *dragon = NULL;
	int dragon_width;
	int dragon_height;
	int dragon_surface;
	int scale_x;
	int scale_y;
	int scale;
	int deltaJ;
	int deltaI;

	struct palette *palette = init_palette(nb_thread);
	if (palette == NULL)
		return -1;

	/* 1. Calculer les limites du dragon */
	dragon_limits_tbb(&limits, size, nb_thread);

	task_scheduler_init init(nb_thread);

	dragon_width = limits.maximums.x - limits.minimums.x;
	dragon_height = limits.maximums.y - limits.minimums.y;
	dragon_surface = dragon_width * dragon_height;
	scale_x = dragon_width / width + 1;
	scale_y = dragon_height / height + 1;
	scale = (scale_x > scale_y ? scale_x : scale_y);
	deltaJ = (scale * width - dragon_width) / 2;
	deltaI = (scale * height - dragon_height) / 2;

	dragon = (char *) malloc(dragon_surface);
	if (dragon == NULL) {
		free_palette(palette);
		return -1;
	}

	data.nb_thread = nb_thread;
	data.dragon = dragon;
	data.image = image;
	data.size = size;
	data.image_height = height;
	data.image_width = width;
	data.dragon_width = dragon_width;
	data.dragon_height = dragon_height;
	data.limits = limits;
	data.scale = scale;
	data.deltaI = deltaI;
	data.deltaJ = deltaJ;
	data.palette = palette;
	data.tid = (int *) calloc(nb_thread, sizeof(int));

	/* 2. Initialiser la surface : DragonClear */
	DragonClear dC = DragonClear(dragon);
	parallel_for(blocked_range<uint64_t>(0, dragon_surface), dC);

	/* 3. Dessiner le dragon : DragonDraw */
	DragonDraw dD = DragonDraw(&data);
	parallel_for(blocked_range<uint64_t>(0, data.size), dD);

	/* 4. Effectuer le rendu final */
	DragonRender dR = DragonRender(&data);
	parallel_for(blocked_range<uint_64>(0, data.size), dR);
	
	init.terminate();

	free_palette(palette);
	FREE(data.tid);
	//*canvas = dragon;
	*canvas = NULL;
	return 0;
}

/*
 * Calcule les limites en terme de largeur et de hauteur de
 * la forme du dragon. Requis pour allouer la matrice de dessin.
 */
int dragon_limits_tbb(limits_t *limits, uint64_t size, int nb_thread)
{
	TODO("dragon_limits_tbb");
	DragonLimits lim;

	task_scheduler_init task(nb_thread); // Pour initialiser le nombre de task necessaire
	parallel_reduce(blocked_range<uint64_t*>(0, size), lim);

	piece_t piece;
	//piece_init(&piece);
	piece = lim.piece;
	*limits = piece.limits;
	return 0;
}
