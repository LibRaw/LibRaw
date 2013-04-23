/* -*- C++ -*-
 * File: dht_demosaic.cpp
 * Copyright 2013 Anton Petrusevich
 * Created: Tue Apr  9, 2013
 *
 * This code is licensed under the GNU LESSER GENERAL PUBLIC LICENSE version 2.1
 * (See file LICENSE.LGPL provided in LibRaw distribution archive for details).
 *
 */

struct ecdir {
	float dist;
	float hue_1, hue_2;
	signed char dx;
	signed char dy;
	signed char dx2;
	signed char dy2;
	void set(signed char _dx, signed char _dy, signed char _dx2, signed char _dy2) {
		dx = _dx;
		dy = _dy;
		dx2 = _dx2;
		dy2 = _dy2;
	}

};

/*
 * функция вычисляет яркостную дистанцию.
 * если две яркости отличаются в два раза, например 10 и 20, то они имеют такой же вес
 * при принятии решения об интерполировании, как и 100 и 200 -- фотографическая яркость между ними 1 стоп.
 * дистанция всегда >=1
 */
static inline float calc_dist(float c1, float c2) {
	return c1 > c2 ? c1 / c2 : c2 / c1;
}

struct DHT {
	int nr_height, nr_width;
	static const int nr_topmargin = 4, nr_leftmargin = 4;
	float (*nraw)[3];
	ushort channel_maximum[3];
	float channel_minimum[3];
	LibRaw &libraw;
	static const int HOR = 2, VER = 4, HORSH = 3, VERSH = 5, LURD = 16, RULD = 32, LURDSH = 24,
			RULDSH = 40;
	static const float T;
	char *ndir;
	int nr_offset(int row, int col) {
		return (row * nr_width + col);
	}
	int get_hv_grb(int x, int y, int kc) {
		float hv1 = 2 * nraw[nr_offset(y - 1, x)][1]
				/ (nraw[nr_offset(y - 2, x)][kc] + nraw[nr_offset(y, x)][kc]);
		float hv2 = 2 * nraw[nr_offset(y + 1, x)][1]
				/ (nraw[nr_offset(y + 2, x)][kc] + nraw[nr_offset(y, x)][kc]);
		float dv = calc_dist(hv1, hv2)
				* calc_dist(nraw[nr_offset(y - 2, x)][kc] * nraw[nr_offset(y + 2, x)][kc],
						nraw[nr_offset(y, x)][kc] * nraw[nr_offset(y, x)][kc]);
		float hh1 = 2 * nraw[nr_offset(y, x - 1)][1]
				/ (nraw[nr_offset(y, x - 2)][kc] + nraw[nr_offset(y, x)][kc]);
		float hh2 = 2 * nraw[nr_offset(y, x + 1)][1]
				/ (nraw[nr_offset(y, x + 2)][kc] + nraw[nr_offset(y, x)][kc]);
		float dh = calc_dist(hh1, hh2)
				* calc_dist(nraw[nr_offset(y, x - 2)][kc] * nraw[nr_offset(y, x + 2)][kc],
						nraw[nr_offset(y, x)][kc] * nraw[nr_offset(y, x)][kc]);
		float e = calc_dist(dh, dv);
		char d = dh < dv ? (e > T ? HORSH : HOR) : (e > T ? VERSH : VER);
		return d;
	}
	int get_hv_rbg(int x, int y, int hc) {
		float hv1 = 2 * nraw[nr_offset(y - 1, x)][hc ^ 2]
				/ (nraw[nr_offset(y - 2, x)][1] + nraw[nr_offset(y, x)][1]);
		float hv2 = 2 * nraw[nr_offset(y + 1, x)][hc ^ 2]
				/ (nraw[nr_offset(y + 2, x)][1] + nraw[nr_offset(y, x)][1]);
		float dv = calc_dist(hv1, hv2)
				* calc_dist(nraw[nr_offset(y - 2, x)][1] * nraw[nr_offset(y + 2, x)][1],
						nraw[nr_offset(y, x)][1] * nraw[nr_offset(y, x)][1]);
		float hh1 = 2 * nraw[nr_offset(y, x - 1)][hc]
				/ (nraw[nr_offset(y, x - 2)][1] + nraw[nr_offset(y, x)][1]);
		float hh2 = 2 * nraw[nr_offset(y, x + 1)][hc]
				/ (nraw[nr_offset(y, x + 2)][1] + nraw[nr_offset(y, x)][1]);
		float dh = calc_dist(hh1, hh2)
				* calc_dist(nraw[nr_offset(y, x - 2)][1] * nraw[nr_offset(y, x + 2)][1],
						nraw[nr_offset(y, x)][1] * nraw[nr_offset(y, x)][1]);
		float e = calc_dist(dh, dv);
		char d = dh > dv ? e > T ? VERSH : VER : e > T ? HORSH : HOR;
		return d;
	}
	int get_diag_grb(int x, int y, int kc) {
		float hlu = nraw[nr_offset(y - 1, x - 1)][1] / nraw[nr_offset(y - 1, x - 1)][kc];
		float hrd = nraw[nr_offset(y + 1, x + 1)][1] / nraw[nr_offset(y + 1, x + 1)][kc];
		float hru = nraw[nr_offset(y - 1, x + 1)][1] / nraw[nr_offset(y - 1, x + 1)][kc];
		float hld = nraw[nr_offset(y + 1, x - 1)][1] / nraw[nr_offset(y + 1, x - 1)][kc];
		float dlurd = calc_dist(hlu, hrd)
				* calc_dist(nraw[nr_offset(y - 1, x - 1)][1] * nraw[nr_offset(y + 1, x + 1)][1],
						nraw[nr_offset(y, x)][1] * nraw[nr_offset(y, x)][1]);
		float druld = calc_dist(hlu, hrd)
				* calc_dist(nraw[nr_offset(y - 1, x + 1)][1] * nraw[nr_offset(y + 1, x - 1)][1],
						nraw[nr_offset(y, x)][1] * nraw[nr_offset(y, x)][1]);
		float e = calc_dist(dlurd, druld);
		char d = druld < dlurd ? (e > T ? RULDSH : RULD) : (e > T ? LURDSH : LURD);
		return d;
	}
	int get_diag_rbg(int x, int y, int hc) {
		float dlurd = calc_dist(nraw[nr_offset(y - 1, x - 1)][1] * nraw[nr_offset(y + 1, x + 1)][1],
				nraw[nr_offset(y, x)][1] * nraw[nr_offset(y, x)][1]);
		float druld = calc_dist(nraw[nr_offset(y - 1, x + 1)][1] * nraw[nr_offset(y + 1, x - 1)][1],
				nraw[nr_offset(y, x)][1] * nraw[nr_offset(y, x)][1]);
		float e = calc_dist(dlurd, druld);
		char d = druld < dlurd ? (e > T ? RULDSH : RULD) : (e > T ? LURDSH : LURD);
		return d;
	}
	DHT(LibRaw &_libraw);
	void copy_to_image();
	void make_greens();
	void make_diag_dirs();
	void make_hv_dirs();
	void refine_hv_dirs(int i, int js);
	void refine_diag_dirs(int i, int js);
	void refine_ihv_dirs(int i);
	void refine_idiag_dirs(int i);
	void illustrate_dirs();
	void illustrate_dline(int i);
	void make_hv_dline(int i);
	void make_diag_dline(int i);
	void make_gline(int i);
	void make_rbdiag(int i);
	void make_rbhv(int i);
	void make_rb();
};

