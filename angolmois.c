/*
 * Angolmois -- the simple BMS player
 * Copyright (c) 2005, 2007, 2009, 2012, Kang Seonghoon.
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
#include <time.h>
#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_image.h>
#include <smpeg.h>

static const char VERSION[] = "Angolmois 2.0.0 alpha 0";

/******************************************************************************/
/* constants, variables */

#define ARRAYSIZE(x) (sizeof(x)/sizeof(*x))
#define SWAP(x,y,t) {(t)=(x);(x)=(y);(y)=(t);}

static const char *bmsheader[] = {
	"title", "genre", "artist", "stagefile", "bpm", "player", "playlevel",
	"rank", "lntype", "lnobj", "wav", "bmp", "bga", "stop", "stp", "random",
	"if", "else", "endif", 0};
static char *bmspath, respath[512];
static char **bmsline = NULL;
static int nbmsline = 0;

static char metadata[4][1024];
static double bpm = 130;
static int value[5] = {1, 0, 2, 1, 0};
#define v_player value[0]
#define v_playlevel value[1]
#define v_rank value[2]
#define lntype value[3]
#define lnobj value[4]

static char *paths[2][1296];
#define sndpath paths[0]
#define imgpath paths[1]
static int (*blitcmd)[8] = NULL, nblitcmd = 0;
static Mix_Chunk *sndres[1296];
static SDL_Surface *imgres[1296];
static int stoptab[1296] = {0};
static double bpmtab[1296] = {0};
static SMPEG *mpeg = NULL;
int imgmpeg = -2;

typedef struct {
	double time; /* time */
	int type; /* for notes: start(0) and end(1) of longnote, visible(2), invisible(3)
			   * for BGA: lower layer(0), upper layer(1), poor BGA(2)
			   * for BPM: direct(0), indirect(1)
			   * for STOP: unit=1/192 measure, indirect(0), unit=msec, direct(1)
			   * otherwise: always 0. note that removed one has type -1. */
	int index; /* associated resource or key value (if any) */
} bmsnote;

static bmsnote *channel[22]; /* 0..17: notes, 18: BGM; 19: BGA; 20: BPM; 21: STOP */
static double _shorten[2005], *shorten = _shorten + 1;
static int nchannel[22] = {0};
static double length;

/******************************************************************************/
/* system dependent functions */

#ifdef WIN32

#include <windows.h>

static const char sep = '\\';

static int filedialog(char *buf)
{
	OPENFILENAME ofn = {
		.lStructSize = sizeof ofn,
		.lpstrFilter =
			"All Be-Music Source File (*.bms;*.bme;*.bml)\0*.bms;*.bme;*.bml\0"
			"Be-Music Source File (*.bms)\0*.bms\0"
			"Extended Be-Music Source File (*.bme)\0*.bme\0"
			"Longnote Be-Music Source File (*.bml)\0*.bml\0"
			"All Files (*.*)\0*.*\0",
		.lpstrFile = buf,
		.nMaxFile = 512,
		.lpstrTitle = "Choose a file to play",
		.Flags = OFN_HIDEREADONLY};
	return GetOpenFileName(&ofn);
}

static int errormsg(const char *c, const char *s)
{
	char b[512];
	sprintf(b, c, s);
	return MessageBox(0, b, VERSION, 0);
}

static void dirinit(void) {}
static void dirfinal(void) {}
static const char *adjust_path_case(char *file) { return file; }

#else /* WIN32 */

#include <dirent.h>

static const char sep = '/';
static char *flist[2592];
static int nfiles = 0;

static int filedialog(char *buf)
{
	return 0;
}

static int errormsg(const char *c, const char *s)
{
	return fprintf(stderr, c, s);
}

static int stricmp(const char *a, const char *b); /* DUMMY */
static char *strcopy(const char*); /* DUMMY */

static void dirinit(void)
{
	DIR *d;
	struct dirent *e;

	if (d = opendir(bmspath)) {
		while (e = readdir(d))
			flist[nfiles++] = strcopy(e->d_name);
		closedir(d);
	}
}

static void dirfinal(void)
{
	while (nfiles--) free(flist[nfiles]);
}

static const char *adjust_path_case(char *file)
{
	int i;
	for (i = 0; i < nfiles; ++i) {
		if (stricmp(flist[i], file)) return flist[i];
	}
	return ""; /* always nonexistent file */
}
#endif

/******************************************************************************/
/* general functions */

static int lcase(char c)
{
	return ((c|32)-19)/26-3 ? c : c|32;
}

static int stricmp(const char *a, const char *b)
{
	while (*a && *b && lcase(*a) == lcase(*b)) ++a, ++b;
	return *a == *b;
}

static char *adjust_path(char *path)
{
	strcpy(respath, bmspath);
	strcat(respath, adjust_path_case(path)); /* XXX could be overflow */
	return respath;
}

static char *adjust_path_with_ext(char *path, char *ext)
{
	int len = strlen(bmspath);
	char *oldext;
	strcpy(respath, bmspath);
	strcpy(respath + len, path);
	oldext = strrchr(respath + len, '.');
	if (oldext) {
		strcpy(oldext, ext);
		strcpy(respath + len, adjust_path_case(respath + len));
	}
	return respath;
}

static char *strcopy(const char *src)
{
	char *dest = malloc(strlen(src)+1);
	return strcpy(dest, src);
}

/******************************************************************************/
/* bms parser */

#define GET_CHANNEL(player, chan) ((player)*9+(chan)-1)
#define ADD_NOTE(player, chan, time, index) \
	add_note(GET_CHANNEL(player,chan), (time), 2/*NOTE*/, (index))
#define ADD_INVNOTE(player, chan, time, index) \
	add_note(GET_CHANNEL(player,chan), (time), 3/*INVNOTE*/, (index))
#define ADD_LNDONE(player, chan, time, index) \
	add_note(GET_CHANNEL(player,chan), (time), 0/*LNDONE*/, (index))
#define ADD_LNSTART(player, chan, time, index) \
	add_note(GET_CHANNEL(player,chan), (time), 1/*LNSTART*/, (index))
#define ADD_BGM(time, index) \
	add_note(18, (time), 0, (index))
#define ADD_BGA(time, index) \
	add_note(19, (time), 0, (index))
#define ADD_BGA2(time, index) \
	add_note(19, (time), 1, (index))
#define ADD_POORBGA(time, index) \
	add_note(19, (time), 2, (index))
#define ADD_BPM(time, index) \
	add_note(20, (time), 0, (index))
#define ADD_BPM2(time, index) \
	add_note(20, (time), 1, (index))
#define ADD_STOP(time, index) \
	add_note(21, (time), 0, (index))
#define ADD_STP(time, index) \
	add_note(21, (time), 1, (index))

static int getdigit(int n)
{
	return 47<n && n<58 ? n-48 : ((n|32)-19)/26==3 ? (n|32)-87 : -1296;
}

static int key2index(const char *a)
{
	return getdigit(*a) * 36 + getdigit(a[1]);
}

static int compare_bmsline(const void *a, const void *b)
{
	int i, j;
	for (i = 0; i < 6; ++i) {
		if (j = (*(char**)a)[i] - (*(char**)b)[i]) return j;
	}
	return 0;
}

static int compare_bmsnote(const void *a, const void *b)
{
	const bmsnote *A = a, *B = b;
	return (A->time > B->time ? 1 : A->time < B->time ? -1 : A->type - B->type);
}

static void add_note(int chan, double time, int type, int index)
{
	bmsnote temp = {time, type, index};
	channel[chan] = realloc(channel[chan], sizeof(bmsnote) * (nchannel[chan]+1));
	channel[chan][nchannel[chan]++] = temp;
}

static void remove_note(int chan, int index)
{
	if (chan < 18 && channel[chan][index].index) {
		ADD_BGM(channel[chan][index].time, channel[chan][index].index);
	}
	channel[chan][index].type = -1;
}

#define KEY_PATTERN "%2[0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz]"

