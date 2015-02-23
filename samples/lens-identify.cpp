/* -*- C++ -*-
 * File: identify.cpp
 * Copyright 2008-2013 LibRaw LLC (info@libraw.org)
 * Created: Sat Mar  8, 2008
 *
 * LibRaw C++ demo: emulates dcraw -i [-v]
 *

 LibRaw is free software; you can redistribute it and/or modify
 it under the terms of the one of three licenses as you choose:

 1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
 (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

 2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
 (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 3. LibRaw Software License 27032010
 (See file LICENSE.LibRaw.pdf provided in LibRaw distribution archive for details).


*/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "libraw/libraw.h"

// lens & camera mounts
#define	Mnt_Unknown 0
#define Minolta_A		1	// Minolta
#define Sony_E			2	// Sony
#define Canon_EF		3
#define Canon_EF_S	4	// Canon short
#define	Canon_EF_M	5	// Canon mirrorless
#define Nikon_F			6
#define Nikon_CX		7	// used in Nikon 1 series
#define FT					8	// original 4/3
#define mFT					9	// micro 4/3
#define Pentax_K		10
#define Pentax_Q		11
#define Pentax_645	12
#define Fuji_X			13
#define Leica_M			14
#define Leica_R			15
#define Leica_S			16
#define Samsung_NX	17
#define RicohModule	18
#define Samsung_NX_M	19
#define Leica_T     20
#define Contax_N    21
#define Sigma_X3F   22
#define FixedLens		99

// lens & camera formats, to differentiate Sony F/FE A/DT, etc.
#define APSC	1
#define FF		2
#define MF		3
#define APSH 	4

#include "external/FixedLens.h"
#include "external/lens_types.h"
#include "external/CanonLens.h"
#include "external/NikonLens.h"
#include "external/OlympusLens.h"
#include "external/PentaxLens.h"
#include "external/SamsungLens.h"
#include "external/SonyLens.h"

static lens_t RicohModuleLensList [] = {
  {1,	0,	50,	50,	2.5f,	2.5f,	"GR Lens A12 50mm F2.5 Macro"},
  {2,	0,	24,	70,	2.5f,	4.4f,	"Ricoh Lens S10 24-70mm F2.5-4.4 VC"},
  {3,	0,	28,	300,	3.5f,	5.6f,	"Ricoh Lens P10 28-300mm F3.5-5.6 VC"},
  {5,	0,	28,	28,	2.5f,	2.5f,	"GR Lens A12 28mm F2.5"},
  {6,	0,	24,	85,	3.5f,	5.5f,	"Ricoh Lens A16 24-85mm F3.5-5.5"},
  {0xFFFFFFFFFFFFFFFF,	0,	0,	0,	0.0f,	0.0f,	"Unknown"},
};
#define RicohModuleLensList_nEntries (sizeof(RicohModuleLensList) / sizeof(lens_t))