const float DHT::T= 1.41f;

typedef float float3[3];

/*
 * информация о цветах копируется во float в общем то с одной целью -- чтобы вместо 0 можно было писать 0.5,
 * что больше подходит для вычисления яркостной разницы.
 * причина: в целых числах разница в 1 стоп составляет ряд 8,4,2,1,0 -- последнее число должно быть 0.5,
 * которое непредствамио в целых числах.
 * так же это изменение позволяет не думать о специальных случаях деления на ноль.
 *
 * альтернативное решение: умножить все данные на 2 и в младший бит внести 1. правда,
 * всё равно придётся следить, чтобы при интерпретации зелёного цвета не получился 0 при округлении,
 * иначе проблема при интерпретации синих и красных.
 *
 */
DHT::DHT(LibRaw& _libraw) :
		libraw(_libraw) {
	nr_height = libraw.imgdata.sizes.iheight + nr_topmargin * 2;
	nr_width = libraw.imgdata.sizes.iwidth + nr_leftmargin * 2;
	nraw = (float3*) malloc(nr_height * nr_width * sizeof(float3));
	int iwidth = libraw.imgdata.sizes.iwidth;
	ndir = (char*) calloc(nr_height * nr_width, 1);
	channel_maximum[0] = channel_maximum[1] = channel_maximum[2] = 0;
	channel_minimum[0] = libraw.imgdata.image[0][0];
	channel_minimum[1] = libraw.imgdata.image[0][1];
	channel_minimum[2] = libraw.imgdata.image[0][2];
	for (int i = 0; i < nr_height * nr_width; ++i)
		nraw[i][0] = nraw[i][1] = nraw[i][2] = 0.5;
	for (int i = 0; i < libraw.imgdata.sizes.iheight; ++i) {
		int col_cache[48];
		for (int j = 0; j < 48; ++j) {
			int l = libraw.COLOR(i, j);
			if (l == 3)
				l = 1;
			col_cache[j] = l;
		}
		for (int j = 0; j < iwidth; ++j) {
			int l = col_cache[j % 48];
			unsigned short c = libraw.imgdata.image[i * iwidth + j][l];
			if (c != 0) {
				if (channel_maximum[l] < c)
					channel_maximum[l] = c;
				if (channel_minimum[l] > c)
					channel_minimum[l] = c;
				nraw[nr_offset(i + nr_topmargin, j + nr_leftmargin)][l] = (float) c;
			}
		}
	}
	channel_minimum[0] += .5;
	channel_minimum[1] += .5;
	channel_minimum[2] += .5;
}

