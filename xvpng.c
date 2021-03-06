/*
 * xvpng.c - load and write routines for 'PNG' format pictures
 *
 * callable functions
 *
 *    CreatePNGW()
 *    PNGDialog(vis)
 *    PNGCheckEvent(xev)
 *    PNGSaveParams(fname, col)
 *    LoadPNG(fname, pinfo)
 */

/*#include "copyright.h"*/
/* (c) 1995 by Alexander Lehmann <lehmann@mathematik.th-darmstadt.de>
 *   this file is a suplement to xv and is supplied under the same copying
 *   conditions (except the shareware part)
 * Modified by Andreas Dilger <adilger@enel.ucalgary.ca> to fix
 *   error handling for bad PNGs, add dialogs for interlacing and
 *   compression selection, and upgrade to libpng-0.89
 * The copyright will be passed on to JB at some future point if he
 * so desires.
 */

#include "xv.h"

#ifdef HAVE_PNG

#include "png.h"

/*** Stuff for PNG Dialog box ***/
#define PWIDE 318
#define PHIGH 215

#define DISPLAY_GAMMA 2.20  /* Default display gamma */
/* Default zlib compression level
#define COMPRESSION   Z_BEST_COMPRESSION
*/
#define COMPRESSION   6

#define DWIDE     86
#define DHIGH    104
#define PFX PWIDE-93
#define PFY       44
#define PFH       20

#define P_BOK    0
#define P_BCANC  1
#define P_NBUTTS 2

#define BUTTH    24

#define LF       10   /* a.k.a. '\n' on ASCII machines */
#define CR       13   /* a.k.a. '\r' on ASCII machines */

/*** local functions ***/
static    void drawPD         PARM((int, int, int, int));
static    void clickPD        PARM((int, int));
static    void doCmd          PARM((int));
static    void writePNG       PARM((void));
static    int  WritePNG       PARM((FILE *, byte *, int, int, int,
                                    byte *, byte *, byte *, int));

static    void png_xv_error   PARM((png_structp png_ptr,
                                    png_const_charp message));
static    void png_xv_warning PARM((png_structp png_ptr,
                                    png_const_charp message));

/*** local variables ***/
static char *filename;
static char *fbasename;
static int   colorType;
static int   read_anything;
static double Display_Gamma = DISPLAY_GAMMA;

static DIAL  cDial, gDial;
static BUTT  pbut[P_NBUTTS];
static CBUTT interCB;
static CBUTT FdefCB, FnoneCB, FsubCB, FupCB, FavgCB, FPaethCB;

/**************************************************************************/
/* PNG SAVE DIALOG ROUTINES ***********************************************/
/**************************************************************************/


#include <zlib.h>


/*******************************************/
void CreatePNGW()
{
  pngW = CreateWindow("xv png", "XVPNG", NULL,
                      PWIDE, PHIGH, infofg, infobg, 0);
  if (!pngW) FatalError("can't create PNG window!");

  XSelectInput(theDisp, pngW, ExposureMask | ButtonPressMask | KeyPressMask);

  DCreate(&cDial, pngW,  12, 25, DWIDE, DHIGH, (double)Z_NO_COMPRESSION,
          (double)Z_BEST_COMPRESSION, COMPRESSION, 1.0, 3.0,
          infofg, infobg, hicol, locol, "Compression", NULL);

  DCreate(&gDial, pngW, DWIDE+27, 25, DWIDE, DHIGH, 1.0, 3.5,DISPLAY_GAMMA,0.01,0.2,
          infofg, infobg, hicol, locol, "Disp. Gamma", NULL);

  CBCreate(&interCB, pngW,  DWIDE+30, DHIGH+3*LINEHIGH+2, "interlace",
           infofg, infobg, hicol, locol);

  CBCreate(&FdefCB,   pngW, PFX, PFY, "Default",
           infofg, infobg, hicol, locol);
  FdefCB.val = 1;

  CBCreate(&FnoneCB,  pngW, PFX, FdefCB.y + PFH + 4, "none",
           infofg, infobg, hicol, locol);
  CBCreate(&FsubCB,   pngW, PFX, FnoneCB.y + PFH, "sub",
           infofg, infobg, hicol, locol);
  CBCreate(&FupCB,    pngW, PFX, FsubCB.y  + PFH, "up",
           infofg, infobg, hicol, locol);
  CBCreate(&FavgCB,   pngW, PFX, FupCB.y   + PFH, "average",
           infofg, infobg, hicol, locol);
  CBCreate(&FPaethCB, pngW, PFX, FavgCB.y  + PFH, "Paeth",
           infofg, infobg, hicol, locol);

  FnoneCB.val = FsubCB.val = FupCB.val = FavgCB.val = FPaethCB.val = 1;
  CBSetActive(&FnoneCB, !FdefCB.val);
  CBSetActive(&FsubCB, !FdefCB.val);
  CBSetActive(&FupCB, !FdefCB.val);
  CBSetActive(&FavgCB, !FdefCB.val);
  CBSetActive(&FPaethCB, !FdefCB.val);

  BTCreate(&pbut[P_BOK], pngW, PWIDE-180-1, PHIGH-10-BUTTH-1, 80, BUTTH,
          "Ok", infofg, infobg, hicol, locol);
  BTCreate(&pbut[P_BCANC], pngW, PWIDE-90-1, PHIGH-10-BUTTH-1, 80, BUTTH,
          "Cancel", infofg, infobg, hicol, locol);

  XMapSubwindows(theDisp, pngW);          
}


