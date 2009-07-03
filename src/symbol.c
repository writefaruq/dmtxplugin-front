/***************************************************************************
Dmtx-Daemon
Copyright (C) 2009, M Omar Faruque Sarker

libdmtx - Data Matrix Encoding/Decoding Library
Copyright (C) 2008, 2009 Mike Laughton
Copyright (C) 2008 Ryan Raasch
Copyright (C) 2008 Olivier Guilyardi

/***************************************************************************
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.              *
 *
 *       Contact: writefaruq@gmail.com
***************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <wand/magick-wand.h>
#include <dmtx.h>
#include "dmtxutil.h"

#include "symbol.h"

char *programName;

int symbol_decode(char *infile, char *outfile){

   char *filePath;
   int err;
   int imgPageIndex;
   int imgScanCount = 0, pageScanCount = 0;
   int width, height;
   unsigned char *pxl;
   DecodeOptions opt;
   DmtxImage *img;
   DmtxDecode *dec;
   DmtxRegion *reg;
   DmtxMessage *msg;
   DmtxTime timeout;
   MagickBooleanType success;
   MagickWand *wand;

   opt = GetDefaultDecodeOptions();
   //opt.timeoutMS = timeout;

   MagickWandGenesis();

  /* Open image from file or stream (might contain multiple pages) */
  filePath = infile;

  wand = NewMagickWand();
  if(wand == NULL) {
     FatalError(EX_OSERR, "Magick error");
  }

  if(opt.dpi != DmtxUndefined) {
     success = MagickSetResolution(wand, (double)opt.dpi, (double)opt.dpi);
     if(success == MagickFalse) {
        CleanupMagick(&wand, DmtxTrue);
        FatalError(EX_OSERR, "Magick error");
     }
  }

  success = MagickReadImage(wand, filePath);
  if(success == MagickFalse) {
     CleanupMagick(&wand, DmtxTrue);
     FatalError(EX_OSERR, "Magick error");
  }

  width = MagickGetImageWidth(wand);
  height = MagickGetImageHeight(wand);

  /* Loop once for each page within image */
  MagickResetIterator(wand);
  for(imgPageIndex = 0; MagickNextImage(wand) != MagickFalse; imgPageIndex++) {

     /* Reset timeout for each new page */
     if(opt.timeoutMS != DmtxUndefined)
        timeout = dmtxTimeAdd(dmtxTimeNow(), opt.timeoutMS);

     /* Allocate memory for pixel data */
     pxl = (unsigned char *)malloc(3 * width * height * sizeof(unsigned char));
     if(pxl == NULL) {
        CleanupMagick(&wand, DmtxFalse);
        FatalError(EX_OSERR, "malloc() error");
     }

     /* Copy pixels to known format */
     success = MagickGetImagePixels(wand, 0, 0, width, height, "RGB", CharPixel, pxl);
     if(success == MagickFalse || pxl == NULL) {
        CleanupMagick(&wand, DmtxTrue);
        FatalError(EX_OSERR, "malloc() error");
     }

     /* Initialize libdmtx image */
     img = dmtxImageCreate(pxl, width, height, DmtxPack24bppRGB);
     if(img == NULL) {
        CleanupMagick(&wand, DmtxFalse);
        FatalError(EX_SOFTWARE, "dmtxImageCreate() error");
     }

     dmtxImageSetProp(img, DmtxPropImageFlip, DmtxFlipNone);

     /* Initialize scan */
     dec = dmtxDecodeCreate(img, opt.shrinkMin);
     if(dec == NULL) {
        CleanupMagick(&wand, DmtxFalse);
        FatalError(EX_SOFTWARE, "decode create error");
     }

     err = SetDecodeOptions(dec, img, &opt);
     if(err != DmtxPass) {
        CleanupMagick(&wand, DmtxFalse);
        FatalError(EX_SOFTWARE, "decode option error");
     }

     /* Find and decode every barcode on page */
     pageScanCount = 0;
     for(;;) {
        /* Find next barcode region within image, but do not decode yet */
        if(opt.timeoutMS == DmtxUndefined)
           reg = dmtxRegionFindNext(dec, NULL);
        else
           reg = dmtxRegionFindNext(dec, &timeout);

        /* Finished file or ran out of time before finding another region */
        if(reg == NULL)
           break;

        /* Decode region based on requested barcode mode */
        if(opt.mosaic == DmtxTrue)
           msg = dmtxDecodeMosaicRegion(dec, reg, opt.correctionsMax);
        else
           msg = dmtxDecodeMatrixRegion(dec, reg, opt.correctionsMax);

        if(msg != NULL) {
           PrintStats(dec, reg, msg, imgPageIndex, &opt);
           PrintMessage(reg, msg, &opt, outfile);

           pageScanCount++;
           imgScanCount++;

           dmtxMessageDestroy(&msg);
        }

        dmtxRegionDestroy(&reg);

        if(opt.stopAfter != DmtxUndefined && imgScanCount >= opt.stopAfter)
           break;
     }

     if(opt.diagnose == DmtxTrue)
        WriteDiagnosticImage(dec, "debug.pnm");

     dmtxDecodeDestroy(&dec);
     dmtxImageDestroy(&img);
     free(pxl);
  }

  CleanupMagick(&wand, DmtxFalse);

   MagickWandTerminus();

   /* exit((imgScanCount > 0) ? EX_OK : 1); */
   return imgScanCount;
}