void DHT::make_diag_dirs() {
#if defined(LIBRAW_USE_OPENMP)
#pragma omp parallel for schedule(guided)
#endif
	for (int i = 0; i < libraw.imgdata.sizes.iheight; ++i) {
		make_diag_dline(i);
	}
#if defined(LIBRAW_USE_OPENMP)
#pragma omp parallel for schedule(guided)
#endif
	for (int i = 0; i < libraw.imgdata.sizes.iheight; ++i) {
		refine_diag_dirs(i, i & 1);
	}
#if defined(LIBRAW_USE_OPENMP)
#pragma omp parallel for schedule(guided)
#endif
	for (int i = 0; i < libraw.imgdata.sizes.iheight; ++i) {
		refine_diag_dirs(i, (i & 1) ^ 1);
	}
#if defined(LIBRAW_USE_OPENMP)
#pragma omp parallel for schedule(guided)
#endif
	for (int i = 0; i < libraw.imgdata.sizes.iheight; ++i) {
		refine_idiag_dirs(i);
	}
}

void DHT::make_hv_dirs() {
#if defined(LIBRAW_USE_OPENMP)
#pragma omp parallel for schedule(guided)
#endif
	for (int i = 0; i < libraw.imgdata.sizes.iheight; ++i) {
		make_hv_dline(i);
	}
#if defined(LIBRAW_USE_OPENMP)
#pragma omp parallel for schedule(guided)
#endif
	for (int i = 0; i < libraw.imgdata.sizes.iheight; ++i) {
		refine_hv_dirs(i, i & 1);
	}
#if defined(LIBRAW_USE_OPENMP)
#pragma omp parallel for schedule(guided)
#endif
	for (int i = 0; i < libraw.imgdata.sizes.iheight; ++i) {
		refine_hv_dirs(i, (i & 1) ^ 1);
	}
#if defined(LIBRAW_USE_OPENMP)
#pragma omp parallel for schedule(guided)
#endif
	for (int i = 0; i < libraw.imgdata.sizes.iheight; ++i) {
		refine_ihv_dirs(i);
	}
}

