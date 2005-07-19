/*
 * Angolmois -- the Simple BMS Player
 * Copyright (c) 2005, Kang Seonghoon (Tokigun).
 * Project Angolmois is copyright (c) 2003-2005, Choi Kaya (CHKY).
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
 *
 *
 * phase 14 (2005-07-20)
 * variables were renamed, optimized under 15,000 bytes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include <SDL_mixer.h>
#include <smpeg.h>

#define Z(n)for(i=0;i<n;i++)

/******************************************************************************/
/* constants, variables */

typedef int _;
typedef struct { double t; _ m,i; } b_;
typedef _ i_[22];
typedef Uint8 *c_;
typedef SDL_Rect *R_;
typedef SDL_Surface *S_;

S_ IMG_Load(c_);
size_t time(size_t*);

c_ L, D[4], A[1296], bi[1296], tS[]={"MISS", "BAD", "GOOD", "GREAT", "COOL"}, _F[2600];
char b[1536], bR[512], Fr[16][96], (*Fz[4])[96]={0,Fr};
double f=130, bb[1296], T[2005], ln, ps=1, E, po=-1, ph=1, Sf;
_ (*bc)[8], bC, bt[1296], pT, pt, pp, pa, xf, xn, xs, xl, gr, PB, Pv=1, Sc, So, SO, ST, SM, Sg=256, t, J, _N, v, V, X, Y;
i_ U={1,0,2,1}, _n, N, P, Q, Pc, Pu, Pb={-1,-1,0,-1}, Sn, o={0,1,1}, I, W, tc, tk,
	tk={122,115,120,100,99,304,308,102,118,109,107,44,108,46,303,307,59,47},
	tC={~0xffff, ~0xff00, ~0x00ff, 0xff00, 0x00ff}, K;
Mix_Chunk *bS[1296];
S_ bI[1296], s, S, ss;
SMPEG *M;
b_ *C[22];
SDL_Rect gR[2];
SDL_AudioSpec AS={44100, MIX_DEFAULT_FORMAT, 2};

/******************************************************************************/
/* general functions */

_ is(_ n){return!(n-9&&n-10&&n-13&&n-32);}
_ sl(c_ s){_ i=0;while(*s++)i++;return i;}
_ rs(c_ s,_*i){while(is(s[*i]))++*i;if(!s[*i])return 0;for(s+=sl(s);is(*--s);*s=0);return 1;}
c_ sc(c_ s){_ i=sl(s)+1;c_ t=malloc(i);for(;*t++=*s++;);return t-i;}
sd(c_ s,c_ t){_ i=0,j=0;for(;*s;s++)if(*s>98)for(j=*s++-97;--j;t++)*t=t[34-*s];else if(*s>34){i|=(*s-35)<<j*6%8;if(j++&3){*t++=i&255;i>>=8;}}}
H(void**x){if(*x)free(*x);*x=0;}

/******************************************************************************/
/* system dependent functions */

c_ jP(c_);

#ifdef WIN32
#include <windows.h>
c_(*_j)(c_)=jP;_ __=92;_i(){}
_o(c_ s){OPENFILENAME o={76,0,0,b,0,0,0,s,512,0,0,0,"Choose a file to play",4};sd("$T)>C+7<PW7@VHY;C/X>X,Z;H$E4LT9<CCE-Q+9>VP&f(H$g(OH%#s5##}]iQ#7'AW8I>G8)<C#|NH$h}H$#6R\\Y<Q`)@H$}NO$hzO$33OT)geV$ffM#fdM###",b);GetOpenFileName(&o);}
#else
#include <dirent.h>
_ __=47;_o(c_ s){}
_i(){DIR*d;struct dirent*e;_ i=0,j=0;for(;L[i];j=L[i++]-__?j:i);L[j-1]*=j<0;if(d=opendir(j?L:".")){while(e=readdir(d))_F[_N++]=sc(e->d_name);closedir(d);}if(j>0)L[j-1]=__;}
c_ _j(c_ p){_ i;Z(_N){c_ a=_F[i],b=p;while(*a&&*b&&(64<*a&&*a<91?*a|32:*a)==(64<*b&&*b<91?*b|32:*b))a++,b++;if(*a==*b)return jP(_F[i]);}return 0;}
#endif

c_ jP(c_ p){_ i=0,j=0;for(;*p-47&&*p-92&&L[i];i++)j=(bR[i]=L[i])==__?i+1:j;for(;*p;p++)bR[j++]=*p-47&&*p-92?*p:__;bR[j]=0;return bR;}

