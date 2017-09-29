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

static TidMap* tid = NULL;

class DragonLimits {
	public:
		piece_t _value;

		DragonLimits(){
			piece_init(&_value);
		}
		DragonLimits(const DragonLimits& dL, split){
			piece_init(&_value);
		}
		~DragonLimits(){
		}
		void operator()(const blocked_range<uint64_t>& r){
			piece_limit(r.begin(), r.end(), &_value);
		}
		void join(const DragonLimits& dL){
			piece_merge(&_value, dL._value);
		}
};

class DragonDraw {
	
	public:
		uint64_t _i;
		struct draw_data* _data;

		DragonDraw(struct draw_data* data) : _i(0), _data(data) {}
		DragonDraw(const DragonDraw& dD) : _data(dD._data){
			//cout << " Copy " << gettid() << endl;
			_i = tid->getIdFromTid(gettid());
		}
		void operator()(const blocked_range<uint64_t>& r) const {
			//Si la ligne ci-dessous est decommentee, on ralenti
			//le calcul, donc on force la repartition des taches sur plusieurs
			//threads. Sinon, lorsqu'elle est commentee, seulement 
			//notre image sera dessinee avec un seul thread.
			
			//cout << " operator " << _i << endl;
			
			//if (r.begin() < (_data->size / _data->nb_thread))
			
			dragon_draw_raw(r.begin(), r.end(), _data->dragon, _data->dragon_width, _data->dragon_height, _data->limits, _i);
		}
};

class DragonRender {
	public:
		struct draw_data* _data;

		DragonRender(struct draw_data* data) : _data(data) {}
		DragonRender(const DragonRender& dR) : _data(dR._data){}
		void operator()(const blocked_range<int>& r) const {
			scale_dragon(r.begin(), r.end(), _data->image, _data->image_width, _data->image_height, _data->dragon, _data->dragon_width, _data->dragon_height, _data->palette);
		}
};

class DragonClear {
	public:
		char _value;
		char* _canvas;

		DragonClear(char value, char* canvas) : _value(value), _canvas(canvas) {}
		DragonClear(const DragonClear& dC) : _value(dC._value), _canvas(dC._canvas){}
		void operator()(const blocked_range<int>& r) const {
			init_canvas(r.begin(), r.end(), _canvas, -1);
		}
};

int dragon_draw_tbb(char **canvas, struct rgb *image, int width, int height, uint64_t size, int nb_thread)
{
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
	
	tid = new TidMap(nb_thread);	

	/* 2. Initialiser la surface : DragonClear */
	DragonClear dC = DragonClear(-1, dragon);
	parallel_for(blocked_range<int>(0, dragon_surface), dC);

	/* 3. Dessiner le dragon : DragonDraw */
	DragonDraw dD = DragonDraw(&data);
	parallel_for(blocked_range<uint64_t>(0, data.size), dD);

	/* 4. Effectuer le rendu final */
	DragonRender dR = DragonRender(&data);
	parallel_for(blocked_range<int>(0, data.image_height), dR);
	
	init.terminate();

	free_palette(palette);
	FREE(data.tid);
	*canvas = dragon;
	tid->dump();
	delete tid;
	// *canvas = NULL;
	return 0;
}

/*
 * Calcule les limites en terme de largeur et de hauteur de
 * la forme du dragon. Requis pour allouer la matrice de dessin.
 */
int dragon_limits_tbb(limits_t *limits, uint64_t size, int nb_thread)
{
	DragonLimits lim;

	task_scheduler_init task(nb_thread); // Pour initialiser le nombre de task necessaire
	parallel_reduce(blocked_range<uint64_t>(0, size), lim);

	piece_t piece;
	// piece_init(&piece);
	piece = lim._value; // on veut celle calcule.
	*limits = piece.limits;
	return 0;
}