void DHT::refine_hv_dirs(int i, int js) {
	int iwidth = libraw.imgdata.sizes.iwidth;
	for (int j = js; j < iwidth; j += 2) {
		int x = j + nr_leftmargin;
		int y = i + nr_topmargin;
		if (ndir[nr_offset(y, x)] & 1)
			continue;
		int nv = (ndir[nr_offset(y - 1, x)] & VER) + (ndir[nr_offset(y + 1, x)] & VER)
				+ (ndir[nr_offset(y, x - 1)] & VER) + (ndir[nr_offset(y, x + 1)] & VER);
		int nh = (ndir[nr_offset(y - 1, x)] & HOR) + (ndir[nr_offset(y + 1, x)] & HOR)
				+ (ndir[nr_offset(y, x - 1)] & HOR) + (ndir[nr_offset(y, x + 1)] & HOR);
		bool codir =
				(ndir[nr_offset(y, x)] & VER) ?
						((ndir[nr_offset(y - 1, x)] & VER) || (ndir[nr_offset(y + 1, x)] & VER)) :
						((ndir[nr_offset(y, x - 1)] & HOR) || (ndir[nr_offset(y, x + 1)] & HOR));
		nv /= VER;
		nh /= HOR;
		if ((ndir[nr_offset(y, x)] & VER) && (nh > 2 && !codir))
			ndir[nr_offset(y, x)] = HOR;
		if ((ndir[nr_offset(y, x)] & HOR) && (nv > 2 && !codir))
			ndir[nr_offset(y, x)] = VER;
	}
}

void DHT::refine_ihv_dirs(int i) {
	int iwidth = libraw.imgdata.sizes.iwidth;
	for (int j = 0; j < iwidth; j++) {
		int x = j + nr_leftmargin;
		int y = i + nr_topmargin;
		if (ndir[nr_offset(y, x)] & 1)
			continue;
		int nv = (ndir[nr_offset(y - 1, x)] & VER) + (ndir[nr_offset(y + 1, x)] & VER)
				+ (ndir[nr_offset(y, x - 1)] & VER) + (ndir[nr_offset(y, x + 1)] & VER);
		int nh = (ndir[nr_offset(y - 1, x)] & HOR) + (ndir[nr_offset(y + 1, x)] & HOR)
				+ (ndir[nr_offset(y, x - 1)] & HOR) + (ndir[nr_offset(y, x + 1)] & HOR);
		nv /= VER;
		nh /= HOR;
		if ((ndir[nr_offset(y, x)] & VER) && nh > 3)
			ndir[nr_offset(y, x)] = HOR;
		if ((ndir[nr_offset(y, x)] & HOR) && nv > 3)
			ndir[nr_offset(y, x)] = VER;

	}
}
void DHT::make_hv_dline(int i) {
	int iwidth = libraw.imgdata.sizes.iwidth;
	int js = libraw.COLOR(i, 0) & 1;
	int kc = libraw.COLOR(i, js);
	/*
	 * js -- начальная х-координата, которая попадает мимо известного зелёного
	 * kc -- известный цвет в точке интерполирования
	 */
	for (int j = 0; j < iwidth; j++) {
		int x = j + nr_leftmargin;
		int y = i + nr_topmargin;
		char d = 0;
		if ((j & 1) == js)
			d = get_hv_grb(x, y, kc);
		else
			d = get_hv_rbg(x, y, kc);
		ndir[nr_offset(y, x)] = d;
	}
}

void DHT::make_diag_dline(int i) {
	int iwidth = libraw.imgdata.sizes.iwidth;
	int js = libraw.COLOR(i, 0) & 1;
	int kc = libraw.COLOR(i, js);
	/*
	 * js -- начальная х-координата, которая попадает мимо известного зелёного
	 * kc -- известный цвет в точке интерполирования
	 */
	for (int j = 0; j < iwidth; j++) {
		int x = j + nr_leftmargin;
		int y = i + nr_topmargin;
		char d = 0;
		if ((j & 1) == js)
			d = get_diag_grb(x, y, kc);
		else
			d = get_diag_rbg(x, y, kc);
		ndir[nr_offset(y, x)] |= d;
	}
}

