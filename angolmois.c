/*
 * Angolmois -- the simple BMS player
 * Copyright (c) 2005, 2007, 2009, 2012, 2013, Kang Seonghoon.
 * Project Angolmois is copyright (c) 2003-2007, Choi Kaya (CHKY).
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <float.h>
#include <time.h>
#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_image.h>
#include <smpeg.h>

static const char VERSION[] = "Angolmois 2.0 alpha 3";
static const char *argv0 = "angolmois";

/******************************************************************************/
/* utility declarations */

#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)

#define R(x,y,w,h) &(SDL_Rect){x,y,w,h}
#define ARRAYSIZE(arr) ((int) (sizeof(arr) / sizeof(*(arr))))

#define MEASURE_TO_MSEC(measure,bpm) ((measure) * 24e4 / (bpm))
#define MSEC_TO_MEASURE(msec,bpm) ((msec) * (bpm) / 24e4)

static void die(const char *msg, ...); /* platform dependent */
#define SHOULD(x) ((x) ? (void) 0 : die("assertion failure at line %d: %s", __LINE__, #x))

static void warn(const char *msg, ...)
{
	va_list v;
	fprintf(stderr, "*** Warning: ");
	va_start(v, msg);
	vfprintf(stderr, msg, v);
	va_end(v);
	fprintf(stderr, "\n");
}

#define UPPERCASE(c) ('a' <= (c) && (c) <= 'z' ? (c) + ('A' - 'a') : (c))
#define STRACOPY(s) strcpy(malloc(strlen(s) + 1), (s))

static int strieq(const char *a, const char *b)
{
	while (*a && *b && UPPERCASE(*a) == UPPERCASE(*b)) ++a, ++b;
	return *a == *b;
}

static int strisuffix(const char *a, const char *b)
{
	size_t alen = strlen(a), blen = strlen(b);
	return alen >= blen && strieq(a + (alen - blen), b);
}

/* generic extensible vector */
struct xv_base { ptrdiff_t xv__size, xv__alloc; };
#define XV(...) struct { struct xv_base xv__base; __VA_ARGS__ *xv__ptr; }
#define XV_BASE(xv) ((xv).xv__base)
#define XV_SIZE(xv) (XV_BASE(xv).xv__size)
#define XV_ALLOC(xv) (XV_BASE(xv).xv__alloc)
#define XV_PTR(xv) ((xv).xv__ptr)
#define XV_ITEMSIZE(xv) (ptrdiff_t) sizeof(*XV_PTR(xv))
#define XV_EMPTY {.xv__base = {.xv__size = 0, .xv__alloc = 0}, .xv__ptr = NULL}
#define XV_INIT(xv) (XV_SIZE(xv) = XV_ALLOC(xv) = 0, XV_PTR(xv) = NULL)
#define XV_INVARIANT(xv) \
	(XV_SIZE(xv) >= 0 && XV_ALLOC(xv) >= 0 && XV_SIZE(xv) <= XV_ALLOC(xv) && \
	 (XV_ALLOC(xv) > 0) == (XV_PTR(xv) != NULL))
#define XV_CHECK(xv,i) ((ptrdiff_t) (i) >= 0 && (ptrdiff_t) (i) < XV_SIZE(xv))
#define XV_RESERVE(xv,n) \
	((ptrdiff_t) (n) > XV_ALLOC(xv) && \
	 (XV_PTR(xv) = xv_do_resize(&XV_BASE(xv), XV_PTR(xv), (n), XV_ITEMSIZE(xv)), 1))
#define XV_RESIZE(xv,n) (XV_SIZE(xv) = (ptrdiff_t) (n), XV_RESERVE(xv, XV_SIZE(xv)))
#define XV_RESIZEBY(xv,n) XV_RESIZE(xv, XV_SIZE(xv) + (ptrdiff_t) (n))
#define XV_AT(xv,i) (XV_PTR(xv)[(ptrdiff_t) (i)])
#define XV_LAST(xv) (XV_PTR(xv)[XV_SIZE(xv)-1])
#define XV_LOOP(xv,itype,i,before,after) \
	for (itype (i) = 0; (ptrdiff_t) (i) < XV_SIZE(xv) && ((void) (before), 1); \
	     (void) (after), ++(i))
#define XV_IEACH(i,v,xv) XV_LOOP(xv, ptrdiff_t, i, (v) = XV_AT(xv,i), 0)
#define XV_IEACHPTR(i,p,xv) XV_LOOP(xv, ptrdiff_t, i, (p) = &XV_AT(xv,i), 0)
#define XV_EACH(v,xv) XV_IEACH(xv__i_##__LINE__,v,xv)
#define XV_EACHPTR(p,xv) XV_IEACHPTR(xv__i_##__LINE__,p,xv)
#define XV_PUSH(xv,x) ((void) XV_RESERVE(xv, XV_SIZE(xv)+1), XV_AT(xv, XV_SIZE(xv)++) = (x))
#define XV_POP(xv) (XV_PTR(xv)[--XV_SIZE(xv)])
#define XV_COPYTO(p,xv,i,n) memcpy((p), XV_PTR(xv)+i, (n)*XV_ITEMSIZE(xv))
#define XV_COPYFROM(xv,i,p,n) ((void) memcpy(XV_PTR(xv)+i, (p), (n)*XV_ITEMSIZE(xv)))
#define XV_COPYPUSH(xv,p,n) \
	((void) XV_RESERVE(xv, XV_SIZE(xv) + (ptrdiff_t) (n)), \
	 XV_COPYFROM(xv, XV_SIZE(xv), p, n), (void) (XV_SIZE(xv) += (n)))
#define XV_ZEROIZE(xv,i,n) memset(XV_PTR(xv) + (i), 0, (n) * XV_ITEMSIZE(xv))
#define XV_FREE(xv) if (XV_ALLOC(xv) == 0) { } else free(XV_PTR(xv))

static void *xv_do_resize(struct xv_base *base, void *ptr, ptrdiff_t n, ptrdiff_t itemsize)
{
	static const ptrdiff_t GROWLIMIT = 0x7fff;
	if (n <= GROWLIMIT) {
		--n; n |= n >> 1; n |= n >> 2; n |= n >> 4; n |= n >> 8; ++n;
		if (n < 4) n = 4;
	} else {
		if (n > (PTRDIFF_MAX & ~GROWLIMIT)) die("memory error"); /* overflow */
		n = (n + GROWLIMIT) & ~GROWLIMIT;
	}
	ptr = realloc(ptr, n * itemsize);
	if (!ptr) die("memory error");
	base->xv__alloc = n;
	return ptr;
}

/* George Marsaglia's MWC256 generator (period 2^8222) */
struct rngstate { uint32_t state[257]; uint8_t index; }; /* state[256] for carry */

static void rng_seed(struct rngstate *r, uint32_t seed)
{
	r->index = 0;
	r->state[256] = seed;
	for (int i = 255; i >= 0; --i) {
		r->state[i] = 1812433253ull * (r->state[i+1] ^ (r->state[i+1] >> 30)) + i;
	}
}

static uint32_t rng_gen(struct rngstate *r, uint32_t range)
{
	uint32_t div = 0xffffffffull / range, max = div * range, x; /* so max < 2^32 */
	do {
		uint64_t t = 1540315826ull * r->state[++r->index] + r->state[256];
		r->state[256] = t >> 32;
		x = r->state[256] + (t & 0xffffffffull);
		if (x < r->state[256]) ++x, ++r->state[256];
		r->state[r->index] = x;
	} while (x >= max);
	return x / div;
}

/******************************************************************************/
/* path resolution & system dependent functions */

static char *bmspath; /* initially a path to BMS file, later a dirname of it */

static const char *SOUND_EXTS[] = {".wav", ".ogg", ".mp3", NULL};
static const char *IMAGE_EXTS[] = {".bmp", ".png", ".jpg", ".jpeg", ".gif", NULL};

static int match_filename(const char *s, const char *t, const char **exts)
{
	const char *sbegin = s, *send = s + strlen(s);
	while (*s && *t && UPPERCASE(*s) == UPPERCASE(*t)) ++s, ++t;
	if (*s == *t) return 1;
	if (sbegin != s && exts) {
		for (; *exts; ++exts) {
			int l = (int) strlen(*exts);
			if (send-s < l && strieq(send-l, *exts)) return 1;
		}
	}
	return 0;
}

#ifdef _WIN32

#include <windows.h>
static const char DIRSEP = '\\';

static int filedialog(char *buf)
{
	OPENFILENAME ofn = {
		.lStructSize = sizeof ofn,
		.lpstrFilter =
			"All Be-Music Source File (*.bms;*.bme;*.bml;*.pms)\0*.bms;*.bme;*.bml;*.pms\0"
			"Be-Music Source File (*.bms)\0*.bms\0"
			"Extended Be-Music Source File (*.bme)\0*.bme\0"
			"Longnote Be-Music Source File (*.bml)\0*.bml\0"
			"Po-Mu Source File (*.pms)\0*.pms\0"
			"All Files (*.*)\0*.*\0",
		.lpstrFile = buf,
		.nMaxFile = 512,
		.lpstrTitle = "Choose a file to play",
		.Flags = OFN_HIDEREADONLY};
	return GetOpenFileName(&ofn);
}

static void die(const char *msg, ...)
{
	va_list v;
	char buf[512];
	va_start(v, msg);
	vsnprintf(buf, sizeof buf, msg, v);
	va_end(v);
	MessageBox(0, buf, VERSION, 0);
	exit(1);
}

static SDL_RWops *resolve_relative_path(char *path, const char **exts)
{
	HANDLE h;
	WIN32_FIND_DATAA fdata;
	char pathbuf[strlen(bmspath)+strlen(path)+4], *p, *basename = path;
	SDL_RWops *ops = NULL;

	strcpy(pathbuf, *bmspath ? bmspath : ".");
	p = pathbuf + strlen(pathbuf);
	for (*p++ = '\\'; *path; ) {
		if (*path == '/' || *path == '\\') {
			*p++ = '\\';
			basename = ++path;
		} else {
			*p++ = *path++;
		}
	}
	*p = '\0';
	path = strrchr(pathbuf, '\\') + 1;
	p = strrchr(path, '.');
	if (p) strcpy(p, ".*");
	h = FindFirstFileA(pathbuf, &fdata);
	if (h == INVALID_HANDLE_VALUE) return NULL;
	do {
		if (match_filename(fdata.cFileName, basename, exts)) {
			strcpy(path, fdata.cFileName);
			ops = SDL_RWFromFile(pathbuf, "rb");
			break;
		}
	} while (FindNextFileA(h, &fdata));
	FindClose(h);
	return ops;
}

#else /* _WIN32 */

#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
static const char DIRSEP = '/';

static int filedialog(char *buf)
{
	(void) buf;
	return 0;
}

static void die(const char *msg, ...)
{
	va_list v;
	fprintf(stderr, "%s: ", argv0);
	va_start(v, msg);
	vfprintf(stderr, msg, v);
	va_end(v);
	fprintf(stderr, "\n");
	exit(1);
}