void dmtx_encode_symbol(char *infile, char *outfile){
   int err;
   char *format;
   EncodeOptions opt;
   DmtxEncode *enc;
   unsigned char codeBuffer[DMTXWRITE_BUFFER_SIZE];
   int codeBufferSize = sizeof codeBuffer;

   opt = GetDefaultEncodeOptions();
   opt.inputPath = infile;
   opt.outputPath = outfile;

   /* Override defaults with requested options
   err = HandleEncodeArgs(&opt, &argc, &argv);
   if(err != DmtxPass)
      ShowDecodeUsage(EX_USAGE); */

   /* Create and initialize libdmtx encoding struct */
   enc = dmtxEncodeCreate();
   if(enc == NULL)
      FatalError(EX_SOFTWARE, "create error");

   /* Set output image properties */
   dmtxEncodeSetProp(enc, DmtxPropPixelPacking, DmtxPack24bppRGB);
   dmtxEncodeSetProp(enc, DmtxPropImageFlip, DmtxFlipNone);
   dmtxEncodeSetProp(enc, DmtxPropRowPadBytes, 0);

   /* Set encoding options */
   dmtxEncodeSetProp(enc, DmtxPropMarginSize, opt.marginSize);
   dmtxEncodeSetProp(enc, DmtxPropModuleSize, opt.moduleSize);
   dmtxEncodeSetProp(enc, DmtxPropScheme, opt.scheme);
   dmtxEncodeSetProp(enc, DmtxPropSizeRequest, opt.sizeIdx);

   /* Read input data into buffer */
   ReadInputData(&codeBufferSize, codeBuffer, &opt);

   /* Create barcode image */
   if(opt.mosaic == DmtxTrue)
      err = dmtxEncodeDataMosaic(enc, codeBufferSize, codeBuffer);
   else
      err = dmtxEncodeDataMatrix(enc, codeBufferSize, codeBuffer);

   if(err == DmtxFail)
      FatalError(EX_SOFTWARE,
            _("Unable to encode message (possibly too large for requested size)"));

   /* Write image file, but only if preview and codewords are not used */
   if(opt.preview == DmtxTrue || opt.codewords == DmtxTrue) {
      if(opt.preview == DmtxTrue)
         WriteAsciiPreview(enc);
      if(opt.codewords == DmtxTrue)
         WriteCodewordList(enc);
   }
   else {
      format = GetImageFormat(&opt);
      if(format == NULL)
         format = "png";

      if(StrNCmpI(format, "svg", 3) == DmtxTrue)
         WriteSvgFile(&opt, enc, format);
      else
         WriteImageFile(&opt, enc, format);
   }

   /* Clean up */
   dmtxEncodeDestroy(&enc);

   exit(0);
}

DecodeOptions GetDefaultDecodeOptions(void)
{
   DecodeOptions opt;

   memset(&opt, 0x00, sizeof(DecodeOptions));

   /* Default options */
   opt.codewords = DmtxFalse;
   opt.edgeMin = DmtxUndefined;
   opt.edgeMax = DmtxUndefined;
   opt.scanGap = 2;
   opt.timeoutMS = DmtxUndefined;
   opt.newline = DmtxFalse;
   opt.page = DmtxUndefined;
   opt.squareDevn = DmtxUndefined;
   opt.dpi = DmtxUndefined;
   opt.sizeIdxExpected = DmtxSymbolShapeAuto;
   opt.edgeThresh = 5;
   opt.xMin = NULL;
   opt.xMax = NULL;
   opt.yMin = NULL;
   opt.yMax = NULL;
   opt.correctionsMax = DmtxUndefined;
   opt.diagnose = DmtxFalse;
   opt.mosaic = DmtxFalse;
   opt.stopAfter = DmtxUndefined;
   opt.pageNumbers = DmtxFalse;
   opt.corners = DmtxFalse;
   opt.shrinkMin = 1;
   opt.shrinkMax = 1;
   opt.unicode = DmtxFalse;
   opt.verbose = DmtxFalse;

   return opt;
}

/**
 * @brief  Set and validate user-requested options from command line arguments.
 * @param  opt runtime options from defaults or command line
 * @param  argcp pointer to argument count
 * @param  argvp pointer to argument list
 * @param  fileIndex pointer to index of first non-option arg (if successful)
 * @return DmtxPass | DmtxFail
 */
  DmtxPassFail