void DHT::refine_diag_dirs(int i, int js) {
	int iwidth = libraw.imgdata.sizes.iwidth;
	for (int j = js; j < iwidth; j += 2) {
		int x = j + nr_leftmargin;
		int y = i + nr_topmargin;
		if (ndir[nr_offset(y, x)] & 8)
			continue;
		int nv = (ndir[nr_offset(y - 1, x)] & LURD) + (ndir[nr_offset(y + 1, x)] & LURD)
				+ (ndir[nr_offset(y, x - 1)] & LURD) + (ndir[nr_offset(y, x + 1)] & LURD)
				+ (ndir[nr_offset(y - 1, x - 1)] & LURD) + (ndir[nr_offset(y - 1, x + 1)] & LURD)
				+ (ndir[nr_offset(y + 1, x - 1)] & LURD) + (ndir[nr_offset(y + 1, x + 1)] & LURD);
		int nh = (ndir[nr_offset(y - 1, x)] & RULD) + (ndir[nr_offset(y + 1, x)] & RULD)
				+ (ndir[nr_offset(y, x - 1)] & RULD) + (ndir[nr_offset(y, x + 1)] & RULD)
				+ (ndir[nr_offset(y - 1, x - 1)] & RULD) + (ndir[nr_offset(y - 1, x + 1)] & RULD)
				+ (ndir[nr_offset(y + 1, x - 1)] & RULD) + (ndir[nr_offset(y + 1, x + 1)] & RULD);
		bool codir =
				(ndir[nr_offset(y, x)] & LURD) ?
						((ndir[nr_offset(y - 1, x - 1)] & LURD)
								|| (ndir[nr_offset(y + 1, x + 1)] & LURD)) :
						((ndir[nr_offset(y - 1, x + 1)] & RULD)
								|| (ndir[nr_offset(y + 1, x - 1)] & RULD));
		nv /= LURD;
		nh /= RULD;
		if ((ndir[nr_offset(y, x)] & LURD) && (nh > 4 && !codir)) {
			ndir[nr_offset(y, x)] &= ~LURD;
			ndir[nr_offset(y, x)] |= RULD;
		}
		if ((ndir[nr_offset(y, x)] & RULD) && (nv > 4 && !codir)) {
			ndir[nr_offset(y, x)] &= ~RULD;
			ndir[nr_offset(y, x)] |= LURD;
		}
	}
}

void DHT::refine_idiag_dirs(int i) {
	int iwidth = libraw.imgdata.sizes.iwidth;
	for (int j = 0; j < iwidth; j++) {
		int x = j + nr_leftmargin;
		int y = i + nr_topmargin;
		if (ndir[nr_offset(y, x)] & 8)
			continue;
		int nv = (ndir[nr_offset(y - 1, x)] & LURD) + (ndir[nr_offset(y + 1, x)] & LURD)
				+ (ndir[nr_offset(y, x - 1)] & LURD) + (ndir[nr_offset(y, x + 1)] & LURD)
				+ (ndir[nr_offset(y - 1, x - 1)] & LURD) + (ndir[nr_offset(y - 1, x + 1)] & LURD)
				+ (ndir[nr_offset(y + 1, x - 1)] & LURD) + (ndir[nr_offset(y + 1, x + 1)] & LURD);
		int nh = (ndir[nr_offset(y - 1, x)] & RULD) + (ndir[nr_offset(y + 1, x)] & RULD)
				+ (ndir[nr_offset(y, x - 1)] & RULD) + (ndir[nr_offset(y, x + 1)] & RULD)
				+ (ndir[nr_offset(y - 1, x - 1)] & RULD) + (ndir[nr_offset(y - 1, x + 1)] & RULD)
				+ (ndir[nr_offset(y + 1, x - 1)] & RULD) + (ndir[nr_offset(y + 1, x + 1)] & RULD);
		bool codir =
				(ndir[nr_offset(y, x)] & LURD) ?
						((ndir[nr_offset(y - 1, x - 1)] & LURD)
								|| (ndir[nr_offset(y + 1, x + 1)] & LURD)) :
						((ndir[nr_offset(y - 1, x + 1)] & RULD)
								|| (ndir[nr_offset(y + 1, x - 1)] & RULD));
		nv /= LURD;
		nh /= RULD;
		if ((ndir[nr_offset(y, x)] & LURD) && nh > 7) {
			ndir[nr_offset(y, x)] &= ~LURD;
			ndir[nr_offset(y, x)] |= RULD;
		}
		if ((ndir[nr_offset(y, x)] & RULD) && nv > 7) {
			ndir[nr_offset(y, x)] &= ~RULD;
			ndir[nr_offset(y, x)] |= LURD;
		}
	}
}