static SDL_RWops *resolve_relative_path(char *path, const char **exts)
{
	DIR *d;
	struct dirent *e;
	int origcwd, i;
	char pathbuf[strlen(path)+1]; /* so that we won't overwrite the original path */
	SDL_RWops *ops = NULL;

	path = strcpy(pathbuf, path);
	origcwd = open(".", O_RDONLY);
	if (origcwd < 0) return NULL;
	if (*bmspath && chdir(bmspath) < 0) goto exit;
	for (; path[i = strcspn(path, "\\/")]; ++path) {
		if (i == 0) continue; /* skips "//" or trailing "/" etc. */
		path[i] = '\0';
		d = opendir(".");
		if (!d) goto exit;
		while ((e = readdir(d))) {
			if (strieq(e->d_name, path)) {
				if (chdir(e->d_name) < 0) {
					closedir(d);
					goto exit;
				}
				break;
			}
		}
		closedir(d);
		if (!e) goto exit;
		path += i;
	}
	d = opendir(".");
	if (!d) goto exit;
	while ((e = readdir(d))) {
		if (match_filename(e->d_name, path, exts)) {
			ops = SDL_RWFromFile(e->d_name, "rb");
			break;
		}
	}
	closedir(d);
exit:
	fchdir(origcwd);
	close(origcwd);
	return ops;
}

#endif

/******************************************************************************/
/* bms parser */

enum { S_TITLE = 0, S_GENRE = 1, S_ARTIST = 2, S_STAGEFILE = 3, S_BASEPATH = 4 };
enum { V_PLAYER = 0, V_PLAYLEVEL = 1, V_RANK = 2, V_LNTYPE = 3, V_LNOBJ = 4 };

#define MAXSTRING 1023
static char string[5][MAXSTRING+1];
static double initbpm = 130;
static int value[] = {[V_PLAYER]=1, [V_PLAYLEVEL]=0, [V_RANK]=2, [V_LNTYPE]=1, [V_LNOBJ]=0};

#define MAXKEY 1296
static char *sndpath[MAXKEY], *imgpath[MAXKEY];
static struct sndres { Mix_Chunk *res; int lastch; } sndres[MAXKEY];
static XV(int) sndlastchmap;
static struct imgres { SDL_Texture *tex; SMPEG *movie; SMPEG_Frame *frame; } imgres[MAXKEY];
static struct bgares { struct imgres *image; int x, y, tx, ty, w, h; } bgares[MAXKEY];
static double bpmtab[MAXKEY], stoptab[MAXKEY];

#define NNOTECHANS (2*36)
#define IS_NOTE_CHANNEL(c) ((c) >= 0 && (c) < NNOTECHANS)
enum { BGM_CHANNEL = NNOTECHANS, BGA_CHANNEL = NNOTECHANS+1, BPM_CHANNEL = NNOTECHANS+2,
	STOP_CHANNEL = NNOTECHANS+3, NCHANS = NNOTECHANS+3 };
enum NOTE_type { LNDONE = 0, LNSTART = 1, NOTE = 2, INVNOTE = 3, BOMB = 4 };
enum BGA_type { BGA_LAYER = 0, BGA2_LAYER = 1, BGA3_LAYER = 2, POORBGA_LAYER = 3 };
enum BPM_type { BPM_BY_VALUE = 0, BPM_BY_INDEX = 1 };
enum STOP_type { STOP_BY_MEASURE = 0, STOP_BY_MSEC = 1 };

static const char *preset, *leftkeys, *rightkeys;
static const struct preset { const char *name1, *name2, *left, *right; } presets[] = {
	{"5", "10", "16s 11a 12b 13a 14b 15a", "21a 22b 23a 24b 25a 26s"},
	{"5/fp", "10/fp", "16s 11a 12b 13a 14b 15a 17p", "27p 21a 22b 23a 24b 25a 26s"},
	{"7", "14", "16s 11a 12b 13a 14b 15a 18b 19a", "21a 22b 23a 24b 25a 28b 29a 26s"},
	{"7/fp", "14/fp", "16s 11a 12b 13a 14b 15a 18b 19a 17p", "27p 21a 22b 23a 24b 25a 28b 29a 26s"},
	{"9", NULL, "11q 12w 13e 14r 15t 22r 23e 24w 25q", NULL},
	{"9-bme", NULL, "11q 12w 13e 14r 15t 18r 19e 16w 17q", NULL}};
static const char KEYKIND_MNEMONICS[] = "*aybqwertsp"; /* should align with tkeykinds */

static struct obj { double time; int chan, type, index, value, nograding:1; } *objs;
static int nobjs;
static double shortens[1000], originoffset = 0.0, length;
static int nleftkeys, nrightkeys, keyorder[NNOTECHANS], keykind[NNOTECHANS];
static int nkeys, haslongnote, hasbpmchange, nnotes, maxscore, duration;

static double shorten(double time)
{
	return (time < 0 || time >= ARRAYSIZE(shortens) ? 1.0 : shortens[(int) time]);
}

static int getdigit(int n)
{
	if ('0' <= n && n <= '9') return n - '0';
	if ('a' <= n && n <= 'z') return (n - 'a') + 10;
	if ('A' <= n && n <= 'Z') return (n - 'A') + 10;
	return -MAXKEY;
}

static int key2index(const char *s, int *v) /* requires s[0] and s[1] allocated */
{
	*v = getdigit(s[0]) * 36 + getdigit(s[1]);
	return (*v >= 0);
}

static int compare_bmsline(const void *a, const void *b)
{
	return strncmp(*(const char *const *)a, *(const char *const *)b, 6);
}

static int compare_obj(const void *a, const void *b)
{
	const struct obj *A = a, *B = b;
	return (A->time > B->time ? 1 : A->time < B->time ? -1 : A->type - B->type);
}

static void add_obj(int chan, double time, int type, int idx, int val)
{
	objs = realloc(objs, sizeof(struct obj) * (nobjs+1));
	objs[nobjs++] = (struct obj) {.time=time, .chan=chan, .type=type, .index=idx, .value=val};
}

#define KEY_STRING "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
#define KEY_PATTERN "%2[" KEY_STRING "]"
#define TO_KEY(key) ((char[3]){ KEY_STRING[(key)/36], KEY_STRING[(key)%36], '\0' })

static void parse_bms(struct rngstate *r)
{
	static const char *bmsheader[] = {
		NULL, "TITLE", "GENRE", "ARTIST", "STAGEFILE", "PATH_WAV", "BPM", "PLAYER",
		"PLAYLEVEL", "RANK", "LNTYPE", "LNOBJ", "WAV", "BMP", "BGA", "STOP", "STP",
		"RANDOM", "SETRANDOM", "ENDRANDOM", "IF", "ELSEIF", "ELSE", "ENDSW", "END"};

	FILE *fp;
	int i, j, k, a, b, measure = 0, chan, poorbgafix = 1;
	int prev12[NNOTECHANS] = {0}, prev56[NNOTECHANS] = {0};
	double t;
	char *line, linebuf[4096], buf1[4096], buf2[4096];
	XV(struct rnd { int val, state, skip; }) rnd = XV_EMPTY;
	XV(char*) bmsline = XV_EMPTY;

	fp = fopen(bmspath, "r");
	if (!fp) die("Couldn't load BMS file: %s", bmspath);

	XV_PUSH(rnd, ((struct rnd) {.val=0, .state=-1, .skip=0}));
	while (fgets(line = linebuf, sizeof linebuf, fp)) {
		while (*line == ' ' || *line == '\t') ++line;
		if (*line++ != '#') continue;

		for (i = 1; i < ARRAYSIZE(bmsheader); ++i) {
			for (j = 0; bmsheader[i][j]; ++j) {
				if (bmsheader[i][j] != UPPERCASE(line[j])) break;
			}
			if (!bmsheader[i][j]) {
				line += j;
				break;
			}
		}

		switch (XV_LAST(rnd).skip || XV_LAST(rnd).state > 0 ? -i : i) {
		case 1: /* title */
		case 2: /* genre */
		case 3: /* artist */
		case 4: /* stagefile */
		case 5: /* path_wav */
			sscanf(line, "%*[ \t]%" STRINGIFY(MAXSTRING) "[^\r\n]", string[i-1]);
			break;

		case 6: /* bpm */
			if (sscanf(line, "%*[ \t]%lf", &t) >= 1) {
				if (t > 0) initbpm = t;
			} else if (sscanf(line, KEY_PATTERN "%*[ ]%lf", buf1, &t) >= 2 && key2index(buf1, &i)) {
				bpmtab[i] = t;
			}
			break;

		case 7: /* player */
		case 8: /* playlevel */
		case 9: /* rank */
		case 10: /* lntype */
			sscanf(line, "%*[ \t]%d", &value[i-7]);
			break;

		case 11: /* lnobj */
			if (sscanf(line, "%*[ \t]" KEY_PATTERN, buf1) >= 1 && key2index(buf1, &i)) {
				value[V_LNOBJ] = i;
			}
			break;

		case 12: /* wav## */
		case 13: /* bmp## */
			if (sscanf(line, KEY_PATTERN "%*[ \t]%[^\r\n]", buf1, buf2) >= 2 && key2index(buf1, &j)) {
				char **path = (i==12 ? sndpath : imgpath);
				free(path[j]);
				path[j] = STRACOPY(buf2);
			}
			break;

		case 14: /* bga## */
			{
				int dst, src, x1, y1, x2, y2, dx, dy;
				if (sscanf(line, KEY_PATTERN "%*[ \t]" KEY_PATTERN "%*[ ]%d %d %d %d %d %d",
						   buf1, buf2, &x1, &y1, &x2, &y2, &dx, &dy) >= 8 &&
						key2index(buf1, &dst) && key2index(buf2, &src)) {
					if (x1 < 0) x1 = 0;
					if (y1 < 0) y1 = 0;
					if (x2 > x1 + 256) x2 = x1 + 256;
					if (y2 > y1 + 256) y2 = y1 + 256;
					bgares[dst] = (struct bgares) {.image=&imgres[src], .x=x1, .y=y1, .tx=dx, .ty=dy, .w=x2-x1, .h=y2-y1};
				}
			}
			break;

		case 15: /* stop## */
			if (sscanf(line, KEY_PATTERN "%*[ \t]%d", buf1, &j) >= 2 && key2index(buf1, &i)) {
				if (j > 0) stoptab[i] = j / 192.0;
			}
			break;

		case 16: /* stp## */
			if (sscanf(line, "%d.%d %d", &i, &j, &k) >= 3) {
				if (k > 0) add_obj(STOP_CHANNEL, i+j/1e3, STOP_BY_MSEC, k, 0);
			}
			break;

		case 17: case -17: /* random */
		case 18: case -18: /* setrandom */
			if (sscanf(line, "%*[ \t]%d", &j) >= 1) {
				/* do not generate a random value if the entire block is skipped */
				k = (XV_LAST(rnd).state > 0 || XV_LAST(rnd).skip);
				if (i == 17 && !k && j > 0) j = rng_gen(r, j) + 1;
				XV_PUSH(rnd, ((struct rnd) {.val=j, .state=-1, .skip=k}));
			}
			break;

		case 19: case -19: /* endrandom */
			if (XV_SIZE(rnd) > 1) --XV_SIZE(rnd);
			break;

		case 20: case -20: /* if */
		case 21: case -21: /* elseif */
			if (sscanf(line, "%*[ \t]%d", &j) >= 1) {
				if (i == 20 || XV_LAST(rnd).state == 1) {
					XV_LAST(rnd).state = (j <= 0 || XV_LAST(rnd).val != j);
				} else {
					XV_LAST(rnd).state = 2; /* ignore further branches */
				}
			}
			break;

		case 22: case -22: /* else */
			XV_LAST(rnd).state = (XV_LAST(rnd).state == 1 ? 0 : 2);
			break;

		case 24: case -24: /* end(if) but not endsw */
			for (j = (int) XV_SIZE(rnd) - 1; j > 0 && XV_AT(rnd,j).state < 0; --j);
			if (j > 0) XV_SIZE(rnd) = j + 1; /* implicitly closes #RANDOMs if needed */
			XV_LAST(rnd).state = -1;
			break;

		case ARRAYSIZE(bmsheader): /* #####:... */
			/* only check validity, do not store them yet */
			if (sscanf(line, "%*1[0123456789]%*1[0123456789]%*1[0123456789]"
			           "%*1[" KEY_STRING "]%*1[" KEY_STRING "]:%c", buf1) >= 1) {
				XV_PUSH(bmsline, STRACOPY(line));
			}
		}
	}
	fclose(fp);
	XV_FREE(rnd);

	qsort(XV_PTR(bmsline), XV_SIZE(bmsline), XV_ITEMSIZE(bmsline), compare_bmsline);
	XV_EACH(line, bmsline) {
		measure = (line[0] - '0') * 100 + (line[1] - '0') * 10 + (line[2] - '0');
		if (!key2index(line+3, &chan)) SHOULD(0);
		if (chan == 2) {
			shortens[measure] = atof(line+6);
		} else {
			j = 6 + strspn(line+6, " \t\r\n");
			a = strcspn(line+j, " \t\r\n") / 2;
			for (k = 0; k < a; ++k, j+=2) {
				if (!key2index(line+j, &b) || !b) continue;
				t = measure + 1. * k / a;
				if (chan == 1) {
					add_obj(BGM_CHANNEL, t, 0, b, 0);
				} else if (chan == 3) {
					if (b/36<16 && b%36<16) add_obj(BPM_CHANNEL, t, BPM_BY_VALUE, b/36*16+b%36, 0);
				} else if (chan == 4) {
					add_obj(BGA_CHANNEL, t, BGA_LAYER, b, 0);
				} else if (chan == 6) {
					add_obj(BGA_CHANNEL, t, POORBGA_LAYER, b, 0);
					if (t == 0) poorbgafix = 0;
				} else if (chan == 7) {
					add_obj(BGA_CHANNEL, t, BGA2_LAYER, b, 0);
				} else if (chan == 8) {
					add_obj(BPM_CHANNEL, t, BPM_BY_INDEX, b, 0);
				} else if (chan == 9) {
					add_obj(STOP_CHANNEL, t, STOP_BY_MEASURE, b, 0);
				} else if (chan == 10) {
					add_obj(BGA_CHANNEL, t, BGA3_LAYER, b, 0);
				} else if (chan >= 1*36 && chan < 3*36) { /* channels 1x/2x */
					int c = chan - 1*36;
					if (value[V_LNOBJ] && b == value[V_LNOBJ]) {
						if (prev12[c] && objs[prev12[c]-1].type==NOTE) {
							objs[prev12[c]-1].type = LNSTART;
							add_obj(c, t, LNDONE, b, 0);
							prev12[c] = 0;
						}
					} else {
						add_obj(c, t, NOTE, b, 0);
						prev12[c] = nobjs;
					}
				} else if (chan >= 3*36 && chan < 5*36) { /* channels 3x/4x */
					add_obj(chan - 3*36, t, INVNOTE, b, 0);
				} else if (chan >= 5*36 && chan < 7*36) { /* channels 5x/6x */
					int c = chan - 5*36;
					if (value[V_LNTYPE] == 1) {
						if (prev56[c]) {
							add_obj(c, t, LNDONE, b, 0);
							prev56[c] = 0;
						} else {
							add_obj(c, t, LNSTART, b, 0);
							prev56[c] = nobjs;
						}
					} else if (value[V_LNTYPE] == 2) {
						double t2 = measure + 1. * (k + 1) / a;
						if (prev56[c] && objs[prev56[c]-1].time == t) {
							objs[prev56[c]-1].time = t2;
							objs[prev56[c]-1].index = b;
						} else {
							add_obj(c, t, LNSTART, b, 0);
							add_obj(c, t2, LNDONE, b, 0);
							prev56[c] = nobjs;
						}
					}
				} else if (chan >= 13*36 && chan < 15*36) { /* channels Dx/Ex */
					int dmg = (b == MAXKEY-1 ? -1 : b * 512 / 200);
					if (dmg && dmg <= 512) add_obj(chan - 13*36, t, BOMB, 0, dmg);
				}
			}
		}
		free(line);
	}
	XV_FREE(bmsline);
	if (poorbgafix) add_obj(BGA_CHANNEL, 0, POORBGA_LAYER, 0, 0); /* for POOR BGA movie */

	length = measure + 2;
	for (int i = 0; i < NNOTECHANS; ++i) {
		if (prev12[i] || (value[V_LNTYPE] == 1 && prev56[i])) {
			add_obj(i, measure + 1, LNDONE, 0, 0);
		}
	}
	for (int i = 0; i < ARRAYSIZE(shortens); ++i) {
		if (shortens[i] <= .001) shortens[i] = 1;
	}
}