HandleDecodeArgs(DecodeOptions *opt, int *fileIndex, int *argcp, char **argvp[])
{
   int i;
   int err;
   int optchr;
   int longIndex;
   char *ptr;

   struct option longOptions[] = {
         {"codewords",        no_argument,       NULL, 'c'},
         {"minimum-edge",     required_argument, NULL, 'e'},
         {"maximum-edge",     required_argument, NULL, 'E'},
         {"gap",              required_argument, NULL, 'g'},
         {"list-formats",     no_argument,       NULL, 'l'},
         {"milliseconds",     required_argument, NULL, 'm'},
         {"newline",          no_argument,       NULL, 'n'},
         {"page",             required_argument, NULL, 'p'},
         {"square-deviation", required_argument, NULL, 'q'},
         {"resolution",       required_argument, NULL, 'r'},
         {"symbol-size",      required_argument, NULL, 's'},
         {"threshold",        required_argument, NULL, 't'},
         {"x-range-min",      required_argument, NULL, 'x'},
         {"x-range-max",      required_argument, NULL, 'X'},
         {"y-range-min",      required_argument, NULL, 'y'},
         {"y-range-max",      required_argument, NULL, 'Y'},
         {"max-corrections",  required_argument, NULL, 'C'},
         {"diagnose",         no_argument,       NULL, 'D'},
         {"mosaic",           no_argument,       NULL, 'M'},
         {"stop-after",       required_argument, NULL, 'N'},
         {"page-numbers",     no_argument,       NULL, 'P'},
         {"corners",          no_argument,       NULL, 'R'},
         {"shrink",           required_argument, NULL, 'S'},
         {"unicode",          no_argument,       NULL, 'U'},
         {"verbose",          no_argument,       NULL, 'v'},
         {"version",          no_argument,       NULL, 'V'},
         {"help",             no_argument,       NULL,  0 },
         {0, 0, 0, 0}
   };

   programName = Basename((*argvp)[0]);

   *fileIndex = 0;

   for(;;) {
      optchr = getopt_long(*argcp, *argvp,
            "ce:E:g:lm:np:q:r:s:t:x:X:y:Y:vC:DMN:PRS:UV", longOptions, &longIndex);
      if(optchr == -1)
         break;

      switch(optchr) {
         case 0: /* --help */
            ShowDecodeUsage(EX_OK);
            break;
         case 'l':
            ListImageFormats();
            exit(EX_OK);
            break;
         case 'c':
            opt->codewords = DmtxTrue;
            break;
         case 'e':
            err = StringToInt(&(opt->edgeMin), optarg, &ptr);
            if(err != DmtxPass || opt->edgeMin <= 0 || *ptr != '\0')
               FatalError(EX_USAGE, _("Invalid edge length specified \"%s\""), optarg);
            break;
         case 'E':
            err = StringToInt(&(opt->edgeMax), optarg, &ptr);
            if(err != DmtxPass || opt->edgeMax <= 0 || *ptr != '\0')
               FatalError(EX_USAGE, _("Invalid edge length specified \"%s\""), optarg);
            break;
         case 'g':
            err = StringToInt(&(opt->scanGap), optarg, &ptr);
            if(err != DmtxPass || opt->scanGap <= 0 || *ptr != '\0')
               FatalError(EX_USAGE, _("Invalid gap specified \"%s\""), optarg);
            break;
         case 'm':
            err = StringToInt(&(opt->timeoutMS), optarg, &ptr);
            if(err != DmtxPass || opt->timeoutMS < 0 || *ptr != '\0')
               FatalError(EX_USAGE, _("Invalid timeout (in milliseconds) specified \"%s\""), optarg);
            break;
         case 'n':
            opt->newline = DmtxTrue;
            break;
         case 'p':
            err = StringToInt(&(opt->page), optarg, &ptr);
            if(err != DmtxPass || opt->page < 1 || *ptr != '\0')
               FatalError(EX_USAGE, _("Invalid page specified \"%s\""), optarg);
         case 'q':
            err = StringToInt(&(opt->squareDevn), optarg, &ptr);
            if(err != DmtxPass || *ptr != '\0' ||
                  opt->squareDevn < 0 || opt->squareDevn > 90)
               FatalError(EX_USAGE, _("Invalid squareness deviation specified \"%s\""), optarg);
            break;
         case 'r':
            err = StringToInt(&(opt->dpi), optarg, &ptr);
            if(err != DmtxPass || *ptr != '\0' || opt->dpi < 1)
               FatalError(EX_USAGE, _("Invalid resolution specified \"%s\""), optarg);
            break;
         case 's':
            /* Determine correct barcode size and/or shape */
            if(*optarg == 'a') {
               opt->sizeIdxExpected = DmtxSymbolShapeAuto;
            }
            else if(*optarg == 's') {
               opt->sizeIdxExpected = DmtxSymbolSquareAuto;
            }
            else if(*optarg == 'r') {
               opt->sizeIdxExpected = DmtxSymbolRectAuto;
            }
            else {
               for(i = 0; i < DmtxSymbolSquareCount + DmtxSymbolRectCount; i++) {
                  if(strncmp(optarg, symbolSizes[i], 8) == 0) {
                     opt->sizeIdxExpected = i;
                     break;
                  }
               }
               if(i == DmtxSymbolSquareCount + DmtxSymbolRectCount)
                  return DmtxFail;
            }
            break;
         case 't':
            err = StringToInt(&(opt->edgeThresh), optarg, &ptr);
            if(err != DmtxPass || *ptr != '\0' ||
                  opt->edgeThresh < 1 || opt->edgeThresh > 100)
               FatalError(EX_USAGE, _("Invalid edge threshold specified \"%s\""), optarg);
            break;
         case 'x':
            opt->xMin = optarg;
            break;
         case 'X':
            opt->xMax = optarg;
            break;
         case 'y':
            opt->yMin = optarg;
            break;
         case 'Y':
            opt->yMax = optarg;
            break;
         case 'v':
            opt->verbose = DmtxTrue;
            break;
         case 'C':
            err = StringToInt(&(opt->correctionsMax), optarg, &ptr);
            if(err != DmtxPass || opt->correctionsMax < 0 || *ptr != '\0')
               FatalError(EX_USAGE, _("Invalid max corrections specified \"%s\""), optarg);
            break;
         case 'D':
            opt->diagnose = DmtxTrue;
            break;
         case 'M':
            opt->mosaic = DmtxTrue;
            break;
         case 'N':
            err = StringToInt(&(opt->stopAfter), optarg, &ptr);
            if(err != DmtxPass || opt->stopAfter < 1 || *ptr != '\0')
               FatalError(EX_USAGE, _("Invalid count specified \"%s\""), optarg);
            break;
         case 'P':
            opt->pageNumbers = DmtxTrue;
            break;
         case 'R':
            opt->corners = DmtxTrue;
            break;
         case 'S':
            err = StringToInt(&(opt->shrinkMin), optarg, &ptr);
            if(err != DmtxPass || opt->shrinkMin < 1 || *ptr != '\0')
               FatalError(EX_USAGE, _("Invalid shrink factor specified \"%s\""), optarg);

            /* XXX later populate shrinkMax based on specified N-N range */
            opt->shrinkMax = opt->shrinkMin;
            break;
         case 'U':
            opt->unicode = DmtxTrue;
            break;
         case 'V':
            fprintf(stdout, "%s version %s\n", programName, DmtxVersion);
            fprintf(stdout, "libdmtx version %s\n", dmtxVersion());
            exit(EX_OK);
            break;
         default:
            return DmtxFail;
            break;
      }
   }
   *fileIndex = optind;

   return DmtxPass;
}

/**
 * @brief  Display program usage and exit with received status.
 * @param  status error code returned to OS
 * @return void
 */