/*
 * вычисление недостающих зелёных точек.
 */
void DHT::make_greens() {
#if defined(LIBRAW_USE_OPENMP)
#pragma omp parallel for schedule(guided)
#endif
	for (int i = 0; i < libraw.imgdata.sizes.iheight; ++i) {
		make_gline(i);
	}
}

void DHT::make_gline(int i) {
	int iwidth = libraw.imgdata.sizes.iwidth;
	int js = libraw.COLOR(i, 0) & 1;
	int kc = libraw.COLOR(i, js);
	/*
	 * js -- начальная х-координата, которая попадает мимо известного зелёного
	 * kc -- известный цвет в точке интерполирования
	 */
	for (int j = js; j < iwidth; j += 2) {
		int x = j + nr_leftmargin;
		int y = i + nr_topmargin;
		int dx, dy, dx2, dy2;
		float h1, h2;
		if (ndir[nr_offset(y, x)] & VER) {
			dx = dx2 = 0;
			dy = -1;
			dy2 = 1;
			h1 = 2 * nraw[nr_offset(y - 1, x)][1]
					/ (nraw[nr_offset(y - 2, x)][kc] + nraw[nr_offset(y, x)][kc]);
			h2 = 2 * nraw[nr_offset(y + 1, x)][1]
					/ (nraw[nr_offset(y + 2, x)][kc] + nraw[nr_offset(y, x)][kc]);
		} else {
			dy = dy2 = 0;
			dx = 1;
			dx2 = -1;
			h1 = 2 * nraw[nr_offset(y, x + 1)][1]
					/ (nraw[nr_offset(y, x + 2)][kc] + nraw[nr_offset(y, x)][kc]);
			h2 = 2 * nraw[nr_offset(y, x - 1)][1]
					/ (nraw[nr_offset(y, x - 2)][kc] + nraw[nr_offset(y, x)][kc]);
		}
		float b1 = 1
				/ calc_dist(nraw[nr_offset(y, x)][kc], nraw[nr_offset(y + dy * 2, x + dx * 2)][kc]);
		float b2 = 1
				/ calc_dist(nraw[nr_offset(y, x)][kc],
						nraw[nr_offset(y + dy2 * 2, x + dx2 * 2)][kc]);
		b1 *= b1;
		b2 *= b2;
		float eg = nraw[nr_offset(y, x)][kc] * (b1 * h1 + b2 * h2) / (b1 + b2);
		if (eg > channel_maximum[1])
			eg = channel_maximum[1];
		else if (eg < channel_minimum[1])
			eg = channel_minimum[1];
		nraw[nr_offset(y, x)][1] = eg;
	}
}

/*
 * отладочная функция
 */

void DHT::illustrate_dirs() {
#if defined(LIBRAW_USE_OPENMP)
#pragma omp parallel for schedule(guided)
#endif
	for (int i = 0; i < libraw.imgdata.sizes.iheight; ++i) {
		illustrate_dline(i);
	}
}

void DHT::illustrate_dline(int i) {
	int iwidth = libraw.imgdata.sizes.iwidth;
	for (int j = 0; j < iwidth; j++) {
		int x = j + nr_leftmargin;
		int y = i + nr_topmargin;
		nraw[nr_offset(y, x)][0] = nraw[nr_offset(y, x)][1] = nraw[nr_offset(y, x)][2] = 0.5;
		int l = ndir[nr_offset(y, x)] & 8;
		l >>= 3;
		if (ndir[nr_offset(y, x)] & RULD)
			nraw[nr_offset(y, x)][0] = l * channel_maximum[0] / 4 + channel_maximum[0] / 4;
		else
			nraw[nr_offset(y, x)][2] = l * channel_maximum[2] / 4 + channel_maximum[2] / 4;
	}

}

/*
 * интерполяция красных и синих.
 *
 * сначала интерполируются недостающие цвета, по диагональным направлениям от которых находятся известные,
 * затем ситуация сводится к тому как интерполировались зелёные.
 */