static void remove_or_replace_note(int i)
{
	if (IS_NOTE_CHANNEL(objs[i].chan) && objs[i].type > LNDONE && objs[i].type < INVNOTE && objs[i].index) {
		objs[i].chan = BGM_CHANNEL;
		objs[i].type = 0;
	} else {
		objs[i].chan = -1;
	}
}

static void sanitize_bms(void)
{
	if (!objs) return;
	qsort(objs, nobjs, sizeof(struct obj), compare_obj);
	for (int i = 0; i < NCHANS; ++i) if (i != BGM_CHANNEL && i != STOP_CHANNEL) {
		int inside = 0, j = 0;
		while (j < nobjs) {
			int k = j, types = 0, lowest;
			for (; k < nobjs && objs[k].time <= objs[j].time; ++k) if (objs[k].chan == i) {
				if (types & (1 << objs[k].type)) {
					remove_or_replace_note(k);
				} else {
					types |= 1 << objs[k].type;
				}
			}

			/* remove overlapping LN endpoints altogether */
			if (!(~types & ((1<<LNSTART)|(1<<LNDONE)))) types &= ~((1<<LNSTART)|(1<<LNDONE));
			/* remove prohibited types according to inside */
			types &= ~(inside ? (1<<LNSTART)|(1<<NOTE)|(1<<BOMB) : (1<<LNDONE));
			/* invisible note cannot overlap with long note endpoints */
			if (types & ((1<<LNSTART)|(1<<LNDONE))) types &= ~(1<<INVNOTE);
			/* keep the most important (lowest) type, except for BOMB/INVNOTE combination */
			lowest = types & -types;
			types = lowest | (lowest == (1<<INVNOTE) ? types & (1<<BOMB) : 0);

			inside = (types & (1<<LNSTART) ? 1 : types & (1<<LNDONE) ? 0 : inside);

			for (; j < k; ++j) if (objs[j].chan == i) {
				if (IS_NOTE_CHANNEL(i) && !(types & (1 << objs[j].type))) {
					remove_or_replace_note(j);
				}
			}
		}
		if (IS_NOTE_CHANNEL(i) && inside) {
			/* remove last starting longnote which is unfinished */
			for (j = nobjs - 1; j >= 0 && objs[j].chan != i; --j);
			if (j >= 0 && objs[j].type == LNSTART) remove_or_replace_note(j);
		}
	}
}

static const char *detect_preset(const char *preset)
{
	int present[NNOTECHANS] = {0};
	for (int i = 0; i < nobjs; ++i) {
		if (IS_NOTE_CHANNEL(objs[i].chan)) present[objs[i].chan] = 1;
	}
	if (!preset || strieq(preset, "bms") || strieq(preset, "bme") || strieq(preset, "bml")) {
		int isbme = (present[8] || present[9] || present[36+8] || present[36+9]);
		int haspedal = (present[7] || present[36+7]);
		if (value[V_PLAYER] == 2 || value[V_PLAYER] == 3) {
			preset = (isbme ? (haspedal ? "14/fp" : "14") : (haspedal ? "10/fp" : "10"));
		} else {
			preset = (isbme ? (haspedal ? "7/fp" : "7") : (haspedal ? "5/fp" : "5"));
		}
	} else if (strieq(preset, "pms")) {
		preset = (present[6] || present[7] || present[8] || present[9] ? "9-bme" : "9");
	}
	return preset;
}

#define KEYKIND_IS_KEY(kind) ((kind) >= 1 && (kind) <= 8) /* no scratch nor pedal */
static int parse_key_spec(const char *s, int offset)
{
	s += strspn(s, " \t\r\n");
	do {
		int chan, kind;
		if (!s[0] || !key2index(s, &chan)) return -1;
		chan -= 1*36;
		if (chan < 0 || chan >= NNOTECHANS || keykind[chan]) return -1;
		for (kind = 1; KEYKIND_MNEMONICS[kind] && KEYKIND_MNEMONICS[kind] != s[2]; ++kind);
		if (!KEYKIND_MNEMONICS[kind]) return -1;
		s += 3 + strspn(s + 3, " \t\r\n");
		keyorder[offset++] = chan;
		keykind[chan] = kind;
		if (KEYKIND_IS_KEY(kind)) ++nkeys;
	} while (*s);
	return offset;
}

static void analyze_and_compact_bms(const char *left, const char *right)
{
	int i, j;

	if (!left) die("No key model is specified using -k or -K");
	nleftkeys = parse_key_spec(left, 0);
	if (nleftkeys < 0) die("Invalid key spec for left hand side: %s", left);
	if (right && *right) {
		nrightkeys = parse_key_spec(right, nleftkeys);
		if (nrightkeys < 0) die("Invalid key spec for right hand side: %s", right);
		if (value[V_PLAYER] != 2) nleftkeys = nrightkeys; /* no split panes */
	}

	hasbpmchange = haslongnote = nnotes = maxscore = 0;
	for (i = 0; i < nobjs; ++i) {
		if (IS_NOTE_CHANNEL(objs[i].chan)) {
			if (keykind[objs[i].chan]) {
				if (objs[i].type == LNSTART) haslongnote = 1;
				if (objs[i].type == LNSTART || objs[i].type == NOTE) {
					++nnotes;
					if (objs[i].time < 1.0) originoffset = -1.0;
				}
			} else {
				remove_or_replace_note(i);
			}
		} else if (objs[i].chan == BPM_CHANNEL) {
			hasbpmchange = 1;
		}
	}
	for (i = 0; i < nnotes; ++i) {
		maxscore += (int)(300 * (1 + 1. * i / nnotes));
	}

	for (i = j = 0; i < nobjs; ++i) {
		if (objs[i].chan >= 0) objs[i-j] = objs[i]; else ++j;
	}
	nobjs -= j;
}

static double adjust_object_time(double base, double offset)
{
	int i = (int)(base+1)-1;
	if ((i + 1 - base) * shorten(i) > offset) return base + offset / shorten(i);
	offset -= (i + 1 - base) * shorten(i);
	while (shorten(++i) <= offset) offset -= shorten(i);
	return i + offset / shorten(i);
}