static int parse_bms(void)
{
	FILE *fp;
	int i, j, k;
	int rnd = 1, ignore = 0;
	double t;
	char linebuf[4096], buf1[4096], buf2[4096];
	char *line = linebuf;

	fp = fopen(bmspath, "r");
	if (!fp) return 1;

	srand(time(0));
	while (fgets(line = linebuf, sizeof linebuf, fp)) {
		if (line[0] != '#') continue;
		++line;

		for (i = 0; bmsheader[i]; ++i) {
			for (j = 0; bmsheader[i][j]; ++j)
				if (((bmsheader[i][j] ^ line[j]) | 32) != 32) break;
			if (!bmsheader[i][j]) break;
		}

		switch (i) {
		case 15: /* random */
			if (sscanf(line+j, "%*[ ]%d", &j) >= 1) {
				rnd = rand() % abs(j) + 1;
			}
			break;

		case 16: /* if */
			if (sscanf(line+j, "%*[ ]%d", &j) >= 1) {
				ignore = (rnd != j);
			}
			break;

		case 17: /* else */
			ignore = !ignore;
			break;

		case 18: /* endif */
			ignore = 0;
			break;
		}

		if (!ignore) {
			switch (i) {
			case 0: /* title */
			case 1: /* genre */
			case 2: /* artist */
			case 3: /* stagefile */
				sscanf(line+j, "%*[ ]%[^\r\n]", metadata[i]);
				break;

			case 4: /* bpm */
				if (sscanf(line+j, "%*[ ]%lf", &bpm) >= 1) {
					/* do nothing, bpm is set */
				} else if (sscanf(line+j, KEY_PATTERN "%*[ ]%lf", buf1, &t) >= 2) {
					i = key2index(buf1);
					if (i >= 0) bpmtab[i] = t;
				}
				break;

			case 5: /* player */
			case 6: /* playlevel */
			case 7: /* rank */
			case 8: /* lntype */
				sscanf(line+j, "%*[ ]%d", &value[i-5]);
				break;

			case 9: /* lnobj */
				if (sscanf(line+j, "%*[ ]" KEY_PATTERN, buf1) >= 1) {
					lnobj = key2index(buf1);
				}
				break;

			case 10: /* wav## */
			case 11: /* bmp## */
				if (sscanf(line+j, KEY_PATTERN "%*[ ]%[^\r\n]", buf1, buf2) >= 2) {
					j = key2index(buf1);
					if (j >= 0) {
						free(paths[i-10][j]);
						paths[i-10][j] = strcopy(buf2);
					}
				}
				break;

			case 12: /* bga## */
				i = nblitcmd;
				blitcmd = realloc(blitcmd, sizeof(int) * 8 * (i+1));
				if (sscanf(line+j, KEY_PATTERN "%*[ ]" KEY_PATTERN "%*[ ]%d %d %d %d %d %d",
							buf1, buf2, blitcmd[i]+2, blitcmd[i]+3, blitcmd[i]+4,
							blitcmd[i]+5, blitcmd[i]+6, blitcmd[i]+7) >= 8) {
					blitcmd[i][0] = key2index(buf1);
					blitcmd[i][1] = key2index(buf2);
					if (blitcmd[i][0] >= 0 && blitcmd[i][1] >= 0) ++nblitcmd;
				}
				break;

			case 13: /* stop## */
				if (sscanf(line+j, KEY_PATTERN "%*[ ]%d", buf1, &j) >= 2) {
					i = key2index(buf1);
					if (i >= 0) stoptab[i] = j;
				}
				break;

			case 14: /* stp## */
				if (sscanf(line+j, "%d.%d %d", &i, &j, &k) >= 3) {
					ADD_STP(i+j/1e3, k);
				}
				break;

			case 19: /* #####:... */
				/* only check validity, do not store them yet */
				if (sscanf(line+j, "%*5c:%c", buf1) >= 1) {
					bmsline = realloc(bmsline, sizeof(char*) * (nbmsline+1));
					bmsline[nbmsline] = strcopy(line);
					++nbmsline;
				}
			}
		}
	}
	fclose(fp);

	return 0;
}

static int parse_bmsline(void)
{
	int prev[40] = {0};
	int i, j, k, a, b, c;
	int measure, chan;
	double t;

	qsort(bmsline, nbmsline, sizeof(char*), compare_bmsline);

	for (i = 0; i < nbmsline; ++i) {
		j = atoi(bmsline[i]);
		measure = j / 100;
		chan = j % 100;
		if (chan == 2) {
			shorten[measure] = atof(bmsline[i]+6);
		} else {
			j = 6 + strspn(bmsline[i]+6, " \t\r\n");
			a = strcspn(bmsline[i]+j, " \t\r\n") / 2;
			for (k = 0; k < a; ++k, j+=2) {
				b = key2index(bmsline[i]+j);
				t = measure + 1. * k / a;
				if (b) {
					if (chan == 1) {
						ADD_BGM(t, b);
					} else if (chan == 3 && (b/36<16 && b%36<16)) {
						ADD_BPM(t, b/36*16+b%36);
					} else if (chan == 4) {
						ADD_BGA(t, b);
					} else if (chan == 6) {
						ADD_POORBGA(t, b);
					} else if (chan == 7) {
						ADD_BGA2(t, b);
					} else if (chan == 8) {
						ADD_BPM2(t, b);
					} else if (chan == 9) {
						ADD_STOP(t, b);
					} else if (chan % 10 != 0 && chan > 9 && chan < 30) {
						if (lnobj && b == lnobj) {
							c = GET_CHANNEL(chan>20, chan%10);
							if (nchannel[c] && channel[c][nchannel[c]-1].type==2/*NOTE*/) {
								channel[c][nchannel[c]-1].type = 1/*LNSTART*/;
								ADD_LNDONE(chan>20, chan%10, t, b);
							}
						} else {
							ADD_NOTE(chan>20, chan%10, t, b);
						}
					} else if (chan % 10 != 0 && chan > 29 && chan < 50) {
						ADD_INVNOTE(chan>40, chan%10, t, b);
					}
				}
				if (chan % 10 != 0 && chan > 49 && chan < 70) {
					if (lntype == 1 && b) {
						if (prev[chan-50]) {
							prev[chan-50] = 0;
							ADD_LNDONE(chan>60, chan%10, t, 0);
						} else {
							prev[chan-50] = b;
							ADD_LNSTART(chan>60, chan%10, t, b);
						}
					} else if (lntype == 2) {
						if (prev[chan-50] || prev[chan-50] != b) {
							if (prev[chan-50]) {
								if (prev[chan-30] + 1 < measure) {
									ADD_LNDONE(chan>60, chan%10, prev[chan-30]+1, 0);
								} else if (prev[chan-50] != b) {
									ADD_LNDONE(chan>60, chan%10, t, 0);
								}
							}
							if (b && (prev[chan-50]!=b || prev[chan-30]+1<measure)) {
								ADD_LNSTART(chan>60, chan%10, t, b);
							}
							prev[chan-30] = measure;
							prev[chan-50] = b;
						}
					}
				}
			}
		}
		free(bmsline[i]);
	}
	free(bmsline);
	length = measure + 2;
	for (i = 0; i < 20; ++i) {
		if (prev[i]) {
			if (lntype == 2 && prev[i+20] + 1 < measure) {
				ADD_LNDONE(i>10, i%10, prev[i+20]+1, 0);
			} else {
				ADD_LNDONE(i>10, i%10, length - 1, 0);
			}
		}
	}

	return 0;
}

static int sanitize_bmsline(void)
{
	int i, j, k, b, c;

	for (i = 0; i < 22; ++i) {
		if (!channel[i]) continue;
		qsort(channel[i], nchannel[i], sizeof(bmsnote), compare_bmsnote);

		if (i != 18 && i < 21) {
			b = 0;
			j = 0;
			while (j < nchannel[i]) {
				k = j;
				c = 0;
				for (k = j; k < nchannel[i] && channel[i][k].time <= channel[i][j].time; ++k) {
					if (c & 1 << channel[i][k].type) {
						remove_note(i, k);
					} else {
						c |= 1 << channel[i][k].type;
					}
				}

				if (b) {
					/* remove starting longnote if there's no ending longnote */
					c &= ~(c & 1 ? 0 : 2);
					/* remove visible note */
					c &= ~4;
					/* remove invisible note if there is any longnote */
					c &= ~(c & 3 ? 8 : 0);

					b = !(c & 1);
				} else {
					/* remove starting longnote if there's also ending longnote */
					c &= ~(c & 1 ? 2 : 0);
					/* remove ending longnote */
					c &= ~1;
					/* keep only one note, in the order of importance */
					c &= -c;

					b = (c & 2);
				}

				for (; j < k; ++j) {
					if (channel[i][j].type < 0) continue;

					if (i < 18 && !(c & 1 << channel[i][j].type)) {
						remove_note(i, j);
					}
				}
			}
			if (i < 18 && b) {
				/* remove last starting longnote which is unfinished */
				while (j >= 0 && channel[i][--j].type < 0);
				if (j >= 0 && channel[i][j].type == 1/*LNSTART*/) remove_note(i, j);
			}
		}
		k = 0;
		for (j = 0; j < nchannel[i]; ++j) {
			if (channel[i][j].type >= 0) {
				channel[i][j-k] = channel[i][j];
			} else {
				++k;
			}
		}
		nchannel[i] -= k;
	}

	for (i = 0; i < 2005; ++i) {
		if (_shorten[i] <= .001)
			_shorten[i] = 1;
	}

	return 0;
}