/******************************************************************************/
/* bms parser */

_ ki(c_ s){_ a;sprintf(b,"%.2s",s);a=strtol(b,&s,36);return*s?-1:a;}
_ BL(const void*a,const void*b){c_ A=*(c_*)a,B=*(c_*)b;_ i=0,j;for(;*A&&*B;i++)if(j=*A++-*B++)return i<6?j:-j;return*B-*A;}
_ BN(const void*a,const void*b){b_*A=(b_*)a,*B=(b_*)b;return A->t>B->t?1:A->t<B->t?-1:A->m-B->m;}
B(_ i,double t,_ m,_ j){b_ x={t,m,j};N[i]++[C[i]=realloc(C[i],sizeof(b_)*(N[i]+2))]=x;}
Br(_ i,_ j){if(i<18&&C[i][j].i)B(18,C[i][j].t,0,C[i][j].i);C[i][j].m=-1;}

_ Bp() {
	FILE* o = fopen(L, "r");
	_ i, j, k, a, b, c, r=1, g=1, x, y, n=0;
	i_ p={0}, q={0};
	double t;
	char d[4096];
	c_ *m=0, s=d+1, z, h[]={"title", "genre", "artist", "stagefile", "player", "playlevel", "rank", "lntype", "bpm", "lnobj", "wav", "bmp", "bga", "stop", "stp", "random", "if", "else", "endif"};
	
	Z(4) *(D[i]=malloc(1))=0;

	if(!o) return 1;
	for(_i(); fgets(d, 4096, o); )
		if(*d == 35) {
			Z(19) {
				for(j=0; h[i][j] && (h[i][j]|32)==(s[j]|32); j++);
				if(!h[i][j]) break;
			}
			x = is(s[j]);
			r = i==15 && x && (j=abs(atoi(s+j))) ? rand()%j+1 : r;
			if(g = i-16 || !x ? i-17 ? i-18 ? g : 1 : !g : r==atoi(s+j)) {
				if(i<4 && rs(s, &j)) { H(D+i); D[i] = sc(s+j); }
				if(i/4==1 && x) U[i-4] = atoi(s+j);
				if(i==8) *(x ? &f : (k=ki(s+j))+1 && is(s[j+=2]) ? bb+k : &t) = atof(s+j);
				if(i==9 && x && rs(s, &j)) U[4] = ki(s+j);
				if(i/2==5 && (k=ki(s+j))+1) {
					j += 2;
					if(rs(s, &j)) {
						c_ *x = k + (i%2 ? bi : A);
						H(x); *x = sc(s+j);
					}
				}
				if(i==12) {
					k = bC;
					k[bc = realloc(bc, sizeof(_) * 8 * (k+1))][0] = ki(s+j);
					if(is(s[j+=2]) && rs(s, &j)) {
						bc[k][1] = ki(s+j);
						z = s + j + 2;
						for(j=2; *z && j<8; k+=++j>7)
							bc[k][j] = strtol(z, &z, 10);
					}
					bC-k ? (bC = k) : (bc[k][0] = -1);
				}
				if(i==13 && (k=ki(s+j))+1 && is(s[j+=2])) bt[k] = atoi(s+j);
				if(i==14 && sscanf(s+j, "%d.%d %d", &i, &j, &k)>2) B(21, i+j/1e3, 1, k);
				
				strtol(s, &z, 10);
				if(s+5==z && s[5]==58 && s[6])
					n++[m = realloc(m, sizeof(c_) * (n+1))] = sc(s);
			}
		}
	fclose(o);

	qsort(m, n, sizeof(c_), BL);
	Z(n) {
		x = atoi(*m); y = x % 100; x /= 100;
		if(y - 2) {
			j = 6;
			rs(*m, &j);
			a = (sl(*m) - j) / 2;
			for(k=0; k<a; k++,j+=2) {
				c = 8 + y%10 - y/10%2*9;
				t = x + 1. * k / a;
				b = ki(*m+j);
				if(b > 0) {
					if(y<10 && !is(c=" 6 <9 ;:=?"[y]))
						B(c/3, t, c%3, y-3 ? b : b/36*16+b%36);
					if(y%10 && y>9 && y<30)
						if(!U[4] || b-U[4])
							B(c, t, 0, b);
						else if(N[c] && !C[c][N[c]-1].m)
							C[c][N[c]-1].m = 3, B(c, t, 2, b);
					if(y%10 && y>29 && y<50) B(c, t, 1, b);
				}
				if(y%10 && y>49 && y<70) {
					if(U[3] == 1 && b) {
						B(c, t, 2 + !p[c], b *= !p[c]);
						p[c] = b;
					}
					if(U[3] == 2 && (p[c] || p[c]-b)) {
						if(p[c])
							if(q[c]+1 < x) B(c, q[c]+1, 2, 0);
							else if(p[c] - b) B(c, t, 2, 0);
						if(b && (p[c]-b || q[c]+1<x)) B(c, t, 3, b);
						q[c] = x; p[c] = b;
					}
				}
			}
		} else T[x+1] = atof(*m+6);
		H(m++);
	}
	free(m-n);
	ln = x + 2;
	Z(18) if(p[i]) B(i, U[3]==2 && q[i]+1<x ? q[i]+1 : ln-1, 2, 0);
	
	Z(22)
		if(C[i]) {
			qsort(C[i], N[i], sizeof(b_), BN);
			if(i-18 && i<21) {
				t = -1;
				for(b=j=0; j<=N[i]; a|=1<<C[i][j++].m)
					if(j == N[i] || C[i][j].t > t) {
						if(t >= 0) {
							for(c=0; k<j; k++) {
								r = 1<<C[i][k].m;
								if(i<18 ? (c&r) || (b ? !(a&4) || r<4 : r-((a&12)-8 ? a&1 ? 1 : 2 : 8)) : i-19 ? c : c&r)
									Br(i, k);
								else
									c |= r;
							}
							b = b ? (a&12)-4 : (a&12)==8;
						}
						a = 0;
						t = C[i][k=j].t;
					}
				if(i<18 && b) {
					while(j >= 0 && C[i][--j].t<0);
					if(j >= 0 && C[i][j].m == 3) Br(i, j);
				}
				for(j=k=0; j<N[i]; j++)
					if(C[i][j].m<0) k++; else C[i][j-k] = C[i][j];
				N[i] -= k;
			}
		}

	Z(2005) if(T[i] < .01) T[i] = 1;

	return 0;
}