static double adjust_object_position(double base, double time)
{
	int i = (int)(base+1)-1, j = (int)(time+1)-1;
	base = (time - j) * shorten(j) - (base - i) * shorten(i);
	while (i < j) base += shorten(i++);
	return base;
}

static int get_bms_duration(void)
{
	double pos = originoffset, bpm = initbpm, time = 0, sndtime = 0;
	for (int i = 0; i < nobjs; ++i) {
		int chan = objs[i].chan, type = objs[i].type, index = objs[i].index;
		double sndlen = 0;
		time += MEASURE_TO_MSEC(adjust_object_position(pos, objs[i].time), bpm);
		if ((IS_NOTE_CHANNEL(chan) && (type == NOTE || type == LNSTART)) || chan == BGM_CHANNEL) {
			if (index && sndres[index].res) sndlen = sndres[index].res->alen / 176.4;
		} else if (chan == BPM_CHANNEL) {
			double tmp = (type == BPM_BY_INDEX ? bpmtab[index] : index);
			if (tmp > 0) {
				bpm = tmp;
			} else if (tmp < 0) {
				time += MEASURE_TO_MSEC(adjust_object_position(originoffset, pos), -tmp);
				goto earlyexit;
			}
		} else if (chan == STOP_CHANNEL) {
			time += (type == STOP_BY_MSEC ? index : MEASURE_TO_MSEC(stoptab[index], bpm));
		}
		if (sndtime < time + sndlen) sndtime = time + sndlen;
		pos = objs[i].time;
	}
	time += MEASURE_TO_MSEC(adjust_object_position(pos, length), bpm);
earlyexit: /* choose the last note or all key sound ceases to play, whichever comes later */
	return (int) (time > sndtime ? time : sndtime);
}

enum modf { NO_MODF, MIRROR_MODF, SHUFFLE_MODF, SHUFFLEEX_MODF, RANDOM_MODF, RANDOMEX_MODF };
static void shuffle_bms(enum modf mode, struct rngstate *r, int begin, int end)
{
	int perm[NCHANS], map[NNOTECHANS], nmap = 0;

	for (int i = 0; i < ARRAYSIZE(perm); ++i) perm[i] = i;
	for (int i = begin; i < end; ++i) {
		int chan = keyorder[i];
		if (mode != SHUFFLEEX_MODF && mode != RANDOMEX_MODF && !KEYKIND_IS_KEY(keykind[chan])) continue;
		map[nmap++] = chan;
	}

#define SWAP(x,y) do { int temp = (x); (x) = (y); (y) = temp; } while (0)

	if (mode <= MIRROR_MODF) { /* mirror */
		for (int i = 0, j = nmap-1; i < j; ++i, --j) SWAP(perm[map[i]], perm[map[j]]);
		for (int i = 0; i < nobjs; ++i) objs[i].chan = perm[objs[i].chan];
	} else if (mode <= SHUFFLEEX_MODF) { /* shuffle */
		for (int i = nmap-1; i > 0; --i) {
			int other = rng_gen(r, i);
			SWAP(perm[map[i]], perm[map[other]]);
		}
		for (int i = 0; i < nobjs; ++i) objs[i].chan = perm[objs[i].chan];
	} else if (mode <= RANDOMEX_MODF) { /* random */
		double lasttime = DBL_MIN;
		for (int i = 0; i < nobjs; ++i) {
			if (IS_NOTE_CHANNEL(objs[i].chan) && objs[i].type == LNSTART) {
				int j;
				for (j = 0; map[j] != objs[i].chan; ++j);
				map[j] = map[--nmap];
			}
			if (lasttime < objs[i].time) { /* reshuffle required */
				lasttime = objs[i].time + 1e-4;
				for (int j = nmap-1; j > 0; --j) {
					int other = rng_gen(r, j);
					SWAP(perm[map[j]], perm[map[other]]);
				}
			}
			if (IS_NOTE_CHANNEL(objs[i].chan) && objs[i].type == LNDONE) {
				map[nmap++] = objs[i].chan;
			}
			objs[i].chan = perm[objs[i].chan];
		}
	}
}

/******************************************************************************/
/* general graphic functions and font functions */

static SDL_Window *screen;
static SDL_Renderer *renderer;
static Uint16 fontdata[3072];
static Uint8 (*zoomfont[16])[96] = {NULL};

static int putpixel(SDL_Surface *s, int x, int y, int c)
{
	c = SDL_MapRGB(s->format, c >> 16, (c >> 8) & 255, c & 255);
	return ((Uint32*)s->pixels)[x+y*s->pitch/4] = c;
}

static int blend(int x, int y, int a, int b)
{
	for (int i = 0; i < 24; i += 8) {
		y += ((x>>i&255) - (y>>i&255))*a/b << i;
	}
	return y;
}

static void fontdecompress(void)
{
	int words[] = /* delta coded words */
		{0, 0, 2, 6, 2, 5, 32, 96, 97, 15, 497, 15, 1521, 15, 1537, 16, 48,
		 176, 1, 3, 1, 3, 7, 1, 4080, 4096, 3, 1, 8, 3, 4097, 4080, 16,
		 16128, 240, 1, 2, 9, 3, 8177, 15, 16385, 240, 15, 1, 47, 721, 143,
		 2673, 2, 6, 7, 1, 31, 17, 16, 63, 64, 33, 0, 1, 2, 1, 8, 3};
	const char *indices = /* LZ77-compressed indices to words */
		"!!7a/&/&s$7a!f!'M*Q*Qc$(O&J!!&J&Jc(e!2Q2Qc$-Bg2m!2bB[Q7Q2[e&2Q!Qi>"
		"&!&!>UT2T2&2>WT!c*T2GWc8icM2U2D!.8(M$UQCQ-jab!'U*2*2*2TXbZ252>9ZWk"
		"@*!*!*8(J$JlWi@cxQ!Q!d$#Q'O*?k@e2dfejcNl!&JTLTLG_&J>]c*&Jm@cB&J&J7"
		"[e(o>pJM$Qs<7[{Zj`Jm40!3!.8(M$U!C!-oR>UQ2U2]2a9Y[S[QCQ2GWk@*M*Q*B*"
		"!*!g$aQs`G8.M(U$[!Ca[o@Q2Q!IJQ!Q!c,GWk@787M6U2C2d!a[2!2k?!bnc32>[u"
		"`>Uc4d@b(q@abXU!D!.8(J&J&d$q`Q2IXu`g@Q2aWQ!q@!!ktk,x@M$Qk@3!.8(M$U"
		"!H#W'O,?4m_f!7[i&n!:eX5ghCk=>UQ2Q2U2Dc>J!!&J&b&k@J)LKg!GK!)7Wk@'8,"
		"M=UWCcfa[c&Q2l`f4If(Q2G[l@MSUQC!2!2c$Q:RWGOk@,[<2WfZQ2U2D2.l`a[eZ7"
		"f(!2b2|@b$j!>MSUQCc6[2W2Q:RWGOk@Q2Q2c$a[g*Ql`7[&J&Jk$7[l`!Qi$d^GWk"
		"@U2D2.9([$[#['[,@<2W2k@!2!2m$a[l`:^[a[a[T2Td~c$k@d2:R[V[a@_b|o@,M="
		"UWCgZU:EW.Ok@>[g<G[!2!2d$k@Ug@Q2V2a2IW_!Wt`Ih*q`!2>WQ!Q!c,Gk_!7[&J"
		"&Jm$k@gti$m`k:U:EW.O(?s@T2Tb$a[CW2Qk@M+U:^[GbX,M>U`[WCO-l@'U,D<.W("
		"O&J&Je$k@a[Q!U!]!G8.M(U$[!Ca[k@*Q!Q!l$b2m!+!:#W'O,?4!1n;c`*!*!l$h`"
		"'8,M=UWCO-pWz!a[i,#Q'O,?4~R>QQ!Q!aUQ2Q2Q2aWl=2!2!2>[e<c$G[p`dZcHd@"
		"l`czi|c$al@i`b:[!2Un`>8TJTJ&J7[&b&e$o`i~aWQ!c(hd2!2!2>[g@e$k]epi|e"
		"0i!bph(d$dbGWhA2!2U2D2.9(['[,@<2W2k`*J*?*!*!k$o!;[a[T2T2c$c~o@>[c6"
		"i$p@Uk>GW}`G[!2!2b$h!al`aWQ!Q!Qp`fVlZf@UWb6>eX:GWk<&J&J7[c&&JTJTb$"
		"G?o`c~i$m`k@U:EW.O(v`T2Tb$a[Fp`M+eZ,M=UWCO-u`Q:RWGO.A(M$U!Ck@a[]!G"
		"8.M(U$[!Ca[i:78&J&Jc$%[g*7?e<g0w$cD#iVAg*$[g~dB]NaaPGft~!f!7[.W(O";

	int i, c = 0, d;
	for (i = 0; i < ARRAYSIZE(words); ++i) {
		c += words[i];
		words[i] = c;
	}
	for (i = 0; (c = *indices++); ) {
		if (c >= 98) {
			for (d = *indices++ - 32; c-- >= 96; ++i) fontdata[i] = fontdata[i-d];
		} else if (c >= 33) {
			fontdata[i++] = words[c - 33];
		}
	}
}

static void fontprocess(int z)
{
	int i, j, k, l, v, p, q, f;
	zoomfont[z] = calloc(16*z*z, 96);
	for (i = l = 0; i < 96; ++i) {
		for (j = 0; j < 16; ++j, l+=2) {
			v = fontdata[l] << 16 | fontdata[l+1];
			for (k = 0; k < 8; ++k, v >>= 4) {
				for (p = 0; p < z; ++p) {
					for (q = 0; q < z; ++q) {
						f = (p+q>=z) | (p>q)<<1 | (p<q)<<2 | (p+q<z-1)<<3;
						if ((v&f) || (v&15)==15) { /* 1 /|, 2 |\, 4 \|, 8 |/, 15 square */
							zoomfont[z][(j*z+p)*z+q][i] |= 1<<(7-k);
						}
					}
				}
			}
		}
	}
}

static void printchar(int x, int y, int z, int c, int u, int v)
{
	if (c == ' ' || c == '\t' || c == '\n' || c == '\r') return;
	c -= (c<0 ? -96 : c<33 || c>126 ? c : 32);
	for (int i = 0; i < 16*z; ++i) {
		Uint32 blended = blend(u, v, i, 16*z-1);
		SDL_SetRenderDrawColor(renderer, blended >> 16, blended >> 8, blended, 255);
		for (int j = 0; j < 8*z; ++j) {
			if (zoomfont[z][i*z+j%z][c] & (1<<(7-j/z))) SDL_RenderDrawPoint(renderer, x+j, y+i);
		}
	}
}

static void printstr(int x, int y, int z, int a, const char *c, int u, int v)
{
	if (a) x -= strlen(c) * a * (4*z);
	for (; *c; x += 8*z) printchar(x, y, z, (Uint8)*c++, u, v);
}

/******************************************************************************/
/* resource loading */

static void update_movie(void *data, SMPEG_Frame *frame)
{
	((struct imgres *)data)->frame = frame;
}