static int read_bms(void)
{
	if (parse_bms()) return 1;
	if (parse_bmsline()) return 1;
	if (sanitize_bmsline()) return 1;

	return 0;
}

static SDL_Surface *newsurface(int f, int w, int h); /* DUMMY */
static SDL_Rect *newrect(int x, int y, int w, int h); /* DUMMY */

static int load_resource(void (*callback)(const char *))
{
	SDL_Surface *temp;
	int i;

	for (i = 0; i < 1296; ++i) {
		if (sndpath[i]) {
			if (callback) callback(sndpath[i]);
			sndres[i] = Mix_LoadWAV(adjust_path(sndpath[i]));
			if (!sndres[i]) sndres[i] = Mix_LoadWAV(adjust_path_with_ext(sndpath[i], ".mp3"));
			if (!sndres[i]) sndres[i] = Mix_LoadWAV(adjust_path_with_ext(sndpath[i], ".ogg"));
			free(sndpath[i]);
			sndpath[i] = 0;
		}
		if (imgpath[i]) {
			char *ext = strrchr(imgpath[i], '.');
			if (callback) callback(imgpath[i]);
			if (ext && stricmp(ext, ".mpg") && !mpeg) {
				mpeg = SMPEG_new(adjust_path(imgpath[i]), NULL, 0);
				imgmpeg = i;
			} else {
				temp = IMG_Load(adjust_path(imgpath[i]));
				if (!temp) temp = IMG_Load(adjust_path_with_ext(imgpath[i], ".png"));
				if (!temp) temp = IMG_Load(adjust_path_with_ext(imgpath[i], ".jpg"));
				if (temp) {
					imgres[i] = SDL_DisplayFormat(temp);
					SDL_FreeSurface(temp);
					SDL_SetColorKey(imgres[i], SDL_SRCCOLORKEY|SDL_RLEACCEL, 0);
				}
			}
			free(imgpath[i]);
			imgpath[i] = 0;
		}
	}

	for (i = 0; i < nblitcmd; ++i) {
		if (blitcmd[i][0]<0 || blitcmd[i][1]<0 || !imgres[blitcmd[i][1]]) continue;
		temp = imgres[blitcmd[i][0]];
		if (!temp) {
			imgres[blitcmd[i][0]] = temp = newsurface(SDL_SWSURFACE, 256, 256);
			SDL_FillRect(temp, 0, SDL_MapRGB(imgres[blitcmd[i][0]]->format, 0, 0, 0));
			SDL_SetColorKey(temp, SDL_SRCCOLORKEY|SDL_RLEACCEL, 0);
		}
		if (blitcmd[i][2] < 0) blitcmd[i][2] = 0;
		if (blitcmd[i][3] < 0) blitcmd[i][3] = 0;
		if (blitcmd[i][4] > blitcmd[i][2] + 256) blitcmd[i][4] = blitcmd[i][2] + 256;
		if (blitcmd[i][5] > blitcmd[i][3] + 256) blitcmd[i][5] = blitcmd[i][3] + 256;
		SDL_BlitSurface(imgres[blitcmd[i][1]],
			newrect(blitcmd[i][2], blitcmd[i][3], blitcmd[i][4]-blitcmd[i][2], blitcmd[i][5]-blitcmd[i][3]),
			temp, newrect(blitcmd[i][6], blitcmd[i][7], 0, 0));
	}
	free(blitcmd);

	return 0;
}

static int xflag, xnnotes, xscore; /* DUMMY */

/*
bit 0: it uses 7 keys?
bit 1: it uses long-note?
bit 2: it uses pedal?
bit 3: it has bpm variation?
*/
static void get_bms_info(void)
{
	int i, j;

	xflag = xnnotes = 0;
	if (nchannel[7] || nchannel[8] || nchannel[16] || nchannel[17]) xflag |= 1;
	if (nchannel[6] || nchannel[15]) xflag |= 4;
	if (nchannel[20]) xflag |= 8;
	for (i = 0; i < 18; ++i) {
		for (j = 0; j < nchannel[i]; ++j) {
			if (channel[i][j].type == 1/*LNSTART*/) {
				xflag |= 2;
				++xnnotes;
			} else if (channel[i][j].type == 2/*NOTE*/) {
				++xnnotes;
			}
		}
	}
	for (i = 0; i < xnnotes; ++i) {
		xscore += (int)(300 * (1 + 1. * i / xnnotes));
	}
}

static double adjust_object_position(double base, double time); /* DUMMY */

static int pcur[22]; /* DUMMY */

static int get_bms_duration(void)
{
	int i, j, time, rtime, ttime;
	double pos, xbpm, tmp;

	xbpm = bpm;
	time = rtime = 0;
	pos = -1;
	while (1) {
		for (i = 0, j = -1; i < 22; ++i) {
			if (pcur[i] < nchannel[i] && (j < 0 || channel[i][pcur[i]].time < channel[j][pcur[j]].time)) j = i;
		}
		if (j < 0) {
			time += (int)(adjust_object_position(pos, length) * 24e7 / xbpm);
			break;
		}
		time += (int)(adjust_object_position(pos, channel[j][pcur[j]].time) * 24e7 / xbpm);

		i = channel[j][pcur[j]].index;
		ttime = 0;
		if (j < 19) {
			if (sndres[i]) ttime = (int)(sndres[i]->alen / .1764);
		} else if (j == 20) {
			tmp = (channel[j][pcur[j]].type ? bpmtab[i] : i);
			if (tmp > 0) {
				xbpm = tmp;
			} else if (tmp < 0) {
				time += (int)(adjust_object_position(-1, pos) * 24e7 / xbpm);
				break;
			}
		} else if (j == 21) {
			if (channel[j][pcur[j]].type) {
				time += i;
			} else {
				time += (int)(stoptab[i] * 125e4 * shorten[(int)(pos+1)-1] / xbpm);
			}
		}
		if (rtime < time + ttime) rtime = time + ttime;
		pos = channel[j][pcur[j]].time;
		++pcur[j];
	}
	return (time > rtime ? time : rtime) / 1000;
}

static void clone_bms(void)
{
	int i;

	for (i = 0; i < 9; ++i) {
		free(channel[i+9]);
		nchannel[i+9] = nchannel[i];
		channel[i+9] = malloc(sizeof(bmsnote) * nchannel[i]);
		memcpy(channel[i+9], channel[i], sizeof(bmsnote) * nchannel[i]);
	}
}

static void shuffle_bms(int mode, int player)
{
	bmsnote *tempchan, *result[18] = {0};
	int nresult[18] = {0};
	int map[18], perm[18], nmap;
	int ptr[18] = {0,}, thru[18] = {0}, target[18];
	int thrumap[18], thrurmap[18];
	int i, j, flag = 1, temp;
	double t;

	srand(time(0));
	for (i = 0; i < 18; ++i) map[i] = perm[i] = i;
	if (!nchannel[7] && !nchannel[8] && !nchannel[16] && !nchannel[17]) {
		map[7] = map[8] = map[16] = map[17] = -1;
	}
	if (!nchannel[6] && !nchannel[15]) {
		map[6] = map[15] = -1;
	}
	if (mode != 3 && mode != 5) {
		map[5] = map[6] = map[14] = map[15] = -1;
	}
	if (player) {
		for (i = 0; i < 9; ++i) map[player==1 ? i+9 : i] = -1;
	}
	for (i = j = 0; i < 18; ++i) {
		if (map[i] < 0) {
			++j;
		} else {
			map[i-j] = map[i];
		}
	}
	nmap = 18 - j;

	if (mode < 2) { /* mirror */
		for (i = 0, j = nmap-1; i < j; ++i, --j) {
			SWAP(channel[map[i]], channel[map[j]], tempchan);
			SWAP(nchannel[map[i]], nchannel[map[j]], temp);
		}
	} else if (mode < 4) { /* shuffle */
		for (i = nmap-1; i > 0; --i) {
			j = rand() % i;
			SWAP(channel[map[i]], channel[map[j]], tempchan);
			SWAP(nchannel[map[i]], nchannel[map[j]], temp);
		}
	} else if (mode < 6) { /* random */
		while (flag) {
			for (i = nmap-1; i > 0; --i) {
				j = rand() % i;
				SWAP(perm[i], perm[j], temp);
			}
			t = 1e4;
			flag = 0;
			for (i = 0; i < nmap; ++i) {
				if (ptr[map[i]] < nchannel[map[i]]) {
					flag = 1;
					target[i] = 1;
					if (t > channel[map[i]][ptr[map[i]]].time)
						t = channel[map[i]][ptr[map[i]]].time - 1e-4;
				} else {
					target[i] = 0;
				}
			}
			t += 2e-4;
			for (i = 0; i < nmap; ++i) {
				if (target[i] && channel[map[i]][ptr[map[i]]].time > t)
					target[i] = 0;
			}
			for (i = 0; i < nmap; ++i) {
				if (!target[i]) continue;
				temp = channel[map[i]][ptr[map[i]]].type;
				if (temp == 0/*LNDONE*/) {
					j = thrumap[i];
					thru[j] = 2;
				} else {
					j = perm[i];
					while (thru[j]) j = perm[thrurmap[j]];
					if (temp == 1/*LNSTART*/) {
						thrumap[i] = j;
						thrurmap[j] = i;
						thru[j] = 1;
					}
				}
				result[j] = realloc(result[j], sizeof(bmsnote) * (nresult[j]+1));
				result[j][nresult[j]++] = channel[map[i]][ptr[map[i]]++];
			}
			for(i = 0; i < nmap; ++i) {
				if (thru[i] == 2) thru[i] = 0;
			}
		}
		for (i = 0; i < nmap; ++i) {
			free(channel[map[i]]);
			channel[map[i]] = result[i];
			nchannel[map[i]] = nresult[i];
		}
	}
}