/*******************************************/
void PNGDialog(vis)
     int vis;
{
  if (vis) {
    CenterMapWindow(pngW, pbut[P_BOK].x + (int) pbut[P_BOK].w/2,
                          pbut[P_BOK].y + (int) pbut[P_BOK].h/2,
                    PWIDE, PHIGH);
  }
  else XUnmapWindow(theDisp, pngW);
  pngUp = vis;
}


/*******************************************/
int PNGCheckEvent(xev)
     XEvent *xev;
{
  /* check event to see if it's for one of our subwindows.  If it is,
     deal accordingly, and return '1'.  Otherwise, return '0' */

  int rv;
  rv = 1;

  if (!pngUp) return 0;

  if (xev->type == Expose) {
    int x,y,w,h;
    XExposeEvent *e = (XExposeEvent *) xev;
    x = e->x; y = e->y; w = e->width; h = e->height;

    /* throw away excess expose events for 'dumb' windows */
    if (e->count > 0 && (e->window == cDial.win)) {}

    else if (e->window == pngW)        drawPD(x, y, w, h);
    else if (e->window == cDial.win)   DRedraw(&cDial);
    else if (e->window == gDial.win)   DRedraw(&gDial);
    else rv = 0;
  }

  else if (xev->type == ButtonPress) {
    XButtonEvent *e = (XButtonEvent *) xev;
    int x,y;
    x = e->x;  y = e->y;
    
    if (e->button == Button1) {
      if      (e->window == pngW)       clickPD(x,y);
      else if (e->window == cDial.win)  DTrack(&cDial,x,y);
      else if (e->window == gDial.win)  DTrack(&gDial,x,y);
      else rv = 0;
    }  /* button1 */
    else rv = 0;
  }  /* button press */

  else if (xev->type == KeyPress) {
    XKeyEvent *e = (XKeyEvent *) xev;
    char buf[128];  KeySym ks;
    int stlen;
    
    stlen = XLookupString(e,buf,128,&ks,(XComposeStatus *) NULL);
    buf[stlen] = '\0';
    
    RemapKeyCheck(ks, buf, &stlen);
    
    if (e->window == pngW) {
      if (stlen) {
        if (buf[0] == '\r' || buf[0] == '\n') { /* enter */
          FakeButtonPress(&pbut[P_BOK]);
        }
        else if (buf[0] == '\033') {            /* ESC */
          FakeButtonPress(&pbut[P_BCANC]);
        }
      }
    }
    else rv = 0;
  }
  else rv = 0;

  if (rv==0 && (xev->type == ButtonPress || xev->type == KeyPress)) {
    XBell(theDisp, 50);
    rv = 1;   /* eat it */
  }

  return rv;
}


/*******************************************/
void PNGSaveParams(fname, col)
     char *fname;
     int col;
{
  filename = fname;
  colorType = col;
}


