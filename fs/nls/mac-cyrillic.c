/*
 * linux/fs/nls/mac-cyrillic.c
 *
 * Charset maccyrillic translation tables.
 * Generated automatically from the Unicode and charset
 * tables from the Unicode Organization (www.unicode.org).
 * The Unicode to charset table has only exact mappings.
 */

/*
 * COPYRIGHT AND PERMISSION NOTICE
 *
 * Copyright 1991-2012 Unicode, Inc.  All rights reserved.  Distributed under
 * the Terms of Use in http://www.unicode.org/copyright.html.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of the Unicode data files and any associated documentation (the "Data
 * Files") or Unicode software and any associated documentation (the
 * "Software") to deal in the Data Files or Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, and/or sell copies of the Data Files or Software, and
 * to permit persons to whom the Data Files or Software are furnished to do
 * so, provided that (a) the above copyright notice(s) and this permission
 * notice appear with all copies of the Data Files or Software, (b) both the
 * above copyright notice(s) and this permission notice appear in associated
 * documentation, and (c) there is clear notice in each modified Data File or
 * in the Software as well as in the documentation associated with the Data
 * File(s) or Software that the data or software has been modified.
 *
 * THE DATA FILES AND SOFTWARE ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
 * KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THE DATA FILES OR SOFTWARE.
 *
 * Except as contained in this notice, the name of a copyright holder shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in these Data Files or Software without prior written
 * authorization of the copyright holder.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/nls.h>
#include <linux/errno.h>

static const wchar_t charset2uni[256] = {
	/* 0x00 */
	0x0000, 0x0001, 0x0002, 0x0003,
	0x0004, 0x0005, 0x0006, 0x0007,
	0x0008, 0x0009, 0x000a, 0x000b,
	0x000c, 0x000d, 0x000e, 0x000f,
	/* 0x10 */
	0x0010, 0x0011, 0x0012, 0x0013,
	0x0014, 0x0015, 0x0016, 0x0017,
	0x0018, 0x0019, 0x001a, 0x001b,
	0x001c, 0x001d, 0x001e, 0x001f,
	/* 0x20 */
	0x0020, 0x0021, 0x0022, 0x0023,
	0x0024, 0x0025, 0x0026, 0x0027,
	0x0028, 0x0029, 0x002a, 0x002b,
	0x002c, 0x002d, 0x002e, 0x002f,
	/* 0x30 */
	0x0030, 0x0031, 0x0032, 0x0033,
	0x0034, 0x0035, 0x0036, 0x0037,
	0x0038, 0x0039, 0x003a, 0x003b,
	0x003c, 0x003d, 0x003e, 0x003f,
	/* 0x40 */
	0x0040, 0x0041, 0x0042, 0x0043,
	0x0044, 0x0045, 0x0046, 0x0047,
	0x0048, 0x0049, 0x004a, 0x004b,
	0x004c, 0x004d, 0x004e, 0x004f,
	/* 0x50 */
	0x0050, 0x0051, 0x0052, 0x0053,
	0x0054, 0x0055, 0x0056, 0x0057,
	0x0058, 0x0059, 0x005a, 0x005b,
	0x005c, 0x005d, 0x005e, 0x005f,
	/* 0x60 */
	0x0060, 0x0061, 0x0062, 0x0063,
	0x0064, 0x0065, 0x0066, 0x0067,
	0x0068, 0x0069, 0x006a, 0x006b,
	0x006c, 0x006d, 0x006e, 0x006f,
	/* 0x70 */
	0x0070, 0x0071, 0x0072, 0x0073,
	0x0074, 0x0075, 0x0076, 0x0077,
	0x0078, 0x0079, 0x007a, 0x007b,
	0x007c, 0x007d, 0x007e, 0x007f,
	/* 0x80 */
	0x0410, 0x0411, 0x0412, 0x0413,
	0x0414, 0x0415, 0x0416, 0x0417,
	0x0418, 0x0419, 0x041a, 0x041b,
	0x041c, 0x041d, 0x041e, 0x041f,
	/* 0x90 */
	0x0420, 0x0421, 0x0422, 0x0423,
	0x0424, 0x0425, 0x0426, 0x0427,
	0x0428, 0x0429, 0x042a, 0x042b,
	0x042c, 0x042d, 0x042e, 0x042f,
	/* 0xa0 */
	0x2020, 0x00b0, 0x0490, 0x00a3,
	0x00a7, 0x2022, 0x00b6, 0x0406,
	0x00ae, 0x00a9, 0x2122, 0x0402,
	0x0452, 0x2260, 0x0403, 0x0453,
	/* 0xb0 */
	0x221e, 0x00b1, 0x2264, 0x2265,
	0x0456, 0x00b5, 0x0491, 0x0408,
	0x0404, 0x0454, 0x0407, 0x0457,
	0x0409, 0x0459, 0x040a, 0x045a,
	/* 0xc0 */
	0x0458, 0x0405, 0x00ac, 0x221a,
	0x0192, 0x2248, 0x2206, 0x00ab,
	0x00bb, 0x2026, 0x00a0, 0x040b,
	0x045b, 0x040c, 0x045c, 0x0455,
	/* 0xd0 */
	0x2013, 0x2014, 0x201c, 0x201d,
	0x2018, 0x2019, 0x00f7, 0x201e,
	0x040e, 0x045e, 0x040f, 0x045f,
	0x2116, 0x0401, 0x0451, 0x044f,
	/* 0xe0 */
	0x0430, 0x0431, 0x0432, 0x0433,
	0x0434, 0x0435, 0x0436, 0x0437,
	0x0438, 0x0439, 0x043a, 0x043b,
	0x043c, 0x043d, 0x043e, 0x043f,
	/* 0xf0 */
	0x0440, 0x0441, 0x0442, 0x0443,
	0x0444, 0x0445, 0x0446, 0x0447,
	0x0448, 0x0449, 0x044a, 0x044b,
	0x044c, 0x044d, 0x044e, 0x20ac,
};