void DHT::make_rbdiag(int i) {
	int iwidth = libraw.imgdata.sizes.iwidth;
	int js = libraw.COLOR(i, 0) & 1;
	int uc = libraw.COLOR(i, js);
	int cl = uc ^ 2;
	/*
	 * js -- начальная х-координата, которая попадает на уже интерполированный зелёный
	 * al -- известный цвет (кроме зелёного) в точке интерполирования
	 * cl -- неизвестный цвет
	 */
	for (int j = js; j < iwidth; j += 2) {
		int x = j + nr_leftmargin;
		int y = i + nr_topmargin;
		int dx, dy, dx2, dy2;
		if (ndir[nr_offset(y, x)] & LURD) {
			dx = -1;
			dx2 = 1;
			dy = -1;
			dy2 = 1;
		} else {
			dx = -1;
			dx2 = 1;
			dy = 1;
			dy2 = -1;
		}
		float g1 = 1 / calc_dist(nraw[nr_offset(y, x)][1], nraw[nr_offset(y + dy, x + dx)][1]);
		float g2 = 1 / calc_dist(nraw[nr_offset(y, x)][1], nraw[nr_offset(y + dy2, x + dx2)][1]);
		g1 *= g1;
		g2 *= g2;

		float eg;
		eg = nraw[nr_offset(y, x)][1]
				* (g1 * nraw[nr_offset(y + dy, x + dx)][cl] / nraw[nr_offset(y + dy, x + dx)][1]
						+ g2 * nraw[nr_offset(y + dy2, x + dx2)][cl]
								/ nraw[nr_offset(y + dy2, x + dx2)][1]) / (g1 + g2);
		float max = nraw[nr_offset(y - 1, x - 1)][cl], min = nraw[nr_offset(y + 1, x + 1)][cl];
		for (int n = -1; n < 2; n += 2)
			for (int k = -1; k < 2; k += 2) {
				if (nraw[nr_offset(y + n, x + k)][cl] < min)
					min = nraw[nr_offset(y + n, x + k)][cl];
				if (nraw[nr_offset(y + n, x + k)][cl] > max)
					max = nraw[nr_offset(y + n, x + k)][cl];
			}
		max *= 1.4;
		min /= 1.4;
		if (eg > max)
			eg = max;
		else if (eg < min)
			eg = min;
		if (eg > channel_maximum[cl])
			eg = channel_maximum[cl];
		else if (eg < channel_minimum[cl])
			eg = channel_minimum[cl];
		nraw[nr_offset(y, x)][cl] = eg;
	}

}

/*
 * интерполяция красных и синих в точках где был известен только зелёный,
 * направления горизонтальные или вертикальные
 */