/*******************************************/
static void drawPD(x, y, w, h)
     int x, y, w, h;
{
  char *title   = "Save PNG file...";

  char ctitle1[20];
  char *ctitle2 = "Useful range";
  char *ctitle3 = "is 2 - 7.";
  char *ctitle4 = "Uncompressed = 0";

  char *ftitle  = "Row Filters:";

  char gtitle[20];

  int i;
  XRectangle xr;
  
  xr.x = x;  xr.y = y;  xr.width = w;  xr.height = h;
  XSetClipRectangles(theDisp, theGC, 0,0, &xr, 1, Unsorted);

  XSetForeground(theDisp, theGC, infofg);
  XSetBackground(theDisp, theGC, infobg);

  for (i=0; i<P_NBUTTS; i++) BTRedraw(&pbut[i]);

  DrawString(pngW,       15,  6+ASCENT,                          title);

  sprintf(ctitle1, "Default = %d", COMPRESSION);
  DrawString(pngW,       18,  6+DHIGH+cDial.y+ASCENT,            ctitle1);
  DrawString(pngW,       17,  6+DHIGH+cDial.y+ASCENT+LINEHIGH,   ctitle2);
  DrawString(pngW,       17,  6+DHIGH+cDial.y+ASCENT+2*LINEHIGH, ctitle3);
  DrawString(pngW,       17,  6+DHIGH+cDial.y+ASCENT+3*LINEHIGH, ctitle4);

  sprintf(gtitle, "Default = %g", DISPLAY_GAMMA);
  DrawString(pngW, DWIDE+30,  6+DHIGH+gDial.y+ASCENT,            gtitle);

  ULineString(pngW, FdefCB.x, FdefCB.y-3-DESCENT, ftitle);
  XDrawRectangle(theDisp, pngW, theGC, FdefCB.x-11, FdefCB.y-LINEHIGH-3,
                                       93, 8*LINEHIGH+15);
  CBRedraw(&FdefCB);
  XDrawLine(theDisp, pngW, theGC, FdefCB.x-11, FdefCB.y+LINEHIGH+4,
                                  FdefCB.x+82, FdefCB.y+LINEHIGH+4);

  CBRedraw(&FnoneCB);
  CBRedraw(&FupCB);
  CBRedraw(&FsubCB);
  CBRedraw(&FavgCB);
  CBRedraw(&FPaethCB);

  CBRedraw(&interCB);

  XSetClipMask(theDisp, theGC, None);
}


/*******************************************/
static void clickPD(x,y)
     int x,y;
{
  int i;
  BUTT *bp;

  /* check BUTTs */
  
  for (i=0; i<P_NBUTTS; i++) {
    bp = &pbut[i];
    if (PTINRECT(x, y, bp->x, bp->y, bp->w, bp->h)) break;
  }
  
  if (i<P_NBUTTS) {  /* found one */
    if (BTTrack(bp)) doCmd(i);
  }

  /* check CBUTTs */

  else if (CBClick(&FdefCB,x,y)) {
    int oldval = FdefCB.val;

    CBTrack(&FdefCB);

    if (oldval != FdefCB.val)
    {
      CBSetActive(&FnoneCB, !FdefCB.val);
      CBSetActive(&FsubCB, !FdefCB.val);
      CBSetActive(&FupCB, !FdefCB.val);
      CBSetActive(&FavgCB, !FdefCB.val);
      CBSetActive(&FPaethCB, !FdefCB.val);

      CBRedraw(&FnoneCB);
      CBRedraw(&FupCB);
      CBRedraw(&FsubCB);
      CBRedraw(&FavgCB);
      CBRedraw(&FPaethCB);
    }
  }
  else if (CBClick(&FnoneCB,x,y))  CBTrack(&FnoneCB);
  else if (CBClick(&FsubCB,x,y))   CBTrack(&FsubCB);
  else if (CBClick(&FupCB,x,y))    CBTrack(&FupCB);
  else if (CBClick(&FavgCB,x,y))   CBTrack(&FavgCB);
  else if (CBClick(&FPaethCB,x,y)) CBTrack(&FPaethCB);
  else if (CBClick(&interCB,x,y))  CBTrack(&interCB);
}


/*******************************************/
static void doCmd(cmd)
     int cmd;
{
  switch (cmd) {
  case P_BOK: {
    char *fullname;

    writePNG();
    PNGDialog(0);
    
    fullname = GetDirFullName();
    if (!ISPIPE(fullname[0])) {
      XVCreatedFile(fullname);
      StickInCtrlList(0);
    }
  }
    break;

  case P_BCANC:  PNGDialog(0);  break;

  default:        break;
  }
}