/******************************************************************************/
/* general graphic functions */

static SDL_Surface *screen;
static SDL_Event event;
static SDL_Rect rect[2];
static int _rect=0;

static int getpixel(SDL_Surface *s, int x, int y)
{
	Uint8 r, g, b;
	SDL_GetRGB(((Uint32*)s->pixels)[x+y*s->pitch/4], s->format, &r, &g, &b);
	return (int)(r << 16) | ((int)g << 8) | (int)b;
}

static Uint32 map(int c)
{
	return SDL_MapRGB(screen->format, c >> 16, (c >> 8) & 255, c & 255);
}

static int putpixel(SDL_Surface *s, int x, int y, int c)
{
	c = SDL_MapRGB(s->format, c >> 16, (c >> 8) & 255, c & 255);
	return ((Uint32*)s->pixels)[x+y*s->pitch/4] = c;
}

static int blend(int x, int y, int a, int b)
{
	int i = 0;
	for (; i < 24; i += 8)
		y += ((x>>i&255) - (y>>i&255))*a/b << i;
	return y;
}

static void putblendedpixel(SDL_Surface *s, int x, int y, int c, int o)
{
	putpixel(s, x, y, blend(getpixel(s,x,y), c, o, 255));
}

static SDL_Surface *newsurface(int f, int w, int h)
{
	return SDL_CreateRGBSurface(f, w, h, 32, 0xff0000, 0xff00, 0xff, 0);
}

static SDL_Rect *newrect(int x, int y, int w, int h)
{
	SDL_Rect *r = rect+_rect++%2;
	r->x = x;
	r->y = y;
	r->w = w;
	r->h = h;
	return r;
}

static int bicubic_kernel(int x, int y) {
	if (x < 0) x = -x;
	if (x < y) {
		return ((y*y - 2*x*x + x*x/y*x) << 11) / y / y;
	} else if (x < y * 2) {
		return ((4*y*y - 8*x*y + 5*x*x - x*x/y*x) << 11) / y / y;
	} else {
		return 0;
	}
}

static int bicubic_interpolation(SDL_Surface *src, SDL_Surface *dest) {
	int x, dx, y, dy, ww, hh, w, h;
	int i, j, k, r, g, b, c, xx, yy, a[4][2], d;

	dx = x = 0;
	ww = src->w - 1;
	hh = src->h - 1;
	w = dest->w - 1;
	h = dest->h - 1;
	for (i = 0; i <= w; ++i) {
		dy = y = 0;
		for (j = 0; j <= h; ++j) {
			r = g = b = 0;
			for (k = 0; k < 4; ++k) {
				a[k][0] = bicubic_kernel((x+k-1)*w - i*ww, w);
				a[k][1] = bicubic_kernel((y+k-1)*h - j*hh, h);
			}
			for (k = 0; k < 16; ++k) {
				xx = x + k/4 - 1;
				yy = y + k%4 - 1;
				if (xx>=0 && xx<=ww && yy>=0 && yy<=hh) {
					c = getpixel(src, xx, yy);
					d = a[k/4][0] * a[k%4][1] >> 6;
					r += (c>>16) * d;
					g += (c>>8&255) * d;
					b += (c&255) * d;
				}
			}
			r = (r<0 ? 0 : r>>24 ? 255 : r>>16);
			g = (g<0 ? 0 : g>>24 ? 255 : g>>16);
			b = (b<0 ? 0 : b>>24 ? 255 : b>>16);
			putpixel(dest, i, j, (r<<16) | (g<<8) | b);

			dy += hh;
			if (dy > h) {
				++y;
				dy -= h;
			}
		}
		dx += ww;
		if (dx > w) {
			++x;
			dx -= w;
		}
	}

	return 0;
}

/******************************************************************************/
/* font functions */

static int fontdatawords[] = {0, 0, 2, 6, 2, 5, 32, 96, 97, 15, 497, 15,
	1521, 15, 1537, 16, 48, 176, 1, 3, 1, 3, 7, 1, 4080, 4096, 3, 1, 8,
	3, 4097, 4080, 16, 16128, 240, 1, 2, 9, 3, 8177, 15, 16385, 240, 15,
	1, 47, 721, 143, 2673, 2, 6, 7, 1, 31, 17, 16, 63, 64, 33, 0, 1, 2,
	1, 8, 3};
static const char *fontdataindices =
	"!!7a/&w7a!!g'M*Qg(O&J!!&Je!!g2Qg-B!!u2cQ[b7b2[[Q7b2[bQ!c!i&d>UT2c&"
	"b>WT!b2eGW&!c!i2MbUbD!.8(M$UQCb-b!!l'U*2eTXb2Gb5b>9ZW!!k*e8(J$b!!u"
	"*8(M$Q!h#b'O*?!!k#Q'O*?b!h8(M$Q!!m&JTLcG_&J>]TLc&J!!o&Je7[&Je!!{&J"
	"dM$Q!!s7[!!}e&Je!!m0b3b.8(M$U!Cb-b!o>UQ2Ub]ba9Y[SbQCb2GW!!k*MbQbBb"
	"!kaQ!!k>UQ2Ub]bG8.M(U$[!Ca[!!k>UQ2d!IJQ!d2cGW!!k78bM6U2Cb2ea[2!c!l"
	"a[!2e>[Q!d2cGW!!k>UQ2c!b>[Q2gGW!!ka[Q!UbDb.8(J&h!!k>UQ2eIXQ2gGW!!k"
	">UQ2gaWQ!b2cGW!!m&Je!!e&Je!!m&Je!!e&JdM$Q!!k3b.8(M$U!H#W'O,?4!!t7["
	"!!c7[!!r:#W'O,?5!.8(M$U!Cb!k>UQ2cUbD!.8(J!!&Jc!!k>UJ)LKKhGb!)7W!!k"
	"'8,M=UWCQ2ca[Q2e!!k>[Q2eI[Q2gG[!!k>MSUQC!2gQ:RWGO!!k,[<2WbQhUbDb.["
	"!!ka[!2e7[!2ga[!!ka[!2e7[!2j!k>MSUQC!2c[bWbQ:RWGO!!kQ2ga[Q2i!!k7[&"
	"Jo7[!!kQm2eGW!!kU2Db.9([$b#b'b,@<2Wb!!l2qa[!!kU:^[adT2cQh!!kQ2d:R["
	"Vba@_2WbQd!!k,M=UWCQ2gU:EW.O!!k>[Q2gG[!2h!k>UQ2iVbabIW_!Wb!j>[Q2gI"
	"[Q2g!!k>UQ2c!b>WQ!d2cGW!!k7[&Jq!!kQ2qGW!!kQ2kU:EW.O(?!!kQ2gTfa[CW2"
	"Q!!kM+U:^[GW.O,M>U`[WCO-!!kM'U,D<.W(O&Ji!!ka[Q!Ub]bG8.M(U$[!Ca[!!k"
	"*Q!p*b!!n+b:#W'O,?4!1b!p*Qb!pQ!!g'8,M=UWCO-!!}oa[!!i#Q'O,?4!!}b>QQ"
	"!caUQ2eaW!!l2e>[Q2iG[!!o>UQ2c!dQdGW!!kQfaUQ2iaW!!o>UQ2ea[!2UbGW!!k"
	">8TJc&b7[&Ji!!oaUQ2gaWQ!b2cGW!!f2e>[Q2m!!k&Jc!!&Jm!!kQd!dQi2eGW!!h"
	"2cUbDb.9(['b,@<2Wb!!k*Jb?b!o!p;[abT2gQd!!o>[Q2m!!o>UQ2kGW!!o>[Q2kG"
	"[!2f!iaUQ2kaWQ!e!j>[Q2c!k!o>UW2!b>WQ!d:GW!!k&Jc7[&JeTfG?!!oQ2mGW!!"
	"oQ2gU:EW.O(?!!oQ2eTfa[FW!!oM+U:EW.O,M=UWCO-!!oQ2f:RWGO.A(M$U!Cb!ka"
	"[]!G8.M(U$[!Ca[!!i78&Jg%[&Jg7?!!e&J}c!!c#[&Jg7A&Jg$[!!k#]NaaPG?!!}"
	"o7[.W(O";

