#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
/* Information used here is taken from the FIG Format 3.2 specification
   <URL:http://www.xfig.org/userman/fig-format.html>
   Some items left unspecified in the specifications are taken from the
   XFig source v. 3.2.3c
 */

#include <string.h>
#include <math.h>
#include <glib.h>
#include <stdlib.h>
#include <errno.h>
#include <locale.h>

#include "color.h"

#include "xfig.h"

Color fig_default_colors[FIG_MAX_DEFAULT_COLORS] =
{ { 0x00, 0x00, 0x00}, {0x00, 0x00, 0xff}, {0x00, 0xff, 0x00}, {0x00, 0xff, 0xff}, 
  { 0xff, 0x00, 0x00}, {0xff, 0x00, 0xff}, {0xff, 0xff, 0x00}, {0xff, 0xff, 0xff},
  { 0x00, 0x00, 0x8f}, {0x00, 0x00, 0xb0}, {0x00, 0x00, 0xd1}, {0x87, 0xcf, 0xff},
  { 0x00, 0x8f, 0x00}, {0x00, 0xb0, 0x00}, {0x00, 0xd1, 0x00}, {0x00, 0x8f, 0x8f},
  { 0x00, 0xb0, 0xb0}, {0x00, 0xd1, 0xd1}, {0x8f, 0x00, 0x00}, {0xb0, 0x00, 0x00},
  { 0xd1, 0x00, 0x00}, {0x8f, 0x00, 0x8f}, {0xb0, 0x00, 0xb0}, {0xd1, 0x00, 0xd1},
  { 0x7f, 0x30, 0x00}, {0xa1, 0x3f, 0x00}, {0xbf, 0x61, 0x00}, {0xff, 0x7f, 0x7f},
  { 0xff, 0xa1, 0xa1}, {0xff, 0xbf, 0xbf}, {0xff, 0xe1, 0xe1}, {0xff, 0xd7, 0x00}};

/** These are the "old-name" font names corresponding to the XFig standard
 *  fonts.  See the list in font.c.
 */
char *fig_fonts[] =
{
  "Times-Roman",
  "Times-Italic",
  "Times-Bold",
  "Times-BoldItalic",
  "AvantGarde-Book",
  "AvantGarde-BookOblique",
  "AvantGarde-Demi",
  "AvantGarde-DemiOblique",
  "Bookman-Light",
  "Bookman-LightItalic",
  "Bookman-Demi",
  "Bookman-DemiItalic",
  "Courier",
  "Courier-Oblique",
  "Courier-Bold",
  "Courier-BoldOblique",
  "Helvetica",
  "Helvetica-Oblique",
  "Helvetica-Bold",
  "Helvetica-BoldOblique",
  "Helvetica-Narrow",
  "Helvetica-Narrow-Oblique",
  "Helvetica-Narrow-Bold",
  "Helvetica-Narrow-BoldOblique",
  "NewCenturySchoolbook-Roman",
  "NewCenturySchoolbook-Italic",
  "NewCenturySchoolbook-Bold",
  "NewCenturySchoolbook-BoldItalic",
  "Palatino-Roman",
  "Palatino-Italic",
  "Palatino-Bold",
  "Palatino-BoldItalic",
  "Symbol",
  "ZapfChancery-MediumItalic",
  "ZapfDingbats",
  NULL
};