enum bga { BGA_AND_MOVIE, BGA_BUT_NO_MOVIE, NO_BGA };
static void load_resource(enum bga range, void (*resource_loaded)(const char *path))
{
	SDL_RWops *rwops;

	for (int i = 0; i < MAXKEY; ++i) {
		sndres[i].lastch = -1;
		if (sndpath[i]) {
			resource_loaded(sndpath[i]);
			rwops = resolve_relative_path(sndpath[i], SOUND_EXTS);
			if (rwops) sndres[i].res = Mix_LoadWAV_RW(rwops, 1);
			if (!sndres[i].res) {
				warn("failed to load sound #WAV%s (%s)", TO_KEY(i), sndpath[i]);
			}
			free(sndpath[i]);
			sndpath[i] = 0;
		}

		if (imgpath[i]) {
			resource_loaded(imgpath[i]);
			if (strisuffix(imgpath[i], ".mpg")) {
				if (range < BGA_BUT_NO_MOVIE) {
					SMPEG *movie = NULL;
					rwops = resolve_relative_path(imgpath[i], NULL);
					if (rwops) movie = SMPEG_new_rwops(rwops, NULL, 1, 0);
					if (!movie) {
						warn("failed to load image #BMP%s (%s)", TO_KEY(i), imgpath[i]);
					} else {
						imgres[i].tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, 256, 256);
						imgres[i].movie = movie;
						SMPEG_enablevideo(movie, 1);
						SMPEG_loop(movie, 1);
						SMPEG_setdisplay(movie, update_movie, &imgres[i], NULL);
					}
				}
			} else if (range < NO_BGA) {
				SDL_Surface *image = NULL;
				rwops = resolve_relative_path(imgpath[i], IMAGE_EXTS);
				if (rwops) image = IMG_Load_RW(rwops, 1);
				if (image) {
					if (!image->format->Amask) {
						SDL_Surface *temp = SDL_ConvertSurfaceFormat(image, SDL_PIXELFORMAT_RGB888, 0);
						if (temp) {
							SDL_FreeSurface(image);
							image = temp;
							SDL_SetColorKey(image, SDL_TRUE, SDL_MapRGB(image->format, 0, 0, 0));
						}
					}
					if (image) {
						imgres[i].tex = SDL_CreateTextureFromSurface(renderer, image);
						SDL_FreeSurface(image);
					}
				} else {
					warn("failed to load image #BMP%s (%s)", TO_KEY(i), imgpath[i]);
				}
			}
			free(imgpath[i]);
			imgpath[i] = 0;
		}

		if (!bgares[i].image) { /* default blit dimensions without #BGA */
			bgares[i] = (struct bgares) {.image=&imgres[i], .x=0, .y=0, .tx=0, .ty=0, .w=256, .h=256};
		}
	}
}

/******************************************************************************/
/* main routines */

static enum mode { PLAY_MODE, AUTOPLAY_MODE, EXCLUSIVE_MODE } opt_mode = PLAY_MODE;
static enum modf opt_modf = NO_MODF;
static enum bga opt_bga = BGA_AND_MOVIE;
static int opt_showinfo = 1, opt_fullscreen = 1, opt_joystick = -1;

static double playspeed = 1, targetspeed, bpm;
static int now, origintime, starttime, stoptime = 0, poorlimit = 0;
static double startoffset, startshorten;
static int pcur, pfront, pcheck, pthru[NNOTECHANS]; /* indices to objs */
static int bga[] = {[BGA_LAYER]=-1, [BGA2_LAYER]=-1, [BGA3_LAYER]=-1, [POORBGA_LAYER]=0};
static int bgamask = (1<<BGA_LAYER)|(1<<BGA2_LAYER)|(1<<BGA3_LAYER), poormask = (1<<POORBGA_LAYER);
static int score = 0, scocnt[5], scombo = 0, smaxcombo = 0;
static double gradefactor;
static int gradetime = 0, grademode, gauge = 256, survival = 150;

static SDL_Texture *sprite = NULL;
static int keymap[SDL_NUM_SCANCODES]; /* -1: none, 0..NNOTECHANS-1: notes, +0..+1: speed down/up */
static XV(int) joybmap, joyamap;
static int keypressed[2][NNOTECHANS]; /* keypressed[0] for buttons, keypressed[1] for axes */
struct tkeykind { int spriteleft, spritebombleft, width, color; };
static const struct tkeykind *tkey[NNOTECHANS], tkeykinds[] = {{0,0,0,0},
	{25,0,25,0x808080}, {50,0,25,0xf0e080}, {75,0,25,0x8080ff}, {130,100,30,0xe0e0e0},
	{160,100,30,0xffff40}, {190,100,30,0x80ff80}, {220,100,30,0x8080ff},
	{250,100,30,0xff4040}, {320,280,40,0xff8080}, {360,280,40,0x80ff80}};
static int tkeyleft[NNOTECHANS], tpanel1 = 0, tpanel2 = 800, tbgax = 0, tbgay = 0;
static const char *tgradestr[] = {"MISS", "BAD", "GOOD", "GREAT", "COOL"};
static const int tgradecolor[] = {0xff4040, 0xff40ff, 0xffff40, 0x40ff40, 0x4040ff};

static Mix_Chunk *beep;
static SDL_Joystick *joystick;

static char *assign_keymap(int chan, int kind, const char *envname, char *s)
{
	char sep;
	do {
		int i, k = strcspn(s, "%|");
		sep = s[k];
		s[k] = '\0';
		if (chan >= NNOTECHANS || keykind[chan] == kind) {
			SDL_Scancode key;
			for (key = 0; key < SDL_NUM_SCANCODES; ++key) {
				if (strieq(SDL_GetScancodeName(key), s)) break;
			}
			if (key < SDL_NUM_SCANCODES) {
				keymap[key] = chan;
			} else if (sscanf(s, "button %d", &i) >= 1) {
				if (XV_CHECK(joybmap,i)) XV_AT(joybmap,i) = chan;
			} else if (sscanf(s, "axis %d", &i) >= 1) {
				if (XV_CHECK(joyamap,i)) XV_AT(joyamap,i) = chan;
			} else if (s[strspn(s, " \t\r\n")]) { /* skip an empty key name */
				die("Unknown key name in the environment variable %s: %s", envname, s);
			}
		}
		s[k] = sep; /* recover the buffer, assign_keymap should be idempotent */
		s += k;
		if (*s) ++s;
	} while (sep == '%');
	return s;
}

static void read_keymap(void)
{
	static const struct { const char *name, *def, *kinds; int keys[9][3]; } envvars[] = {
		{"ANGOLMOIS_1P_KEYS", "left shift%axis 3|z%button 3|s%button 6|x%button 2|"
		                      "d%button 7|c%button 1|f%button 4|v%axis 2|left alt",
		 "sabababap", {{6},{1},{2},{3},{4},{5},{8},{9},{7}}},
		{"ANGOLMOIS_2P_KEYS", "right alt|m|k|,|l|.|;|/|right shift",
		 "pabababas", {{36+6},{36+1},{36+2},{36+3},{36+4},{36+5},{36+8},{36+9},{36+7}}},
		{"ANGOLMOIS_PMS_KEYS", "z%button 0|s%button 1|x%button 7|d%button 2|c%button 6|"
		                       "f%button 3|v%button 5|g%axis 1|b%button 4",
		 "qwertrewq", {{1},{2},{3},{4},{5},{8,36+2},{9,36+3},{6,36+4},{7,36+5}}},
		{"ANGOLMOIS_SPEED_KEYS", "f3|f4", "**", {{NNOTECHANS},{NNOTECHANS+1}}}};
	char buf[1024] = "", name[32], *s;

	for (int i = 0; i < ARRAYSIZE(keymap); ++i) keymap[i] = -1;
	for (ptrdiff_t i = 0; i < XV_SIZE(joybmap); ++i) XV_AT(joybmap,i) = -1;
	for (ptrdiff_t i = 0; i < XV_SIZE(joyamap); ++i) XV_AT(joyamap,i) = -1;

	for (int i = 0; i < ARRAYSIZE(envvars); ++i) {
		s = getenv(envvars[i].name);
		s = strncpy(buf, s ? s : envvars[i].def, sizeof(buf) - 1);
		for (int j = 0; envvars[i].kinds[j]; ++j) {
			int kind = strchr(KEYKIND_MNEMONICS, envvars[i].kinds[j]) - KEYKIND_MNEMONICS;
			char *prevs = s;
			for (int k = 0; envvars[i].keys[j][k]; ++k) {
				s = assign_keymap(envvars[i].keys[j][k], kind, envvars[i].name, prevs);
			}
		}
	}
	for (int i = 0; i < nleftkeys || i < nrightkeys; ++i) {
		int chan = keyorder[i], kind = keykind[chan];
		sprintf(name, "ANGOLMOIS_%s%c_KEY", TO_KEY(36+chan), KEYKIND_MNEMONICS[kind]);
		s = getenv(name);
		if (s) assign_keymap(chan, kind, name, strncpy(buf, s, sizeof(buf) - 1));
	}
}

static Mix_Chunk *create_beep(void)
{
	Sint32 samples[12000]; /* approx. 0.14 seconds */
	for (int i = 0; i < 12000; ++i) {
		/* sawtooth wave at 3150 Hz, quadratic decay after 0.02 seconds. */
		samples[i] = (i%28-14)*(i<2000 ? 2000 : (12000-i)*(12000-i)/50000);
	}
	return Mix_QuickLoad_RAW((Uint8*)samples, sizeof samples);
}

static void init_video(void)
{
	int width = (opt_mode < EXCLUSIVE_MODE ? 800 : 256);
	int height = (opt_mode < EXCLUSIVE_MODE ? 600 : 256);
	int flags = (opt_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_OPENGL);
	if (SDL_CreateWindowAndRenderer(width, height, flags, &screen, &renderer)) {
		die("SDL Video Initialization Failure: %s", SDL_GetError());
	}
	if (opt_mode < EXCLUSIVE_MODE) SDL_ShowCursor(SDL_DISABLE);
 	SDL_SetWindowTitle(screen, VERSION);
	SDL_RenderSetLogicalSize(renderer, width, height);
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
}

static void init_ui(void)
{
	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_JOYSTICK) < 0) {
		die("SDL Initialization Failure: %s", SDL_GetError());
	}
	atexit(SDL_Quit);
	if (opt_joystick >= 0) {
		SDL_JoystickEventState(SDL_ENABLE);
		joystick = SDL_JoystickOpen(opt_joystick);
		if (!joystick) die("SDL Joystick Initialization Failure: %s", SDL_GetError());
		XV_RESIZE(joyamap, SDL_JoystickNumAxes(joystick));
		XV_RESIZE(joybmap, SDL_JoystickNumButtons(joystick));
	}
	IMG_Init(IMG_INIT_JPG|IMG_INIT_PNG);
	Mix_Init(MIX_INIT_OGG|MIX_INIT_MP3);
	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
		die("SDL Mixer Initialization Failure: %s", Mix_GetError());
	}

	if (opt_mode < EXCLUSIVE_MODE || opt_bga < NO_BGA) init_video();

	fontdecompress();
	fontprocess(1);
	fontprocess(2);
	fontprocess(3);
	read_keymap(); /* depends on SDL_Init and keyorder/keykind */
	beep = create_beep();
}