void
ShowDecodeUsage(int status)
{
   if(status != 0) {
      fprintf(stderr, _("Usage: %s [OPTION]... [FILE]...\n"), programName);
      fprintf(stderr, _("Try `%s --help' for more information.\n"), programName);
   }
   else {
      fprintf(stdout, _("Usage: %s [OPTION]... [FILE]...\n"), programName);
      fprintf(stdout, _("\
Scan image FILE for Data Matrix barcodes and print decoded results to\n\
standard output.  Note that %s may find multiple barcodes in one image.\n\
\n\
Example: Scan top third of IMAGE001.png and stop after first barcode is found:\n\
\n\
   %s -n -Y33%% -N1 IMAGE001.png\n\
\n\
OPTIONS:\n"), programName, programName);
      fprintf(stdout, _("\
  -c, --codewords             print codewords extracted from barcode pattern\n\
  -e, --minimum-edge=N        pixel length of smallest expected edge in image\n\
  -E, --maximum-edge=N        pixel length of largest expected edge in image\n\
  -g, --gap=NUM               use scan grid with gap of NUM pixels between lines\n\
  -l, --list-formats          list supported image formats\n\
  -m, --milliseconds=N        stop scan after N milliseconds (per image)\n\
  -n, --newline               print newline character at the end of decoded data\n\
  -p, --page=N                only scan Nth page of images\n\
  -q, --square-deviation=N    allow non-squareness of corners in degrees (0-90)\n"));
      fprintf(stdout, _("\
  -r, --resolution=N          resolution for vector images (PDF, SVG, etc...)\n\
  -s, --symbol-size=[asr|RxC] only consider barcodes of specific size or shape\n\
        a = All sizes         [default]\n\
        s = Only squares\n\
        r = Only rectangles\n\
      RxC = Exactly this many rows and columns (10x10, 8x18, etc...)\n\
  -t, --threshold=N           ignore weak edges below threshold N (1-100)\n"));
      fprintf(stdout, _("\
  -x, --x-range-min=N[%%]      do not scan pixels to the left of N (or N%%)\n\
  -X, --x-range-max=N[%%]      do not scan pixels to the right of N (or N%%)\n\
  -y, --y-range-min=N[%%]      do not scan pixels above N (or N%%)\n\
  -Y, --y-range-max=N[%%]      do not scan pixels below N (or N%%)\n\
  -C, --corrections-max=N     correct at most N errors (0 = correction disabled)\n\
  -D, --diagnose              make copy of image with additional diagnostic data\n\
  -M, --mosaic                interpret detected regions as Data Mosaic barcodes\n"));
      fprintf(stdout, _("\
  -N, --stop-after=N          stop scanning after Nth barcode is returned\n\
  -P, --page-numbers          prefix decoded message with fax/tiff page number\n\
  -R, --corners               prefix decoded message with corner locations\n\
  -S, --shrink=N              internally shrink image by a factor of N\n\
  -U, --unicode               print Extended ASCII in Unicode (UTF-8)\n\
  -v, --verbose               use verbose messages\n\
  -V, --version               print program version information\n\
      --help                  display this help and exit\n"));
      fprintf(stdout, _("\nReport bugs to <mike@dragonflylogic.com>.\n"));
   }

   exit(status);
}

/**
 *
 *
 */
  DmtxPassFail
SetDecodeOptions(DmtxDecode *dec, DmtxImage *img, DecodeOptions *opt)
{
   int err;

#define RETURN_IF_FAILED(e) if(e != DmtxPass) { return DmtxFail; }

   err = dmtxDecodeSetProp(dec, DmtxPropScanGap, opt->scanGap);
   RETURN_IF_FAILED(err)

   if(opt->edgeMin != DmtxUndefined) {
      err = dmtxDecodeSetProp(dec, DmtxPropEdgeMin, opt->edgeMin);
      RETURN_IF_FAILED(err)
   }

   if(opt->edgeMax != DmtxUndefined) {
      err = dmtxDecodeSetProp(dec, DmtxPropEdgeMax, opt->edgeMax);
      RETURN_IF_FAILED(err)
   }

   if(opt->squareDevn != DmtxUndefined) {
      err = dmtxDecodeSetProp(dec, DmtxPropSquareDevn, opt->squareDevn);
      RETURN_IF_FAILED(err)
   }

   err = dmtxDecodeSetProp(dec, DmtxPropSymbolSize, opt->sizeIdxExpected);
   RETURN_IF_FAILED(err)

   err = dmtxDecodeSetProp(dec, DmtxPropEdgeThresh, opt->edgeThresh);
   RETURN_IF_FAILED(err)

   if(opt->xMin) {
      err = dmtxDecodeSetProp(dec, DmtxPropXmin, ScaleNumberString(opt->xMin, img->width));
      RETURN_IF_FAILED(err)
   }

   if(opt->xMax) {
      err = dmtxDecodeSetProp(dec, DmtxPropXmax, ScaleNumberString(opt->xMax, img->width));
      RETURN_IF_FAILED(err)
   }

   if(opt->yMin) {
      err = dmtxDecodeSetProp(dec, DmtxPropYmin, ScaleNumberString(opt->yMin, img->height));
      RETURN_IF_FAILED(err)
   }

   if(opt->yMax) {
      err = dmtxDecodeSetProp(dec, DmtxPropYmax, ScaleNumberString(opt->yMax, img->height));
      RETURN_IF_FAILED(err)
   }

#undef RETURN_IF_FAILED

   return DmtxPass;
}

/**
 * @brief  Print decoded message to standard output
 * @param  opt runtime options from defaults or command line
 * @param  dec pointer to DmtxDecode struct
 * @return DmtxPass | DmtxFail
 */
  DmtxPassFail
PrintStats(DmtxDecode *dec, DmtxRegion *reg, DmtxMessage *msg,
      int imgPageIndex, DecodeOptions *opt)
{
   int height;
   int dataWordLength;
   int rotateInt;
   double rotate;
   DmtxVector2 p00, p10, p11, p01;

   height = dmtxDecodeGetProp(dec, DmtxPropHeight);

   p00.X = p00.Y = p10.Y = p01.X = 0.0;
   p10.X = p01.Y = p11.X = p11.Y = 1.0;
   dmtxMatrix3VMultiplyBy(&p00, reg->fit2raw);
   dmtxMatrix3VMultiplyBy(&p10, reg->fit2raw);
   dmtxMatrix3VMultiplyBy(&p11, reg->fit2raw);
   dmtxMatrix3VMultiplyBy(&p01, reg->fit2raw);

   dataWordLength = dmtxGetSymbolAttribute(DmtxSymAttribSymbolDataWords, reg->sizeIdx);
   if(opt->verbose == DmtxTrue) {

      rotate = (2 * M_PI) + (atan2(reg->fit2raw[0][1], reg->fit2raw[1][1]) -
            atan2(reg->fit2raw[1][0], reg->fit2raw[0][0])) / 2.0;

      rotateInt = (int)(rotate * 180/M_PI + 0.5);
      if(rotateInt >= 360)
         rotateInt -= 360;

      fprintf(stdout, "--------------------------------------------------\n");
      fprintf(stdout, "       Matrix Size: %d x %d\n",
            dmtxGetSymbolAttribute(DmtxSymAttribSymbolRows, reg->sizeIdx),
            dmtxGetSymbolAttribute(DmtxSymAttribSymbolCols, reg->sizeIdx));
      fprintf(stdout, "    Data Codewords: %d (capacity %d)\n",
            dataWordLength - msg->padCount, dataWordLength);
      fprintf(stdout, "   Error Codewords: %d\n",
            dmtxGetSymbolAttribute(DmtxSymAttribSymbolErrorWords, reg->sizeIdx));
      fprintf(stdout, "      Data Regions: %d x %d\n",
            dmtxGetSymbolAttribute(DmtxSymAttribHorizDataRegions, reg->sizeIdx),
            dmtxGetSymbolAttribute(DmtxSymAttribVertDataRegions, reg->sizeIdx));
      fprintf(stdout, "Interleaved Blocks: %d\n",
            dmtxGetSymbolAttribute(DmtxSymAttribInterleavedBlocks, reg->sizeIdx));
      fprintf(stdout, "    Rotation Angle: %d\n", rotateInt);
      fprintf(stdout, "          Corner 0: (%0.1f, %0.1f)\n", p00.X, height - 1 - p00.Y);
      fprintf(stdout, "          Corner 1: (%0.1f, %0.1f)\n", p10.X, height - 1 - p10.Y);
      fprintf(stdout, "          Corner 2: (%0.1f, %0.1f)\n", p11.X, height - 1 - p11.Y);
      fprintf(stdout, "          Corner 3: (%0.1f, %0.1f)\n", p01.X, height - 1 - p01.Y);
      fprintf(stdout, "--------------------------------------------------\n");
   }

   if(opt->pageNumbers == DmtxTrue)
      fprintf(stdout, "%d:", imgPageIndex + 1);

   if(opt->corners == DmtxTrue) {
      fprintf(stdout, "%d,%d:", (int)(p00.X + 0.5), height - 1 - (int)(p00.Y + 0.5));
      fprintf(stdout, "%d,%d:", (int)(p10.X + 0.5), height - 1 - (int)(p10.Y + 0.5));
      fprintf(stdout, "%d,%d:", (int)(p11.X + 0.5), height - 1 - (int)(p11.Y + 0.5));
      fprintf(stdout, "%d,%d:", (int)(p01.X + 0.5), height - 1 - (int)(p01.Y + 0.5));
   }

   return DmtxPass;
}