/*******************************************/
static void writePNG()
{
  FILE       *fp;
  int         w, h, nc, rv, ptype, pfree;
  byte       *inpix, *rmap, *gmap, *bmap;

  fp = OpenOutFile(filename);
  if (!fp) return;

  fbasename = BaseName(filename);

  WaitCursor();
  inpix = GenSavePic(&ptype, &w, &h, &pfree, &nc, &rmap, &gmap, &bmap);

  rv = WritePNG(fp, inpix, ptype, w, h, rmap, gmap, bmap, nc);

  SetCursors(-1);

  if (CloseOutFile(fp, filename, rv) == 0) DirBox(0);

  if (pfree) free(inpix);
}


/*******************************************/
int WritePNG(fp, pic, ptype, w, h, rmap, gmap, bmap, numcols)
     FILE *fp;
     byte *pic;
     int   ptype, w, h;
     byte *rmap, *gmap, *bmap;
     int   numcols;
{
  png_struct *png_ptr;
  png_info   *info_ptr;
  png_color   palette[256];
  png_textp   text;
  byte        remap[256];
  int         i, filter, linesize = 0, pass;
  byte       *p, *png_line;
  char        software[256];
  char       *savecmnt = NULL;

  if ((png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL,
       png_xv_error, png_xv_warning)) == NULL) {
    FatalError("malloc failure in WritePNG");
  }

  if ((info_ptr = png_create_info_struct(png_ptr)) == NULL)
  {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    FatalError("malloc failure in WritePNG");
  }

#ifdef OLDCODE
  if (setjmp(png_ptr->jmpbuf)) {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return -1;
  }
#else
  if (setjmp(png_jmpbuf (png_ptr))) {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return -1;
  }
#endif

  png_init_io(png_ptr, fp);

  png_set_compression_level(png_ptr, (int)cDial.val);

  /* Don't bother filtering if we aren't compressing the image */
  if (FdefCB.val)
  {
    if ((int)cDial.val == 0)
      png_set_filter(png_ptr, 0, PNG_FILTER_NONE);
  }
  else
  {
    filter  = FnoneCB.val  ? PNG_FILTER_NONE  : 0;
    filter |= FsubCB.val   ? PNG_FILTER_SUB   : 0;
    filter |= FupCB.val    ? PNG_FILTER_UP    : 0;
    filter |= FavgCB.val   ? PNG_FILTER_AVG   : 0;
    filter |= FPaethCB.val ? PNG_FILTER_PAETH : 0;

    png_set_filter(png_ptr, 0, filter);
  }

  struct  {
    png_uint_32 width;
    png_uint_32 height;
    int bit_depth;
    int color_type;
    int interlace_type;
    int compression_type;
    int filter_method;
  } _imageHeader;

  
#ifdef OLDCODE
  info_ptr->width = w;
  info_ptr->height = h;
  info_ptr->interlace_type = interCB.val ? 1 : 0;
#else
  _imageHeader.width = w;
  _imageHeader.height = h;
  _imageHeader.interlace_type = interCB.val ? 1 : 0;