static Uint16 fontdata[3072];
static Uint8 (*zoomfont[16])[96] = {NULL};

static void fontdecompress(void)
{
	int i, ch;

	for (i = ch = 0; i < ARRAYSIZE(fontdatawords); ++i) {
		ch += fontdatawords[i];
		fontdatawords[i] = ch;
	}
	for (i = 0; ch = *fontdataindices++; ) {
		if (ch > 97) {
			while (ch-- > 97) fontdata[i] = fontdata[i-2], ++i;
		} else if (ch > 32) {
			fontdata[i++] = fontdatawords[ch - 33];
		}
	}
}

static int _fontprocess(int x, int y, int z, int c, int s)
{
	int i, j;
	for (i = 0; i < z; ++i) {
		for (j = (s==1 ? z-i : s==3 ? i+1 : 0); j < (s==2 ? i : s==4 ? z-i-1 : z); ++j)
			zoomfont[z][(y*z+i)*z+j][c] |= 1<<(7-x);
	}
	return z;
}

static void fontprocess(int z)
{
	int i, j, k, l, v;

	zoomfont[z] = calloc(16*z*z, 96);
	for (i = l = 0; i < 96; ++i) {
		for (j = 0; j < 16; ++j, l+=2) {
			v = fontdata[l] << 16 | fontdata[l+1];
			for (k = 0; k < 8; ++k, v >>= 4) {
				if ((v & 15) == 15) _fontprocess(k, j, z, i, 0); /* XXX? */
				if (v & 1) _fontprocess(k, j, z, i, 1); /* /| */
				if (v & 2) _fontprocess(k, j, z, i, 2); /* |\ */
				if (v & 4) _fontprocess(k, j, z, i, 3); /* \| */
				if (v & 8) _fontprocess(k, j, z, i, 4); /* |/ */
			}
		}
	}
}

static int printchar(SDL_Surface *s, int x, int y, int z, int c, int u, int v)
{
	int i, j;

	if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
		c -= (c<0 ? -96 : c<33 || c>126 ? c : 32);
		for (i = 0; i < 16*z; ++i) {
			for (j = 0; j < 8*z; ++j) {
				if (zoomfont[z][i*z+j%z][c] & (1<<(7-j/z)))
					putpixel(s, x+j, y+i, blend(u, v, i, 16*z-1));
			}
		}
	}

	return 8*z;
}

static void printstr(SDL_Surface *s, int x, int y, int z, const char *c, int u, int v)
{
	while (*c) {
		x += printchar(s, x, y, z, (Uint8)*c++, u, v);
	}
}

/******************************************************************************/
/* main routines */

static double playspeed = 1, targetspeed;
static int now, origintime, starttime, stoptime = 0, adjustspeed = 0;
static double startoffset = -1, startshorten = 1;
static int xflag, xnnotes, xscore, xduration;
static int pcur[22] = {0}, pfront[22] = {0}, prear[22] = {0}, pcheck[18] = {0}, thru[22] = {0};
static int bga[3] = {-1,-1,0}, poorbga = 0, bga_updated = 1;
static int score = 0, scocnt[5] = {0}, scombo = 0, smaxcombo = 0;
static double gradefactor;
static int gradetime = 0, grademode, gauge = 256;
static int opt_mode = 0, opt_showinfo = 1, opt_fullscreen = 1, opt_random = 0;

static SDL_Surface *sprite=0;
static int keymap[18]={
	SDLK_z, SDLK_s, SDLK_x, SDLK_d, SDLK_c, SDLK_LSHIFT, SDLK_LALT, SDLK_f, SDLK_v,
	SDLK_m, SDLK_k, SDLK_COMMA, SDLK_l, SDLK_PERIOD, SDLK_RSHIFT, SDLK_RALT, SDLK_SEMICOLON, SDLK_SLASH};
static int keypressed[18] = {0};
static int tkeyleft[18] = {41,67,93,119,145,0,223,171,197, 578,604,630,656,682,760,537,708,734};
static int tkeywidth[18] = {25,25,25,25,25,40,40,25,25, 25,25,25,25,25,40,40,25,25};
static int tpanel1 = 264, tpanel2 = 536, tbga = 272;
#define WHITE 0x808080
#define BLUE 0x8080ff
static int tkeycolor[18] = {
	WHITE, BLUE, WHITE, BLUE, WHITE, 0xff8080, 0x80ff80, BLUE, WHITE,
	WHITE, BLUE, WHITE, BLUE, WHITE, 0xff8080, 0x80ff80, BLUE, WHITE};
#undef WHITE
#undef BLUE
static char *tgradestr[5] = {"MISS", "BAD", "GOOD", "GREAT", "COOL"};
static int tgradecolor[5][2] = {
	{0xff4040, 0xffc0c0}, {0xff40ff, 0xffc0ff}, {0xffff40, 0xffffc0},
	{0x40ff40, 0xc0ffc0}, {0x4040ff, 0xc0c0ff}};

static int check_exit(void)
{
	int i = 0;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT || (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE)) i = 1;
	}
	return i;
}

static SDLKey get_sdlkey_from_name(const char *str)
{
	SDLKey i;

	/* XXX maybe won't work in SDL 1.3 */
	for (i = SDLK_FIRST; i < SDLK_LAST; ++i) {
		if (stricmp(SDL_GetKeyName(i), str)) return i;
	}
	return SDLK_UNKNOWN;
}

static void read_envvars(void)
{
	static const char *keymapname[] = {
		"KEY1P_1", "KEY1P_2", "KEY1P_3", "KEY1P_4", "KEY1P_5", "KEY1P_SCRATCH", "KEY1P_PEDAL", "KEY1P_6", "KEY1P_7",
		"KEY2P_1", "KEY2P_2", "KEY2P_3", "KEY2P_4", "KEY2P_5", "KEY2P_SCRATCH", "KEY2P_PEDAL", "KEY2P_6", "KEY2P_7",
	};

	const char *ptr;
	int i;
	SDLKey key;

	ptr = getenv("PLAYSPEED");
	playspeed = ptr ? atof(ptr) : 0;
	if (playspeed <= 0) playspeed = 1;
	if (playspeed < .1) playspeed = .1;
	if (playspeed > 99) playspeed = 99;

	for (i = 0; i < 18; ++i) {
		ptr = getenv(keymapname[i]);
		key = ptr ? get_sdlkey_from_name(ptr) : SDLK_UNKNOWN;
		if (key != SDLK_UNKNOWN) keymap[i] = key;
	}
}

static int initialize(void)
{
	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO) < 0)
		return errormsg("SDL Initialization Failure: %s", SDL_GetError());
	atexit(SDL_Quit);
	screen = SDL_SetVideoMode(800, 600, 32, opt_fullscreen ? SDL_FULLSCREEN : SDL_SWSURFACE|SDL_DOUBLEBUF);
	if (!screen)
		return errormsg("SDL Video Initialization Failure: %s", SDL_GetError());
	SDL_ShowCursor(SDL_DISABLE);
	IMG_Init(IMG_INIT_JPG|IMG_INIT_PNG);
	Mix_Init(MIX_INIT_OGG|MIX_INIT_MP3);
	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048)<0)
		return errormsg("SDL Mixer Initialization Failure: %s", Mix_GetError());
	SDL_WM_SetCaption(VERSION, 0);

	fontdecompress();
	fontprocess(1);
	fontprocess(2);
	fontprocess(3);
	read_envvars();
	return 0;
}

static SDL_Surface *stagefile_tmp;

static void callback_resource(const char *path) {
	SDL_BlitSurface(stagefile_tmp, newrect(0,0,800,20), screen, newrect(0,580,800,20));
	printstr(screen, 797-8*strlen(path), 582, 1, path, 0x808080, 0xc0c0c0);
	SDL_Flip(screen);
	if (check_exit()) exit(0);
}

