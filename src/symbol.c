/***************************************************************************
Dmtx-Daemon
Copyright (C) 2009, M Omar Faruque Sarker

libdmtx - Data Matrix Encoding/Decoding Library
Copyright (C) 2008, 2009 Mike Laughton
Copyright (C) 2008 Ryan Raasch
Copyright (C) 2008 Olivier Guilyardi

 **************************************************************************
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

int symbol_decode( char *infile, char *outfile )
{

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

   return imgScanCount;
}

/* Internal functions */
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
            /* ShowDecodeUsage(EX_OK); */
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

   fclose(fp);
   return DmtxPass;
}

void WriteDiagnosticImage(DmtxDecode *dec, char *imagePath)
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


int ScaleNumberString(char *s, int extent)
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



/* Common Utility Functions */
void CleanupMagick(MagickWand **wand, int magickError)
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