_ jp(double t, double u) {
	_ i=(_)(t+1), j=(_)(u+1);
	for(t=(u-j+1)*T[j]-(t-i+1)*T[i]; i<j; t+=T[i++]);
	return (_)(E ? 530 - 400 * ps * t : t * 24e7 / f);
}

/******************************************************************************/
/* general graphic functions */

Uint32*gg(S_ s,_ x,_ y){return(Uint32*)s->pixels+x+y*s->pitch/4;}
_ gb(_ x,_ y,_ a,_ b){_ i=0;for(;i<24;i+=8)y+=((x>>i&255)-(y>>i&255))*a/b<<i;return y;}
S_ gs(_ w,_ h){return SDL_CreateRGBSurface(0,w,h,32,255<<16,65280,255,0);}
R_ R(_ x,_ y,_ w,_ h){R_ r=gR+gr++%2;r->x=x;r->y=y;r->w=w;r->h=h;return r;}
G(S_ c,R_ r,S_ d,_ x,_ y){d?SDL_BlitSurface(c,r,d,R(x,y,0,0)):c?y?SDL_SetColorKey(c,20480,0):SDL_FillRect(c,r,x):SDL_SetClipRect(s,r);}
_ gi(_ x,_ y){return((x<y?y*y-2*x*x+x*x/y*x:x<y*2?4*y*y-8*x*y+5*x*x-x*x/y*x:0)<<11)/y/y;}
gf(S_ x){if(x)SDL_FreeSurface(x);}
_ O(_ n){return n?n>1?!M||SMPEG_status(M)>0:SDL_Flip(s):SDL_GetTicks();}
S_ gl(c_ c){S_ a=IMG_Load(_j(c)),b;b=a?SDL_DisplayFormat(a):0;gf(a);return b;}
_ gv(){SDL_Event x;return SDL_PollEvent(&x)?(V=x.type,v=x.key.keysym.sym,1):0;}

/******************************************************************************/
/* font functions */

F_(_ x, _ y, _ z, _ c) {
	_ i, j=0;
	if(!is(c))
		for(c-=c<0?-96:c<33||c>126?c:32; j<16*z; j++)
			Z(8*z)
				if(Fz[z][j*z+i%z][c]&1<<(7-i/z))
					*gg(s, x+i, y+j) = gb(X, Y, j, 16*z-1);
}