static void play_show_stagefile(void)
{
	SDL_Surface *temp, *stagefile;
	char buf[256];
	int i, j, t;

	sprintf(buf, "%s: %s - %s", VERSION, metadata[2], metadata[0]);
	//SDL_WM_SetCaption(buf, 0);
	printstr(screen, 248, 284, 2, "loading bms file...", 0x202020, 0x808080);
	SDL_Flip(screen);

	stagefile_tmp = newsurface(SDL_SWSURFACE, 800, 20);
	if (*metadata[3] && (temp = IMG_Load(adjust_path(metadata[3])))) {
		stagefile = SDL_DisplayFormat(temp);
		bicubic_interpolation(stagefile, screen);
		SDL_FreeSurface(temp);
		SDL_FreeSurface(stagefile);
	}
	if (opt_showinfo) {
		for (i = 0; i < 800; ++i) {
			for (j = 0; j < 42; ++j) putblendedpixel(screen, i, j, 0x101010, 64);
			for (j = 580; j < 600; ++j) putblendedpixel(screen, i, j, 0x101010, 64);
		}
		printstr(screen, 6, 4, 2, metadata[0], 0x808080, 0xffffff);
		printstr(screen, 792-8*strlen(metadata[1]), 4, 1, metadata[1], 0x808080, 0xffffff);
		printstr(screen, 792-8*strlen(metadata[2]), 20, 1, metadata[2], 0x808080, 0xffffff);
		i = sprintf(buf, "Level %d | BPM %.2f%s | %d note%s [%dKEY%s]",
			v_playlevel, bpm, "?"+((xflag&8)==0), xnnotes,
			"s"+(xnnotes==1), (xflag&1) ? 7 : 5, (xflag&2) ? "-LN" : "");
		printstr(screen, 3, 582, 1, buf, 0x808080, 0xffffff);
		SDL_BlitSurface(screen, newrect(0,580,800,20), stagefile_tmp, newrect(0,0,800,20));
	}
	SDL_Flip(screen);

	t = SDL_GetTicks() + 3000;
	load_resource(opt_showinfo ? callback_resource : 0);
	if (opt_showinfo) {
		callback_resource("loading...");
		SDL_FreeSurface(stagefile_tmp);
	}
	while ((int)SDL_GetTicks() < t) {
		if (check_exit()) exit(0);
	}
}

static void play_prepare(void)
{
	int i, j, c;

	/* panel position */
	if ((xflag&4) == 0) {
		tkeyleft[6] = tkeyleft[15] = -1;
		tpanel1 -= tkeywidth[6] + 1;
		tpanel2 += tkeywidth[15] + 1;
	}
	if ((xflag&1) == 0) {
		tkeyleft[7] = tkeyleft[8] = tkeyleft[16] = tkeyleft[17] = -1;
		for (i = 9; i < 16; ++i) {
			if (i != 14 && tkeyleft[i] >= 0)
				tkeyleft[i] += tkeywidth[16] + tkeywidth[17] + 2;
		}
		if (tkeyleft[6] > 0)
			tkeyleft[6] -= tkeywidth[7] + tkeywidth[8] + 2;
		tpanel1 -= tkeywidth[7] + tkeywidth[8] + 2;
		tpanel2 += tkeywidth[16] + tkeywidth[17] + 2;
	}
	if (v_player == 1) {
		for (i = 9; i < 18; ++i) tkeyleft[i] = -1;
	} else if (v_player == 3) {
		for (i = 9; i < 18; ++i) tkeyleft[i] += tpanel1 - tpanel2;
		tpanel1 += 801 - tpanel2;
	}
	if (v_player % 2) {
		tpanel2 = 0;
		tbga = tpanel1 / 2 + 282;
	}

	/* sprite */
	sprite = newsurface(SDL_SWSURFACE, 1200, 600);
	for (i = 0; i < 18; ++i) {
		if (tkeyleft[i] < 0) continue;
		for (j = 140; j < 520; ++j)
			SDL_FillRect(sprite, newrect(tkeyleft[i],j,tkeywidth[i],1), blend(tkeycolor[i], 0, j-140, 1000));
		if (i < 9) {
			for (j = 0; j*2 < tkeywidth[i]; ++j)
				SDL_FillRect(sprite, newrect(800+tkeyleft[i]+j,0,tkeywidth[i]-2*j,600), blend(tkeycolor[i], 0xffffff, tkeywidth[i]-j, tkeywidth[i]));
		}
	}
	for (j = -244; j < 556; ++j) {
		for (i = -10; i < 20; ++i) {
			c = (i*2+j*3+750) % 2000;
			c = blend(0xc0c0c0, 0x606060, c>1000 ? 1850-c : c-150, 700);
			putpixel(sprite, j+244, i+10, c);
		}
		for (i = -20; i < 60; ++i) {
			c = (i*3+j*2+750) % 2000;
			c = blend(0xc0c0c0, 0x404040, c>1000 ? 1850-c : c-150, 700);
			putpixel(sprite, j+244, i+540, c);
		}
	}
	SDL_FillRect(sprite, newrect(tpanel1+20,0,(tpanel2?tpanel2:820)-tpanel1-40,600), 0);
	for (i = 0; i < 20; ++i) {
		for (j = 20; j*j+i*i > 400; --j) {
			putpixel(sprite, tpanel1+j, i+10, 0);
			putpixel(sprite, tpanel1+j, 539-i, 0);
			if (tpanel2) {
				putpixel(sprite, tpanel2-j-1, i+10, 0);
				putpixel(sprite, tpanel2-j-1, 539-i, 0);
			}
		}
	}
	if (!tpanel2 && !opt_mode) {
		SDL_FillRect(sprite, newrect(0,584,368,16), 0x404040);
		SDL_FillRect(sprite, newrect(4,588,360,8), 0);
	}
	SDL_FillRect(sprite, newrect(10,564,tpanel1,1), 0x404040);

	/* screen */
	SDL_FillRect(screen, 0, map(0));
	SDL_BlitSurface(sprite, newrect(0,0,800,30), screen, newrect(0,0,0,0));
	SDL_BlitSurface(sprite, newrect(0,520,800,80), screen, newrect(0,520,0,0));

	/* video (if any) */
	if (mpeg) {
		imgres[imgmpeg] = newsurface(SDL_SWSURFACE, 256, 256);
		SMPEG_enablevideo(mpeg, 1);
		SMPEG_setdisplay(mpeg, imgres[imgmpeg], NULL, NULL);
	}

	/* configuration */
	origintime = starttime = SDL_GetTicks();
	targetspeed = playspeed;
	Mix_AllocateChannels(128);
	for (i = 0; i < 22; ++i) pcur[i] = 0;
	gradefactor = 1.5 - v_rank * .25;
}

static double adjust_object_time(double base, double offset)
{
	int i = (int)(base+1)-1;
	if ((i + 1 - base) * shorten[i] > offset)
		return base + offset / shorten[i];
	offset -= (i + 1 - base) * shorten[i];
	while (shorten[++i] <= offset)
		offset -= shorten[i];
	return i + offset / shorten[i];
}

static double adjust_object_position(double base, double time)
{
	int i = (int)(base+1)-1, j = (int)(time+1)-1;
	base = (time - j) * shorten[j] - (base - i) * shorten[i];
	while (i < j) base += shorten[i++];
	return base;
}

static void update_grade(int grade)
{
	++scocnt[grade];
	grademode = grade;
	gradetime = now + 700;

	if (grade < 1) {
		scombo = 0;
		gauge -= 12;
		poorbga = now + 600;
	} else if (grade < 2) {
		scombo = 0;
		gauge -= 5;
	} else if (grade < 3) {
		/* do nothing */
	} else {
		++scombo;
		gauge += (grade<4 ? 3 : 5) + scombo / 100;
	}

	if (scombo > smaxcombo) smaxcombo = scombo;
}