/**
 *
 *
 */
  DmtxPassFail
PrintMessage(DmtxRegion *reg, DmtxMessage *msg, DecodeOptions *opt, const char *outfile)
{
   unsigned int i;
   int remainingDataWords;
   int dataWordLength;

    FILE *fp;

    fp = fopen(outfile, "wb");
    if(fp == NULL) {
        perror("failed to open output file");
        return EXIT_FAILURE;
    }


   if(opt->codewords == DmtxTrue) {
      dataWordLength = dmtxGetSymbolAttribute(DmtxSymAttribSymbolDataWords, reg->sizeIdx);
      for(i = 0; i < msg->codeSize; i++) {
         remainingDataWords = dataWordLength - i;
         if(remainingDataWords > msg->padCount)
            fprintf(fp, "%c:%03d\n", 'd', msg->code[i]);
         else if(remainingDataWords > 0)
            fprintf(fp, "%c:%03d\n", 'p', msg->code[i]);
         else
            fprintf(fp, "%c:%03d\n", 'e', msg->code[i]);
      }
   }
   else {
      if(opt->unicode == DmtxTrue) {
         for(i = 0; i < msg->outputIdx; i++) {
            if(msg->output[i] < 128) {
               fputc(msg->output[i], fp);
            }
            else if(msg->output[i] < 192) {
              fputc(0xc2, fp);
              fputc(msg->output[i], fp);
            }
            else {
               fputc(0xc3, fp);
               fputc(msg->output[i] - 64, fp);
            }
         }
      }
      else {
         (void ) fwrite(msg->output, sizeof(char), msg->outputIdx, fp);
      }

      if(opt->newline)
         fputc('\n', fp);
   }

   return DmtxPass;
}




/**
 *
 *
 */
  void
WriteDiagnosticImage(DmtxDecode *dec, char *imagePath)
{
   int totalBytes, headerBytes;
   int bytesWritten;
   unsigned char *pnm;
   FILE *fp;

   fp = fopen(imagePath, "wb");
   if(fp == NULL) {
      perror(programName);
      FatalError(EX_CANTCREAT, _("Unable to create image \"%s\""), imagePath);
   }

   pnm = dmtxDecodeCreateDiagnostic(dec, &totalBytes, &headerBytes, 0);
   if(pnm == NULL)
      FatalError(EX_OSERR, _("Unable to create diagnostic image"));

   bytesWritten = fwrite(pnm, sizeof(unsigned char), totalBytes, fp);
   if(bytesWritten != totalBytes)
      FatalError(EX_IOERR, _("Unable to write diagnostic image"));

   free(pnm);
   fclose(fp);
}

/**
 *
 *
 */
  int
ScaleNumberString(char *s, int extent)
{
   int err;
   int numValue;
   int scaledValue;
   char *terminate;

   assert(s != NULL);

   err = StringToInt(&numValue, s, &terminate);
   if(err != DmtxPass)
      FatalError(EX_USAGE, _("Integer value required"));

   scaledValue = (*terminate == '%') ? (int)(0.01 * numValue * extent + 0.5) : numValue;

   if(scaledValue < 0)
      scaledValue = 0;

   if(scaledValue >= extent)
      scaledValue = extent - 1;

   return scaledValue;
}


/* ------ Encoding Functions --------*/

EncodeOptions
GetDefaultEncodeOptions(void)
{
   EncodeOptions opt;
   int white[3] = { 255, 255, 255 };
   int black[3] = {   0,   0,   0 };

   memset(&opt, 0x00, sizeof(EncodeOptions));

   opt.inputPath = NULL;    /* default stdin */
   opt.outputPath = NULL;   /* default stdout */
   opt.format = NULL;
   opt.codewords = DmtxFalse;
   opt.marginSize = 10;
   opt.moduleSize = 5;
   opt.scheme = DmtxSchemeEncodeAscii;
   opt.preview = DmtxFalse;
   opt.rotate = 0;
   opt.sizeIdx = DmtxSymbolSquareAuto;
   memcpy(opt.color, black, sizeof(int) * 3);
   memcpy(opt.bgColor, white, sizeof(int) * 3);
   opt.mosaic = DmtxFalse;
   opt.dpi = 0; /* default to native resolution of requested image format */
   opt.verbose = DmtxFalse;

   return opt;
}

/**
 * @brief  Set and validate user-requested options from command line arguments.
 * @param  opt runtime options from defaults or command line
 * @param  argcp pointer to argument count
 * @param  argvp pointer to argument list
 * @return DmtxPass | DmtxFail
 */