static lens_t SigmaX3FLensList [] = {
  {16,	0,	18, 50, 3.5f, 5.6f, "Sigma 18-50mm F3.5-5.6 DC"},
  {103,	0,	180, 180, 3.5f, 3.5f, "Sigma 180mm F3.5 EX IF HSM APO MACRO"},
  {104,	0,	150, 150, 2.8f, 2.8f, "Sigma 150mm F2.8 EX DG HSM APO MACRO"},
  {105,	0,	180, 180, 3.5f, 3.5f, "Sigma 180mm F3.5 EX DG HSM APO MACRO"},
  {106,	0,	150, 150, 2.8f, 2.8f, "Sigma 150mm F2.8 EX DG OS HSM APO MACRO"},
  {107,	0,	180, 180, 2.8f, 2.8f, "Sigma 180mm F2.8 EX DG OS HSM APO MACRO"},
  {129,	0,	14, 14, 2.8f, 2.8f, "Sigma 14mm F2.8 EX ASP"},
  {129,	1,	30, 30, 1.4f, 1.4f, "Sigma 30mm F1.4"},
  {131,	0,	17, 70, 2.8f, 4.5f, "Sigma 17-70mm F2.8-4.5 DC Macro"},
  {134,	0,	100, 300, 4.0f, 4.0f, "Sigma 100-300mm F4 EX DG HSM APO"},
  {135,	0,	120, 300, 2.8f, 2.8f, "Sigma 120-300mm F2.8 EX DG HSM APO"},
  {136,	0,	120, 300, 2.8f, 2.8f, "Sigma 120-300mm F2.8 EX DG OS HSM APO"},
  {137,	0,	120, 300, 2.8f, 2.8f, "Sigma 120-300mm F2.8 DG OS HSM | S"},
  {143,	0,	600, 600, 8.0f, 8.0f, "Sigma 600mm F8 Mirror"},
  {145,	0,	15, 30, 3.5f, 4.5f, "Sigma 15-30mm F3.5-4.5 EX DG ASP"},
  {145,	1,	18, 50, 2.8f, 2.8f, "Sigma 18-50mm F2.8 EX DG"},
  {145,	2,	20, 40, 2.8f, 2.8f, "Sigma 20-40mm F2.8 EX DG"},
//  {145,	3,	17, 70, 2.8f, 4.5f, "Sigma 17-70mm F2.8-4.5"},
  {152,	0,	800, 800, 5.6f, 5.6f, "Sigma 800mm F5.6 EX DG HSM APO"},
  {165,	0,	70, 200, 2.8f, 2.8f, "Sigma 70-200mm F2.8 EX"},
  {169,	0,	18, 50, 2.8f, 2.8f, "Sigma 18-50mm F2.8 EX DC"},
  {183,	0,	500, 500, 4.5f, 4.5f, "Sigma 500mm F4.5 EX HSM APO"},
  {184,	0,	500, 500, 4.5f, 4.5f, "Sigma 500mm F4.5 EX DG HSM APO"},
  {194,	0,	300, 300, 2.8f, 2.8f, "Sigma 300mm F2.8 EX HSM APO"},
  {195,	0,	300, 300, 2.8f, 2.8f, "Sigma 300mm F2.8 EX DG HSM APO"},
  {200,	0,	12, 24, 4.5f, 5.6f, "Sigma 12-24mm F4.5-5.6 EX DG ASP HSM"},
  {201,	0,	10, 20, 4.0f, 5.6f, "Sigma 10-20mm F4-5.6 EX DC HSM"},
  {202,	0,	10, 20, 3.5f, 3.5f, "Sigma 10-20mm F3.5 EX DC HSM"},
  {203,	0,	8, 16, 4.5f, 5.6f, "Sigma 8-16mm F4.5-5.6 DC HSM"},
  {204,	0,	12, 24, 4.5f, 5.6f, "Sigma 12-24mm F4.5-5.6 DG HSM II"},
  {210,	0,	18, 35, 1.8f, 1.8f, "Sigma 18-35mm F1.8 DC HSM | A"},
  {256,	0,	105, 105, 2.8f, 2.8f, "Sigma 105mm F2.8 EX MACRO"},
  {257,	0,	105, 105, 2.8f, 2.8f, "Sigma 105mm F2.8 EX DG MACRO"},
  {258,	0,	105, 105, 2.8f, 2.8f, "Sigma 105mm F2.8 EX DG OS HSM MACRO"},
  {270,	0,	70, 70, 2.8f, 2.8f, "Sigma 70mm F2.8 EX DG MACRO"},
  {300,	0,	30, 30, 1.4f, 1.4f, "Sigma 30mm F1.4 EX DC HSM"},
  {301,	0,	30, 30, 1.4f, 1.4f, "Sigma 30mm F1.4 DC HSM | A"},
  {310,	0,	50, 50, 1.4f, 1.4f, "Sigma 50mm F1.4 EX DG HSM"},
  {311,	0,	50, 50, 1.4f, 1.4f, "Sigma 50mm F1.4 DG HSM | A"},
  {320,	0,	85, 85, 1.4f, 1.4f, "Sigma 85mm F1.4 EX DG HSM"},
  {340,	0,	35, 35, 1.4f, 1.4f, "Sigma 35mm F1.4 DG HSM | A"},
  {345,	0,	50, 50, 2.8f, 2.8f, "Sigma 50mm F2.8 EX MACRO"},
  {346,	0,	50, 50, 2.8f, 2.8f, "Sigma 50mm F2.8 EX DG MACRO"},
  {401,	0,	24, 24, 1.4f, 1.4f, "Sigma 24mm F1.4 DG  HSM | A"},
  {411,	0,	20, 20, 1.8f, 1.8f, "Sigma 20mm F1.8 EX DG ASP RF"},
  {432,	0,	24, 24, 1.8f, 1.8f, "Sigma 24mm F1.8 EX DG ASP Macro"},
  {440,	0,	28, 28, 1.8f, 1.8f, "Sigma 28mm F1.8 EX DG ASP Macro"},
  {461,	0,	14, 14, 2.8f, 2.8f, "Sigma 14mm F2.8 EX ASP HSM"},
  {475,	0,	15, 15, 2.8f, 2.8f, "Sigma 15mm F2.8 EX Diagonal FishEye"},
  {476,	0,	15, 15, 2.8f, 2.8f, "Sigma 15mm F2.8 EX DG Diagonal Fisheye"},
  {477,	0,	10, 10, 2.8f, 2.8f, "Sigma 10mm F2.8 EX DС HSM Fisheye"},
  {483,	0,	8, 8, 4.0f, 4.0f, "Sigma 8mm F4 EX Circular Fisheye"},
  {484,	0,	8, 8, 4.0f, 4.0f, "Sigma 8mm F4 EX DG Circular Fisheye"},
  {485,	0,	8, 8, 3.5f, 3.5f, "Sigma 8mm F3.5 EX DG Circular Fisheye"},
  {486,	0,	4, 4, 2.8f, 2.8f, "Sigma 4.5mm F2.8 EX DC HSM Circular Fisheye"},
  {506,	0,	70, 300, 4.0f, 5.6f, "Sigma 70-300mm F4-5.6 APO Macro Super II"},
  {507,	0,	70, 300, 4.0f, 5.6f, "Sigma 70-300mm F4-5.6 DL Macro Super II"},
  {508,	0,	70, 300, 4.0f, 5.6f, "Sigma 70-300mm F4-5.6 DG APO Macro"},
  {509,	0,	70, 300, 4.0f, 5.6f, "Sigma 70-300mm F4-5.6 DG Macro"},
  {510,	0,	17, 35, 2.8f, 4.0f, "Sigma 17-35 F2.8-4 EX DG ASP"},
  {512,	0,	15, 30, 3.5f, 4.5f, "Sigma 15-30mm F3.5-4.5 EX DG ASP DF"},
  {513,	0,	20, 40, 2.8f, 2.8f, "Sigma 20-40mm F2.8 EX DG"},
  {519,	0,	17, 35, 2.8f, 4.0f, "Sigma 17-35 F2.8-4 EX ASP HSM"},
  {520,	0,	100, 300, 4.5f, 6.7f, "Sigma 100-300mm F4.5-6.7 DL"},
  {521,	0,	18, 50, 3.5f, 5.6f, "Sigma 18-50mm F3.5-5.6 DC Macro"},
  {527,	0,	100, 300, 4.0f, 4.0f, "Sigma 100-300mm F4 EX IF HSM"},
  {529,	0,	120, 300, 2.8f, 2.8f, "Sigma 120-300mm F2.8 EX HSM IF APO"},
  {547,	0,	24, 60, 2.8f, 2.8f, "Sigma 24-60mm F2.8 EX DG"},
  {548,	0,	24, 70, 2.8f, 2.8f, "Sigma 24-70mm F2.8 EX DG Macro"},
  {549,	0,	28, 70, 2.8f, 2.8f, "Sigma 28-70mm F2.8 EX DG"},
  {566,	0,	70, 200, 2.8f, 2.8f, "Sigma 70-200mm F2.8 EX IF APO"},
  {567,	0,	70, 200, 2.8f, 2.8f, "Sigma 70-200mm F2.8 EX IF HSM APO"},
  {568,	0,	70, 200, 2.8f, 2.8f, "Sigma 70-200mm F2.8 EX DG IF HSM APO"},
  {569,	0,	70, 200, 2.8f, 2.8f, "Sigma 70-200 F2.8 EX DG HSM APO Macro"},
  {571,	0,	24, 70, 2.8f, 2.8f, "Sigma 24-70mm F2.8 EX DG HSM IF"},
  {572,	0,	70, 300, 4.0f, 5.6f, "Sigma 70-300mm F4-5.6 DG OS"},
  {579,	0,	70, 200, 2.8f, 2.8f, "Sigma 70-200mm F2.8 EX DG HSM APO Macro"},
  {580,	0,	18, 50, 2.8f, 2.8f, "Sigma 18-50mm F2.8 EX DC"},
  {581,	0,	18, 50, 2.8f, 2.8f, "Sigma 18-50mm F2.8 EX DC Macro"},
  {582,	0,	18, 50, 2.8f, 2.8f, "Sigma 18-50mm F2.8 EX DC HSM Macro"},
  {583,	0,	17, 50, 2.8f, 2.8f, "Sigma 17-50mm F2.8 EX DC OS HSM"},
  {579,	0,	70, 200, 2.8f, 2.8f, "Sigma 70-200mm F2.8 EX DG HSM II APO Macro"},
  {589,	0,	70, 200, 2.8f, 2.8f, "Sigma 70-200mm F2.8 EX DG OS HSM APO"},
  {594,	0,	300, 800, 5.6f, 5.6f, "Sigma 300-800mm F5.6 EX HSM IF APO"},
  {595,	0,	300, 800, 5.6f, 5.6f, "Sigma 300-800mm F5.6 EX DG HSM APO"},
  {597,	0,	200, 500, 2.8f, 2.8f, "Sigma 200-500mm F2.8 EX DG APO"},
// {5A8,	0,	70, 300, 4.0f, 5.6f, "Sigma 70-300mm F4-5.6 DG (Motorized) APO Macro"},
// {5A9,	0,	70, 300, 4.0f, 5.6f, "Sigma 70-300mm F4-5.6 DG (Motorized) Macro"},
  {633,	0,	28, 70, 2.8f, 4.0f, "Sigma 28-70mm F2.8-4 HS"},
  {634,	0,	28, 70, 2.8f, 4.0f, "Sigma 28-70mm F2.8-4 DG"},
  {635,	0,	24, 105, 4.0f, 4.0f, "Sigma 24-105mm F4 DG OS HSM | A"},
  {644,	0,	28, 80, 3.5f, 5.6f, "Sigma 28-80mm F3.5-5.6 ASP HF Macro"},
  {659,	0,	28, 80, 3.5f, 5.6f, "Sigma 28-80mm F3.5-5.6 Mini Zoom Macro II ASP"},
  {661,	0,	28, 105, 2.8f, 4.0f, "Sigma 28-105mm F2.8-4 IF ASP"},
  {663,	0,	28, 105, 3.8f, 5.6f, "Sigma 28-105mm F3.8-5.6 IF UC-III ASP"},
  {664,	0,	28, 105, 2.8f, 4.0f, "Sigma 28-105mm F2.8-4 IF DG ASP"},
  {667,	0,	24, 135, 2.8f, 4.5f, "Sigma 24-135mm F2.8-4.5 IF ASP"},
  {668,	0,	17, 70, 2.8f, 4.0f, "Sigma 17-70mm F2.8-4 DC OS HSM Macro"},
  {669,	0,	17, 70, 2.8f, 4.5f, "Sigma 17-70mm F2.8-4.5 DC HSM Macro"},
  {684,	0,	55, 200, 4.0f, 5.6f, "Sigma 55-200mm F4-5.6 DC"},
  {686,	0,	50, 200, 4.0f, 5.6f, "Sigma 50-200mm F4-5.6 DC OS HSM"},
  {689,	0,	17, 70, 2.8f, 4.5f, "Sigma 17-70mm F2.8-4.5 DC Macro"},
  {690,	0,	50, 150, 2.8f, 2.8f, "Sigma 50-150mm F2.8 EX DC HSM APO"},
  {691,	0,	50, 150, 2.8f, 2.8f, "Sigma 50-150mm F2.8 EX DC HSM II APO"},
  {692,	0,	50, 150, 2.8f, 2.8f, "Sigma 50-150mm F2.8 EX DC OS HSM APO"},
  {709,	0,	28, 135, 3.8f, 5.6f, "Sigma 28-135mm F3.8-5.6 IF ASP Macro"},
  {723,	0,	135, 400, 4.5f, 5.6f, "Sigma 135-400mm F4.5-5.6 ASP APO"},
  {725,	0,	80, 400, 4.5f, 5.6f, "Sigma 80-400mm F4.5-5.6 EX OS"},
  {726,	0,	80, 400, 4.5f, 5.6f, "Sigma 80-400mm F4.5-5.6 EX DG OS APO"},
  {727,	0,	135, 400, 4.5f, 5.6f, "Sigma 135-400mm F4.5-5.6 DG ASP APO"},
  {728,	0,	120, 400, 4.5f, 5.6f, "Sigma 120-400mm F4.5-5.6 DG OS HSM APO"},
  {733,	0,	170, 500, 5.0f, 6.3f, "Sigma 170-500mm F5-6.3 ASP APO"},
  {734,	0,	170, 500, 5.0f, 6.3f, "Sigma 170-500mm F5-6.3 DG ASP APO"},
  {735,	0,	50, 500, 4.0f, 6.3f, "Sigma 50-500mm F4-6.3 EX RF HSM APO"},
  {736,	0,	50, 500, 4.0f, 6.3f, "Sigma 50-500mm F4-6.3 EX DG HSM APO"},
  {737,	0,	150, 500, 5.0f, 6.3f, "Sigma 150-500mm F5-6.3 DG OS HSM APO"},
  {738,	0,	50, 500, 4.5f, 6.3f, "Sigma 50-500mm F4.5-6.3 DG OS HSM APO"},
  {740,	0,	150, 600, 5.0f, 6.3f, "Sigma 150-600mm F5-6.3 DG OS HSM | S"},
  {745,	0,	150, 600, 5.0f, 6.3f, "Sigma 150-600mm F5-6.3 DG OS HSM | C"},
  {777,	0,	18, 200, 3.5f, 6.3f, "Sigma 18-200mm F3.5-6.3 DC"},
// {77D,	0,	18, 200, 3.5f, 6.3f, "Sigma 18-200mm F3.5-6.3 DC (Motorized)"},
  {787,	0,	28, 200, 3.5f, 5.6f, "Sigma 28-200mm F3.5-5.6 Compact ASP HZ Macro"},
  {789,	0,	18, 125, 3.5f, 5.6f, "Sigma 18-125mm F3.5-5.6 DC"},
  {793,	0,	28, 300, 3.5f, 6.3f, "Sigma 28-300mm F3.5-6.3 Macro"},
  {794,	0,	28, 200, 3.5f, 5.6f, "Sigma 28-200mm F3.5-5.6 DG Compact ASP HZ Macro"},
  {795,	0,	28, 300, 3.5f, 6.3f, "Sigma 28-300mm F3.5-6.3 DG Macro"},
  {823,	0,	0, 0, 0.0f, 0.0f, "Sigma 1.4X TС EX APO"},
  {824,	0,	0, 0, 0.0f, 0.0f, "Sigma 1.4X TС EX APO DG"},
  {853,	0,	18, 125, 3.8f, 5.6f, "Sigma 18-125mm F3.8-5.6 DC OS HSM"},
  {861,	0,	18, 50, 2.8f, 4.5f, "Sigma 18-50mm F2.8-4.5 DC OS HSM"},
  {875,	0,	0, 0, 0.0f, 0.0f, "Sigma 2.0X TС EX APO"},
  {876,	0,	0, 0, 0.0f, 0.0f, "Sigma 2.0X TС EX APO DG"},
  {880,	0,	18, 250, 3.5f, 6.3f, "Sigma 18-250mm F3.5-6.3 DC OS HSM"},
  {882,	0,	18, 200, 3.5f, 6.3f, "Sigma 18-200mm F3.5-6.3 II DC OS HSM"},
  {883,	0,	18, 250, 3.5f, 6.3f, "Sigma 18-250mm F3.5-6.3 DC OS HSM Macro"},
  {884,	0,	17, 70, 2.8f, 4.0f, "Sigma 17-70mm F2.8-4 DC OS HSM Macro | C"},
  {885,	0,	18, 200, 3.5f, 6.3f, "Sigma 18-200mm F3.5-6.3 DC OS HSM Macro | C"},
  {886,	0,	18, 300, 3.5f, 6.3f, "Sigma 18-300mm F3.5-6.3 DC OS HSM Macro | C"},
  {888,	0,	18, 200, 3.5f, 6.3f, "Sigma 18-200 F3.5-6.3 DC OS"},
  {1003,	0,	19, 19, 2.8f, 2.8f, "Sigma 19mm F2.8"},
  {1004,	0,	30, 30, 2.8f, 2.8f, "Sigma 30mm F2.8"},
  {1005,	0,	50, 50, 2.8f, 2.8f, "Sigma 50mm F2.8 Macro"},
  {1006,	0,	19, 19, 2.8f, 2.8f, "Sigma 19mm F2.8"},
  {1007,	0,	30, 30, 2.8f, 2.8f, "Sigma 30mm F2.8"},
  {1008,	0,	50, 50, 2.8f, 2.8f, "Sigma 50mm F2.8 Macro"},
  {8900,	0,	70, 300, 4.0f, 5.6f, "Sigma 70-300mm F4-5.6 DG OS"},
// {A100,	0,	24, 70, 2.8f, 2.8f, "Sigma 24-70mm F2.8 DG Macro"},
  {0xFFFFFFFFFFFFFFFF,	0,	0,	0,	0.0f,	0.0f,	"Unknown"},
};
#define SigmaX3FLensList_nEntries (sizeof(SigmaX3FLensList) / sizeof(lens_t))