static const unsigned char page00[256] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, /* 0x00-0x07 */
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, /* 0x08-0x0f */
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, /* 0x10-0x17 */
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, /* 0x18-0x1f */
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, /* 0x20-0x27 */
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, /* 0x28-0x2f */
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, /* 0x30-0x37 */
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, /* 0x38-0x3f */
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, /* 0x40-0x47 */
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, /* 0x48-0x4f */
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, /* 0x50-0x57 */
	0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, /* 0x58-0x5f */
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, /* 0x60-0x67 */
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, /* 0x68-0x6f */
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, /* 0x70-0x77 */
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, /* 0x78-0x7f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x80-0x87 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x88-0x8f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x90-0x97 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x98-0x9f */
	0xca, 0x00, 0x00, 0xa3, 0x00, 0x00, 0x00, 0xa4, /* 0xa0-0xa7 */
	0x00, 0xa9, 0x00, 0xc7, 0xc2, 0x00, 0xa8, 0x00, /* 0xa8-0xaf */
	0xa1, 0xb1, 0x00, 0x00, 0x00, 0xb5, 0xa6, 0x00, /* 0xb0-0xb7 */
	0x00, 0x00, 0x00, 0xc8, 0x00, 0x00, 0x00, 0x00, /* 0xb8-0xbf */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xc0-0xc7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xc8-0xcf */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xd0-0xd7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xd8-0xdf */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xe0-0xe7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xe8-0xef */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd6, /* 0xf0-0xf7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xf8-0xff */
};