DmtxPassFail
HandleEncodeArgs(EncodeOptions *opt, int *argcp, char **argvp[])
{
   int err;
   int i;
   int optchr;
   int longIndex;
   char *ptr;

   struct option longOptions[] = {
         {"codewords",        no_argument,       NULL, 'c'},
         {"module",           required_argument, NULL, 'd'},
         {"margin",           required_argument, NULL, 'm'},
         {"encoding",         required_argument, NULL, 'e'},
         {"format",           required_argument, NULL, 'f'},
         {"list-formats",     no_argument,       NULL, 'l'},
         {"output",           required_argument, NULL, 'o'},
         {"preview",          no_argument,       NULL, 'p'},
         {"rotate",           required_argument, NULL, 'r'},
         {"symbol-size",      required_argument, NULL, 's'},
         {"color",            required_argument, NULL, 'C'},
         {"bg-color",         required_argument, NULL, 'B'},
         {"mosaic",           no_argument,       NULL, 'M'},
         {"resolution",       required_argument, NULL, 'R'},
         {"verbose",          no_argument,       NULL, 'v'},
         {"version",          no_argument,       NULL, 'V'},
         {"help",             no_argument,       NULL,  0 },
         {0, 0, 0, 0}
   };

   programName = Basename((*argvp)[0]);

   for(;;) {
      optchr = getopt_long(*argcp, *argvp, "cd:m:e:f:lo:pr:s:C:B:MR:vV", longOptions, &longIndex);
      if(optchr == -1)
         break;

      switch(optchr) {
         case 0: /* --help */
            ShowDecodeUsage(EX_OK);
            break;
         case 'c':
            opt->codewords = DmtxTrue;
            break;
         case 'd':
            err = StringToInt(&opt->moduleSize, optarg, &ptr);
            if(err != DmtxPass || opt->moduleSize <= 0 || *ptr != '\0')
               FatalError(EX_USAGE, _("Invalid module size specified \"%s\""), optarg);
            break;
         case 'm':
            err = StringToInt(&opt->marginSize, optarg, &ptr);
            if(err != DmtxPass || opt->marginSize <= 0 || *ptr != '\0')
               FatalError(EX_USAGE, _("Invalid margin size specified \"%s\""), optarg);
            break;
         case 'e':
            if(strlen(optarg) != 1) {
               fprintf(stdout, "Invalid encodation scheme \"%s\"\n", optarg);
               return DmtxFail;
            }
            switch(*optarg) {
               case 'b':
                  opt->scheme = DmtxSchemeEncodeAutoBest;
                  break;
               case 'f':
                  opt->scheme = DmtxSchemeEncodeAutoFast;
                  fprintf(stdout, "\"Fast optimized\" not implemented\n");
                  return DmtxFail;
               case 'a':
                  opt->scheme = DmtxSchemeEncodeAscii;
                  break;
               case 'c':
                  opt->scheme = DmtxSchemeEncodeC40;
                  break;
               case 't':
                  opt->scheme = DmtxSchemeEncodeText;
                  break;
               case 'x':
                  opt->scheme = DmtxSchemeEncodeX12;
                  break;
               case 'e':
                  opt->scheme = DmtxSchemeEncodeEdifact;
                  break;
               case '8':
                  opt->scheme = DmtxSchemeEncodeBase256;
                  break;
               default:
                  fprintf(stdout, "Invalid encodation scheme \"%s\"\n", optarg);
                  return DmtxFail;
            }
            break;
         case 'f':
            opt->format = optarg;
            break;
         case 'l':
            ListImageFormats();
            exit(EX_OK);
            break;
         case 'o':
            if(strncmp(optarg, "-", 2) == 0)
               opt->outputPath = NULL;
            else
               opt->outputPath = optarg;
            break;
         case 'p':
            opt->preview = DmtxTrue;
            break;
         case 'r':
            err = StringToInt(&(opt->rotate), optarg, &ptr);
            if(err != DmtxPass || *ptr != '\0')
               FatalError(EX_USAGE, _("Invalid rotation angle specified \"%s\""), optarg);
            break;
         case 's':
            /* Determine correct barcode size and/or shape */
            if(*optarg == 's') {
               opt->sizeIdx = DmtxSymbolSquareAuto;
            }
            else if(*optarg == 'r') {
               opt->sizeIdx = DmtxSymbolRectAuto;
            }
            else {
               for(i = 0; i < DmtxSymbolSquareCount + DmtxSymbolRectCount; i++) {
                  if(strncmp(optarg, symbolSizes[i], 8) == 0) {
                     opt->sizeIdx = i;
                     break;
                  }
               }
               if(i == DmtxSymbolSquareCount + DmtxSymbolRectCount)
                  return DmtxFail;
            }
            break;
         case 'C':
            opt->color[0] = 0;
            opt->color[1] = 0;
            opt->color[2] = 0;
            fprintf(stdout, "Option \"%c\" not implemented\n", optchr);
            break;
         case 'B':
            opt->bgColor[0] = 255;
            opt->bgColor[1] = 255;
            opt->bgColor[2] = 255;
            fprintf(stdout, "Option \"%c\" not implemented\n", optchr);
            break;
         case 'M':
            opt->mosaic = DmtxTrue;
            break;
         case 'R':
            err = StringToInt(&(opt->dpi), optarg, &ptr);
            if(err != DmtxPass || opt->dpi <= 0 || *ptr != '\0')
               FatalError(EX_USAGE, _("Invalid dpi specified \"%s\""), optarg);
            break;
         case 'v':
            opt->verbose = DmtxTrue;
            break;
         case 'V':
            fprintf(stdout, "%s version %s\n", programName, DmtxVersion);
            fprintf(stdout, "libdmtx version %s\n", dmtxVersion());
            exit(0);
            break;
         default:
            return DmtxFail;
            break;
      }
   }

   opt->inputPath = (*argvp)[optind];

   /* XXX here test for incompatibility between options. For example you
      cannot specify dpi if PNM output is requested */

   return DmtxPass;
}

/**
 *
 *
 */