typedef struct {
	char const * shortName;
	float MinFocal;
	float MaxFocal;
	float MaxAp4MinFocal;
	float MaxAp4MaxFocal;
	char const * name;
} name2lens_t;

static name2lens_t HSystemLensList [] = {	// Hasselblad H-System lenses
  {"HCD 24",     24,   24,  4.8f,  4.8f, "HCD 4,8/24mm"},
  {"HCD 28",     28,   28,  4.0f,  4.0f, "HCD 4/28mm"},
  {"HC 35",      35,   35,  3.5f,  3.5f, "HC 3,5/35mm"},
  {"HC 50",      50,   50,  3.5f,  3.5f, "HC 3,5/50mm"},
  {"HC 50 II",   50,   50,  3.5f,  3.5f, "HC 3,5/50mm II"},
  {"HC 80",      80,   80,  2.8f,  2.8f, "HC 2,8/80mm"},
  {"HC 100",    100,  100,  2.2f,  2.2f, "HC 2,2/100mm"},
  {"HC 120",    120,  120,  4.0f,  4.0f, "HC MACRO 4/120mm"},
  {"HC 120 II", 120,  120,  4.0f,  4.0f, "HC MACRO 4/120mm II"},
  {"HC 150",    150,  150,  3.2f,  3.2f, "HC 3,2/150mm"},
  {"HC 150N",   150,  150,  3.2f,  3.2f, "HC 3,2/150mm N"},
  {"HC 210",    210,  210,  4.0f,  4.0f, "HC 4/210mm"},
  {"HC 300",    300,  300,  4.5f,  4.5f, "HC 4,5/300mm"},
  {"HC 50-110",  50,  110,  3.5f,  4.5f, "HC 3,5-4,5/50-110mm"},
  {"HCD 35-90",  35,   90,  4.0f,  5.6f, "HCD 4,0-5,6/35-90mm"},
};
#define HSystemLensList_nEntries (sizeof(HSystemLensList) / sizeof(name2lens_t))
/*
last 2 bits:
1 - 28/90mm frame lines
2 - 24/35mm or 35/135mm frame lines
3 - 50/75mm frame lines
*/
static lens_t LeicaMLensList [] = {
//  { 0*256+2, 1,  35,  35, 2.0f, 2.0f, "Summicron-M 1:2/35 ASPH."},
  { 6*256+0, 1,  35,  35, 1.4f, 1.4f, "Summilux-M 1:1.4/35"},
  { 9*256+0, 1, 135, 135, 3.4f, 3.4f, "Apo-Telyt-M 1:3.4/135"},
  {16*256+1, 1,  16,  16, 4.0f, 4.0f, "Tri-Elmar-M 1:4/16-18-21 ASPH. @16mm"},
  {16*256+2, 1,  18,  18, 4.0f, 4.0f, "Tri-Elmar-M 1:4/16-18-21 ASPH. @18mm"},
  {16*256+3, 1,  21,  21, 4.0f, 4.0f, "Tri-Elmar-M 1:4/16-18-21 ASPH. @21mm"},
  {29*256+0, 1,  35,  35, 1.4f, 1.4f, "Summilux-M 1:1.4/35 ASPH."},
  {31*256+0, 1,  50,  50, 1.2f, 1.2f, "Noctilux-M 1:1.2/50"},
  {39*256+0, 1, 135, 135, 4.0f, 4.0f, "Tele-Elmar-M 1:4/135 (II)"},
  {41*256+3, 1,  50,  50, 2.0f, 2.0f, "Apo-Summicron-M 1:2/50 ASPH."},
  {42*256+1, 1,  28,  28, 4.0f, 4.0f, "Tri-Elmar-M 1:4/28-35-50 ASPH. @28mm"},
  {42*256+2, 1,  35,  35, 4.0f, 4.0f, "Tri-Elmar-M 1:4/28-35-50 ASPH. @35mm"},
  {42*256+3, 1,  50,  50, 4.0f, 4.0f, "Tri-Elmar-M 1:4/28-35-50 ASPH. @50mm"},
  {51*256+2, 1,  14,  14, 3.8f, 3.8f, "Super-Elmar-M 1:3.8/14 ASPH."},
  {53*256+2, 1, 135, 135, 3.4f, 3.4f, "Apo-Telyt-M 1:3.4/135"},
  {53*256+3, 2,  50,  50, 2.0f, 2.0f, "APO-Summicron-M 1:2/50 (VI)"},

  { 1*256, 0,  21,  21, 2.8f, 2.8f, "Elmarit-M 1:2.8/21"},
  { 3*256, 0,  28,  28, 2.8f, 2.8f, "Elmarit-M 1:2.8/28 (III)"},
  { 4*256, 0,  90,  90, 2.8f, 2.8f, "Tele-Elmarit-M 1:2.8/90 (II)"},
  { 5*256, 0,  50,  50, 1.4f, 1.4f, "Summilux-M 1:1.4/50 (II)"},
  { 6*256, 0,  35,  35, 2.0f, 2.0f, "Summicron-M 1:2/35 (IV)"},
  { 7*256, 0,  90,  90, 2.0f, 2.0f, "Summicron-M 1:2/90 (II)"},
  { 9*256, 0, 135, 135, 2.8f, 2.8f, "Elmarit-M 1:2.8/135 (I/II)"},
  {16*256, 0,  16,  21, 4.0f, 4.0f, "Tri-Elmar-M 1:4/16-18-21 ASPH."},
  {23*256, 0,  50,  50, 2.0f, 2.0f, "Summicron-M 1:2/50 (III)"},
  {24*256, 0,  21,  21, 2.8f, 2.8f, "Elmarit-M 1:2.8/21 ASPH."},
  {25*256, 0,  24,  24, 2.8f, 2.8f, "Elmarit-M 1:2.8/24 ASPH."},
  {26*256, 0,  28,  28, 2.0f, 2.0f, "Summicron-M 1:2/28 ASPH."},
  {27*256, 0,  28,  28, 2.8f, 2.8f, "Elmarit-M 1:2.8/28 (IV)"},
  {28*256, 0,  28,  28, 2.8f, 2.8f, "Elmarit-M 1:2.8/28 ASPH."},
  {29*256, 0,  35,  35, 1.4f, 1.4f, "Summilux-M 1:1.4/35 ASPH."},
  {30*256, 0,  35,  35, 2.0f, 2.0f, "Summicron-M 1:2/35 ASPH."},
  {31*256, 0,  50,  50, 1.0f, 1.0f, "Noctilux-M 1:1/50"},
  {32*256, 0,  50,  50, 1.4f, 1.4f, "Summilux-M 1:1.4/50 ASPH."},
  {33*256, 0,  50,  50, 2.0f, 2.0f, "Summicron-M 1:2/50 (IV/V)"},
  {34*256, 0,  50,  50, 2.8f, 2.8f, "Elmar-M 1:2.8/50"},
  {35*256, 0,  75,  75, 1.4f, 1.4f, "Summilux-M 1:1.4/75"},
  {36*256, 0,  75,  75, 2.0f, 2.0f, "Apo-Summicron-M 1:2/75 ASPH."},
  {37*256, 0,  90,  90, 2.0f, 2.0f, "Apo-Summicron-M 1:2/90 ASPH."},
  {38*256, 0,  90,  90, 2.8f, 2.8f, "Elmarit-M 1:2.8/90"},
  {39*256, 0,  90,  90, 4.0f, 4.0f, "Macro-Elmar-M 1:4/90"},
  {40*256, 0,   0,   0, 0.0f, 0.0f, "Macro-Adapter M"},
  {42*256, 0,  28,  50, 4.0f, 4.0f, "Tri-Elmar-M 1:4/28-35-50 ASPH."},
  {43*256, 0,  35,  35, 2.5f, 2.5f, "Summarit-M 1:2.5/35"},
  {44*256, 0,  50,  50, 2.5f, 2.5f, "Summarit-M 1:2.5/50"},
  {45*256, 0,  75,  75, 2.5f, 2.5f, "Summarit-M 1:2.5/75"},
  {46*256, 0,  90,  90, 2.5f, 2.5f, "Summarit-M 1:2.5/90"},
  {47*256, 0,  21,  21, 1.4f, 1.4f, "Summilux-M 1:1.4/21 ASPH."},
  {48*256, 0,  24,  24, 1.4f, 1.4f, "Summilux-M 1:1.4/24 ASPH."},
  {49*256, 0,  50,  50, 0.95f, 0.95f, "Noctilux-M 1:0.95/50 ASPH."},
  {50*256, 0,  24,  24, 3.8f, 3.8f, "Elmar-M 1:3.8/24 ASPH."},
  {51*256, 0,  21,  21, 3.4f, 3.4f, "Super-Elmar-M 1:3.4/21 ASPH."},
  {52*256, 0,  18,  18, 3.8f, 3.8f, "Super-Elmar-M 1:3.8/18 ASPH."},
  {0xFFFFFFFFFFFFFFFF, 0, 0, 0, 0.0f, 0.0f, "Unknown"},
};
#define LeicaMLensList_nEntries (sizeof(LeicaMLensList) / sizeof(lens_t))