static void allocate_more_channels(int n)
{
	int i = XV_SIZE(sndlastchmap);
	n = Mix_AllocateChannels(Mix_AllocateChannels(-1) + n);
	XV_RESIZE(sndlastchmap, n);
	while (i < n) XV_AT(sndlastchmap,i++) = -1;
}

static void play_sound(int i, int group)
{
	int ch;
	if (!sndres[i].res) return;
	while ((ch = Mix_PlayChannel(sndres[i].lastch, sndres[i].res, 0)) < 0) {
		allocate_more_channels(32);
	}
	Mix_Volume(ch, group ? 96 : 128);
	Mix_GroupChannel(ch, group);
	if (XV_AT(sndlastchmap,ch) > 0) sndres[XV_AT(sndlastchmap,ch)].lastch = -1;
	sndres[i].lastch = ch;
	XV_AT(sndlastchmap,ch) = i;
}

static const int INFO_INTERVAL = 47; /* try not to refresh screen or console too fast (tops at 21fps) */
static SDL_Texture *stagefile;
static char stagefile_meta[256];
static int lastinfo;

static void check_exit(void)
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT || (event.type == SDL_KEYUP && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE)) {
			if (opt_mode >= EXCLUSIVE_MODE) fprintf(stderr, "\r%72s\r", "");
			exit(0);
		}
	}
}

static void render_stagefile(const char *path)
{
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	printstr(400, 284, 2, 1, "loading bms file...", 0x202020, 0x808080);
	if (stagefile) SDL_RenderCopy(renderer, stagefile, 0, 0);
	if (opt_showinfo) {
		SDL_SetRenderDrawColor(renderer, 0x10, 0x10, 0x10, 191);
		SDL_RenderFillRect(renderer, R(0,0,800,42));
		SDL_RenderFillRect(renderer, R(0,580,800,20));
		printstr(6, 4, 2, 0, string[S_TITLE], 0x808080, 0xffffff);
		printstr(792, 4, 1, 2, string[S_GENRE], 0x808080, 0xffffff);
		printstr(792, 20, 1, 2, string[S_ARTIST], 0x808080, 0xffffff);
		printstr(3, 582, 1, 0, stagefile_meta, 0x808080, 0xffffff);
		printstr(797, 582, 1, 2, path, 0x808080, 0xc0c0c0);
	}
	SDL_RenderPresent(renderer);
}

static void resource_loaded(const char *path)
{
	int now = SDL_GetTicks();
	if (opt_showinfo && now - lastinfo >= INFO_INTERVAL) {
		lastinfo = now;
		if (opt_mode < EXCLUSIVE_MODE) {
			if (!path) path = "loading...";
			render_stagefile(path);
		} else {
			if (path) {
				fprintf(stderr, "\r%72s\rLoading: %.63s", "", path);
			} else {
				fprintf(stderr, "\r%72s\rLoading done.", "");
			}
		}
		check_exit();
	}
}

static void play_show_stagefile(void)
{
	int t;
	char ibuf[256];

	snprintf(ibuf, sizeof ibuf, "%s: %s - %s", VERSION, string[S_ARTIST], string[S_TITLE]);
	SDL_SetWindowTitle(screen, ibuf);

	sprintf(stagefile_meta, "Level %d | BPM %.2f%s | %d note%s [%dKEY%s]",
		value[V_PLAYLEVEL], initbpm, hasbpmchange ? "?" : "", nnotes,
		nnotes == 1 ? "" : "s", nkeys, haslongnote ? "-LN" : "");

	if (opt_mode < EXCLUSIVE_MODE) {
		render_stagefile("");
		if (*string[S_STAGEFILE]) {
			SDL_Surface *temp = IMG_Load_RW(resolve_relative_path(string[S_STAGEFILE], IMAGE_EXTS), 1);
			if (temp) {
				stagefile = SDL_CreateTextureFromSurface(renderer, temp);
				SDL_FreeSurface(temp);
			}
		}
	} else if (opt_showinfo) {
		fprintf(stderr,
				"------------------------------------------------------------------------\n"
				"Title:    %s\nGenre:    %s\nArtist:   %s\n%s\n"
				"------------------------------------------------------------------------\n",
				string[S_TITLE], string[S_GENRE], string[S_ARTIST], stagefile_meta);
	}

	t = SDL_GetTicks() + 3000;
	lastinfo = -1000;
	load_resource(opt_bga, resource_loaded);
	if (opt_showinfo) {
		lastinfo = -1000; /* force update */
		resource_loaded(0);
		SDL_DestroyTexture(stagefile);
	}
	if (opt_mode < EXCLUSIVE_MODE) {
		while ((int)SDL_GetTicks() < t) check_exit();
	}
}

static void play_prepare(void)
{
	SDL_Surface *spritesurf;

	/* configuration */
	bpm = initbpm;
	origintime = starttime = SDL_GetTicks();
	startoffset = originoffset;
	startshorten = shorten(originoffset);
	targetspeed = playspeed;
	allocate_more_channels(64);
	Mix_ReserveChannels(1); /* so that the beep won't be affected */
	gradefactor = 1.5 - (value[V_RANK] < 5 ? value[V_RANK] : 5) * .25;

	if (opt_mode >= EXCLUSIVE_MODE) return;

	/* panel position */
	for (int i = 0; i < nleftkeys; ++i) {
		int chan = keyorder[i];
		tkey[chan] = &tkeykinds[keykind[chan]];
		tkeyleft[chan] = tpanel1;
		tpanel1 += tkeykinds[keykind[chan]].width + 1;
	}
	for (int i = nrightkeys - 1; i >= nleftkeys; --i) {
		int chan = keyorder[i];
		tpanel2 -= tkeykinds[keykind[chan]].width + 1;
		tkey[chan] = &tkeykinds[keykind[chan]];
		tkeyleft[chan] = tpanel2 + 1;
	}
	if (tpanel2 == 800) tpanel2 = 0;
	if (tpanel1 < 165) {
		for (int i = 0; i < nleftkeys; ++i) tkeyleft[keyorder[i]] += (165 - tpanel1) / 2;
		tpanel1 = 165;
	}
	if (tpanel2 > 800 - 165) {
		for (int i = nleftkeys; i < nrightkeys; ++i) tkeyleft[keyorder[i]] -= (tpanel2 - (800 - 165)) / 2;
		tpanel2 = 800 - 165;
	}
	if ((tpanel2 && tpanel2 - tpanel1 < 40) || tpanel1 > 800 - 20) {
		die("The screen can't hold that many lanes");
	}
	tbgax = tpanel1 + ((tpanel2 ? tpanel2 : 800) - tpanel1 - 256) / 2;
	tbgay = (600 - 256) / 2;

	/* sprite */
	spritesurf = SDL_CreateRGBSurface(SDL_SWSURFACE, 1200, 600, 32, 0xff0000, 0xff00, 0xff, 0);
	for (int i = 0; i < ARRAYSIZE(tkeykinds); ++i) {
		const struct tkeykind *k = &tkeykinds[i];
		for (int j = 140; j < 520; ++j) {
			SDL_FillRect(spritesurf, R(k->spriteleft,j,k->width,1), blend(k->color, 0, j-140, 1000));
		}
		for (int j = 0; j*2 < k->width; ++j) {
			SDL_FillRect(spritesurf, R(k->spriteleft+800+j,0,k->width-2*j,600), blend(k->color, 0xffffff, k->width-j, k->width));
			SDL_FillRect(spritesurf, R(k->spritebombleft+800+j,0,k->width-2*j,600), blend(0xc00000, 0, k->width-j, k->width));
		}
	}
	for (int j = -244; j < 556; ++j) {
		for (int i = -10; i < 20; ++i) {
			int c = (i*2+j*3+750) % 2000;
			c = blend(0xc0c0c0, 0x606060, c>1000 ? 1850-c : c-150, 700);
			putpixel(spritesurf, j+244, i+10, c);
		}
		for (int i = -20; i < 60; ++i) {
			int c = (i*3+j*2+750) % 2000;
			c = blend(0xc0c0c0, 0x404040, c>1000 ? 1850-c : c-150, 700);
			putpixel(spritesurf, j+244, i+540, c);
		}
	}
	SDL_FillRect(spritesurf, R(tpanel1+20,0,(tpanel2?tpanel2-20:800)-(tpanel1+20),30), 0);
	SDL_FillRect(spritesurf, R(tpanel1+20,520,(tpanel2?tpanel2-20:800)-(tpanel1+20),80), 0);
	for (int i = 0; i < 20; ++i) {
		for (int j = 20; j*j+i*i > 400; --j) {
			putpixel(spritesurf, tpanel1+j, i+10, 0);
			putpixel(spritesurf, tpanel1+j, 539-i, 0);
			if (tpanel2) {
				putpixel(spritesurf, tpanel2-j-1, i+10, 0);
				putpixel(spritesurf, tpanel2-j-1, 539-i, 0);
			}
		}
	}
	if (!opt_mode) {
		SDL_FillRect(spritesurf, R(0,584,368,16), 0x404040);
		SDL_FillRect(spritesurf, R(4,588,360,8), 0);
	}
	SDL_FillRect(spritesurf, R(10,564,tpanel1,1), 0x404040);
	sprite = SDL_CreateTextureFromSurface(renderer, spritesurf);
	SDL_FreeSurface(spritesurf);
}

static void update_grade(int grade, int scoredelta, int gaugedelta)
{
	++scocnt[grade];
	grademode = grade;
	gradetime = now + 700; /* disappears after 700ms */
	score += scoredelta + scoredelta * scombo / nnotes;

	if (grade < 1) {
		scombo = 0;
		gauge -= gaugedelta ? gaugedelta : 30;
		poorlimit = now + 600; /* switches to the normal BGA after 600ms */
	} else if (grade < 2) {
		scombo = 0;
		gauge -= 15;
	} else if (grade < 3) {
		/* do nothing */
	} else {
		++scombo;
		gauge += (grade<4 ? 2 : 3) + (scombo<100 ? scombo : 100) / 50;
	}

	if (scombo > smaxcombo) smaxcombo = scombo;
}