void
ReadInputData(int *codeBufferSize, unsigned char *codeBuffer, EncodeOptions *opt)
{
   int fd;

   /* Open file or stdin for reading */
   fd = (opt->inputPath == NULL) ? 0 : open(opt->inputPath, O_RDONLY);
   if(fd == -1)
      FatalError(EX_IOERR, _("Error while opening file \"%s\""), opt->inputPath);

   /* Read input contents into buffer */
   *codeBufferSize = read(fd, codeBuffer, DMTXWRITE_BUFFER_SIZE);
   if(*codeBufferSize == DMTXWRITE_BUFFER_SIZE)
      FatalError(EX_DATAERR, _("Message to be encoded is too large"));

   /* Close file only if not stdin */
   if(fd != 0 && close(fd) != 0)
      FatalError(EX_IOERR, _("Error while closing file"));
}

/**
 * @brief  Display program usage and exit with received status.
 * @param  status error code returned to OS
 * @return void
 */
void
ShowEncodeUsage(int status)
{
   if(status != 0) {
      fprintf(stderr, _("Usage: %s [OPTION]... [FILE]\n"), programName);
      fprintf(stderr, _("Try `%s --help' for more information.\n"), programName);
   }
   else {
      fprintf(stdout, _("Usage: %s [OPTION]... [FILE]\n"), programName);
      fprintf(stdout, _("\
Encode FILE or standard input and write Data Matrix image\n\
\n\
Example: %s message.txt -o message.png\n\
Example: echo -n 123456 | %s -o message.png\n\
\n\
OPTIONS:\n"), programName, programName);
      fprintf(stdout, _("\
  -c, --codewords             print codeword listing\n\
  -d, --module=NUM            module size (in pixels)\n\
  -m, --margin=NUM            margin size (in pixels)\n\
  -e, --encoding=[abcet8x]    primary encodation scheme\n\
            a = ASCII [default]   b = Best optimized [beta]\n\
            c = C40               e = EDIFACT\n\
            t = Text              8 = Base 256\n\
            x = X12\n"));
      fprintf(stdout, _("\
  -f, --format=FORMAT         PNG [default], TIF, GIF, PDF, etc...\n\
  -l, --list-formats          list supported image formats\n\
  -o, --output=FILE           output filename\n\
  -p, --preview               print ASCII art preview\n\
  -r, --rotate=DEGREES        rotation angle (degrees)\n"));
      fprintf(stdout, _("\
  -s, --symbol-size=[sr|RxC]  symbol size (default \"s\")\n\
        Automatic size options:\n\
            s = Auto square         r = Auto rectangle\n\
        Manual size options for square symbols:\n\
            10x10   12x12   14x14   16x16   18x18   20x20\n\
            22x22   24x24   26x26   32x32   36x36   40x40\n\
            44x44   48x48   52x52   64x64   72x72   80x80\n\
            88x88   96x96 104x104 120x120 132x132 144x144\n\
        Manual size options for rectangle symbols:\n\
             8x18    8x32   12x26   12x36   16x36   16x48\n"));
      fprintf(stdout, _("\
  -C, --color=COLOR           barcode color (not implemented)\n\
  -B, --bg-color=COLOR        background color (not implemented)\n\
  -M, --mosaic                create Data Mosaic (non-standard)\n\
  -R, --resolution=NUM        set image print resolution (dpi)\n\
  -v, --verbose               use verbose messages\n\
  -V, --version               print version information\n\
      --help                  display this help and exit\n"));
      fprintf(stdout, _("\nReport bugs to <mike@dragonflylogic.com>.\n"));
   }

   exit(status);
}



/**
 *
 *
 *
 */
char *
GetImageFormat(EncodeOptions *opt)
{
   char *ptr = NULL;

   /* Derive format from filename extension */
   if(opt->outputPath != NULL) {
      ptr = strrchr(opt->outputPath, '.');
      if(ptr != NULL)
         ptr++;
   }

   /* Found filename extension but format was also provided */
   if(ptr != NULL && strlen(ptr) > 0 && opt->format != NULL)
      fprintf(stderr, "WARNING: --format (-f) argument ignored; Format taken from filename\n");

   /* If still undefined then use format argument */
   if(ptr == NULL || strlen(ptr) == 0)
      ptr = opt->format;

   return ptr;
}

/**
 *
 *
 */
DmtxPassFail
WriteImageFile(EncodeOptions *opt, DmtxEncode *enc, char *format)
{
   MagickBooleanType success;
   MagickWand *wand;
   char *outputPath;

   MagickWandGenesis();

   wand = NewMagickWand();
   if(wand == NULL)
      FatalError(EX_OSERR, "Undefined error");

   success = MagickConstituteImage(wand, enc->image->width, enc->image->height,
         "RGB", CharPixel, enc->image->pxl);
   if(success == MagickFalse) {
      CleanupMagick(&wand, DmtxTrue);
      FatalError(EX_OSERR, "Undefined error");
   }

   success = MagickSetImageFormat(wand, format);
   if(success == MagickFalse) {
      CleanupMagick(&wand, DmtxFalse);
      FatalError(EX_OSERR, "Illegal format \"%s\"", format);
   }

   outputPath = (opt->outputPath == NULL) ? "-" : opt->outputPath;

   success = MagickWriteImage(wand, outputPath);
   if(success == MagickFalse) {
      CleanupMagick(&wand, DmtxTrue);
      FatalError(EX_OSERR, "Undefined error");
   }

   CleanupMagick(&wand, DmtxFalse);

   MagickWandTerminus();

   return DmtxPass;
}

/**
 *
 *
 */