static const unsigned char page01[256] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x00-0x07 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x08-0x0f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x10-0x17 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x18-0x1f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x20-0x27 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x28-0x2f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x30-0x37 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x38-0x3f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x40-0x47 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x48-0x4f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x50-0x57 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x58-0x5f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x60-0x67 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x68-0x6f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x70-0x77 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x78-0x7f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x80-0x87 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x88-0x8f */
	0x00, 0x00, 0xc4, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x90-0x97 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x98-0x9f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xa0-0xa7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xa8-0xaf */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xb0-0xb7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xb8-0xbf */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xc0-0xc7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xc8-0xcf */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xd0-0xd7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xd8-0xdf */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xe0-0xe7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xe8-0xef */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xf0-0xf7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xf8-0xff */
};

static const unsigned char page04[256] = {
	0x00, 0xdd, 0xab, 0xae, 0xb8, 0xc1, 0xa7, 0xba, /* 0x00-0x07 */
	0xb7, 0xbc, 0xbe, 0xcb, 0xcd, 0x00, 0xd8, 0xda, /* 0x08-0x0f */
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, /* 0x10-0x17 */
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, /* 0x18-0x1f */
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, /* 0x20-0x27 */
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, /* 0x28-0x2f */
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, /* 0x30-0x37 */
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, /* 0x38-0x3f */
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, /* 0x40-0x47 */
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xdf, /* 0x48-0x4f */
	0x00, 0xde, 0xac, 0xaf, 0xb9, 0xcf, 0xb4, 0xbb, /* 0x50-0x57 */
	0xc0, 0xbd, 0xbf, 0xcc, 0xce, 0x00, 0xd9, 0xdb, /* 0x58-0x5f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x60-0x67 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x68-0x6f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x70-0x77 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x78-0x7f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x80-0x87 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x88-0x8f */
	0xa2, 0xb6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x90-0x97 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x98-0x9f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xa0-0xa7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xa8-0xaf */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xb0-0xb7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xb8-0xbf */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xc0-0xc7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xc8-0xcf */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xd0-0xd7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xd8-0xdf */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xe0-0xe7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xe8-0xef */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xf0-0xf7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xf8-0xff */
};

static const unsigned char page20[256] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x00-0x07 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x08-0x0f */
	0x00, 0x00, 0x00, 0xd0, 0xd1, 0x00, 0x00, 0x00, /* 0x10-0x17 */
	0xd4, 0xd5, 0x00, 0x00, 0xd2, 0xd3, 0xd7, 0x00, /* 0x18-0x1f */
	0xa0, 0x00, 0xa5, 0x00, 0x00, 0x00, 0xc9, 0x00, /* 0x20-0x27 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x28-0x2f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x30-0x37 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x38-0x3f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x40-0x47 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x48-0x4f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x50-0x57 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x58-0x5f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x60-0x67 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x68-0x6f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x70-0x77 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x78-0x7f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x80-0x87 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x88-0x8f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x90-0x97 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x98-0x9f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xa0-0xa7 */
	0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, /* 0xa8-0xaf */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xb0-0xb7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xb8-0xbf */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xc0-0xc7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xc8-0xcf */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xd0-0xd7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xd8-0xdf */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xe0-0xe7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xe8-0xef */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xf0-0xf7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xf8-0xff */
};

static const unsigned char page21[256] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x00-0x07 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x08-0x0f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xdc, 0x00, /* 0x10-0x17 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x18-0x1f */
	0x00, 0x00, 0xaa, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x20-0x27 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x28-0x2f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x30-0x37 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x38-0x3f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x40-0x47 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x48-0x4f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x50-0x57 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x58-0x5f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x60-0x67 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x68-0x6f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x70-0x77 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x78-0x7f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x80-0x87 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x88-0x8f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x90-0x97 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x98-0x9f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xa0-0xa7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xa8-0xaf */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xb0-0xb7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xb8-0xbf */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xc0-0xc7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xc8-0xcf */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xd0-0xd7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xd8-0xdf */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xe0-0xe7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xe8-0xef */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xf0-0xf7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xf8-0xff */
};