static name2lens_t LeicaSLensList [] = {	// Leica S-System lenses
  {"Super-Elmar-S 24",       24,   24,  3.5f,  3.5f,  "Super-Elmar-S 1:3.5/24 ASPH."},
  {"Elmarit-S 30",           30,   30,  2.8f,  2.8f,  "Elmarit-S 1:2.8/30 ASPH."},
  {"Vario-Elmar-S",          30,   90,  3.5f,  5.6f,  "Vario-Elmar-S 1:3.5-5.6/30–90 ASPH."},
  {"Summarit-S 35",          35,   35,  2.5f,  2.5f,  "Summarit-S 1:2.5/35 ASPH."},
  {"Elmarit-S 45",           45,   45,  2.8f,  2.8f,  "Elmarit-S 1:2.8/45 ASPH."},
  {"Summarit-S 70",          70,   70,  2.5f,  2.5f,  "Summarit-S 1:2.5/70 ASPH."},
  {"Summicron-S 100",       100,  100,  2.0f,  2.0f,  "Summicron-S 1:2/100 ASPH."},
  {"APO-Macro-Summarit-S",  120,  120,  2.5f,  2.5f,  "APO-Macro-Summarit-S 1:2.5/120 ASPH."},
  {"TS-APO-Elmar-S",        120,  120,  5.6f,  5.6f,  "TS-APO-Elmar-S 1:5.6/120 ASPH."},
  {"APO-Elmar-S",           180,  180,  3.5f,  3.5f,  "APO-Elmar-S 1:3.5/180 ASPH."},
};
#define LeicaSLensList_nEntries (sizeof(LeicaSLensList) / sizeof(name2lens_t))

static name2lens_t LeicaTLensList [] = {
  {"55-", 55, 135, 3.5f, 4.5f, "APO-Vario-Elmar-T 1:3.5-4.5/55–135 ASPH."},
  {"11-", 11,  23, 3.5f, 4.5f, "Super-Vario-Elmar-T 1:3.5-4.5/11–23 ASPH."},
  {"23",  23,  23, 2.0f, 2.0f, "Summicron-T 1:2/23 ASPH."},
  {"18-", 18,  56, 3.5f, 5.6f, "Vario-Elmar-T 1:3.5-5.6/18-56 ASPH."},
};
#define LeicaTLensList_nEntries (sizeof(LeicaTLensList) / sizeof(name2lens_t))

static name2lens_t ContaxNLensList [] = {
  {"100/2.8",   100, 100, 2.8f, 2.8f, "Makro-Sonnar T* 2.8/100"},
  {"50/1.4",     50,  50, 1.4f, 1.4f, "Planar T* 1.4/50"},
  {"85/1.4",     85,  85, 1.4f, 1.4f, "Planar T* 1.4/85"},
  {"400/4",     400, 400, 4.0f, 4.0f, "Tele-Apotessar T* 4/400"},
  {"17-35/2.8",  17,  35, 2.8f, 2.8f, "Vario-Sonnar T* 2.8/17-35"},
  {"24-85/3.5",  24,  85, 3.5f, 4.5f, "Vario-Sonnar T* 3.5-4.5/24-85"},
  {"70-200/3.5", 70, 200, 3.5f, 4.5f, "Vario-Sonnar T* 3.5-4.5/70-200"},
  {"28-80/3.5",  28,  80, 3.5f, 5.6f, "Vario-Sonnar T* 3.5-5.6/28-80"},
  {"70-300/4",   70, 300, 4.0f, 5.6f, "Vario-Sonnar T* 4.0-5.6/70-300"},
};
#define ContaxNLensList_nEntries (sizeof(ContaxNLensList) / sizeof(name2lens_t))

#ifdef WIN32
#define snprintf _snprintf
#endif

#define P1 MyCoolRawProcessor.imgdata.idata
#define P2 MyCoolRawProcessor.imgdata.other

#define exifLens MyCoolRawProcessor.imgdata.lens
#define dngLens MyCoolRawProcessor.imgdata.lens.dng
#define mnLens MyCoolRawProcessor.imgdata.lens.makernotes

#define S MyCoolRawProcessor.imgdata.sizes
#define O MyCoolRawProcessor.imgdata.params
#define C MyCoolRawProcessor.imgdata.color
#define T MyCoolRawProcessor.imgdata.thumbnail