#endif

  if (colorType == F_FULLCOLOR || colorType == F_REDUCED) {
    if(ptype == PIC24) {
      linesize = 3*w;
#ifdef OLDCODE
      info_ptr->color_type = PNG_COLOR_TYPE_RGB;
      info_ptr->bit_depth = 8;
#else
      _imageHeader.color_type = PNG_COLOR_TYPE_RGB;
      _imageHeader.bit_depth = 8;
#endif
    } else {
      linesize = w;
      _imageHeader.color_type = PNG_COLOR_TYPE_PALETTE;
      if(numcols <= 2)
        _imageHeader.bit_depth = 1;
      else
      if(numcols <= 4)
        _imageHeader.bit_depth = 2;
      else
      if(numcols <= 16)
        _imageHeader.bit_depth = 4;
      else
        _imageHeader.bit_depth = 8;

      for(i = 0; i < numcols; i++) {
        palette[i].red   = rmap[i];
        palette[i].green = gmap[i];
        palette[i].blue  = bmap[i];
      }
#ifdef OLDCODE
      info_ptr->num_palette = numcols;
      info_ptr->palette = palette;
      info_ptr->valid |= PNG_INFO_PLTE;
#else
      png_set_PLTE (png_ptr, info_ptr, palette, numcols);
#endif
    }
  }

  else if(colorType == F_GREYSCALE || colorType == F_BWDITHER) {
    _imageHeader.color_type = PNG_COLOR_TYPE_GRAY;
    if(colorType == F_BWDITHER) {
      /* shouldn't happen */
      if (ptype == PIC24) FatalError("PIC24 and B/W Stipple in WritePNG()");

      _imageHeader.bit_depth = 1;
      if(MONO(rmap[0], gmap[0], bmap[0]) > MONO(rmap[1], gmap[1], bmap[1])) {
        remap[0] = 1;
        remap[1] = 0;
      }
      else {
        remap[0] = 0;
        remap[1] = 1;
      }
      linesize = w;
    }
    else {
      if(ptype == PIC24) {
        linesize = w*3;
        _imageHeader.bit_depth = 8;
      }
      else {
        int low_presc;

        linesize = w;

        for(i = 0; i < numcols; i++)
          remap[i] = MONO(rmap[i], gmap[i], bmap[i]);

        for(; i < 256; i++)
          remap[i]=0;

        _imageHeader.bit_depth = 8;

        /* Note that this fails most of the time because of gamma */
        /* try to adjust to 4 bit prescision grayscale */

        low_presc=1;

        for(i = 0; i < numcols; i++) {
          if((remap[i] & 0x0f) * 0x11 != remap[i]) {
            low_presc = 0;
            break;
          }
        }

        if(low_presc) {
          for(i = 0; i < numcols; i++) {
            remap[i] &= 0xf;
          }
          _imageHeader.bit_depth = 4;

          /* try to adjust to 2 bit prescision grayscale */

          for(i = 0; i < numcols; i++) {
            if((remap[i] & 0x03) * 0x05 != remap[i]) {
              low_presc = 0;
              break;
            }
          }
        }

        if(low_presc) {
          for(i = 0; i < numcols; i++) {
            remap[i] &= 3;
          }
          _imageHeader.bit_depth = 2;

          /* try to adjust to 1 bit prescision grayscale */

          for(i = 0; i < numcols; i++) {
            if((remap[i] & 0x01) * 0x03 != remap[i]) {
              low_presc = 0;
              break;
            }
          }
        }

        if(low_presc) {
          for(i = 0; i < numcols; i++) {
            remap[i] &= 1;
          }
          _imageHeader.bit_depth = 1;
        }
      }
    }
    png_set_IHDR (png_ptr, info_ptr,
		  _imageHeader.width,
		  _imageHeader.height,
		  _imageHeader.bit_depth,
		  _imageHeader.color_type,
		  _imageHeader.interlace_type,
		  _imageHeader.compression_type,
		  _imageHeader.filter_method);
  }
  else
    png_error(png_ptr, "Unknown colorstyle in WritePNG");

  if ((text = (png_textp)malloc(sizeof(png_text)))) {
    sprintf(software, "XV %s", REVDATE);

    text->compression = -1;
    text->key = "Software";
    text->text = software;
    text->text_length = strlen(text->text);

#ifdef OLDCODE
    info_ptr->max_text = 1;
    info_ptr->num_text = 1;
    info_ptr->text = text;
#else
    png_set_text (png_ptr, info_ptr, text, 1);
#endif
    free (text);
  }

  Display_Gamma = gDial.val;  /* Save the current gamma for loading */

#ifdef OLDCODE
  info_ptr->gamma = 1.0/gDial.val;
  info_ptr->valid |= PNG_INFO_gAMA;
#else
  /*  png_set_valid (png_ptr, info_ptr, PNG_INFO_gAMA); */
  png_set_gAMA (png_ptr, info_ptr, 1.0/gDial.val);
#endif

  png_write_info(png_ptr, info_ptr);

#ifdef OLDCODE
  if(info_ptr->bit_depth < 8)
    png_set_packing(png_ptr);
#else
  if (png_get_bit_depth (png_ptr, info_ptr) < 8)
    png_set_packing(png_ptr);
