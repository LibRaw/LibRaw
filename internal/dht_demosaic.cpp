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
	float hue_diff;
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

#if 0
static inline float abs(float c) {
	return c > 0 ? c : -c;
}
#endif

struct DHT {
	int nr_height, nr_width;
	static const int nr_topmargin = 2, nr_leftmargin = 2;
	float (*nraw)[3];
	ushort channel_maximum[3], channel_minimum[3];
	LibRaw &libraw;
	int nr_offset(int row, int col) {
		return (row * nr_width + col);
	}
	/*
	 * вычисление параметров для цвета, который окружён известными цветами горизонтально и вертикально
	 */
	void set_g(ecdir &e, int x, int y, int al, signed char dx, signed char dy, signed char dx2,
			signed char dy2) {
		if ((nraw[nr_offset(y + dy * 2, x + dx * 2)][al] + nraw[nr_offset(y, x)][al])
				> 2 * nraw[nr_offset(y + dy, x + dx)][1])
			/*
			 * смещение оттенка пытаемся вычислять всегда так, чтобы коэффициент получался меньше 1
			 */
			e.hue_diff = fabs(
					(nraw[nr_offset(y + dy, x + dx)][1]
							/ (nraw[nr_offset(y + dy * 2, x + dx * 2)][al]
									+ nraw[nr_offset(y, x)][al]))
							- (nraw[nr_offset(y + dy2, x + dx2)][1]
									/ (nraw[nr_offset(y + dy2 * 2, x + dx2 * 2)][al]
											+ nraw[nr_offset(y, x)][al]))) * 2;
		else
			e.hue_diff = fabs(
					((nraw[nr_offset(y + dy * 2, x + dx * 2)][al] + nraw[nr_offset(y, x)][al])
							/ nraw[nr_offset(y + dy, x + dx)][1])
							- ((nraw[nr_offset(y + dy2 * 2, x + dx2 * 2)][al]
									+ nraw[nr_offset(y, x)][al])
									/ nraw[nr_offset(y + dy2, x + dx2)][1])) / 2;
		/*
		 * дистанция учитывает разницу не только непосредственно прилегающих горизонтальных или вертикальных значений,
		 * но и разницу между известными цветами соответствующих направлений. это помогает фильтровать stripe pattern
		 */
		float c = calc_dist(nraw[nr_offset(y + dy, x + dx)][1],
				nraw[nr_offset(y + dy2, x + dx2)][1]);
		e.dist = c // * c * c
		* calc_dist(nraw[nr_offset(y + dy * 2, x + dx * 2)][al], nraw[nr_offset(y, x)][al])
				* calc_dist(nraw[nr_offset(y + dy2 * 2, x + dx2 * 2)][al],
						nraw[nr_offset(y, x)][al])

						;
//		if (dx == 0 && dx2 == 0)
//			e.dist *= calc_dist(nraw[nr_offset(y + dy, x - 1)][al ^ 2],
//					nraw[nr_offset(y + dy2, x - 1)][al ^ 2])
//					* calc_dist(nraw[nr_offset(y + dy, x + 1)][al ^ 2],
//							nraw[nr_offset(y + dy2, x + 1)][al ^ 2]);
//		else if (dy == 0 && dy2 == 0)
//			e.dist *= calc_dist(nraw[nr_offset(y - 1, x + dx)][al ^ 2],
//					nraw[nr_offset(y - 1, x + dx2)][al ^ 2])
//					* calc_dist(nraw[nr_offset(y + 1, x + dx)][al ^ 2],
//							nraw[nr_offset(y + 1, x + +dx2)][al ^ 2]);
		e.set(dx, dy, dx2, dy2);
	}
	void set_atg(ecdir &e, int x, int y, int al, signed char dx, signed char dy, signed char dx2,
			signed char dy2) {
		if (nraw[nr_offset(y + dy, x + dx)][al] > nraw[nr_offset(y + dy, x + dx)][1])
			e.hue_diff = fabs(
					nraw[nr_offset(y + dy, x + dx)][1] / nraw[nr_offset(y + dy, x + dx)][al]
							- nraw[nr_offset(y + dy2, x + dx2)][1]
									/ nraw[nr_offset(y + dy2, x + dx2)][al]);
		else
			e.hue_diff = fabs(
					nraw[nr_offset(y + dy, x + dx)][al] / nraw[nr_offset(y + dy, x + dx)][1]
							- nraw[nr_offset(y + dy2, x + dx2)][al]
									/ nraw[nr_offset(y + dy2, x + dx2)][1]);
		e.dist = calc_dist(nraw[nr_offset(y + dy, x + dx)][al],
				nraw[nr_offset(y + dy2, x + dx2)][al])
				* calc_dist(nraw[nr_offset(y + dy, x + dx)][1], nraw[nr_offset(y, x)][1])
				* calc_dist(nraw[nr_offset(y + dy2, x + dx2)][1], nraw[nr_offset(y, x)][1]);
		e.set(dx, dy, dx2, dy2);
	}
	/*
	 * вычисление параметров для цвета, который окружён известными цветами по диагонали на основе известного зелёного цвета
	 */
	void set_rb(ecdir &e, int x, int y, int al, signed char dx, signed char dy, signed char dx2,
			signed char dy2) {
//		if (nraw[nr_offset(y + dy, x + dx)][1] < nraw[nr_offset(y + dy, x + dx)][al])
//			e.hue_diff = fabs(
//					nraw[nr_offset(y + dy, x + dx)][1] / nraw[nr_offset(y + dy, x + dx)][al]
//							- nraw[nr_offset(y + dy2, x + dx2)][1]
//									/ nraw[nr_offset(y + dy2, x + dx2)][al]);
//		else
//			e.hue_diff = fabs(
//					nraw[nr_offset(y + dy, x + dx)][al] / nraw[nr_offset(y + dy, x + dx)][1]
//							- nraw[nr_offset(y + dy2, x + dx2)][al]
//									/ nraw[nr_offset(y + dy2, x + dx2)][1]);
		e.dist = calc_dist(nraw[nr_offset(y + dy, x + dx)][al],
				nraw[nr_offset(y + dy2, x + dx2)][al])
				* calc_dist(nraw[nr_offset(y + dy, x + dx)][1],
						nraw[nr_offset(y + dy2, x + dx2)][1])
				* calc_dist(nraw[nr_offset(y + dy, x + dx)][1], nraw[nr_offset(y, x)][1]);
		e.set(dx, dy, dx2, dy2);
	}
	DHT(LibRaw &_libraw);
	void copy_to_image();
	void make_greens();
	void make_gline(int i);
	void make_rbdiag(int i);
	void make_rbhv(int i);
	void make_rb();
};

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
	channel_maximum[0] = channel_maximum[1] = channel_maximum[2] = 0;
	channel_minimum[0] = libraw.imgdata.image[0][0];
	channel_minimum[1] = libraw.imgdata.image[0][1];
	channel_minimum[2] = libraw.imgdata.image[0][2];
	for (int i = 0; i < nr_height * nr_width; ++i)
		nraw[i][0] = nraw[i][1] = nraw[i][2] = 0.5;
	for (int i = 0; i < libraw.imgdata.sizes.iheight; ++i) {
		int col_cache[48];
		for (int j = 0; j < 48; ++j)
			col_cache[j] = libraw.COLOR(i, j);
		for (int j = 0; j < libraw.imgdata.sizes.iwidth; ++j) {
			int l = col_cache[j % 48];
			unsigned short c = libraw.imgdata.image[i * libraw.imgdata.sizes.iwidth + j][l];
			if (c != 0) {
				if (l == 3)
					l = 1;
				if (channel_maximum[l] < c)
					channel_maximum[l] = c;
				if (channel_minimum[l] > c)
					channel_minimum[l] = c;
				nraw[nr_offset(i + nr_topmargin, j + nr_leftmargin)][l] = (float) c;
			}
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
	int js = libraw.COLOR(i, 0) & 1;
	int al = libraw.COLOR(i, js);
	/*
	 * js -- начальная х-координата, которая попадает мимо известного зелёного
	 * al -- известный цвет в точке интерполирования
	 */
	int iwidth = libraw.imgdata.sizes.iwidth;
	for (int j = js; j < iwidth; j += 2) {
		ecdir gd[6];
		int nrx = j + nr_leftmargin;
		int nry = i + nr_topmargin;
		/*
		 * горизонтальные и вертикальные направления
		 */
		set_g(gd[0], nrx, nry, al, 0, -1, 0, 1);
		set_g(gd[1], nrx, nry, al, -1, 0, 1, 0);
		/*
		 * направления уголком
		 */
		set_g(gd[2], nrx, nry, al, 0, -1, 1, 0);
		set_g(gd[3], nrx, nry, al, 1, 0, 0, 1);
		set_g(gd[4], nrx, nry, al, 0, 1, -1, 0);
		set_g(gd[5], nrx, nry, al, -1, 0, 0, -1);
		/*
		 * массив направлений для интерполяции.
		 * сначала данные были общими, но вертикальные и горизонтальные направления дают намного меньше артефактов.
		 * причина, скорее всего, в том, что направление интерполяции должно проходить через интерполируемую точку.
		 */

		if (gd[0].dist > gd[1].dist) {
			ecdir t = gd[1];
			gd[1] = gd[0];
			gd[0] = t;
		}
		/*
		 * в случае, когда изменение оттенка не очень существенно, то выбираем более подходящую дистанцию
		 *
		 *
		 * хорошо бы свести два условия в одно
		 */
		if (gd[0].hue_diff > 0.5 && gd[1].hue_diff < 0.5) {
			ecdir t = gd[1];
			gd[1] = gd[0];
			gd[0] = t;
		}

		/*
		 * сортировка уголковых направлений. подлинная сортировка не нужна, достаточно вычислить наилучшее направление.
		 */
		for (int n = 4; n > 1; --n) {
			if (gd[n].dist > gd[n + 1].dist) {
				ecdir t = gd[n + 1];
				gd[n + 1] = gd[n];
				gd[n] = t;
			}
		}
		for (int n = 2; n < 5; ++n) {
			if (gd[n].hue_diff < 0.5) {
				if (n != 2) {
					ecdir t = gd[2];
					gd[2] = gd[n];
					gd[n] = t;
					break;
				}
			}
		}
		float eg;
		/*
		 * интерполяция по перенесённому оттенку с коэффициентами соответственно дистанции.
		 */
		float b1 = 1
				/ calc_dist(nraw[nr_offset(nry, nrx)][al],
						nraw[nr_offset(nry + gd[0].dy * 2, nrx + gd[0].dx * 2)][al]);
		float b2 = 1
				/ calc_dist(nraw[nr_offset(nry, nrx)][al],
						nraw[nr_offset(nry + gd[0].dy2 * 2, nrx + gd[0].dx2 * 2)][al]);
		b1 *= b1;
		b2 *= b2;
		eg = nraw[nr_offset(nry, nrx)][al]
				* (b1 * nraw[nr_offset(nry + gd[0].dy, nrx + gd[0].dx)][1]
						/ (nraw[nr_offset(nry + gd[0].dy * 2, nrx + gd[0].dx * 2)][al]
								+ nraw[nr_offset(nry, nrx)][al])
						+ b2 * nraw[nr_offset(nry + gd[0].dy2, nrx + gd[0].dx2)][1]
								/ (nraw[nr_offset(nry + gd[0].dy2 * 2, nrx + gd[0].dx2 * 2)][al]
										+ nraw[nr_offset(nry, nrx)][al])) / (b1 + b2) * 2;
		/*
		 * если изменение оттенка по уголковому направлению "не существенно", то его учтём с коэффициентом 1:3.
		 *
		 * коэффициент 0.05 выбран достаточно произвольно.
		 */

		if (fabs(gd[0].hue_diff - gd[2].hue_diff) < 0.05) {
			eg *= 3;
			eg += nraw[nr_offset(nry, nrx)][al]
					* (nraw[nr_offset(nry + gd[2].dy, nrx + gd[2].dx)][1]
							/ (nraw[nr_offset(nry + gd[2].dy * 2, nrx + gd[2].dx * 2)][al]
									+ nraw[nr_offset(nry, nrx)][al])
							+ nraw[nr_offset(nry + gd[2].dy2, nrx + gd[2].dx2)][1]
									/ (nraw[nr_offset(nry + gd[2].dy2 * 2, nrx + gd[2].dx2 * 2)][al]
											+ nraw[nr_offset(nry, nrx)][al]));
			eg /= 4;
		}
		if (eg > channel_maximum[1])
			eg = channel_maximum[1];
		else if (eg < channel_minimum[1])
			eg = channel_minimum[1];
		nraw[nr_offset(nry, nrx)][1] = eg;
	}

}

/*
 * интерполяция красных и синих.
 *
 * сначала интерполируются недостающие цвета, по диагональным направлениям от которых находятся известные,
 * затем ситуация сводится к тому как интерполировались зелёные.
 */

void DHT::make_rbdiag(int i) {
	int js = libraw.COLOR(i, 0) & 1;
	int al = libraw.COLOR(i, js);
	int cl = al ^ 2;
	/*
	 * js -- начальная х-координата, которая попадает на уже интерполированный зелёный
	 * al -- известный цвет (кроме зелёного) в точке интерполирования
	 * cl -- неизвестный цвет
	 */
	int iwidth = libraw.imgdata.sizes.iwidth;
	for (int j = js; j < iwidth; j += 2) {
		ecdir rb[4];
		int nrx = j + nr_leftmargin;
		int nry = i + nr_topmargin;
		set_rb(rb[0], nrx, nry, cl, -1, -1, 1, 1);
		set_rb(rb[1], nrx, nry, cl, -1, 1, 1, -1);
		if (rb[0].dist > rb[1].dist) {
			ecdir t = rb[1];
			rb[1] = rb[0];
			rb[0] = t;
		}
		float eg;
		if (nraw[nr_offset(nry, nrx)][1] > 1.4 * nraw[nr_offset(nry + rb[0].dy, nrx + rb[0].dx)][1]
				&& nraw[nr_offset(nry, nrx)][1]
						> 1.4 * nraw[nr_offset(nry + rb[0].dy2, nrx + rb[0].dx2)][1]) {
			/* неизбежно усиление оттенка */
			rb[0].dist = nraw[nr_offset(nry, nrx)][1] / nraw[nr_offset(nry - 1, nrx - 1)][1];
			rb[0].set(-1, -1, 0, 0);
			rb[1].dist = nraw[nr_offset(nry, nrx)][1] / nraw[nr_offset(nry - 1, nrx + 1)][1];
			rb[1].set(1, -1, 0, 0);
			rb[2].dist = nraw[nr_offset(nry, nrx)][1] / nraw[nr_offset(nry + 1, nrx + 1)][1];
			rb[2].set(1, 1, 0, 0);
			rb[3].dist = nraw[nr_offset(nry, nrx)][1] / nraw[nr_offset(nry + 1, nrx - 1)][1];
			rb[3].set(-1, 1, 0, 0);
			for (int n = 2; n > -1; --n) {
				if (fabs(rb[n].dist - 1) > fabs(rb[n + 1].dist - 1)) {
					ecdir t = rb[n + 1];
					rb[n + 1] = rb[n];
					rb[n] = t;
				}
			}
			eg = nraw[nr_offset(nry, nrx)][1] * nraw[nr_offset(nry + rb[0].dy, nrx + rb[0].dx)][cl]
					/ nraw[nr_offset(nry + rb[0].dy, nrx + rb[0].dx)][1];
		} else {
			float g1 = 1
					/ calc_dist(nraw[nr_offset(nry, nrx)][1],
							nraw[nr_offset(nry + rb[0].dy, nrx + rb[0].dx)][1]);
			float g2 = 1
					/ calc_dist(nraw[nr_offset(nry, nrx)][1],
							nraw[nr_offset(nry + rb[0].dy2, nrx + rb[0].dx2)][1]);
			g1 *= g1;
			g2 *= g2;
			eg = nraw[nr_offset(nry, nrx)][1]
					* (g1 * nraw[nr_offset(nry + rb[0].dy, nrx + rb[0].dx)][cl]
							/ nraw[nr_offset(nry + rb[0].dy, nrx + rb[0].dx)][1]
							+ g2 * nraw[nr_offset(nry + rb[0].dy2, nrx + rb[0].dx2)][cl]
									/ nraw[nr_offset(nry + rb[0].dy2, nrx + rb[0].dx2)][1])
					/ (g1 + g2);
		}
		if (eg > channel_maximum[cl])
			eg = channel_maximum[cl];
		else if (eg < channel_minimum[cl])
			eg = channel_minimum[cl];
		nraw[nr_offset(nry, nrx)][cl] = eg;
	}

}

/*
 * интерполяция красных и синих в точках где был известен только зелёный,
 * направления горизонтальные или вертикальные
 */

void DHT::make_rbhv(int i) {
	int js = (libraw.COLOR(i, 0) & 1) ^ 1;
	int iwidth = libraw.imgdata.sizes.iwidth;
	for (int j = js; j < iwidth; j += 2) {
		ecdir rb[4];
		int nrx = j + nr_leftmargin;
		int nry = i + nr_topmargin;
		/*
		 * поскольку сверху-снизу и справа-слева уже есть все необходимые красные и синие,
		 * то можно выбрать наилучшее направление исходя из информации по обоим цветам.
		 */
		set_atg(rb[0], nrx, nry, 0, 0, -1, 0, 1);
		set_atg(rb[1], nrx, nry, 0, -1, 0, 1, 0);
		set_atg(rb[2], nrx, nry, 2, 0, -1, 0, 1);
		set_atg(rb[3], nrx, nry, 2, -1, 0, 1, 0);
		for (int k = -1; k < 2; ++k) {
			bool go = true;
			for (int n = 2; n > k; --n) {
				if (rb[n].dist > rb[n + 1].dist) {
					ecdir t = rb[n + 1];
					rb[n + 1] = rb[n];
					rb[n] = t;
					go = false;
				}
			}
			if (go)
				break;
		}
		for (int n = 0; n < 4; ++n) {
			if (rb[n].hue_diff < 0.5) {
				if (n != 0) {
					rb[0] = rb[n];
					break;
				}
			}
		}
		float g1 = 1
				/ calc_dist(nraw[nr_offset(nry, nrx)][1],
						nraw[nr_offset(nry + rb[0].dy, nrx + rb[0].dx)][1]);
		float g2 = 1
				/ calc_dist(nraw[nr_offset(nry, nrx)][1],
						nraw[nr_offset(nry + rb[0].dy2, nrx + rb[0].dx2)][1]);
		g1 *= g1;
		g2 *= g2;
		float eg_r, eg_b;
		eg_r = nraw[nr_offset(nry, nrx)][1]
				* (g1 * nraw[nr_offset(nry + rb[0].dy, nrx + rb[0].dx)][0]
						/ nraw[nr_offset(nry + rb[0].dy, nrx + rb[0].dx)][1]
						+ g2 * nraw[nr_offset(nry + rb[0].dy2, nrx + rb[0].dx2)][0]
								/ nraw[nr_offset(nry + rb[0].dy2, nrx + rb[0].dx2)][1]) / (g1 + g2);
		eg_b = nraw[nr_offset(nry, nrx)][1]
				* (g1 * nraw[nr_offset(nry + rb[0].dy, nrx + rb[0].dx)][2]
						/ nraw[nr_offset(nry + rb[0].dy, nrx + rb[0].dx)][1]
						+ g2 * nraw[nr_offset(nry + rb[0].dy2, nrx + rb[0].dx2)][2]
								/ nraw[nr_offset(nry + rb[0].dy2, nrx + rb[0].dx2)][1]) / (g1 + g2);

		if (eg_r > channel_maximum[0])
			eg_r = channel_maximum[0];
		else if (eg_r < channel_minimum[0])
			eg_r = channel_minimum[0];
		if (eg_b > channel_maximum[2])
			eg_b = channel_maximum[2];
		else if (eg_b < channel_minimum[2])
			eg_b = channel_minimum[2];
		nraw[nr_offset(nry, nrx)][0] = eg_r;
		nraw[nr_offset(nry, nrx)][2] = eg_b;
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
#if defined(LIBRAW_USE_OPENMP)
#ifdef WIN32
#pragma omp parallel for
#else
#pragma omp parallel for schedule(guided) collapse(2)
#endif
#endif
	for (int i = 0; i < libraw.imgdata.sizes.iheight; ++i) {
		for (int j = 0; j < libraw.imgdata.sizes.iwidth; ++j) {
			libraw.imgdata.image[i * libraw.imgdata.sizes.iwidth + j][0] =
					(unsigned short) (nraw[nr_offset(i + nr_topmargin, j + nr_leftmargin)][0]);
			libraw.imgdata.image[i * libraw.imgdata.sizes.iwidth + j][2] =
					(unsigned short) (nraw[nr_offset(i + nr_topmargin, j + nr_leftmargin)][2]);
			libraw.imgdata.image[i * libraw.imgdata.sizes.iwidth + j][1] = libraw.imgdata.image[i
					* libraw.imgdata.sizes.iwidth + j][3] = (unsigned short) (nraw[nr_offset(
					i + nr_topmargin, j + nr_leftmargin)][1]);
		}
	}
	free(nraw);
}

void LibRaw::dht_interpolate() {
	printf("DHT interpolating\n");
	DHT dht(*this);
	dht.make_greens();
	dht.make_rb();
	dht.copy_to_image();

}