void DHT::make_rbhv(int i) {
	int iwidth = libraw.imgdata.sizes.iwidth;
	int js = (libraw.COLOR(i, 0) & 1) ^ 1;
	for (int j = js; j < iwidth; j += 2) {
		int x = j + nr_leftmargin;
		int y = i + nr_topmargin;
		/*
		 * поскольку сверху-снизу и справа-слева уже есть все необходимые красные и синие,
		 * то можно выбрать наилучшее направление исходя из информации по обоим цветам.
		 */
		int dx, dy, dx2, dy2;
		float h1, h2;
		if (ndir[nr_offset(y, x)] & VER) {
			dx = dx2 = 0;
			dy = -1;
			dy2 = 1;
		} else {
			dy = dy2 = 0;
			dx = 1;
			dx2 = -1;
		}
		float g1 = 1 / calc_dist(nraw[nr_offset(y, x)][1], nraw[nr_offset(y + dy, x + dx)][1]);
		float g2 = 1 / calc_dist(nraw[nr_offset(y, x)][1], nraw[nr_offset(y + dy2, x + dx2)][1]);
		g1 *= g1;
		g2 *= g2;
		float eg_r, eg_b;
		eg_r = nraw[nr_offset(y, x)][1]
				* (g1 * nraw[nr_offset(y + dy, x + dx)][0] / nraw[nr_offset(y + dy, x + dx)][1]
						+ g2 * nraw[nr_offset(y + dy2, x + dx2)][0]
								/ nraw[nr_offset(y + dy2, x + dx2)][1]) / (g1 + g2);
		eg_b = nraw[nr_offset(y, x)][1]
				* (g1 * nraw[nr_offset(y + dy, x + dx)][2] / nraw[nr_offset(y + dy, x + dx)][1]
						+ g2 * nraw[nr_offset(y + dy2, x + dx2)][2]
								/ nraw[nr_offset(y + dy2, x + dx2)][1]) / (g1 + g2);
		float max = nraw[nr_offset(y - 1, x)][0], min = nraw[nr_offset(y - 1, x)][0];
		for (int n = -1; n < 2; n++)
			for (int k = -1 + ((n + 2) & 1); k < 2; k += 2) {
				if (nraw[nr_offset(y + n, x + k)][0] < min)
					min = nraw[nr_offset(y + n, x + k)][0];
				if (nraw[nr_offset(y + n, x + k)][0] > max)
					max = nraw[nr_offset(y + n, x + k)][0];
			}
		max *= 1.4;
		min /= 1.4;
		if (eg_r > max)
			eg_r = max;
		else if (eg_r < min)
			eg_r = min;
		if (eg_r > channel_maximum[0])
			eg_r = channel_maximum[0];
		else if (eg_r < channel_minimum[0])
			eg_r = channel_minimum[0];
		max = nraw[nr_offset(y - 1, x)][2], min = nraw[nr_offset(y - 1, x)][2];
		for (int n = -1; n < 2; n++)
			for (int k = -1 + ((n + 2) & 1); k < 2; k += 2) {
				if (nraw[nr_offset(y + n, x + k)][2] < min)
					min = nraw[nr_offset(y + n, x + k)][2];
				if (nraw[nr_offset(y + n, x + k)][2] > max)
					max = nraw[nr_offset(y + n, x + k)][2];
			}
		max *= 1.4;
		min /= 1.4;
		if (eg_b > max)
			eg_b = max;
		else if (eg_b < min)
			eg_b = min;
		if (eg_b > channel_maximum[2])
			eg_b = channel_maximum[2];
		else if (eg_b < channel_minimum[2])
			eg_b = channel_minimum[2];
		nraw[nr_offset(y, x)][0] = eg_r;
		nraw[nr_offset(y, x)][2] = eg_b;
	}
}

void DHT::make_rb() {
#if defined(LIBRAW_USE_OPENMP)
#pragma omp barier
#pragma omp parallel for schedule(guided)
#endif
	for (int i = 0; i < libraw.imgdata.sizes.iheight; ++i) {
		make_rbdiag(i);
	}
#if defined(LIBRAW_USE_OPENMP)
#pragma omp barier
#pragma omp parallel for schedule(guided)
#endif
	for (int i = 0; i < libraw.imgdata.sizes.iheight; ++i) {
		make_rbhv(i);
	}

}

/*
 * перенос изображения в выходной массив
 */
void DHT::copy_to_image() {
	int iwidth = libraw.imgdata.sizes.iwidth;
#if defined(LIBRAW_USE_OPENMP)
#ifdef WIN32
#pragma omp parallel for
#else
#pragma omp parallel for schedule(guided) collapse(2)
#endif
#endif
	for (int i = 0; i < libraw.imgdata.sizes.iheight; ++i) {
		for (int j = 0; j < iwidth; ++j) {
			libraw.imgdata.image[i * iwidth + j][0] = (unsigned short) (nraw[nr_offset(
					i + nr_topmargin, j + nr_leftmargin)][0]);
			libraw.imgdata.image[i * iwidth + j][2] = (unsigned short) (nraw[nr_offset(
					i + nr_topmargin, j + nr_leftmargin)][2]);
			libraw.imgdata.image[i * iwidth + j][1] = libraw.imgdata.image[i * iwidth + j][3] =
					(unsigned short) (nraw[nr_offset(i + nr_topmargin, j + nr_leftmargin)][1]);
		}
	}
	free(nraw);
	free(ndir);
}

void LibRaw::dht_interpolate() {
	printf("DHT interpolating\n");
	DHT dht(*this);
	dht.make_hv_dirs();
//	dht.illustrate_dirs();
	dht.make_greens();
	dht.make_diag_dirs();
//	dht.illustrate_dirs();
	dht.make_rb();
	dht.copy_to_image();

}