fixed_lens_t *lookupFixedLens(const char *make, const char *model)
{
  for (int i = 0; i < FixedLensesList_nEntries; i++)
    if (!strcasecmp(FixedLensesList[i].make, make) && !strncasecmp(model, FixedLensesList[i].model, strlen(FixedLensesList[i].model)))
      return &FixedLensesList[i];
  return 0;
}

name2lens_t *lookupLensByShortName(const char *sName, name2lens_t *table, ushort nEntries)
{
  for (int i = 0; i < nEntries; i++)
    if (!strncasecmp(table[i].shortName, sName, strlen(table[i].shortName)))
      return &table[i];
  return 0;
}

name2lens_t *lookupLensBySubName(const char *sName, name2lens_t *table, ushort nEntries)
{
  for (int i = 0; i < nEntries; i++)
    if (strstr(sName, table[i].shortName))
      return &table[i];
  return 0;
}

lens_t *lookupLensByName(const char *Name, lens_t *table, ushort nEntries)
{
  for (int k = nEntries-1; k = 0; k--)
    if (strstr(Name, table[k].name))
      return &table[k];
  return 0;
}

lens_t *lookup_lens_tUniqueID(unsigned long long id, lens_t *table, ushort nEntries)
{
  for (int k = 0; k < nEntries; k++)
    if (id == table[k].id)
      return &table[k];
  return 0;
}

lens_t *lookup_lens_tLeicaM(unsigned long long id, lens_t *table, ushort nEntries)
{

  unsigned long long id1;

  for (int k = 0; k < nEntries; k++)
	{
		if(!table[k].variant)
				id1 = id & 0xffffffffffffff00ULL;
		else
				id1 = id;
		if (id1 == table[k].id)
			return &table[k];
	}
  return 0;
}

lens_t* lookupLens_ID_MinFocal(unsigned long long id, lens_t *table, ushort nEntries, float MinFocal)
{
  int k;
  for (k = 0; k < nEntries-1; k++)
    if ((id == table[k].id) &&
        (fabsf(MinFocal - table[k].MinFocal) < 1.0f))
      return &table[k];
  return 0;
}

lens_t* lookup_lens_tPentax(unsigned long long id, lens_t *table, ushort nEntries, float _cur_focal)
{
  int k;
  for (k = 0; k < nEntries-1; k++)
    if ((id == table[k].id))
    {
      if (!table[k].variant && !table[k + 1].variant)
        return &table[k];
      else if ((_cur_focal >= table[k].MinFocal) &&
               (_cur_focal <= table[k].MaxFocal))
        return &table[k];
    }
  return 0;
}

lens_t * lookup_lens_tCanon(unsigned long long id,
                            lens_t *table, ushort nEntries,
                            float MinFocal, float MaxFocal, float MaxAp,
                            float &TC_factor)
{
  int i, j, k;
  float MinFocal4TC, MaxFocal4TC;
  float factor [5] = {1.0f, 1.4f, 2.0f, 2.8f, 4.0f};

  switch (id) {
  case 0:	case 0x7fff:	case 0xffff:
    return 0;
  case 166:	case 181:	case 187:	case 225:	case 243:	case 252:	case 499:
    i = 1;
    break;
  case 167:	case 182:	case 184:	case 188:	case 226:	case 244:	case 253:
    i = 2;
    break;
  case 189:	case 227:	case 245:
    i = 3;
    break;
  default:
    i = 0;
  }

  for (j = 0; j < 5; j++)
	{
		if (i)
			TC_factor = factor[j] * factor[i];
		else
			TC_factor = factor[j];
		MinFocal4TC = MinFocal / TC_factor;
		MaxFocal4TC = MaxFocal / TC_factor;
		for (k = 0; k< nEntries; k++)
		{
			if ((id == table[k].id) &&
					(MinFocal4TC >= (table[k].MinFocal - 2.7f)) &&
					(MaxFocal4TC <= (table[k].MaxFocal + 2.7f)))
			{
				if ((id != 10) && (id != 26))
					{
						return &table[k];
					}
				else
				{
					if (((fabsf(MaxAp - table[k].MaxAp4MinFocal)) < 0.05) ||
							((fabsf(MaxAp / TC_factor - table[k].MaxAp4MinFocal)) < 0.05))
						return &table[k];
				}
			}
		}
	}
  TC_factor = 1.0f;
  return 0;
}

lens_t* lookup_lens_tSonyMinolta(
                                 unsigned long long id,
                                 lens_t *table,
                                 ushort nEntries,
                                 ushort MinFocal,
                                 ushort MaxFocal,
                                 float MaxAp4MinFocal,
                                 float MaxAp4MaxFocal,
                                 float CurFocal,
                                 float MaxAp,
                                 int format
                                 )
{
  int k;
  for (k=0; k< nEntries-1; k++)
	{
		if (id == ((lens_t *)table)[k].id)
		{
			if (!((lens_t *)table)[k].variant && !((lens_t *)table)[k+1].variant)
				return &table[k];										// lens id is unique
			else if (
							 (fabsf(MinFocal - ((lens_t *)table)[k].MinFocal) < 0.5f) &&
							 (fabsf(MaxFocal - ((lens_t *)table)[k].MaxFocal) < 0.5f) &&
							 (fabsf(MaxAp4MinFocal - ((lens_t *)table)[k].MaxAp4MinFocal) < 0.17f) &&
							 (fabsf(MaxAp4MaxFocal - ((lens_t *)table)[k].MaxAp4MaxFocal) < 0.17f)
							 )
				return &table[k];										// full match
			else if (
							 ((MinFocal < 1.0f) || (MinFocal > ((lens_t *)table)[k].MinFocal - 0.5f)) &&
							 ((MaxFocal < 1.0f) || (MaxFocal < ((lens_t *)table)[k].MaxFocal + 0.5f)) &&
							 ((MaxAp4MinFocal < 1.01f) || (MaxAp4MinFocal > ((lens_t *)table)[k].MaxAp4MinFocal - 0.17f)) &&
							 ((MaxAp4MaxFocal < 1.01f) || (MaxAp4MaxFocal < ((lens_t *)table)[k].MaxAp4MaxFocal + 0.17f)) &&
							 ((CurFocal < 1.0f) || ((CurFocal > (((lens_t *)table)[k].MinFocal - 1.0f)) && (CurFocal < (((lens_t *)table)[k].MaxFocal + 1.0f)))) &&
							 ((MaxAp < 1.01f) || ((MaxAp > (((lens_t *)table)[k].MaxAp4MinFocal - 0.17f) && (MaxAp < (((lens_t *)table)[k].MaxAp4MaxFocal + 0.17f)))))
							 )
			{
				if (format == FF)
				{
					if (strstr(table[k].name, "Sigma") && strstr(table[k].name, " DC") &&
							!strstr(table[k].name, "Sigma 4.5mm F2.8 EX DC HSM Circular Fisheye"))
						continue;
					else if (strstr(table[k].name, "Tamron") && strstr(table[k].name, "Di II"))
						continue;
					else if (strstr(table[k].name, "Tokina") && strstr(table[k].name, " DX"))
						continue;
					else if ((strstr(table[k].name, "Samyang") ||
										strstr(table[k].name, "Rokinon") ||
										strstr(table[k].name, "Bower")) &&
									 strstr(table[k].name, " CS"))
						continue;
					else if ((strstr(table[k].name, "Sony") ||
										strstr(table[k].name, "Zeiss") ||
										strstr(table[k].name, "Minolta")) &&
									 (strstr(table[k].name, " E ") ||
										strstr(table[k].name, " DT ")))
						continue;
				}
				return &table[k];
			}
		}
	}
  return 0;
}