static int play_process(void)
{
	int i, j, k, l, ibottom;
	double bottom, top, line, tmp;
	char buf[99];

	if (adjustspeed) {
		tmp = targetspeed - playspeed;
		if (-0.001 < tmp && tmp < 0.001) {
			adjustspeed = 0;
			playspeed = targetspeed;
			for (i = 0; i < 22; ++i) prear[i] = pfront[i];
		} else {
			playspeed += tmp * 0.05;
		}
	}

	now = SDL_GetTicks();
	if (now < stoptime) {
		bottom = startoffset;
	} else if (stoptime) {
		starttime = now;
		stoptime = 0;
		bottom = startoffset;
	} else {
		bottom = startoffset + (now - starttime) * bpm / startshorten / 24e4;
	}
	ibottom = (int)(bottom + 1) - 1;
	if (bottom > -1 && startshorten != shorten[ibottom]) {
		starttime += (int)((ibottom - startoffset) * 24e4 * startshorten / bpm);
		startoffset = ibottom;
		startshorten = shorten[ibottom];
	}
	line = bottom;//adjust_object_time(bottom, 0.03/playspeed);
	top = adjust_object_time(bottom, 1.25/playspeed);
	for (i = 0; i < 22; ++i) {
		while (pfront[i] < nchannel[i] && channel[i][pfront[i]].time < bottom) ++pfront[i];
		while (prear[i] < nchannel[i] && channel[i][prear[i]].time <= top) ++prear[i];
		while (pcur[i] < nchannel[i] && channel[i][pcur[i]].time < line) {
			j = channel[i][pcur[i]].index;
			if (i == 18) {
				if (sndres[j]) {
					j = Mix_PlayChannel(-1, sndres[j], 0);
					if (j >= 0) {
						Mix_Volume(j, 96);
						Mix_GroupChannel(j, 1);
					}
				}
			} else if (i == 19) {
				bga[channel[i][pcur[i]].type] = j;
				bga_updated = 1;
			} else if (i == 20) {
				if (tmp = (channel[i][pcur[i]].type ? bpmtab[j] : j)) {
					starttime = now;
					startoffset = bottom;
					bpm = tmp;
				}
			} else if (i == 21) {
				if (now >= stoptime) stoptime = now;
				if (channel[i][pcur[i]].type) {
					stoptime += j;
				} else {
					stoptime += (int)(stoptab[j] * 1250 * startshorten / bpm);
				}
				startoffset = bottom;
			} else if (opt_mode && channel[i][pcur[i]].type != 3/*INVNOTE*/ && sndres[j]) {
				j = Mix_PlayChannel(-1, sndres[j], 0);
				if (j >= 0) Mix_GroupChannel(j, 0);
				score += (int)(300 * (1 + 1. * scombo / xnnotes));
				update_grade(4);
			}
			++pcur[i];
		}
		if (i<18 && !opt_mode) {
			for (; pcheck[i] < pcur[i]; ++pcheck[i]) {
				j = channel[i][pcheck[i]].type;
				if (j < 0 || j == 3/*INVNOTE*/ || (j == 0/*LNDONE*/ && !thru[i])) continue;
				tmp = channel[i][pcheck[i]].time;
				tmp = (line - tmp) * shorten[(int)tmp] / bpm * gradefactor;
				if (tmp > 6e-4) update_grade(0); else break;
			}
		}
	}

	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT) {
			return -1;
		} else if (event.type == SDL_KEYUP) {
			if (event.key.keysym.sym == SDLK_ESCAPE) {
				return -1;
			} else if (!opt_mode) {
				for (i = 0; i < 18; ++i) {
					if (tkeyleft[i] >= 0 && event.key.keysym.sym == keymap[i]) {
						keypressed[i] = 0;
						if (nchannel[i] && thru[i]) {
							for (j = pcur[i]; channel[i][j].type != 0/*LNDONE*/; --j);
							thru[i] = 0;
							tmp = (channel[i][j].time - line) * shorten[(int)line] / bpm * gradefactor;
							if (-6e-4 < tmp && tmp < 6e-4) {
								channel[i][j].type ^= -1;
							} else {
								update_grade(0);
							}
						}
					}
				}
			}
		} else if (event.type == SDL_KEYDOWN) {
			if (event.key.keysym.sym == SDLK_F3) {
				if (targetspeed > 20) targetspeed -= 5;
				else if (targetspeed > 10) targetspeed -= 1;
				else if (targetspeed > 1) targetspeed -= .5;
				else if (targetspeed > .201) targetspeed -= .2;
				else continue;
				adjustspeed = 1;
			} else if (event.key.keysym.sym == SDLK_F4) {
				if (targetspeed < 1) targetspeed += .2;
				else if (targetspeed < 10) targetspeed += .5;
				else if (targetspeed < 20) targetspeed += 1;
				else if (targetspeed < 95) targetspeed += 5;
				else continue;
				adjustspeed = 1;
			} else if (!opt_mode) {
				for (i = 0; i < 18; ++i) {
					if (tkeyleft[i] >= 0 && event.key.keysym.sym == keymap[i]) {
						keypressed[i] = 1;
						if (!nchannel[i]) continue;

						j = (pcur[i] < 1 || (pcur[i] < nchannel[i] && channel[i][pcur[i]-1].time + channel[i][pcur[i]].time < 2*line) ? pcur[i] : pcur[i]-1);
						if (sndres[channel[i][j].index]) {
							l = Mix_PlayChannel(-1, sndres[channel[i][j].index], 0);
							if (l >= 0) Mix_GroupChannel(l, 0);
						}

						if (j < pcur[i]) {
							while (j >= 0 && channel[i][j].type == 3/*INVNOTE*/) --j;
							if (j < 0) continue;
						} else {
							while (j < nchannel[i] && channel[i][j].type == 3/*INVNOTE*/) ++j;
							if (j == nchannel[i]) continue;
						}

						if (channel[i][j].type == 0/*LNDONE*/) {
							if (thru[i]) update_grade(0);
						} else if (channel[i][j].type >= 0) {
							tmp = (channel[i][j].time - line) * shorten[(int)line] / bpm * gradefactor;
							if (tmp < 0) tmp *= -1;
							if (channel[i][j].type >= 0 && tmp < 6e-4) {
								if (channel[i][j].type == 1/*LNSTART*/) thru[i] = 1;
								channel[i][j].type ^= -1;
								score += (int)((300 - tmp * 5e5) * (1 + 1. * scombo / xnnotes));
								update_grade(tmp<0.6e-4 ? 4 : tmp<2.0e-4 ? 3 : tmp<3.5e-4 ? 2 : 1);
							}
						}
					}
				}
			}
		}
	}
	if (bottom > length) {
		if (opt_mode ? Mix_Playing(-1)==0 : Mix_GroupNewer(1)==-1) return 0;
	} else if (bottom < -1) {
		return 0;
	}

	SDL_FillRect(screen, newrect(0,30,tpanel1,490), map(0x404040));
	if (tpanel2) SDL_FillRect(screen, newrect(tpanel2,30,800-tpanel2,490), map(0x404040));
	for (i = 0; i < 18; ++i) {
		if (tkeyleft[i] < 0) continue;
		SDL_FillRect(screen, newrect(tkeyleft[i],30,tkeywidth[i],490), map(0));
		if (keypressed[i]) {
			SDL_BlitSurface(sprite, newrect(tkeyleft[i],140,tkeywidth[i],380), screen, newrect(tkeyleft[i],140,0,0));
		}
	}
	SDL_SetClipRect(screen, newrect(0,30,800,490));
	for (i = 0; i < 18; ++i) {
		if (tkeyleft[i] < 0) continue;
		for (j = pfront[i]; j < prear[i]; ++j) {
			k = (int)(525 - 400 * playspeed * adjust_object_position(bottom, channel[i][j].time));
			if (channel[i][j].type == 1/*LNSTART*/) {
				l = k + 5;
				k = (int)(530 - 400 * playspeed * adjust_object_position(bottom, channel[i][++j].time));
				if (k < 30) k = 30;
			} else if (channel[i][j].type == 0/*LNDONE*/) {
				k += 5;
				l = 520;
			} else if (channel[i][j].type == 2/*NOTE*/) {
				l = k + 5;
			} else {
				continue;
			}
			if (k > 0 && l > k) {
				SDL_BlitSurface(sprite, newrect(800+tkeyleft[i%9],0,tkeywidth[i%9],l-k), screen, newrect(tkeyleft[i],k,0,0));
			}
		}
		if (pfront[i]==prear[i] && prear[i]<nchannel[i] && channel[i][prear[i]].type==0/*LNDONE*/) {
			SDL_BlitSurface(sprite, newrect(800+tkeyleft[i%9],0,tkeywidth[i%9],490), screen, newrect(tkeyleft[i],30,0,0));
		}
	}
	for (i = ibottom; i < top; ++i) {
		j = (int)(530 - 400 * playspeed * adjust_object_position(bottom, i));
		SDL_FillRect(screen, newrect(0,j,tpanel1,1), map(0xc0c0c0));
		if (tpanel2) SDL_FillRect(screen, newrect(tpanel2,j,800-tpanel2,1), map(0xc0c0c0));
	}
	if (now < gradetime) {
		int delta = (gradetime - now - 400) / 30;
		if (delta < 0) delta = 0;
		printstr(screen, tpanel1/2-8*strlen(tgradestr[grademode]), 260 - delta, 2,
				tgradestr[grademode], tgradecolor[grademode][0], tgradecolor[grademode][1]);
		if (scombo > 1) {
			i = sprintf(buf, "%d COMBO", scombo);
			printstr(screen, tpanel1/2-4*i, 288 - delta, 1, buf, 0x808080, 0xffffff);
		}
		if (opt_mode) printstr(screen, tpanel1/2-24, 302 - delta, 1, "(AUTO)", 0x404040, 0xc0c0c0);
		if (!grademode) bga_updated = 1;
	}
	SDL_SetClipRect(screen, 0);
	if (bga_updated > 0 || (bga_updated < 0 && now >= poorbga) || (mpeg && SMPEG_status(mpeg) == SMPEG_PLAYING)) {
		SDL_FillRect(screen, newrect(tbga,172,256,256), map(0));
		for (i = 0; i < 3; ++i) {
			if (bga_updated > 0 && bga[i] == imgmpeg && SMPEG_status(mpeg) != SMPEG_PLAYING) SMPEG_play(mpeg);
		}
		if (now < poorbga) {
			if (bga[2] >= 0 && imgres[bga[2]])
				SDL_BlitSurface(imgres[bga[2]], newrect(0,0,256,256), screen, newrect(tbga,172,0,0));
			bga_updated = -1;
		} else {
			for (i = 0; i < 2; ++i)
				if (bga[i] >= 0 && imgres[bga[i]])
					SDL_BlitSurface(imgres[bga[i]], newrect(0,0,256,256), screen, newrect(tbga,172,0,0));
			bga_updated = 0;
		}
	}

	i = (now - origintime) / 1000;
	j = xduration / 1000;
	sprintf(buf, "SCORE %07d%c%4.1fx%c%02d:%02d / %02d:%02d%c@%9.4f%cBPM %6.2f",
			score, 0, targetspeed, 0, i/60, i%60, j/60, j%60, 0, bottom, 0, bpm);
	SDL_BlitSurface(sprite, newrect(0,0,800,30), screen, newrect(0,0,0,0));
	SDL_BlitSurface(sprite, newrect(0,520,800,80), screen, newrect(0,520,0,0));
	printstr(screen, 10, 8, 1, buf, 0, 0);
	printstr(screen, 5, 522, 2, buf+14, 0, 0);
	printstr(screen, tpanel1-94, 565, 1, buf+20, 0, 0x404040);
	printstr(screen, 95, 538, 1, buf+34, 0, 0);
	printstr(screen, 95, 522, 1, buf+45, 0, 0);
	i = (now - origintime) * tpanel1 / xduration;
	printchar(screen, 6+(i<tpanel1?i:tpanel1), 548, 1, -1, 0x404040, 0x404040);
	if (!tpanel2 && !opt_mode) {
		if (gauge > 512) gauge = 512;
		i = (gauge<0 ? 0 : (gauge*400>>9) - (int)(160*startshorten*(1+bottom)) % 40);
		SDL_FillRect(screen, newrect(4,588,i>360?360:i<5?5:i,8), map(0xc00000));
	}

	SDL_Flip(screen);
	return 1;
}