DmtxPassFail
WriteSvgFile(EncodeOptions *opt, DmtxEncode *enc, char *format)
{
   int col, row, rowInv;
   int symbolCols, symbolRows;
   int width, height, module;
   int defineOnly = DmtxFalse;
   unsigned char mosaicRed, mosaicGrn, mosaicBlu;
   char *idString = NULL;
   char style[100];
   FILE *fp;

   if(StrNCmpI(format, "svg:", 4) == DmtxTrue) {
      defineOnly = DmtxTrue;
      idString = &format[4];
   }

   if(idString == NULL || strlen(idString) == 0)
      idString = "dmtx_0001";

   if(opt->outputPath == NULL) {
      fp = stdout;
   }
   else {
      fp = fopen(opt->outputPath, "wb");
      if(fp == NULL)
         FatalError(EX_CANTCREAT, "Unable to create output file \"%s\"", opt->outputPath);
   }

   width = 2 * enc->marginSize + (enc->region.symbolCols * enc->moduleSize);
   height = 2 * enc->marginSize + (enc->region.symbolRows * enc->moduleSize);

   symbolCols = dmtxGetSymbolAttribute(DmtxSymAttribSymbolCols, enc->region.sizeIdx);
   symbolRows = dmtxGetSymbolAttribute(DmtxSymAttribSymbolRows, enc->region.sizeIdx);

   /* Print SVG Header */
   if(defineOnly == DmtxFalse) {
      fprintf(fp, "\
<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n\
<!-- Created with dmtxwrite (http://www.libdmtx.org/) -->\n\
<svg\n\
   xmlns:svg=\"http://www.w3.org/2000/svg\"\n\
   xmlns=\"http://www.w3.org/2000/svg\"\n\
   xmlns:xlink=\"http://www.w3.org/1999/xlink\"\n\
   version=\"1.0\"\n\
   width=\"%d\"\n\
   height=\"%d\"\n\
   id=\"svg2\">\n\
  <defs>\n", width, height);
   }

   fprintf(fp, "  <symbol id=\"%s\">\n", idString);
   fprintf(fp, "    <desc>Layout:%dx%d Symbol:%dx%d Data Matrix</desc>\n",
         width, height, symbolCols, symbolRows);

   /* Write Data Matrix ON modules */
   for(row = 0; row < enc->region.symbolRows; row++) {
      rowInv = enc->region.symbolRows - row - 1;
      for(col = 0; col < enc->region.symbolCols; col++) {
         module = dmtxSymbolModuleStatus(enc->message, enc->region.sizeIdx, row, col);
         if(opt->mosaic == DmtxTrue) {
            mosaicRed = (module & DmtxModuleOnRed) ? 0x00 : 0xff;
            mosaicGrn = (module & DmtxModuleOnGreen) ? 0x00 : 0xff;
            mosaicBlu = (module & DmtxModuleOnBlue) ? 0x00 : 0xff;
            snprintf(style, 100, "style=\"fill:#%02x%02x%02x;fill-opacity:1;stroke:none\" ",
                  mosaicRed, mosaicGrn, mosaicBlu);
         }
         else {
            style[0] = '\0';
         }

         if(module & DmtxModuleOn) {
            fprintf(fp, "    <rect width=\"%d\" height=\"%d\" x=\"%d\" y=\"%d\" %s/>\n",
                  opt->moduleSize, opt->moduleSize,
                  col * opt->moduleSize + opt->marginSize,
                  rowInv * opt->moduleSize + opt->marginSize, style);
         }
      }
   }

   fprintf(fp, "  </symbol>\n");

   /* Close SVG document */
   if(defineOnly == DmtxFalse) {
      fprintf(fp, "\
  </defs>\n\
\n\
  <use xlink:href=\"#%s\" x='0' y='0' style=\"fill:#000000;fill-opacity:1;stroke:none\" />\n\
\n\
</svg>\n", idString);
   }

   return DmtxPass;
}

/**
 *
 *
 */
DmtxPassFail
WriteAsciiPreview(DmtxEncode *enc)
{
   int symbolRow, symbolCol;

   fputc('\n', stdout);

   /* ASCII prints from top to bottom */
   for(symbolRow = enc->region.symbolRows - 1; symbolRow >= 0; symbolRow--) {

      fputs("    ", stdout);
      for(symbolCol = 0; symbolCol < enc->region.symbolCols; symbolCol++) {
         fputs((dmtxSymbolModuleStatus(enc->message, enc->region.sizeIdx,
               symbolRow, symbolCol) & DmtxModuleOnRGB) ? "XX" : "  ", stdout);
      }
      fputs("\n", stdout);
   }

   fputc('\n', stdout);

   return DmtxPass;
}

/**
 *
 *
 */
DmtxPassFail
WriteCodewordList(DmtxEncode *enc)
{
   int i;
   int dataWordLength;
   int remainingDataWords;

   dataWordLength = dmtxGetSymbolAttribute(DmtxSymAttribSymbolDataWords, enc->region.sizeIdx);

   for(i = 0; i < enc->message->codeSize; i++) {
      remainingDataWords = dataWordLength - i;
      if(remainingDataWords > enc->message->padCount)
         fprintf(stdout, "%c:%03d\n", 'd', enc->message->code[i]);
      else if(remainingDataWords > 0)
         fprintf(stdout, "%c:%03d\n", 'p', enc->message->code[i]);
      else
         fprintf(stdout, "%c:%03d\n", 'e', enc->message->code[i]);
   }

   return DmtxPass;
}

/**
 *
 *
 */
DmtxBoolean
StrNCmpI(const char *s1, const char *s2, size_t n)
{
   size_t i;

   if(s1 == NULL || s2 == NULL || n == 0)
      return DmtxFalse;

   for(i = 0; i < n; i++) {
      if(tolower(s1[i]) != tolower(s2[i]))
         return DmtxFalse;
      if(s1[i] == '\0' || s2[i] == '\0')
         break;
   }

   return DmtxTrue;
}

/* Common Utility Functions */
/**
 *
 *
 */
void
CleanupMagick(MagickWand **wand, int magickError)
{
   char *excMessage;
   ExceptionType excSeverity;

   if(magickError == DmtxTrue) {
      excMessage = MagickGetException(*wand, &excSeverity);
      fprintf(stderr, "%s %s %lu %s\n", GetMagickModule(), excMessage);
      MagickRelinquishMemory(excMessage);
   }

   if(*wand != NULL) {
      DestroyMagickWand(*wand);
      *wand = NULL;
   }
}

/**
 * @brief  List supported input image formats on stdout
 * @return void
 */
void
ListImageFormats(void)
{
   int i, index;
   int row, rowCount;
   int col, colCount;
   unsigned long totalCount;
   char **list;

   list = MagickQueryFormats("*", &totalCount);

   if(list == NULL)
      return;

   fprintf(stdout, "\n");

   colCount = 7;
   rowCount = totalCount/colCount;
   if(totalCount % colCount)
      rowCount++;

   for(i = 0; i < colCount * rowCount; i++) {
      col = i%colCount;
      row = i/colCount;
      index = col*rowCount + row;
      fprintf(stdout, "%10s", (index < totalCount) ? list[col*rowCount+row] : " ");
      fprintf(stdout, "%s", (col+1 < colCount) ? " " : "\n");
   }
   fprintf(stdout, "\n");

   MagickRelinquishMemory(list);
}