static int play_process(void)
{
	int i, j, k, l, ibottom, prevpcur;
	double bottom, top, line, tmp;
	SDL_Event event;
	char buf[99];

	if (targetspeed != playspeed) {
		double delta = targetspeed - playspeed;
		if (-0.001 < delta && delta < 0.001) {
			playspeed = targetspeed;
		} else {
			playspeed += delta * 0.1;
		}
	}

	now = SDL_GetTicks();
	if (now < stoptime) {
		bottom = startoffset;
	} else {
		if (stoptime) {
			starttime = stoptime;
			stoptime = 0;
		}
		bottom = startoffset + MSEC_TO_MEASURE(now - starttime, bpm) / startshorten;
	}
	ibottom = (bottom < 0 ? -1 - (int) (-bottom) : (int) bottom);
	if (startshorten != shorten(ibottom)) {
		starttime += (int) (MEASURE_TO_MSEC(ibottom - startoffset, bpm) * startshorten);
		startoffset = ibottom;
		startshorten = shorten(ibottom);
	}
	line = bottom;/*adjust_object_time(bottom, 0.03/playspeed);*/
	top = adjust_object_time(bottom, 1.25/playspeed);

	for (; pfront < nobjs && objs[pfront].time < bottom; ++pfront);
	for (prevpcur = pcur; pcur < nobjs && objs[pcur].time < line; ++pcur) {
		int chan = objs[pcur].chan, type = objs[pcur].type, index = objs[pcur].index;
		if (chan == BGM_CHANNEL) {
			if (index) play_sound(index, 1);
		} else if (chan == BGA_CHANNEL) {
			if (bga[type] >= 0 && imgres[bga[type]].movie) {
				SMPEG_stop(imgres[bga[type]].movie);
			}
			bga[type] = index;
			if (index >= 0 && imgres[index].movie) {
				SMPEG_rewind(imgres[index].movie);
				SMPEG_play(imgres[index].movie);
			}
		} else if (chan == BPM_CHANNEL) {
			double newbpm = (type == BPM_BY_INDEX ? bpmtab[index] : index);
			if (newbpm) {
				starttime += (int) (MEASURE_TO_MSEC(objs[pcur].time - startoffset, bpm) * startshorten);
				startoffset = objs[pcur].time;
				bpm = newbpm;
			}
		} else if (chan == STOP_CHANNEL) {
			if (now >= stoptime) stoptime = now;
			stoptime += (type == STOP_BY_MSEC ? index : (int) MEASURE_TO_MSEC(stoptab[index], bpm));
			startoffset = objs[pcur].time;
		} else if (opt_mode && (type == NOTE || type == LNSTART)) {
			if (index) play_sound(index, 0);
			update_grade(4, 300, 0);
		}
	}
	if (!opt_mode) {
		for (; pcheck < pcur; ++pcheck) {
			int chan = objs[pcheck].chan, type = objs[pcheck].type;
			tmp = objs[pcheck].time;
			tmp = MEASURE_TO_MSEC(line - tmp, bpm) * shorten(tmp) * gradefactor;
			if (tmp < 144) break;
			if (!IS_NOTE_CHANNEL(chan) || objs[pcheck].nograding) continue;
			if (type >= INVNOTE || (type == LNDONE && !pthru[chan])) continue;
			update_grade(0, 0, 0);
			pthru[chan] = 0;
		}
	}

	while (SDL_PollEvent(&event)) {
		int down = 0, joy = 0, key = -1;
		switch (event.type) {
		case SDL_QUIT:
			return 0;
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) return 0;
			down = (event.type == SDL_KEYDOWN);
			if (!event.key.repeat) key = keymap[event.key.keysym.scancode];
			break;
		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP:
			down = (event.type == SDL_JOYBUTTONDOWN);
			key = XV_AT(joybmap, event.jbutton.button);
			break;
		case SDL_JOYAXISMOTION:
			down = (event.jaxis.value < -3200 ? -1 : event.jaxis.value > 3200 ? 1 : 0);
			joy = 1;
			key = XV_AT(joyamap, event.jaxis.axis);
			break;
		}

		if (opt_mode >= EXCLUSIVE_MODE) continue;

		static const double speeds[] = {0.1, 0.2, 0.4, 0.6, 0.8, 1, 1.2, 1.5, 2, 2.5,
			3, 3.5, 4, 4.5, 5, 5.5, 6, 7, 8, 10, 15, 25, 40, 60, 99};
		if (down && key == NNOTECHANS/*SPEED_DOWN*/) {
			for (k = 0; k < ARRAYSIZE(speeds) && speeds[k] < targetspeed - 0.001; ++k);
			if (k > 0) {
				targetspeed = speeds[k-1];
				Mix_PlayChannel(0, beep, 0);
			}
		}
		if (down && key == NNOTECHANS+1/*SPEED_UP*/) {
			for (k = ARRAYSIZE(speeds)-1; k >= 0 && speeds[k] > targetspeed + 0.001; --k);
			if (k < ARRAYSIZE(speeds)-1) {
				targetspeed = speeds[k+1];
				Mix_PlayChannel(0, beep, 0);
			}
		}

		if (opt_mode || key < 0 || key >= NNOTECHANS || !tkey[key]) continue;

		if (!down || (joy && down != keypressed[joy][key])) { /* up, or down with the reverse direction */
			int unpressed = 1;
			if (joy) {
				keypressed[joy][key] = down;
			} else {
				if (keypressed[joy][key] && --keypressed[joy][key]) unpressed = 0;
			}
			if (unpressed && pthru[key]) {
				for (j = pthru[key]; !(objs[j].chan == key && objs[j].type == LNDONE); ++j);
				pthru[key] = 0;
				tmp = MEASURE_TO_MSEC(objs[j].time - line, bpm) * shorten(line) * gradefactor;
				if (-144 < tmp && tmp < 144) {
					objs[j].nograding = 1;
				} else {
					update_grade(0, 0, 0);
				}
			}
		}

		if (down) { /* down */
			int pressed = 1;
			if (joy) {
				keypressed[joy][key] = down;
			} else {
				if (keypressed[joy][key]++) pressed = 0;
			}
			if (pressed && nobjs) {
				for (j = pcur; j < nobjs && !(objs[j].chan == key && objs[j].type <= INVNOTE); ++j);
				for (l = pcur - 1; l >= 0 && !(objs[l].chan == key && objs[l].type <= INVNOTE); --l);
				tmp = (l >= 0 ? line - objs[l].time : DBL_MAX);
				if (j < nobjs && tmp > objs[j].time - line) l = j;
				if (l >= 0 && objs[l].index) play_sound(objs[l].index, 0);

				for (j = pcur; j < nobjs && !(objs[j].chan == key && objs[j].type < INVNOTE); ++j);
				for (l = pcur - 1; l >= 0 && !(objs[l].chan == key && objs[l].type < INVNOTE); --l);
				tmp = (l >= 0 ? line - objs[l].time : DBL_MAX);
				if (j < nobjs && tmp > objs[j].time - line) l = j;

				if (l >= pcheck && !objs[l].nograding && objs[l].type != LNDONE) {
					tmp = MEASURE_TO_MSEC(objs[l].time - line, bpm) * shorten(line) * gradefactor;
					if (tmp < 0) tmp = -tmp;
					if (tmp < 144) {
						if (objs[l].type == LNSTART) pthru[key] = l + 1;
						objs[l].nograding = 1;
						update_grade((tmp<14.4) + (tmp<48) + (tmp<84) + 1, (1-tmp/144)*300, 0);
					}
				}
			}
		}
	}
	if (!opt_mode) {
		for (int i = prevpcur; i < pcur; ++i) if (IS_NOTE_CHANNEL(objs[i].chan)) {
			int chan = objs[i].chan, type = objs[i].type;
			if (type != BOMB || !(keypressed[0][chan] || keypressed[1][chan])) continue;
			pthru[chan] = 0; /* ongoing long note is not graded twice */
			play_sound(objs[i].index, 0);
			if (objs[i].value < 0) { /* instant death */
				gauge = 0;
				pcur = nobjs;
				return 0;
			}
			update_grade(0, 0, objs[i].value);
		}
	}
	if (bottom > length) {
		if (opt_mode ? Mix_Playing(-1)==Mix_Playing(0) : Mix_GroupNewer(1)==-1) return 0;
	} else if (bottom < originoffset) {
		return 0;
	}

	if (opt_mode < EXCLUSIVE_MODE) {
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);
		SDL_SetRenderDrawColor(renderer, 0x40, 0x40, 0x40, 255);
		SDL_RenderFillRect(renderer, R(0,30,tpanel1,490));
		if (tpanel2) SDL_RenderFillRect(renderer, R(tpanel2,30,800-tpanel2,490));
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		for (int i = 0; i < ARRAYSIZE(tkey); ++i) if (tkey[i]) {
			SDL_RenderFillRect(renderer, R(tkeyleft[i],30,tkey[i]->width,490));
			if (keypressed[0][i] || keypressed[1][i]) {
				SDL_RenderCopy(renderer, sprite, R(tkey[i]->spriteleft,140,tkey[i]->width,380), R(tkeyleft[i],140,tkey[i]->width,380));
			}
		}
		SDL_RenderSetClipRect(renderer, R(0,30,800,490));
		for (int i = 0; i < ARRAYSIZE(tkey); ++i) if (tkey[i]) {
			for (j = pfront; j < nobjs && !(objs[j].chan == i && objs[j].type != INVNOTE); ++j);
			if (j < nobjs && objs[j].time > top && objs[j].type == LNDONE) {
				SDL_RenderCopy(renderer, sprite, R(tkey[i]->spriteleft+800,0,tkey[i]->width,490), R(tkeyleft[i],30,tkey[i]->width,490));
			}
			for (; j < nobjs && objs[j].time <= top; ++j) if (objs[j].chan == i) {
				k = (int)(525 - 400 * playspeed * adjust_object_position(bottom, objs[j].time));
				if (objs[j].type == LNSTART) {
					l = k + 5;
					/* the additional check can be omitted as the sanitization ensures LNDONE always exists */
					while (!(objs[j].chan == i && objs[j].type == LNDONE)) ++j;
					k = (int)(530 - 400 * playspeed * adjust_object_position(bottom, objs[j].time));
					if (k < 30) k = 30;
				} else if (objs[j].type == LNDONE) {
					k += 5;
					l = 520;
				} else if (objs[j].type == NOTE || objs[j].type == BOMB) {
					l = k + 5;
				} else {
					continue;
				}
				if (k > 0 && l > k) {
					int sleft = (objs[j].type == BOMB ? tkey[i]->spritebombleft : tkey[i]->spriteleft);
					SDL_RenderCopy(renderer, sprite, R(sleft+800,0,tkey[i]->width,l-k), R(tkeyleft[i],k,tkey[i]->width,l-k));
				}
			}
		}
		SDL_SetRenderDrawColor(renderer, 0xc0, 0xc0, 0xc0, 255);
		for (i = ibottom; i < top; ++i) {
			j = (int)(530 - 400 * playspeed * adjust_object_position(bottom, i));
			SDL_RenderFillRect(renderer, R(0,j,tpanel1,1));
			if (tpanel2) SDL_RenderFillRect(renderer, R(tpanel2,j,800-tpanel2,1));
		}
		if (now < gradetime) {
			int delta = (gradetime - now - 400) / 15;
			if (delta < 0) delta = 0;
			j = tgradecolor[grademode];
			printstr(tpanel1/2, 260 - delta, 2, 1, tgradestr[grademode], j, j|((j<<1)&0xfefefe));
			if (scombo > 1) {
				sprintf(buf, "%d COMBO", scombo);
				printstr(tpanel1/2, 288 - delta, 1, 1, buf, 0x808080, 0xffffff);
			}
			if (opt_mode) printstr(tpanel1/2, 302 - delta, 1, 1, "(AUTO)", 0x404040, 0xc0c0c0);
		}
		SDL_RenderSetClipRect(renderer, 0);

		i = (now - origintime) / 1000;
		j = duration / 1000;
		sprintf(buf, "SCORE %07d%c%4.1fx%c%02d:%02d / %02d:%02d%c@%9.4f%cBPM %6.2f",
				score, 0, targetspeed, 0, i/60, i%60, j/60, j%60, 0, bottom, 0, bpm);
		SDL_RenderCopy(renderer, sprite, R(0,0,800,30), R(0,0,800,30));
		SDL_RenderCopy(renderer, sprite, R(0,520,800,80), R(0,520,800,80));
		printstr(10, 8, 1, 0, buf, 0, 0);
		printstr(5, 522, 2, 0, buf+14, 0, 0);
		printstr(tpanel1-94, 565, 1, 0, buf+20, 0, 0x404040);
		printstr(95, 538, 1, 0, buf+34, 0, 0);
		printstr(95, 522, 1, 0, buf+45, 0, 0);
		i = (now - origintime) * tpanel1 / duration;
		printchar(6+(i<tpanel1?i:tpanel1), 548, 1, -1, 0x404040, 0x404040);
		if (!opt_mode) {
			if (gauge > 512) gauge = 512;
			k = (int)(160*startshorten*(1+bottom)) % 40; /* i.e. cycles four times per measure */
			i = (gauge<0 ? 0 : (gauge*400>>9) - k);
			SDL_SetRenderDrawColor(renderer, (gauge>=survival ? 0xc0 : 0xc0 - k*4), 0, 0, 255);
			SDL_RenderFillRect(renderer, R(4,588,i>360?360:i<5?5:i,8));
		}
	} else if (now - lastinfo >= INFO_INTERVAL) {
		lastinfo = now;
		i = (now - origintime) / 100;
		j = duration / 100;
		fprintf(stderr,
			"\r%72s\r%02d:%02d.%d / %02d:%02d.%d (@%9.4f) | BPM %6.2f | %d / %d notes",
			"", i/600, i/10%60, i%10, j/600, j/10%60, j%10, bottom, bpm, scombo, nnotes);
	}

	if (opt_bga != NO_BGA) {
		int mask = (now < poorlimit ? poormask : bgamask);
		for (j = 0; j < ARRAYSIZE(bga); ++j) {
			if ((mask>>j&1) && bga[j] >= 0 && bgares[bga[j]].image->tex) {
				struct bgares *res = &bgares[bga[j]];
				if (res->image->frame) {
					SDL_UpdateTexture(res->image->tex, NULL, res->image->frame->image, res->image->frame->image_width);
					res->image->frame = NULL;
				}
				SDL_RenderCopy(renderer, res->image->tex, R(res->x,res->y,res->w,res->h), R(tbgax+res->tx,tbgay+res->ty,res->w,res->h));
			}
		}
	}

	if (renderer) SDL_RenderPresent(renderer);
	return 1;
}