int main(int ac, char *av[])
{
  int verbose = 0, ret,print_unpack=0,print_frame=0;
  LibRaw MyCoolRawProcessor;

  for (int i=1;i<ac;i++)
	{
		if(av[i][0]=='-')
			{
				if(av[i][1]=='v' && av[i][2]==0) verbose++;
				continue;
			}
		if( (ret = MyCoolRawProcessor.open_file(av[i])) != LIBRAW_SUCCESS)
		{
			printf("Cannot decode %s: %s\n",av[i],libraw_strerror(ret));
			continue; // no recycle, open_file will recycle
		}
		if( (ret =  MyCoolRawProcessor.adjust_sizes_info_only()))
		{
			printf("Cannot decode %s: %s\n",av[i],libraw_strerror(ret));
			continue; // no recycle, open_file will recycle
		}

		if (verbose)
		{
			printf ("\nFilename: %s\n", av[i]);
			printf ("Timestamp: %s", ctime(&(P2.timestamp)));
			printf ("Camera: =%s= =%s=\n", P1.make, P1.model);
			if (P2.artist[0])
				printf ("Owner: %s\n", P2.artist);
			if (P1.dng_version)
			{
				printf ("DNG Version: ");
				for (int i=24; i >= 0; i -= 8)
					printf ("%d%c", P1.dng_version >> i & 255, i ? '.':'\n');
			}
			printf ("\nEXIF:\n");
			printf ("\tMinFocal: %0.1f mm\n", exifLens.MinFocal);
			printf ("\tMaxFocal: %0.1f mm\n", exifLens.MaxFocal);
			printf ("\tMaxAp @MinFocal: f/%0.1f\n", exifLens.MaxAp4MinFocal);
			printf ("\tMaxAp @MaxFocal: f/%0.1f\n", exifLens.MaxAp4MaxFocal);
			printf ("\tMaxAperture @CurFocal: f/%0.1f\n", exifLens.EXIF_MaxAp);
			printf ("\tFocalLengthIn35mmFormat: %d mm\n", exifLens.FocalLengthIn35mmFormat);
			printf ("\tLensMake: %s\n", exifLens.LensMake);
			printf ("\tLens: %s\n", exifLens.Lens);
			printf ("\n");

			printf ("\nMakernotes:\n");
			if (mnLens.body[0])
					printf("\tMF Camera Body: %s\n", mnLens.body);
			printf("\tCameraFormat: %d, ", mnLens.CameraFormat);
			switch (mnLens.CameraFormat) {
				case 0:  printf("Undefined\n"); break;
				case 1:  printf("APS-C\n"); break;
				case 2:  printf("FF\n"); break;
				case 3:  printf("MF\n"); break;
				case 4:  printf("APS-H\n"); break;
				case 8:  printf("4/3\n"); break;
				default: printf("Unknown\n"); break;
			}
			printf("\tCameraMount: %d, ", mnLens.CameraMount);
			switch (mnLens.CameraMount) {
				case 0:  printf("Undefined or Fixed Lens\n"); break;
				case 1:  printf("Sony/Minolta A\n"); break;
				case 2:  printf("Sony E\n"); break;
				case 3:  printf("Canon EF\n"); break;
				case 4:  printf("Canon EF-S\n"); break;
				case 5:  printf("Canon EF-M\n"); break;
				case 6:  printf("Nikon F\n"); break;
				case 7:  printf("Nikon CX\n"); break;
				case 8:  printf("4/3\n"); break;
				case 9:  printf("m4/3\n"); break;
				case 10: printf("Pentax K\n"); break;
				case 11: printf("Pentax Q\n"); break;
				case 12: printf("Pentax 645\n"); break;
				case 13: printf("Fuji X\n"); break;
				case 14: printf("Leica M\n"); break;
				case 15: printf("Leica R\n"); break;
				case 16: printf("Leica S\n"); break;
				case 17: printf("Samsung NX\n"); break;
				case 18: printf("Ricoh module\n"); break;
				case 19: printf("Samsung NX-M\n"); break;
				case 20: printf("Leica T\n"); break;
				case 21: printf("Contax N\n"); break;
				case 22: printf("Sigma/Polaroid X3F camera\n"); break;
				case 99: printf("Fixed Lens\n"); break;
				default: printf("Unknown\n"); break;
			}

			if (mnLens.LensID == -1)
					printf("\tLensID: n/a\n");
			else
					printf("\tLensID: %llu 0x%0llx\n", mnLens.LensID, mnLens.LensID);
			printf("\tLens: %s\n", mnLens.Lens);
			printf("\tLensFormat: %d, ", mnLens.LensFormat);
			switch (mnLens.LensFormat) {
				case 0:  printf("Undefined\n"); break;
				case 1:  printf("APS-C\n"); break;
				case 2:  printf("FF\n"); break;
				case 3:  printf("MF\n"); break;
				case 8:  printf("4/3\n"); break;
				default: printf("Unknown\n"); break;
			}
			printf("\tLensMount: %d, ", mnLens.LensMount);
			switch (mnLens.LensMount) {
				case 0:  printf("Undefined or Fixed Lens\n"); break;
				case 1:  printf("Sony/Minolta A\n"); break;
				case 2:  printf("Sony E\n"); break;
				case 3:  printf("Canon EF\n"); break;
				case 4:  printf("Canon EF-S\n"); break;
				case 5:  printf("Canon EF-M\n"); break;
				case 6:  printf("Nikon F\n"); break;
				case 7:  printf("Nikon CX\n"); break;
				case 8:  printf("4/3\n"); break;
				case 9:  printf("m4/3\n"); break;
				case 10: printf("Pentax K\n"); break;
				case 11: printf("Pentax Q\n"); break;
				case 12: printf("Pentax 645\n"); break;
				case 13: printf("Fuji X\n"); break;
				case 14: printf("Leica M\n"); break;
				case 15: printf("Leica R\n"); break;
				case 16: printf("Leica S\n"); break;
				case 17: printf("Samsung NX\n"); break;
				case 18: printf("Ricoh module\n"); break;
				case 19: printf("Samsung NX-M\n"); break;
				case 20: printf("Leica T\n"); break;
				case 21: printf("Contax N\n"); break;
				case 22: printf("Lens on X3F camera\n"); break;
				case 99: printf("Fixed Lens\n"); break;
				default: printf("Unknown\n"); break;
			}
			printf("\tFocalType: %d, ", mnLens.FocalType);
			switch (mnLens.FocalType) {
			case 0:  printf("Undefined\n"); break;
			case 1:  printf("Fixed Focal\n"); break;
			case 2:  printf("Zoom\n"); break;
			default: printf("Unknown\n"); break;
			}
			printf("\tLensFeatures_pre: %s\n", mnLens.LensFeatures_pre);
			printf("\tLensFeatures_suf: %s\n", mnLens.LensFeatures_suf);
			printf("\tMinFocal: %0.1f mm\n", mnLens.MinFocal);
			printf("\tMaxFocal: %0.1f mm\n", mnLens.MaxFocal);
			printf("\tMaxAp @MinFocal: f/%0.1f\n", mnLens.MaxAp4MinFocal);
			printf("\tMaxAp @MaxFocal: f/%0.1f\n", mnLens.MaxAp4MaxFocal);
			printf("\tMinAp @MinFocal: f/%0.1f\n", mnLens.MinAp4MinFocal);
			printf("\tMinAp @MaxFocal: f/%0.1f\n", mnLens.MinAp4MaxFocal);
			printf("\tMaxAp: f/%0.1f\n", mnLens.MaxAp);
			printf("\tMinAp: f/%0.1f\n", mnLens.MinAp);
			printf("\tCurFocal: %0.1f mm\n", mnLens.CurFocal);
			printf("\tCurAp: f/%0.1f\n", mnLens.CurAp);
			printf("\tMaxAp @CurFocal: f/%0.1f\n", mnLens.MaxAp4CurFocal);
			printf("\tMinAp @CurFocal: f/%0.1f\n", mnLens.MinAp4CurFocal);

			if (exifLens.makernotes.SamsungFocalLengthIn35mmFormat > 1.0f)
				printf("\tFocalLengthIn35mmFormat: %0.1f mm\n", exifLens.makernotes.SamsungFocalLengthIn35mmFormat);

			if (exifLens.nikon.NikonEffectiveMaxAp > 0.1f)
				printf("\tNikonEffectiveMaxAp: f/%0.1f\n", exifLens.nikon.NikonEffectiveMaxAp);

			if (exifLens.makernotes.LensFStops > 0.1f)
				printf("\tLensFStops @CurFocal: %0.2f\n", exifLens.makernotes.LensFStops);

			printf("\tTeleconverterID: %lld\n", mnLens.TeleconverterID);
			printf("\tTeleconverter: %s\n", mnLens.Teleconverter);
			printf("\tAdapterID: %lld\n", mnLens.AdapterID);
			printf("\tAdapter: %s\n", mnLens.Adapter);
			printf("\tAttachmentID: %lld\n", mnLens.AttachmentID);
			printf("\tAttachment: %s\n", mnLens.Attachment);
			printf ("\n");
		} // Verbose

		{
			float CurFocal = 0.0f, CurAp = 0.0f;
			float MinFocal = 0.0f, MaxFocal = 0.0f;
			float MaxAp4MinFocal = 0.0f, MaxAp4MaxFocal = 0.0f;
			float CropFactor, TCx = 1.0f, EXIF_MaxAp, FocalLengthIn35mmFormat;
			fixed_lens_t *fixed_lens;
			lens_t *lens;
			name2lens_t *lens1;
			char LensModel[128];
			uchar is_fixed = 0;
			unsigned long long LensID = mnLens.LensID;

			LensModel[0] = 0;

			if (P2.focal_len > 0.5f)
				CurFocal = P2.focal_len;
			else  if (mnLens.CurFocal > 0.5f)
				CurFocal = mnLens.CurFocal;

			if (P2.aperture > 0.7f)
				CurAp = P2.aperture;
			else if (mnLens.CurAp > 0.7f)
				CurAp = mnLens.CurAp;

			if ((exifLens.nikon.NikonEffectiveMaxAp > 0.7f) &&
					(exifLens.EXIF_MaxAp < 0.7f))
				EXIF_MaxAp = exifLens.nikon.NikonEffectiveMaxAp;
			else
				EXIF_MaxAp = exifLens.EXIF_MaxAp;

			if ((exifLens.makernotes.SamsungFocalLengthIn35mmFormat > 1.0f) &&
					(exifLens.FocalLengthIn35mmFormat < 1.0f))
				FocalLengthIn35mmFormat = exifLens.makernotes.SamsungFocalLengthIn35mmFormat;
			else
				FocalLengthIn35mmFormat = exifLens.FocalLengthIn35mmFormat;

			fixed_lens = lookupFixedLens(P1.make, P1.model);
			if (fixed_lens)
			{
				is_fixed = 1;
				MinFocal = fixed_lens->MinFocal;
				MaxFocal = fixed_lens->MaxFocal;

				if ((MaxFocal > 1.0f) && (fixed_lens->MaxFocal_eq35mm > 1.0f))
					CropFactor = fixed_lens->MaxFocal_eq35mm / MaxFocal;
				else
					CropFactor = 1.0f;

				MaxAp4MinFocal = fixed_lens->MaxAp4MinFocal;
				MaxAp4MaxFocal = fixed_lens->MaxAp4MaxFocal;

				if ((CurFocal < 1.0f) &&
						(FocalLengthIn35mmFormat > 1.0f))
					CurFocal = FocalLengthIn35mmFormat / CropFactor;

				goto print_it;
			}

			else if (!strcasecmp(P1.make, "Hasselblad") && exifLens.Lens[0])
			{
				lens1 =
					lookupLensByShortName (exifLens.Lens, HSystemLensList, HSystemLensList_nEntries);
				if (lens1)
				{
					MinFocal = lens1->MinFocal;
					MaxFocal = lens1->MaxFocal;
					MaxAp4MinFocal = lens1->MaxAp4MinFocal;
					MaxAp4MaxFocal = lens1->MaxAp4MaxFocal;
					strcpy (LensModel, lens1->name);
					goto print_it;
				}
			}

			else if ((mnLens.LensMount == Leica_S) &&
							 !strcmp(P1.make, "Leica"))
			{
				if (mnLens.Lens[0])
					lens1 =
						lookupLensByShortName (mnLens.Lens, LeicaSLensList, LeicaSLensList_nEntries);
				else if (exifLens.Lens[0])
					lens1 =
						lookupLensByShortName (exifLens.Lens, LeicaSLensList, LeicaSLensList_nEntries);
				if (lens1)
				{
					MinFocal = lens1->MinFocal;
					MaxFocal = lens1->MaxFocal;
					MaxAp4MinFocal = lens1->MaxAp4MinFocal;
					MaxAp4MaxFocal = lens1->MaxAp4MaxFocal;
					strcpy (LensModel, lens1->name);
					goto print_it;
				}
			}

		else if (mnLens.CameraMount == Leica_T)
		{
			 if (mnLens.Lens[0])
				 lens1 =
					 lookupLensBySubName (mnLens.Lens, LeicaTLensList, LeicaTLensList_nEntries);
			 else if (exifLens.Lens[0])
				 lens1 =
					 lookupLensBySubName (exifLens.Lens, LeicaTLensList, LeicaTLensList_nEntries);
			 if (lens1)
			 {
					MinFocal = lens1->MinFocal;
					MaxFocal = lens1->MaxFocal;
					MaxAp4MinFocal = lens1->MaxAp4MinFocal;
					MaxAp4MaxFocal = lens1->MaxAp4MaxFocal;
					strcpy (LensModel, lens1->name);
					goto print_it;
			 }
		}

		else if (mnLens.LensMount == Contax_N)
		{
			lens1 =
				lookupLensBySubName (mnLens.Lens, ContaxNLensList, ContaxNLensList_nEntries);
			if (lens1)
			{
				MinFocal = lens1->MinFocal;
				MaxFocal = lens1->MaxFocal;
				MaxAp4MinFocal = lens1->MaxAp4MinFocal;
				MaxAp4MaxFocal = lens1->MaxAp4MaxFocal;
				strcpy (LensModel, lens1->name);
				goto print_it;
			}
		}

		else if (LensID != -1)
		{
			if ((mnLens.LensMount == Canon_EF) ||
					(mnLens.LensMount == Canon_EF_S) ||
					(mnLens.LensMount == Canon_EF_M) ||
					(mnLens.CameraMount == Canon_EF) ||
					(mnLens.CameraMount == Canon_EF_M))
			{
				lens =
					lookup_lens_tCanon (LensID, CanonLensList, CanonLensList_nEntries,
															mnLens.MinFocal, mnLens.MaxFocal, mnLens.MaxAp, TCx);
				if (lens)
				{
					MinFocal = lens->MinFocal;
					MaxFocal = lens->MaxFocal;
					MaxAp4MinFocal = lens->MaxAp4MinFocal;
					MaxAp4MaxFocal = lens->MaxAp4MaxFocal;
					strcpy (LensModel, lens->name);
					goto print_it;
				}
			}

			else if ((mnLens.LensMount == Leica_M) &&
							 (!strcmp(P1.make, "Leica")))
			{
				if (LensID == 2)
				{
					if (mnLens.Lens[0])
						lens =
							lookupLensByName(mnLens.Lens, LeicaMLensList, LeicaMLensList_nEntries);
					else if (exifLens.Lens[0])
						lens =
							lookupLensByName(exifLens.Lens, LeicaMLensList, LeicaMLensList_nEntries);
				}
				else
					lens =
						lookup_lens_tLeicaM(LensID, LeicaMLensList, LeicaMLensList_nEntries);
				if (lens)
				{
					MinFocal = lens->MinFocal;
					MaxFocal = lens->MaxFocal;
					MaxAp4MinFocal = lens->MaxAp4MinFocal;
					MaxAp4MaxFocal = lens->MaxAp4MaxFocal;
					strcpy (LensModel, lens->name);
					goto print_it;
				}
			}

			else if (mnLens.LensMount == Nikon_F)
			{
				lens =
					lookup_lens_tUniqueID(LensID, NikonLensList, NikonLensList_nEntries);
				if (lens)
				{
					MinFocal = lens->MinFocal;
					MaxFocal = lens->MaxFocal;
					MaxAp4MinFocal = lens->MaxAp4MinFocal;
					MaxAp4MaxFocal = lens->MaxAp4MaxFocal;
					strcpy (LensModel, lens->name);
					goto print_it;
				}
			}

			else if (mnLens.LensMount == RicohModule)
			{
				lens =
					lookup_lens_tUniqueID(LensID, RicohModuleLensList, RicohModuleLensList_nEntries);
				if (lens)
				{
					MaxFocal = lens->MaxFocal;
					MinFocal = lens->MinFocal;
					if ((exifLens.MaxFocal > 1.0f) && (MaxFocal > 1.0f))
						{
							CropFactor = MaxFocal / exifLens.MaxFocal;
							MinFocal /= CropFactor;
							MaxFocal /= CropFactor;
						}
					MaxAp4MinFocal = lens->MaxAp4MinFocal;
					MaxAp4MaxFocal = lens->MaxAp4MaxFocal;
					strcpy (LensModel, lens->name);
					goto print_it;
				}
			}

			else if (mnLens.LensMount == Sigma_X3F)
			{
				lens =
					lookupLens_ID_MinFocal(LensID, SigmaX3FLensList, SigmaX3FLensList_nEntries, mnLens.MinFocal);
				if (lens)
				{
					if (LensID != 486)
					{
						MinFocal = lens->MinFocal;
						MaxFocal = lens->MaxFocal;
					}
					else if (fabsf(lens->MinFocal-4) < 0.1f) // Sigma 4.5mm Circular Fisheye
						MinFocal = MaxFocal = 4.5f;
					MaxAp4MinFocal = lens->MaxAp4MinFocal;
					MaxAp4MaxFocal = lens->MaxAp4MaxFocal;
					strcpy (LensModel, lens->name);
					goto print_it;
				}
			}

			else if (mnLens.LensMount == Samsung_NX)
			{
				lens =
					lookup_lens_tUniqueID(LensID, SamsungLensList, SamsungLensList_nEntries);
				if (lens)
				{
					MinFocal = lens->MinFocal;
					MaxFocal = lens->MaxFocal;
					MaxAp4MinFocal = lens->MaxAp4MinFocal;
					MaxAp4MaxFocal = lens->MaxAp4MaxFocal;
					strcpy (LensModel, lens->name);
					goto print_it;
				}
			}

			else if ((mnLens.LensMount == Sony_E) &&
							 (LensID != 0xffff))
			{
				lens =
					lookup_lens_tUniqueID(LensID, SonyLensList, SonyLensList_nEntries);
				if (lens)
				{
					MinFocal = lens->MinFocal;
					MaxFocal = lens->MaxFocal;
					MaxAp4MinFocal = lens->MaxAp4MinFocal;
					MaxAp4MaxFocal = lens->MaxAp4MaxFocal;
					strcpy (LensModel, lens->name);
					goto print_it;
				}
			}

			else if ((mnLens.CameraMount == Sony_E) &&
							 (LensID == 0xffff))
			{
				LensID = -1;
				if (fabsf(CurAp - 1.0f) < 0.17f)
						CurAp = EXIF_MaxAp = 0.0f;
			}

			else if ((mnLens.LensMount == FT) ||
							 (mnLens.LensMount == mFT))
			{
				lens =
					lookup_lens_tUniqueID(LensID, OlympusLensList, OlympusLensList_nEntries);
				if (lens)
				{
					MinFocal = lens->MinFocal;
					MaxFocal = lens->MaxFocal;
					MaxAp4MinFocal = lens->MaxAp4MinFocal;
					MaxAp4MaxFocal = lens->MaxAp4MaxFocal;
					strcpy (LensModel, lens->name);
					goto print_it;
				}
			}

			else if ((mnLens.LensMount == Pentax_K) ||
							 (mnLens.LensMount == Pentax_Q) ||
							 (mnLens.LensMount == Pentax_645))
			{
				lens =
					lookup_lens_tPentax(LensID, PentaxLensList, PentaxLensList_nEntries, CurFocal);
				if (lens)
				{
					MinFocal = lens->MinFocal;
					MaxFocal = lens->MaxFocal;
					MaxAp4MinFocal = lens->MaxAp4MinFocal;
					MaxAp4MaxFocal = lens->MaxAp4MaxFocal;
					strcpy (LensModel, lens->name);
					goto print_it;
				}
			}
		}

			if ((mnLens.CameraMount == Samsung_NX_M) &&
							 !strncmp(exifLens.Lens, "NX-M 9-27", 9))
			{
				MinFocal = 9.0f;
				MaxFocal = 27.0f;
				MaxAp4MinFocal = 3.5f;
				MaxAp4MaxFocal = 5.6f;
				goto print_it;
			}

			if (!strncmp(mnLens.Lens, "Schneider LS 75-150mm f/4.0-5.6", 31))
					MaxAp4MaxFocal = 5.6f;

			if (exifLens.MinFocal > 0.5f)
				MinFocal = exifLens.MinFocal;
			else if (dngLens.MinFocal > 0.5f)
				MinFocal = dngLens.MinFocal;
			else if (mnLens.MinFocal > 0.5f)
				MinFocal = mnLens.MinFocal;

			if (CurFocal < (MinFocal - 0.17f))
				MinFocal = 0.0f;

			if (exifLens.MaxFocal > 0.5f)
				MaxFocal = exifLens.MaxFocal;
			else if (dngLens.MaxFocal > 0.5f)
				MaxFocal = dngLens.MaxFocal;
			else if (mnLens.MaxFocal > 0.5f)
				MaxFocal = mnLens.MaxFocal;

			if (CurFocal > (MaxFocal + 0.17f))
				MaxFocal = 0.0f;

			if ((MaxFocal < 0.5f) && (MinFocal > 0.5f))
				MaxFocal = MinFocal;
			else if ((MinFocal < 0.5f) && (MaxFocal > 0.5f))
				MinFocal = MaxFocal;

			if ((MaxFocal < 0.5f) && (MinFocal < 0.5f) && (CurFocal > 0.5f))
				MaxFocal = MinFocal = CurFocal;

			if (fabsf(MaxFocal-MinFocal) < 1.0f)		// process fixed focal
			{
				if (CurFocal > 0.5f)
					MinFocal = MaxFocal = CurFocal;
				if (EXIF_MaxAp > 0.7f)
					MaxAp4MinFocal = MaxAp4MaxFocal = EXIF_MaxAp;
				else if (mnLens.MaxAp4CurFocal > 0.7f)
					MaxAp4MinFocal = MaxAp4MaxFocal = mnLens.MaxAp4CurFocal;
				else if (mnLens.MaxAp > 0.7f)
					MaxAp4MinFocal = MaxAp4MaxFocal = mnLens.MaxAp;
				else if (mnLens.MaxAp4MaxFocal > 0.7f)
					MaxAp4MinFocal = MaxAp4MaxFocal = mnLens.MaxAp4MaxFocal;
				else if (mnLens.MaxAp4MinFocal > 0.7f)
					MaxAp4MinFocal = MaxAp4MaxFocal = mnLens.MaxAp4MinFocal;
				goto got_lens;
			}

			if (!strcmp(P1.make, "Sigma") && !LensModel[0])
			  goto print_it;

			if (exifLens.MaxAp4MinFocal > 0.7f)
				MaxAp4MinFocal = exifLens.MaxAp4MinFocal;
			else if (dngLens.MaxAp4MinFocal > 0.7f)
				MaxAp4MinFocal = dngLens.MaxAp4MinFocal;
			else if (mnLens.MaxAp4MinFocal > 0.7f)
				MaxAp4MinFocal = mnLens.MaxAp4MinFocal;
			else if (mnLens.MaxAp4CurFocal > 0.7f)
				MaxAp4MinFocal = mnLens.MaxAp4CurFocal;
			else if (mnLens.MaxAp > 0.7f)
				MaxAp4MinFocal = mnLens.MaxAp;

			if (CurAp < (MaxAp4MinFocal - 0.17f))
				MaxAp4MinFocal = 0.0f;

			if (exifLens.MaxAp4MaxFocal > 0.7f)
				MaxAp4MaxFocal = exifLens.MaxAp4MaxFocal;
			else if (dngLens.MaxAp4MaxFocal > 0.7f)
				MaxAp4MaxFocal = dngLens.MaxAp4MaxFocal;
			else if (mnLens.MaxAp4MaxFocal > 0.7f)
				MaxAp4MaxFocal = mnLens.MaxAp4MaxFocal;

			if ((MaxAp4MaxFocal < 0.7f) && (MaxAp4MinFocal > 0.7f))
				MaxAp4MaxFocal = MaxAp4MinFocal;
			else if ((MaxAp4MinFocal < 0.7f) && (MaxAp4MaxFocal > 0.7f))
				MaxAp4MinFocal = MaxAp4MaxFocal;

			if ((fabsf(MaxAp4MinFocal-MaxAp4MaxFocal) < 0.17f) &&
					(EXIF_MaxAp > 0.7f))
				MaxAp4MinFocal = MaxAp4MaxFocal = EXIF_MaxAp;

			if (fabsf(MaxAp4MinFocal - 0.7f) < 0.12f)
				MaxAp4MinFocal = 0.0f;

			if (fabsf(MaxAp4MaxFocal - 0.7f) < 0.12f)
				MaxAp4MaxFocal = 0.0f;

			if (fabsf(CurAp - 0.7f) < 0.12f)
				CurAp = 0.0f;

		got_lens:

			if ((LensID != -1) &&
					((mnLens.LensMount == Minolta_A) ||
					 (mnLens.CameraMount == Minolta_A)))
				{
					lens =
						lookup_lens_tSonyMinolta (LensID, SonyMinoltaLensList, SonyMinoltaLensList_nEntries,
																			MinFocal, MaxFocal, MaxAp4MinFocal, MaxAp4MaxFocal,
																			CurFocal, EXIF_MaxAp, mnLens.CameraFormat);
					if (lens)
					{
						MinFocal = lens->MinFocal;
						MaxFocal = lens->MaxFocal;
						MaxAp4MinFocal = lens->MaxAp4MinFocal;
						MaxAp4MaxFocal = lens->MaxAp4MaxFocal;
						strcpy (LensModel, lens->name);
					}
				}

		print_it:

			printf ("\nDerived:\n\tLens model: ");
			if (LensModel[0])
				printf ("%s", LensModel);
			else if (mnLens.Lens[0])
				printf ("%s", mnLens.Lens);
			else if (exifLens.Lens[0])
				printf ("%s", exifLens.Lens);
			else if (is_fixed)
				printf ("Fixed lens");
			else
				printf("n/a", mnLens.Lens);

			printf("\n\tLens ID: ");
			if (LensID == -1)
				printf("n/a");
			else
				printf("%llu 0x%0llx", LensID, LensID);

			printf("\n\tLens data: ");
			if (fabsf(MaxFocal-MinFocal) < 1.0f)
				printf ("%0.1f mm", MinFocal);
			else
				printf ("%0.1f-%0.1f mm", MinFocal, MaxFocal);
			if (fabsf(MaxAp4MinFocal-MaxAp4MaxFocal) < 0.17f)
				printf (" f/%0.1f", MaxAp4MinFocal);
			else
				printf (" f/%0.1f-%0.1f", MaxAp4MinFocal, MaxAp4MaxFocal);
			if (fabsf(TCx-1.0f) > 0.1f)
				printf (" + %0.1fx TC", TCx);

			printf("\n\tLens taking data: ");
			printf("%0.1f mm ", CurFocal);
			printf("f/%0.1f\n", CurAp);
			printf ("\n");
		}

		MyCoolRawProcessor.recycle();
	}// endfor
  return 0;
}