static const unsigned char page22[256] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc6, 0x00, /* 0x00-0x07 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x08-0x0f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x10-0x17 */
	0x00, 0x00, 0xc3, 0x00, 0x00, 0x00, 0xb0, 0x00, /* 0x18-0x1f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x20-0x27 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x28-0x2f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x30-0x37 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x38-0x3f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x40-0x47 */
	0xc5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x48-0x4f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x50-0x57 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x58-0x5f */
	0xad, 0x00, 0x00, 0x00, 0xb2, 0xb3, 0x00, 0x00, /* 0x60-0x67 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x68-0x6f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x70-0x77 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x78-0x7f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x80-0x87 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x88-0x8f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x90-0x97 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x98-0x9f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xa0-0xa7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xa8-0xaf */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xb0-0xb7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xb8-0xbf */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xc0-0xc7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xc8-0xcf */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xd0-0xd7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xd8-0xdf */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xe0-0xe7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xe8-0xef */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xf0-0xf7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xf8-0xff */
};

static const unsigned char *const page_uni2charset[256] = {
	page00, page01, NULL,   NULL,   page04, NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	page20, page21, page22, NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
};

static const unsigned char charset2lower[256] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x00-0x07 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x08-0x0f */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x10-0x17 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x18-0x1f */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x20-0x27 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x28-0x2f */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x30-0x37 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x38-0x3f */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x40-0x47 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x48-0x4f */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x50-0x57 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x58-0x5f */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x60-0x67 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x68-0x6f */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x70-0x77 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x78-0x7f */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x80-0x87 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x88-0x8f */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x90-0x97 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x98-0x9f */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0xa0-0xa7 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0xa8-0xaf */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0xb0-0xb7 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0xb8-0xbf */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0xc0-0xc7 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0xc8-0xcf */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0xd0-0xd7 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0xd8-0xdf */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0xe0-0xe7 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0xe8-0xef */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0xf0-0xf7 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0xf8-0xff */
};

static const unsigned char charset2upper[256] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x00-0x07 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x08-0x0f */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x10-0x17 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x18-0x1f */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x20-0x27 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x28-0x2f */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x30-0x37 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x38-0x3f */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x40-0x47 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x48-0x4f */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x50-0x57 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x58-0x5f */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x60-0x67 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x68-0x6f */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x70-0x77 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x78-0x7f */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x80-0x87 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x88-0x8f */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x90-0x97 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0x98-0x9f */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0xa0-0xa7 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0xa8-0xaf */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0xb0-0xb7 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0xb8-0xbf */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0xc0-0xc7 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0xc8-0xcf */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0xd0-0xd7 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0xd8-0xdf */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0xe0-0xe7 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0xe8-0xef */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0xf0-0xf7 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0xf8-0xff */
};

static int uni2char(wchar_t uni, unsigned char *out, int boundlen)
{
	const unsigned char *uni2charset;
	unsigned char cl = uni & 0x00ff;
	unsigned char ch = (uni & 0xff00) >> 8;

	if (boundlen <= 0)
		return -ENAMETOOLONG;

	uni2charset = page_uni2charset[ch];
	if (uni2charset && uni2charset[cl])
		out[0] = uni2charset[cl];
	else
		return -EINVAL;
	return 1;
}

static int char2uni(const unsigned char *rawstring, int boundlen, wchar_t *uni)
{
	*uni = charset2uni[*rawstring];
	if (*uni == 0x0000)
		return -EINVAL;
	return 1;
}

static struct nls_table table = {
	.charset	= "maccyrillic",
	.uni2char	= uni2char,
	.char2uni	= char2uni,
	.charset2lower	= charset2lower,
	.charset2upper	= charset2upper,
	.owner		= THIS_MODULE,
};

static int __init init_nls_maccyrillic(void)
{
	return register_nls(&table);
}

static void __exit exit_nls_maccyrillic(void)
{
	unregister_nls(&table);
}

module_init(init_nls_maccyrillic)
module_exit(exit_nls_maccyrillic)

MODULE_LICENSE("Dual BSD/GPL");