#endif

  pass=png_set_interlace_handling(png_ptr);

  if((png_line = malloc(linesize)) == NULL)
    png_error(png_ptr, "cannot allocate temp image line");

  for(i = 0; i < pass; i++) {
    int j;
    p = pic;
    for(j = 0; j < h; j++) {
      if (png_get_color_type (png_ptr, info_ptr) == PNG_COLOR_TYPE_GRAY) {
        int k;
        for(k = 0; k < w; k++)
          png_line[k] = ptype==PIC24 ? MONO(p[k*3], p[k*3+1], p[k*3+2]) :
                                       remap[p[k]];
        png_write_row(png_ptr, png_line);
      } else  /* RGB or palette */
        png_write_row(png_ptr, p);
      if((j & 0x1f) == 0) WaitCursor();
      p += linesize;
    }
  }

  free(png_line);
  {
    int numText = 0;
    int maxNumText = 16;
    if ((text = (png_textp) malloc (maxNumText * sizeof(png_text)))) {
      if (picComments &&
	  strlen (picComments) &&
	  (savecmnt = (char *) malloc ((strlen(picComments) + 1)*sizeof(char)))) {
	png_textp tp;
	char *comment, *key;
	strcpy (savecmnt, picComments);
	key     = savecmnt;
	tp      = text;
	comment = strchr(key, ':');
	do  {
	  /* Allocate a larger structure for comments if necessary */
	  if (numText >= maxNumText) {
	    maxNumText *= 2;
	    if ((tp = realloc(text, maxNumText * sizeof(png_text))) == NULL)
	      break;
	    else {
	      text = tp;
	      tp = &text [numText];
	    }
	  }
	  /* See if it looks like a PNG keyword from LoadPNG */
	  /* GRR: should test for strictly < 80, right? (key = 1-79 chars only) */
	  if (comment && comment[1] == ':' && comment - key <= 80) {
	    *(comment++) = '\0';
	    *(comment++) = '\0';

	    /* If the comment is the 'Software' chunk XV writes, we remove it,
	       since we have already stored one */
	    if (strcmp(key, "Software") == 0 && strncmp(comment, "XV", 2) == 0) {
	      key = strchr(comment, '\n');
	      if(key)
		key++; /* skip \n */
	      comment = strchr(key, ':');
	    }
	    /* We have another keyword and/or comment to write out */
	    else {
	      tp->key  = key;
	      tp->text = comment;
	      /* We have to find the end of this comment, and the next keyword
		 if there is one */
	      for (; NULL != (key = comment = strchr(comment, ':')); comment++)
		if (key[1] == ':')
		  break;
	      /* It looks like another keyword, go backward to the beginning */
	      if (key) {
		while(key > tp->text && *key != '\n')
		  key--;

		if (key > tp->text && comment - key <= 80) {
		  *key = '\0';
		  key++;
		}
	      }

	      tp->text_length = strlen(tp->text);

	      /* We don't have another keyword, so remove the last newline */
	      if (!key && tp->text[tp->text_length - 1] == '\n')
		{
		  tp->text[tp->text_length] = '\0';
		  tp->text_length--;
		}

	      tp->compression = tp->text_length > 640 ? 0 : -1;
	      numText++;
	      tp++;
	    }
	  }
	  /* Just a generic comment:  make sure line-endings are valid for PNG */
	  else {
	    char *p=key, *q=key;     /* only deleting chars, not adding any */

	    while (*p) {
	      if (*p == CR) {        /* lone CR or CR/LF:  EOL either way */
		*q++ = LF;           /* LF is the only allowed PNG line-ending */
		if (p[1] == LF)      /* get rid of any original LF */
		  ++p;
	      } else if (*p == LF)   /* lone LF */
		*q++ = LF;
	      else
		*q++ = *p;
	      ++p;
	    }
	    *q = '\0';               /* unnecessary...but what the heck */
	    tp->key = "Comment";
	    tp->text = key;
	    tp->text_length = q - key;
	    tp->compression = tp->text_length > 750 ? 0 : -1;
	    numText++;
	    key = NULL;
	  }
	} while (key && *key);
      }
    }
    png_set_text (png_ptr, info_ptr, text, numText);
  }


  {

    png_time pngTime;

    png_convert_from_time_t (&pngTime , time (NULL));
    png_set_tIME (png_ptr, info_ptr, &pngTime);
    
    /*********   info_ptr->valid |= PNG_INFO_tIME; */
  }

  png_write_end(png_ptr, info_ptr);
  fflush(fp);   /* just in case we core-dump before finishing... */

  if (text)
  {
    free(text);
    /* must do this or png_destroy_write_struct() 0.97+ will free text again: */
#ifdef OLDCODE
    info_ptr->text = (png_textp) NULL;
#else
    png_set_text (png_ptr, info_ptr, (png_textp) NULL, 0);
#endif
    if (savecmnt)
    {
      free(savecmnt);
      savecmnt = (char *)NULL;
    }
  }

  png_destroy_write_struct(&png_ptr, &info_ptr);

  return 0;
}