/******************************************************************************/
/* entry point */

static int play(void)
{
	struct rngstate r;

	rng_seed(&r, (uint32_t) time(0));
	parse_bms(&r);
	sanitize_bms();

	if (!leftkeys && !rightkeys) {
		if (!preset && strisuffix(bmspath, ".pms")) preset = "pms";
		preset = detect_preset(preset);
		for (int i = 0; i < ARRAYSIZE(presets); ++i) {
			if (strieq(preset, presets[i].name1)) {
				leftkeys = presets[i].left;
				break;
			}
			if (presets[i].name2 && strieq(preset, presets[i].name2)) {
				leftkeys = presets[i].left;
				rightkeys = presets[i].right;
				break;
			}
		}
		if (!leftkeys) die("Invalid preset name: %s", preset);
	}

	analyze_and_compact_bms(leftkeys, rightkeys);
	if (opt_modf) {
		shuffle_bms(opt_modf, &r, 0, nleftkeys);
		if (nleftkeys < nrightkeys) shuffle_bms(opt_modf, &r, nleftkeys, nrightkeys);
	}

	/* get basename of bmspath, or use #PATH_WAV if any */
	if (*string[S_BASEPATH]) {
		bmspath = string[S_BASEPATH];
		for (int i = 0; bmspath[i]; ++i) {
			if (bmspath[i] == '\\') bmspath[i] = '/';
		}
	} else {
		char *pos1 = strrchr(bmspath, '/');
		char *pos2 = strrchr(bmspath, '\\');
		if (!pos1) pos1 = bmspath;
		if (!pos2) pos2 = bmspath;
		*(pos1>pos2 ? pos1 : pos2) = '\0';
	}

	init_ui();
	play_show_stagefile();

	duration = get_bms_duration();
	play_prepare();
	lastinfo = -1000;
	while (play_process());
	for (int i = 0; i < ARRAYSIZE(imgres); ++i) {
		if (imgres[i].movie) SMPEG_delete(imgres[i].movie);
	}

	for (; pcur < nobjs; ++pcur) {
		if (IS_NOTE_CHANNEL(objs[pcur].chan) && objs[pcur].type < INVNOTE) break;
	}
	if (!opt_mode && pcur == nobjs) {
		if (gauge >= survival) {
			printf("*** CLEARED! ***\n");
			for (int i = 4; i >= 0; --i) {
				printf("%-5s %4d    %s", tgradestr[i], scocnt[i], i == 2 ? "\n" : "");
			}
			printf("MAX COMBO %d\nSCORE %07d (max %07d)\n", smaxcombo, score, maxscore);
		} else {
			printf("YOU FAILED!\n");
		}
	} else if (opt_mode >= EXCLUSIVE_MODE) {
		fprintf(stderr, "\r%72s\r", "");
	}
	return 0;
}

int usage(void)
{
	fprintf(stderr,
		"%s -- the simple BMS player\n"
		"http://mearie.org/projects/angolmois/\n\n"
		"Usage: %s <options> <path>\n"
		"  Accepts any BMS, BME, BML or PMS file.\n"
		"  Resources should be in the same directory as the BMS file.\n\n"
		"Options:\n"
		"  -h, --help              This help\n"
		"  -V, --version           Shows the version\n"
		"  -a #.#, --speed #.#     Sets the initial play speed (default: 1.0x)\n"
		"  -#                      Same as '-a #.0'\n"
		"  -v, --autoplay          Enables AUTO PLAY (viewer) mode\n"
		"  -x, --exclusive         Enables exclusive (BGA and sound only) mode\n"
		"  -X, --sound-only        Enables sound only mode, equivalent to -xB\n"
		"  --fullscreen            Enables the fullscreen mode (default)\n"
		"  -w, --no-fullscreen     Disables the fullscreen mode\n"
		"  --info                  Shows a brief information about the song (default)\n"
		"  -q, --no-info           Do not show an information about the song\n"
		"  -m, --mirror            Uses a mirror modifier\n"
		"  -s, --shuffle           Uses a shuffle modifier\n"
		"  -S, --shuffle-ex        Uses a shuffle modifier, even for scratches\n"
		"  -r, --random            Uses a random modifier\n"
		"  -R, --random-ex         Uses a random modifier, even for scratches\n"
		"  -k NAME, --preset NAME  Forces a use of given key preset (default: bms)\n"
		"  -K LEFT RIGHT, --key-spec LEFT RIGHT\n"
		"                          Sets a custom key specification (see the manual)\n"
		"  --bga                   Loads and shows the BGA (default)\n"
		"  -B, --no-bga            Do not load and show the BGA\n"
		"  -M, --no-movie          Do not load and show the BGA movie\n"
		"  -j #, --joystick #      Enable the joystick with index # (normally 0)\n\n"
		"Environment Variables:\n"
		"  ANGOLMOIS_1P_KEYS=<scratch>|<key 1>|<2>|<3>|<4>|<5>|<6>|<7>|<pedal>\n"
		"  ANGOLMOIS_2P_KEYS=<pedal>|<key 1>|<2>|<3>|<4>|<5>|<6>|<7>|<scratch>\n"
		"  ANGOLMOIS_PMS_KEYS=<key 1>|<2>|<3>|<4>|<5>|<6>|<7>|<8>|<9>\n"
		"  ANGOLMOIS_SPEED_KEYS=<speed down>|<speed up>\n"
		"  ANGOLMOIS_XXy_KEY=<keys for channel XX and channel kind y>\n"
		"    Sets keys used for game play. Use either SDL key names or joystick names\n"
		"    like 'button #' or 'axis #' can be used. Separate multiple keys by '%%'.\n"
		"    See the manual for more information.\n\n",
		VERSION, argv0);
	return 1;
}

int main(int argc, char **argv)
{
	static const char *longargs[] =
		{"h--help", "V--version", "a--speed", "v--autoplay", "x--exclusive",
		 "X--sound-only", "w--windowed", "w--no-fullscreen", " --fullscreen",
		 " --info", "q--no-info", "m--mirror", "s--shuffle", "S--shuffle-ex",
		 "r--random", "R--random-ex", "k--preset", "K--key-spec", " --bga",
		 "B--no-bga", " --movie", "M--no-movie", "j--joystick", NULL};
	char buf[512] = "", *arg = NULL;
	int i, j;

#define FETCH_ARG(arg, opt) \
		if (argv[i] && ((arg) = j > 0 && argv[i][++j] ? argv[i]+j : argv[++i])) j = 0; \
		else die("No argument to the option -%c", (opt)) \

	argv0 = argv[0];
	for (i = 1; i < argc; ++i) {
		if (argv[i][0] != '-') {
			if (!bmspath) bmspath = argv[i];
		} else if (!strcmp(argv[i], "--")) {
			if (!bmspath) bmspath = argv[++i];
			break;
		} else {
			if (argv[i][1] == '-') {
				for (j = 0; longargs[j]; ++j) {
					if (!strcmp(argv[i], longargs[j]+1)) {
						argv[i][1] = longargs[j][0];
						argv[i][2] = '\0';
						break;
					}
				}
				if (!longargs[j]) die("Invalid option: %s", argv[i]);
			}
			for (j = 1; argv[i][j]; ++j) {
				switch (argv[i][j]) {
				case 'h': return usage();
				case 'V': printf("%s\n", VERSION); return 0;
				case 'v': opt_mode = AUTOPLAY_MODE; break;
				case 'x': opt_mode = EXCLUSIVE_MODE; break;
				case 'X': opt_mode = EXCLUSIVE_MODE; opt_bga = NO_BGA; break;
				case 'w': opt_fullscreen = 0; break;
				case 'q': opt_showinfo = 0; break;
				case 'm': opt_modf = MIRROR_MODF; break;
				case 's': opt_modf = SHUFFLE_MODF; break;
				case 'S': opt_modf = SHUFFLEEX_MODF; break;
				case 'r': opt_modf = RANDOM_MODF; break;
				case 'R': opt_modf = RANDOMEX_MODF; break;
				case 'k': FETCH_ARG(preset, 'k'); goto endofarg;
				case 'K': FETCH_ARG(leftkeys, 'K'); FETCH_ARG(rightkeys, 'K'); goto endofarg;
				case 'a':
					FETCH_ARG(arg, 'a');
					playspeed = atof(arg);
					if (playspeed <= 0) playspeed = 1;
					if (playspeed < .1) playspeed = .1;
					if (playspeed > 99) playspeed = 99;
					goto endofarg;
				case 'B': opt_bga = NO_BGA; break;
				case 'M': opt_bga = BGA_BUT_NO_MOVIE; break;
				case 'j': FETCH_ARG(arg, 'j'); opt_joystick = atoi(arg); goto endofarg;
				case ' ': break; /* for ignored long options */
				default:
					if (argv[i][j] >= '1' && argv[i][j] <= '9') {
						playspeed = argv[i][j] - '0';
					} else {
						die("Invalid option: -%c", argv[i][j]);
					}
				}
			}
		endofarg:;
		}
	}

	if (!bmspath && filedialog(buf)) bmspath = buf;
	return bmspath ? play() : usage();
}

/* vim: set ts=4 sw=4: */