F(_ x, _ y, _ z, c_ c) {
	for(x-=z/4*4*sl(c); *c; x+=z%4*8)
		F_(x,y,z%4,*c++);
}

/******************************************************************************/
/* main routines */

_ xx() {
	_ i = 0;
	while(gv()) i |= V-12 ? V==3 && v==27 : 1;
	return i;
}

void mf(void) {
	_ i;
 
	Z(1296) {
		H(A+i); H(bi+i); gf(bI[i]);
		if(bS[i]) Mix_FreeChunk(bS[i]);
	}
	Z(4) H(D+i);
	Z(22) H(C+i);
	Z(2) H((void**)Fz+i+2);
	free(bc);

	gf(S); gf(ss);
	if(M) { SMPEG_stop(M); SMPEG_delete(M); }
	Mix_HookMusic(0, 0);
	Mix_CloseAudio();
	while(_N--) H(_F+_N);
	SDL_Quit();
}

_ mi() {
	_ z, i, j, k, l, m, n, p;
	i_ a={218,63,434,63,155,504,182,504,16},c={10,11,34,38,136,200,160,416,16};

	if(SDL_Init(48)<0 || !(s = SDL_SetVideoMode(800, 600, 32, o[2]<<31)) || Mix_OpenAudio(44100, AS.format, 2, 2048)<0) return 1;
	SDL_ShowCursor(0);
	SDL_WM_SetCaption("TokigunStudio Angolmois", 0);
	atexit(mf);

	sd("##k#;C$)i/%;#&;#&;#$l=jBS#lPa$n<>RPB;[ZY>&jbfma$lqeog#;S#hte+g#jq[S)>OPV=Q<I>^#iRe=3#j~#C#%aOY=K\\V%NPY=aC#%fuC(Y;)#evF0W#grY#e#G#kQ##C$Y?F2Y$e%Y#&gqb('m#b$fy;S&e#;C$#;#e&gdC$&)/#ftC$hha/Y;F$e1eBjtf>fFi2eza$g%iBhkkql2S#krfvF\\&hrg.b/#n2C\\*mBb0S#)S#evjr1[D0I$e#b<C$iBC$);a0#obJ\\#*[#*;b$kr&[$qBRPZ?kR?S&./#g#b#hrF0Y;VPZ>J$e)iBY#e4e7YS$hrC$j#b$iRZ`ZBNP)fFhr&#g#eha#irIT)AS$*AO<)irfpb$guhraD$i#a$hrb$e~a$e&C$)hrA/V;C$Y<F0Y/A#hr_<)g_F<)BqRb$ire~f&F$irgbC$)kba'7:8$e#A$G2hr+SD0esb$ewj0$$h{r(b$i9#$)/;SC$%#jG_S#i#_#iI&/S$1S$1S$)iR_#&i#iB$0Y@aS$*a?Z;$$h~$1O<_C$g#htF$h#YS$%l2NPY=b<F0hra$hOrRF0I2iBfcg'hra/Y;C\\V#&#qBC$e#kBeeN`I2*/#hshdkbjDjb?S#j#jBY`Z=e#jR&#ei&#e#jTC$Y;IT)A[TI<hrC$);a$hriR;C$e%g$hr1OT);[*m2##S2fRb#esgre~g3F`&jBa#erb$)h@###;C$IBgBa$mBC$)i`##&)/;#j{jbhtirSHbJ1#qCikaT&)r#eQf61#f;S$jyb@C&?C&?C`*grh=gGjx$0I0?SD0F('j~F$e#V[$&;#&;l2eE+#mBNPY=b<&hb;CDBeu>OT)krgRF0I2jra0Y;C$f#jba/);a/S#oBfUa$jEb#hcb/S#&#", b);
	Z(1536) Fr[i%16][i/16^14] = b[i];
	for(z=2; z<4; z++) {
		Fz[z] = malloc(1536*z*z);
		Z(1536*z*z) i[(c_)Fz[z]] = 0;
		Z(96)
			for(j=0; j<16; j++)
				for(k=0; k<8; k++) {
					l = 0;
					for(m=j-1; m<j+2; m++)
						l = l<<3 | (m>>4 ? 0 : Fr[m][i]<<k>>6&7);
					l |= !(i-3 && i-20) && (k+6)&4 ? (l&146)/2*5 : 0;
					for(m=0; m<9; m++)
						for(n=0; (l&a[m])==c[m] && n<z; n++)
							for(p=m/2?m/2-2?0:n+1:z-n; p<(m/2-1?m/2-3?z:z-n-1:n); p++)
								Fz[z][(j*z+n)*z+p][i] |= 1<<(7-k);
				}
	}
	return 0;
}