/*******************************************/
int LoadPNG(fname, pinfo)
     char    *fname;
     PICINFO *pinfo;
/*******************************************/
{
  /* returns '1' on success */

  FILE  *fp;
  png_struct *png_ptr;
  png_info *info_ptr;
  png_color_16 my_background;
  int i,j;
  int linesize;
  int filesize;
  int pass;
  size_t commentsize;

  fbasename = BaseName(fname);

  pinfo->pic     = (byte *) NULL;
  pinfo->comment = (char *) NULL;

  read_anything=0;

  /* open the file */
  fp = xv_fopen(fname,"r");
  if (!fp)
  {
    SetISTR(ISTR_WARNING,"%s:  can't open file", fname);
    return 0;
  }

  /* find the size of the file */
  fseek(fp, 0L, 2);
  filesize = ftell(fp);
  fseek(fp, 0L, 0);
  
  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,
                                   png_xv_error, png_xv_warning);
  if(!png_ptr) {
    fclose(fp);
    FatalError("malloc failure in LoadPNG");
  }

  info_ptr = png_create_info_struct(png_ptr);

  if(!info_ptr) {
    fclose(fp);
    png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
    FatalError("malloc failure in LoadPNG");
  }

  if (setjmp(png_jmpbuf (png_ptr))) {
    fclose(fp);
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
    if(!read_anything) {
      if(pinfo->pic) {
        free(pinfo->pic);
        pinfo->pic = NULL;
      }
      if(pinfo->comment) {
        free(pinfo->comment);
        pinfo->comment = NULL;
      }
    }
    return read_anything;
  }

  png_init_io(png_ptr, fp);
  png_read_info(png_ptr, info_ptr);

  pinfo->w = pinfo->normw = png_get_image_width  (png_ptr, info_ptr);
  pinfo->h = pinfo->normh = png_get_image_height (png_ptr, info_ptr);

  pinfo->frmType = F_PNG;

  sprintf (pinfo->fullInfo, "PNG, %d bit ", png_get_bit_depth (png_ptr, info_ptr) *
	   png_get_channels (png_ptr, info_ptr));

  switch (png_get_color_type (png_ptr, info_ptr)) {
    case PNG_COLOR_TYPE_PALETTE:
      strcat(pinfo->fullInfo, "palette color");
      break;

    case PNG_COLOR_TYPE_GRAY:
      strcat(pinfo->fullInfo, "grayscale");
      break;

    case PNG_COLOR_TYPE_GRAY_ALPHA:
      strcat(pinfo->fullInfo, "grayscale+alpha");
      break;

    case PNG_COLOR_TYPE_RGB:
      strcat(pinfo->fullInfo, "truecolor");
      break;

    case PNG_COLOR_TYPE_RGB_ALPHA:
      strcat(pinfo->fullInfo, "truecolor+alpha");
      break;
  }

  sprintf(pinfo->fullInfo + strlen(pinfo->fullInfo),
	  ", %sinterlaced. (%d bytes)",
	  png_get_interlace_type (png_ptr, info_ptr) ? "" : "non-", filesize);

  sprintf (pinfo->shrtInfo, "%dx%d PNG",
	   png_get_image_width  (png_ptr, info_ptr),
	   png_get_image_height (png_ptr, info_ptr));

  if (png_get_bit_depth (png_ptr, info_ptr) < 8)
      png_set_packing(png_ptr);

  if (png_get_valid (png_ptr, info_ptr, PNG_INFO_gAMA))
    png_set_gamma(png_ptr, Display_Gamma, 1.0 / gDial.val);
  else
    png_set_gamma(png_ptr, Display_Gamma, 0.45);

  if (png_get_valid (png_ptr, info_ptr, PNG_INFO_bKGD)) {

    png_color_16p backGround;

    png_get_bKGD (png_ptr, info_ptr, &backGround);
    png_set_background(png_ptr, backGround,
                       PNG_BACKGROUND_GAMMA_FILE, 1, 1.0);
  }
  else {
    my_background.red = my_background.green = my_background.blue =
      my_background.gray = 0;
    png_set_background(png_ptr, &my_background, PNG_BACKGROUND_GAMMA_SCREEN,
                       0, Display_Gamma);
  }

  if (png_get_bit_depth (png_ptr, info_ptr) == 16)
    png_set_strip_16 (png_ptr);

  if (png_get_color_type (png_ptr, info_ptr) == PNG_COLOR_TYPE_GRAY ||
      png_get_color_type (png_ptr, info_ptr) == PNG_COLOR_TYPE_GRAY_ALPHA)
  {
    if (png_get_bit_depth (png_ptr, info_ptr) == 1)
      pinfo->colType = F_BWDITHER;
    else
      pinfo->colType = F_GREYSCALE;
    png_set_expand(png_ptr);
  }

  pass=png_set_interlace_handling(png_ptr);

  png_read_update_info(png_ptr, info_ptr);

  if(png_get_color_type (png_ptr, info_ptr) == PNG_COLOR_TYPE_RGB ||
     png_get_color_type (png_ptr, info_ptr) == PNG_COLOR_TYPE_RGB_ALPHA) {
    linesize = pinfo->w * 3;
    pinfo->colType = F_FULLCOLOR;
    pinfo->type = PIC24;
  } else {
    linesize = pinfo->w;
    pinfo->type = PIC8;
    if (png_get_color_type (png_ptr, info_ptr) == PNG_COLOR_TYPE_GRAY ||
	png_get_color_type (png_ptr, info_ptr) == PNG_COLOR_TYPE_GRAY_ALPHA) {
      for(i = 0; i < 256; i++)
        pinfo->r[i] = pinfo->g[i] = pinfo->b[i] = i;
    } else {

      png_colorp  plt;
      int nPlt;
      png_get_PLTE (png_ptr, info_ptr, &plt, &nPlt);
      pinfo->colType = F_FULLCOLOR;
      
      for(i = 0; i < nPlt; i++) {
        pinfo->r[i] = plt[i].red;
        pinfo->g[i] = plt[i].green;
        pinfo->b[i] = plt[i].blue;
      }
    }
  }
  pinfo->pic = calloc((size_t)(linesize*pinfo->h), (size_t)1);

  if(!pinfo->pic) {
    png_error(png_ptr, "can't allocate space for PNG image");
  }

  /*  png_start_read_image(png_ptr); */

  for(i = 0; i < pass; i++) {
    byte *p = pinfo->pic;
    for(j = 0; j < pinfo->h; j++) {
      png_read_row(png_ptr, p, NULL);
      read_anything = 1;
      if((j & 0x1f) == 0) WaitCursor();
      p += linesize;
    }
  }

  png_read_end(png_ptr, info_ptr);
  
  {
    png_textp pTxt;
    int nTxt;

    png_get_text (png_ptr, info_ptr, &pTxt, &nTxt);

    if(nTxt > 0) {
      commentsize = 1;

      for(i = 0; i < nTxt; i++)
	commentsize += strlen(pTxt[i].key) + 1 +
	  pTxt[i].text_length + 2;

      if((pinfo->comment = malloc(commentsize)) == NULL) {
	png_warning(png_ptr,"can't allocate comment string");
      }
      else {
	pinfo->comment[0] = '\0';
	for(i = 0; i < nTxt; i++) {
	  strcat(pinfo->comment, pTxt[i].key);
	  strcat(pinfo->comment, "::");
	  strcat(pinfo->comment, pTxt[i].text);
	  strcat(pinfo->comment, "\n");
	}
      }
    }
  }
  png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);

  fclose(fp);

  return 1;
}


/*******************************************/
static void
png_xv_error(png_ptr, message)
     png_structp png_ptr;
     png_const_charp message;
{
  SetISTR(ISTR_WARNING,"%s:  libpng error: %s", fbasename, message);

#ifdef OLDCODE
  longjmp(png_ptr->jmpbuf, 1);
#else
  png_longjmp (png_ptr, 1);
#endif
}


/*******************************************/
static void
png_xv_warning(png_ptr, message)
     png_structp png_ptr;
     png_const_charp message;
{
  if (!png_ptr)
    return;

  SetISTR(ISTR_WARNING,"%s:  libpng warning: %s", fbasename, message);
}

#endif