/******************************************************************************/
/* entry point */

static int play(void)
{
	int i;
	char *pos1, *pos2;

	if (initialize()) return 1;

	if (read_bms()) {
		return *bmspath && errormsg("Couldn't load BMS file: %s", bmspath);
	}
	if (v_player == 4) clone_bms();
	if (opt_random) {
		if (v_player == 3) {
			shuffle_bms(opt_random, 0);
		} else {
			shuffle_bms(opt_random, 1);
			if (v_player != 1) shuffle_bms(opt_random, 2);
		}
	}
	get_bms_info();

	/* get basename of bmspath */
	pos1 = strrchr(bmspath, '/');
	pos2 = strrchr(bmspath, '\\');
	if (!pos1) pos1 = bmspath + 1;
	if (!pos2) pos2 = bmspath + 1;
	(pos1>pos2 ? pos1 : pos2)[1] = '\0';
	if (pos1 == pos2) { /* will be pos1 = pos2 = bmspath + 1 */
		bmspath[0] = '.';
		bmspath[1] = sep;
	}

	dirinit();
	play_show_stagefile();
	dirfinal();

	xduration = get_bms_duration();
	play_prepare();
	while ((i = play_process()) > 0);

	if (!opt_mode && i == 0) {
		if (gauge > 150) {
			printf("*** CLEARED! ***\n");
			for (i = 4; i >= 0; --i)
				printf("%-5s %4d    %s", tgradestr[i], scocnt[i], "\n"+(i!=2));
			printf("MAX COMBO %d\nSCORE %07d (max %07d)\n", smaxcombo, score, xscore);
		} else {
			printf("YOU FAILED!\n");
		}
	}

	return 0;
}

static int credit(void)
{
	SDL_Surface *credit;
	const char *s[] = {
		"TokigunStudio Angolmois", "\"the Simple BMS Player\"", VERSION + 10,
		"Original Character Design from", "Project Angolmois", "by", "Choi Kaya (CHKY)", "[ http://angolmois.net/ ]",
		"Programmed & Obfuscated by", "Kang Seonghoon (Tokigun)", "[ http://tokigun.net/ ]",
		"Graphics & Interface Design by", "Kang Seonghoon (Tokigun)",
		"Special Thanks to", "Park J. K. (mono*)", "Park Jiin (Mithrandir)", "Hye-Shik Chang (perky)",
		"Greetings", "Kang Junho (MysticMist)", "Joon-cheol Park (exman)",
		"Jae-kyun Lee (kida)", "Park Byeong-uk (Minan2DJ07)",
		"Park Jaesong (klutzy)", "HanIRC #tokigun, #perky", "ToEZ2DJ.net",
		"Powered by", "SDL, SDL_mixer, SDL_image", "gVim, Python", "and",
		"DemiSoda Apple/Grape", "POCARISWEAT", "Shovel Works",
		"Copyright (c) 2005, Kang Seonghoon (Tokigun).",
		"This program is free software; you can redistribute it and/or",
		"modify it under the terms of the GNU General Public License",
		"as published by the Free Software Foundation; either version 2",
		"of the License, or (at your option) any later version.",
		"for more information, visit http://dev.tokigun.net/angolmois/.",
	};
	char f[] = "DNNTITITTITTIQFFFQFFFFFFFQFFEFFFKKKKKW";
	int y[] = {
		20, 80, 100, 200, 220, 255, 275, 310, 410, 430, 465, 580, 600, 800,
		820, 855, 890, 1000, 1020, 1055, 1090, 1125, 1160, 1195, 1230, 1400,
		1420, 1455, 1490, 1510, 1545, 1580, 2060, 2090, 2110, 2130, 2150, 2180
	};
	int c[] = {0x4040c0, 0x408040, 0x808040, 0x808080, 0x8080c0, 0x80c080, 0xc0c080, 0xc0c0c0};
	int i, t = -750;

	opt_fullscreen = 0;
	if (initialize()) return 1;
	credit = newsurface(SDL_SWSURFACE, 800, 2200);
	SDL_FillRect(credit, 0, 0x000010);
	SDL_FillRect(credit, newrect(0, 1790, 800, 410), 0);
	for (i = 1; i < 16; ++i)
		SDL_FillRect(credit, newrect(0, 1850-i*5, 800, 5), i);
	for (i = 0; i < ARRAYSIZE(s); ++i) {
		printstr(credit, 400-strlen(s[i])*(f[i]%3+1)*4, y[i], f[i]%3+1, s[i], c[f[i]/3-22], 0xffffff);
	}

	SDL_FillRect(screen, 0, 0x000010);
	while (++t < 1820) {
		if (check_exit()) exit(0);
		SDL_BlitSurface(credit, newrect(0,t,800,600), screen, 0);
		SDL_Flip(screen);
		SDL_Delay(20);
	}
	if (t == 1820) {
		SDL_FillRect(screen, newrect(0, 0, 800, 100), 0);
		SDL_Flip(screen);
		t = SDL_GetTicks() + 8000;
		while ((int)SDL_GetTicks() < t) {
			if (check_exit()) exit(0);
		}
	}

	SDL_FreeSurface(credit);
	return 0;
}

int main(int argc, char **argv)
{
	char buf[512]={0};
	int i, j, use_buf;

	if (argc < 2 || !*argv[1]) {
		use_buf = 1;
		if (!filedialog(buf)) use_buf = 0;
	} else {
		use_buf = 0;
	}
	if (argc > 2) {
		for (j = 0; i = argv[2][j]; ++j) {
			if ((i|32) == 'v') opt_mode = 1;
			else if ((i|32) == 'i') opt_showinfo = i&32;
			else if ((i|32) == 'w') opt_fullscreen = !(i&32);
			else if ((i|32) == 'm') opt_random = 1;
			else if ((i|32) == 's') opt_random = (i=='s' ? 2 : 3);
			else if ((i|32) == 'r') opt_random = (i=='r' ? 4 : 5);
		}
	}

	if (argc > 1 || use_buf) {
		bmspath = use_buf ? buf : argv[1];
		return play();
	} else {
		return credit();
	}
}

/* vim: set ts=4 sw=4: */