mS(c_ p) {
	if(o[1]) {
		G(ss, R(0,0,800,20), s, 0, 580);
		F(797, 582, 9, p);
		O(1);
	}
}

mg(_ v) {
	Sn[SM = v]++;
	PB = v ? PB : t+600;
	ST = t+700;
	Sg += "+2789"[v] - 55;
	SO += v>2 ? ++So>SO : 0;
	So *= v>1;
}

_ mp(_ x, _ y){
	_ z = bS[x] ? Mix_PlayChannel(-1, bS[x], 0) : -1;
	z<0 || Mix_GroupChannel(z,y);
	return z;
}

/******************************************************************************/
/* entry point */

_ main(_ c, char **cc) {
	S_ n;
	char b[512]="";
	_ *h, i, j, k, l, m, q, r, d, x, z;
	double p=-1, e, g;

	Z(8) K[i] = i * 0x202020;

	if(c < 2 || !*cc[1]) _o(b);
	ps = c>2 && (e=atof(cc[2]))>0 ? e<.1 ? .1 : e>99 ? 99 : e : 1;
	if(c > 3)
		for(j=0; i=cc[3][j]; j++) {
			q = i|32; r = i&32;
			*o |= q == 'v';
			if(q == 'i') o[1] = r;
			if(q == 'w') o[2] = !r;
			o[3] = q-'m' ? q-'s' ? q-'r' ? o[3] : 4+!r : 2+!r : 1;
			if(i/3 == 16) U[3] = i - 48;
			o[4] |= j == 42 && i == 42;
		}
	for(q=r=0; --c>3; )
		if(i = (c<22) * atoi(cc[c])) tk[c-4] = i;
	if(o[4]) return 2;

	srand(time(0));
	if(c < 2 && !*b) return 0;
	L = *b ? b : cc[1];
	if(mi() || Bp()) return 1;

	xf = 13 - !(N[7]+N[8]+N[16]+N[17]) - 4*!(N[6]+N[15]) - 8*!N[20];
	Z(18)
		for(j=0; j<N[i]; j++)
			xf |= (C[i][j].m > 1) * 2, xn += C[i][j].m < 3;
	Z(xn) xs += (_)(300+300.*i/xn);

	Z(18)
		W[i] = (i+2)%9>6 ? 40 : 25,
		tc[i] = K[4] | 255 << 16 >> (i%9-5 ? i%9-6 ? 24-i%9%2*8 : 8 : 0);
	Z(18) {
		j = "E@ABCDGHFOIJKLMPQN"[i] - 64;
		if((j<9 || *U>1) && (j%9-6 ? j%9<7 ? 1 : xf&1 : xf&4)) {
			I[j] = J += j==9; J += W[j] + 1;
		} else I[j] = -1;
	}
	
	if(d = o[3]) {
		b_ *s, *l[18]={0};
		i_ n={0}, m, p, q={0}, u={0}, z, v, w;

		j = 0;
		Z(18) m[i] = I[i]>=0 && !(d-3 && d-5 && (i+2)%9>6) ? i : -1, p[i] = i;
		Z(18) m[i]<0 ? j++ : (m[i-j] = m[i]);
		x = 18 - j;

		if(d < 4)
			for(i=x,j=0; --i>j; j*=d<2) {
				s = C[m[i]]; C[m[i]] = C[m[j = d<2 ? x-i-1 : rand()%i]]; C[m[j]] = s;
				k = N[m[i]]; N[m[i]] = N[m[j]]; N[m[j]] = k;
			}
		else
			for(r=1; r; ) {
				e = 9e9; r = 0;
				for(i=x-1; i>=0; i--) {
					if(i) {
						k = p[i]; p[i] = p[j=rand()%i]; p[j] = k;
					}
					if(z[i] = q[m[i]] < N[m[i]])
						r = 1, e = (g=C[m[i]][q[m[i]]].t)<e ? g : e;
				}
				Z(x)
					if(z[i] && (s=C[m[i]]+q[m[i]])->t < e + 1e-9) {
						k = s->m;
						if(k-2) {
							for(j=p[i]; u[j]; j=p[w[j]]);
							if(k == 3) v[i]=j, w[j]=i, u[j]=1;
						} else u[j = v[i]] = 2;
						n[j]++[l[j] = realloc(l[j], sizeof(b_) * (n[j]+2))] = *s;
						q[m[i]]++;
					}
				Z(x)
					if(r) u[i] *= u[i] != 2;
					else { H(C+m[i]); C[m[i]] = l[i]; N[m[i]] = n[i]; }
			}
	}

	X = K[1]; Y = K[4];
	F(320, 284, 2, "loading...");
	O(1);

	ss = gs(800, 20);
	if(*(D[3]) && (n = gl(D[3]))) {
		_ x, y, u, v, w=s->w-1, h=s->h-1, l, r, g, b, a, c, p[4], q;

		for(i=u=x=0; i<=w; i++) {
			for(k=x; k<x+4; k++)
				p[k-x] = gi(abs(k*w+i-w-i*n->w), w);
			for(j=v=y=0; j<=h; j++) {
				r = g = b = 0;
				for(l=y; l<y+4; l++) {
					q = gi(abs(l*h+j-h-j*n->h), h);
					for(k=x; k<x+4; k++)
						if(k>0 && k<=n->w && l>0 && l<=n->h) {
							c = *gg(n, k-1, l-1); a = p[k-x] * q >> 6;
							r += (c&255) * a; g += (c>>8&255) * a; b += (c>>16&255) * a;
						}
				}
				*gg(s, i, j) =
					(r<0 ? 0 : r>>24 ? 255 : r>>16) |
					(g<0 ? 0 : g>>24 ? 255 : g>>16)<<8 |
					(b<0 ? 0 : b>>24 ? 255 : b>>16)<<16;
				v += n->h - 1; y += v/h; v %= h;
			}
			u += n->w - 1; x += u/w; u %= w;
		}
		gf(n);
	}
	X = Y; Y = ~0;
	if(o[1]) {
		Z(600)
			if(i<42 || i>579)
				for(j=0; j<800; *h=0xc0c0c+((*h&~0x30303)>>2)) h = gg(s,j++,i);
		F(6, 4, 2, *D);
		F(792, 4, 9, D[1]);
		F(792, 20, 9, D[2]);
		sprintf(b, "Level %d | BPM %.2f%s | %d note%s [%dKEY%s]", U[1], f, "?"+!(xf&8), xn, "s"+(xn==1), (xf&1 ? 7 : 5) << (*U>1), xf&2 ? "-LN" : "");
		F(3, 582, 1, b);
		G(s, R(0,580,800,20), ss, 0, 0);
	}
	O(1);
	k = O(0) + 3000;

	Y = K[6];
	Z(1296) {
		if(xx()) return 0;
		if(A[i]) {
			mS(A[i]);
			bS[i] = Mix_LoadWAV(_j(A[i]));
			for(j=sl(A[i]),q=0; q<4 && "3pm."[q]==A[i][--j]; q++);
			if(q<4) H(A+i);
		}
		if(bi[i]) {
			mS(bi[i]);
			if(n = gl(bi[i])) G(bI[i] = n, 0, 0, 0, 1);
			H(bi+i);
		}
	}
	
	mS("loading...");
	Z(bC)
		if(1[h=bc[i]]>=0 && *h>=0 && bI[h[1]]) {
			n = bI[*h];
			if(!n) G(bI[*h] = n = gs(256, 256), 0, 0, 0, 1);
			h[2] *= h[2] > 0; h[3] *= h[3] > 0;
			G(bI[h[1]], R(h[2], h[3], h[4]-h[2], h[5]-h[3]), n, h[6], h[7]);
		}

	while(O(0) < k && !xx());
	for(e=f; i>=0; ) {
		for(i=-1,j=0; j<22; j++)
			i = P[j] < N[j] && (i < 0 || C[j][P[j]].t < C[i][P[i]].t) ? j : i;
		q += jp(p, i<0 ? ln : C[i][P[i]].t);
		if(i >= 0) {
			j = C[i][P[i]].i;
			d = (_)(i<19 && bS[j] ? bS[j]->alen / .1764 : 0);
			if(i==20 && (g=C[i][P[i]].m?bb[j]:j) && (f=g<0?-g:g)-g) {
				q += jp(-1, p); break;
			}
			d += q += i-21 ? 0 : C[i][P[i]].m ? j : (_)(bt[j] * 125e4 * T[(_)(p+1)] / f);
			r = r < d ? d : r;
			p = C[i][P[i]++].t;
		}
	}
	f = e;
	xl = (q > r ? q : r) / 1000;

	S = gs(1200, 600);
	Z(18)
		if(I[i] >= 0) {
			for(j=0; j<380; j++)
				G(S, R(I[i],j+140,W[i],1), 0, gb(tc[i], 0, j, 1000), 0);
			for(j=0; i<9 && j*2<W[i]; j++)
				G(S, R(800+I[i]+j,0,W[i]-2*j,600), 0, gb(~0, tc[i], j, W[i]), 0);
		}
	for(j=0; j<800; j++) {
		Z(30) *gg(S, j, i) = gb(K[6], K[3], 850 - abs((i*2+j*3) % 2000 - 1000), 700);
		Z(80) *gg(S, j, i+520) = gb(K[6], K[2], 850 - abs((i*3+j*2+200) % 2000 - 1000), 700);
	}
	G(S, R(J+20,0,780-J,600), 0, 0, 0);
	Z(20)
		for(j=20; j*j+i*i>400; j--)
			*gg(S, J+j, i+10) = *gg(S, J+j, 539-i) = 0;
	if(!*o)
		G(S, R(0,584,368,16), 0, K[2], 0),
		G(S, R(4,588,360,8), 0, 0, 0);
	G(S, R(10,564,J,1), 0, K[2], 0);
	G(s, 0, 0, 0, 0);

	pT = pt = O(0);
	E = ps;
	Mix_AllocateChannels(128);
	Z(22) P[i]=0;
	Sf = 15e4 - U[2] / 25e3;

	for(;; O(1)) {
		double x, y, u;

		if(pa) {
			u = E - ps;
			if(pa = (_)(u * 1e3))
				ps += u * .015;
			else {
				ps = E;
				Z(22) Q[i] = P[i];
			}
		}

		t = O(0);
		x = po + (t<pp ? 0 : pp ? (pt=t, pp=0) : (t-pt)*f/ph/24e4);
		z = (_)(x + 1);
		if(x > -1 && ph - T[z]) {
			pt += (_)((z - po - 1) * 24e4 * ph / f);
			po = z - 1;
			ph = T[z];
		}

		y = 1.25 / ps;
		u = i = z;
		if((z - x) * T[z] > y)
			u = x;
		else
			for(y-=(i-x)*T[i]; T[++i]<=y; y-=T[i]) u = i;
		y = u + y / T[i];
		Z(22) {
			for(; Q[i]<N[i] && C[i][Q[i]].t<=y; Q[i]++);
			for(; P[i]<N[i] && C[i][P[i]].t<x; P[i]++) {
				j = C[i][P[i]].i;
				k = C[i][P[i]].m;
				if(i == 18) {
					mp(j, 1);
					if(A[j] && O(2)) {
						if(M) {
							Mix_HookMusic(0, 0);
							SMPEG_delete(M);
						}
						if(M = SMPEG_new(_j(A[j]), 0, 0)) {
							SMPEG_actualSpec(M, &AS);
							Mix_HookMusic(SMPEG_playAudioSDL, M);
							SMPEG_enableaudio(M, 1);
							SMPEG_play(M);
						}
					}
				}
				if(i == 19)
					Pb[k] = j, Pv = 1;
				if(i == 20 && (u = k ? bb[j] : j))
					pt = t, po = x, f = u;
				if(i == 21)
					pp = (t<pp ? pp : t) + (_)(k ? j : bt[j] * 1250 / f), po = x;
				if(i<18 && *o && k-1) mp(j, 1);
			}
			for(; i<18 && !*o && Pc[i]<P[i]; Pc[i]++)
				if((j=C[i][Pc[i]].m)>=0 && j-1 && (j-2 || Pu[i])) {
					u = C[i][Pc[i]].t;
					if((x - u) * T[(_)(u+1)] / f * Sf > 60) mg(0); else break;
				}
		}

		while(gv()) {
			if(V-12 ? V==3 && v==27 : 1) return 0;
			if(V==2 && v/2==142)
				pa=1,E+=v%2?
					E<1?.2:E<5?.5:E<10?1:E<95?5:0:
					E>10?-5:E>5?-1:E>1?-.5:E>.201?-.2:0;
			if(!*o)
				Z(18)
					if(I[i] >= 0 && v == tk[i]) {
						if(V==3 && Pu[i]) {
							Pu[i] = 0;
							for(j=P[i]; C[i][j].m-2; j--);
							u = (C[i][j].t - x) * T[z] / f * Sf;
							if(-60 < u && u < 60) C[i][j].m ^= -1; else mg(0);
						}
						if(V==2 && N[i]) {
							j = P[i];
							j = j - !(j<1 || (j<N[i] && C[i][j-1].t+C[i][j].t<2*x));
							for(m=k=j<P[i]?-1:1; m && C[i][j].m==1; m=j>=0&&j<N[i]) j += k;
							if(m) {
								u = (C[i][j].t - x) * T[z] / f * Sf;
								k = C[i][j].m;
								if(j<N[i] && k==2) mg(0);
								if(m = k-2 && k>=0 && (u = u<0 ? -u : u)<60) {
									Pu[i] = (C[i][j].m = ~k) < -3;
									mg(u<6 ? 4 : u<20 ? 3 : u<35 ? 2 : 1);
									Sc += (_)((60 - u) * (5 + 5. * So / xn));
								}
							}
							mp(C[i][j].i, !!m);
						}
					}
		}
		if(x>ln ? O(2) && Mix_GroupNewer(1)<0 : x<-1) {
			if(!*o)
				if(Sg > 150) {
					puts("*** CLEARED! ***");
					Z(5) printf("%-5s%5d   %c", tS[4-i], Sn[4-i], "  \n  "[i]);
					printf("MAX COMBO %d\nSCORE %07d (max %07d)\n", SO, Sc, xs);
				} else puts("YOU FAILED!");
			return 0;
		}

		G(s, R(0,30,J,490), 0, K[2], 0);
		G(0, R(0,30,800,490), 0, 0, 0);
		Z(18)
			if(I[i] >= 0) {
				G(s, R(I[i],30,W[i],490), 0, 0, 0);
				if(!*o && SDL_GetKeyState(0)[tk[i]])
					G(S, R(I[i],140,W[i],380), s, I[i], 140);
				for(j=P[i]; j<Q[i]; j++) {
					k = jp(x, C[i][j].t) - 5;
					m = C[i][j].m;
					m ^= -(m<0);
					if(m - 1) {
						l = m-2 ? k+5 : 520;
						k = m ? m-2 ? jp(x, C[i][++j].t) : k+5 : k;
						k = k<30 ? 30 : k;
						if(l>k) G(S, R(800+I[i%9],0,W[i%9],l-k), s, I[i], k);
					}
				}
				if(P[i]==Q[i] && Q[i]<N[i] && C[i][Q[i]].m==2)
					G(S, R(800+I[i%9],0,W[i%9],490), s, I[i], 30);
			}
		for(i=z-1; i<y; G(s, R(0,jp(x,i++),J,1), 0, K[6], 0));
		if(t < ST) {
			Y = (X = tC[SM] | K[2]) | K[4];
			F(J/2, 292, 10, tS[SM]);
			if(So > 1) {
				X = K[4]; Y = ~0;
				sprintf(b, "%d COMBO", So);
				F(J/2, 320, 5, b);
			}
			Pv = SM ? Pv : 1;
		}
		G(0, 0, 0, 0, 0);
		if(Pv > 0 || (Pv < 0 && t >= PB)) {
			G(s, R(272+J/2,172,256,256), 0, 0, 0);
			Z(4)
				if(i/2 == (t<PB) && Pb[i] >= 0 && bI[Pb[i]])
					G(bI[Pb[i]], R(0,0,256,256), s, 272+J/2, 172);
			Pv = -(t < PB);
		}

		i = (t - pT) / 1000; j = xl / 1000;
		sprintf(b, "SCORE %07d%c%4.1fx%c%02d:%02d / %02d:%02d%c@%9.4f%cBPM %6.2f",
				Sc, 0, E, 0, i/60, i%60, j/60, j%60, 0, x, 0, f);
		G(S, R(0,0,800,30), s, 0, 0);
		G(S, R(0,520,800,80), s, 0, 520);
		X = Y = 0;
		F(10, 8, 1, b);
		F(5, 522, 2, b+14);
		F(95, 538, 1, b+34);
		F(95, 522, 1, b+45);
		Y = K[2];
		F(J-94, 565, 1, b+20);
		X = Y;
		F_(6+(t-pT<xl ? (t-pT)*J/xl : J), 548, 1, -1);
		if(!*o) {
			i = (Sg = Sg>512 ? 512 : Sg)<0 ? 0 : (Sg*400>>9) - (_)(160*ph*(1+x)) % 40;
			G(s, R(4,588,i>360?360:i<5?5:i,8), 0, 0xc00000, 0);
		}
	}
}

/* vim: set ts=4 sw=4: */
