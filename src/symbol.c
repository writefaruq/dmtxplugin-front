/***************************************************************************
 *       Dmtx-Plugin-Front                                                 *
 *       Author: Md Omar Faruque Sarker <writefaruq@gmail.com>             *
 *       libdmtx - Data Matrix Encoding/Decoding Library                   *
 *       Copyright (C) 2008, 2009 Mike Laughton                            *
 *       Copyright (C) 2008 Ryan Raasch                                    *
 *       Copyright (C) 2008 Olivier Guilyardi                              *
 *                                                                         *
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

int symbol_decode(char *infile, char *outfile)
{

   char *file_path;
   int err;
   int img_page_index;
   int img_scan_count = 0, page_scan_count = 0;
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

   opt = get_default_decode_options();

   MagickWandGenesis();

  /* Open image from file or stream (might contain multiple pages) */
  file_path = infile;

  wand = NewMagickWand();
  if (wand == NULL) {
     fatal_error(EX_OSERR, "Magick error");
  }

  if (opt.dpi != DmtxUndefined) {
     success = MagickSetResolution(wand, (double)opt.dpi, (double)opt.dpi);
     if (success == MagickFalse) {
        cleanup_magick(&wand, DmtxTrue);
        fatal_error(EX_OSERR, "Magick error");
     }
  }

  success = MagickReadImage(wand, file_path);
  if (success == MagickFalse) {
     cleanup_magick(&wand, DmtxTrue);
     fatal_error(EX_OSERR, "Magick error");
  }

  width = MagickGetImageWidth(wand);
  height = MagickGetImageHeight(wand);

  /* Loop once for each page within image */
  MagickResetIterator(wand);
  for (img_page_index = 0; MagickNextImage(wand) != MagickFalse; img_page_index++) {

     /* Reset timeout for each new page */
     if (opt.timeout != DmtxUndefined)
        timeout = dmtxTimeAdd(dmtxTimeNow(), opt.timeout);

     /* Allocate memory for pixel data */
     pxl = (unsigned char *)malloc(3 * width * height * sizeof(unsigned char));
     if (pxl == NULL) {
        cleanup_magick(&wand, DmtxFalse);
        fatal_error(EX_OSERR, "malloc() error");
     }

     /* Copy pixels to known format */
     success = MagickGetImagePixels(wand, 0, 0, width, height, "RGB", CharPixel, pxl);
     if (success == MagickFalse || pxl == NULL) {
        cleanup_magick(&wand, DmtxTrue);
        fatal_error(EX_OSERR, "malloc() error");
     }

     /* Initialize libdmtx image */
     img = dmtxImageCreate(pxl, width, height, DmtxPack24bppRGB);
     if (img == NULL) {
        cleanup_magick(&wand, DmtxFalse);
        fatal_error(EX_SOFTWARE, "dmtxImageCreate() error");
     }

     dmtxImageSetProp(img, DmtxPropImageFlip, DmtxFlipNone);

     /* Initialize scan */
     dec = dmtxDecodeCreate(img, opt.shrink_min);
     if (dec == NULL) {
        cleanup_magick(&wand, DmtxFalse);
        fatal_error(EX_SOFTWARE, "decode create error");
     }

     err = set_decode_options(dec, img, &opt);
     if (err != DmtxPass) {
        cleanup_magick(&wand, DmtxFalse);
        fatal_error(EX_SOFTWARE, "decode option error");
     }

     /* Find and decode every barcode on page */
     page_scan_count = 0;
     for (;;) {
        /* Find next barcode region within image, but do not decode yet */
        if (opt.timeout == DmtxUndefined)
           reg = dmtxRegionFindNext(dec, NULL);
        else
           reg = dmtxRegionFindNext(dec, &timeout);

        /* Finished file or ran out of time before finding another region */
        if (reg == NULL)
           break;

        /* Decode region based on requested barcode mode */
        if (opt.mosaic == DmtxTrue)
           msg = dmtxDecodeMosaicRegion(dec, reg, opt.corrections_max);
        else
           msg = dmtxDecodeMatrixRegion(dec, reg, opt.corrections_max);

        if (msg != NULL) {
           print_stats(dec, reg, msg, img_page_index, &opt);
           print_message(reg, msg, &opt, outfile);

           page_scan_count++;
           img_scan_count++;

           dmtxMessageDestroy(&msg);
        }

        dmtxRegionDestroy(&reg);

        if (opt.stop_after != DmtxUndefined && img_scan_count >= opt.stop_after)
           break;
     }

     if(opt.diagnose == DmtxTrue)
        write_diagnostic_image(dec, "debug.pnm");

     dmtxDecodeDestroy(&dec);
     dmtxImageDestroy(&img);
     free(pxl);
  }

  cleanup_magick(&wand, DmtxFalse);

   MagickWandTerminus();

   return img_scan_count;
}

/* Internal functions */
DecodeOptions get_default_decode_options(void)
{
   DecodeOptions opt;

   memset(&opt, 0x00, sizeof(DecodeOptions));

   /* Default options */
   opt.codewords = DmtxFalse;
   opt.edgemin = DmtxUndefined;
   opt.edgemax = DmtxUndefined;
   opt.scangap = 2;
   opt.timeout = DmtxUndefined;
   opt.newline = DmtxFalse;
   opt.page = DmtxUndefined;
   opt.square_devn = DmtxUndefined;
   opt.dpi = DmtxUndefined;
   opt.size_idx_expected = DmtxSymbolShapeAuto;
   opt.edge_thresh = 5;
   opt.xmin = NULL;
   opt.xmax = NULL;
   opt.ymin = NULL;
   opt.ymax = NULL;
   opt.corrections_max = DmtxUndefined;
   opt.diagnose = DmtxFalse;
   opt.mosaic = DmtxFalse;
   opt.stop_after = DmtxUndefined;
   opt.page_numbers = DmtxFalse;
   opt.corners = DmtxFalse;
   opt.shrink_min = 1;
   opt.shrink_max = 1;
   opt.unicode = DmtxFalse;
   opt.verbose = DmtxFalse;

   return opt;
}

DmtxPassFail set_decode_options(DmtxDecode *dec, DmtxImage *img, DecodeOptions *opt)
{
   int err;

#define RETURN_IF_FAILED(e) if(e != DmtxPass) { return DmtxFail; }

   err = dmtxDecodeSetProp(dec, DmtxPropScanGap, opt->scangap);
   RETURN_IF_FAILED(err)

   if (opt->edgemin != DmtxUndefined) {
      err = dmtxDecodeSetProp(dec, DmtxPropEdgeMin, opt->edgemin);
      RETURN_IF_FAILED(err)
   }

   if (opt->edgemax != DmtxUndefined) {
      err = dmtxDecodeSetProp(dec, DmtxPropEdgeMax, opt->edgemax);
      RETURN_IF_FAILED(err)
   }

   if (opt->square_devn != DmtxUndefined) {
      err = dmtxDecodeSetProp(dec, DmtxPropSquareDevn, opt->square_devn);
      RETURN_IF_FAILED(err)
   }

   err = dmtxDecodeSetProp(dec, DmtxPropSymbolSize, opt->size_idx_expected);
   RETURN_IF_FAILED(err)

   err = dmtxDecodeSetProp(dec, DmtxPropEdgeThresh, opt->edge_thresh);
   RETURN_IF_FAILED(err)

   if (opt->xmin) {
      err = dmtxDecodeSetProp(dec, DmtxPropXmin, scale_number_string(opt->xmin, img->width));
      RETURN_IF_FAILED(err)
   }

   if (opt->xmax) {
      err = dmtxDecodeSetProp(dec, DmtxPropXmax, scale_number_string(opt->xmax, img->width));
      RETURN_IF_FAILED(err)
   }

   if (opt->ymin) {
      err = dmtxDecodeSetProp(dec, DmtxPropYmin, scale_number_string(opt->ymin, img->height));
      RETURN_IF_FAILED(err)
   }

   if (opt->ymax) {
      err = dmtxDecodeSetProp(dec, DmtxPropYmax, scale_number_string(opt->ymax, img->height));
      RETURN_IF_FAILED(err)
   }

#undef RETURN_IF_FAILED

   return DmtxPass;
}

DmtxPassFail print_stats(DmtxDecode *dec, DmtxRegion *reg, DmtxMessage *msg,
      int img_page_index, DecodeOptions *opt)
{
   int height;
   int data_word_length;
   int rotate_int;
   double rotate;
   DmtxVector2 p00, p10, p11, p01;

   height = dmtxDecodeGetProp(dec, DmtxPropHeight);

   p00.X = p00.Y = p10.Y = p01.X = 0.0;
   p10.X = p01.Y = p11.X = p11.Y = 1.0;
   dmtxMatrix3VMultiplyBy(&p00, reg->fit2raw);
   dmtxMatrix3VMultiplyBy(&p10, reg->fit2raw);
   dmtxMatrix3VMultiplyBy(&p11, reg->fit2raw);
   dmtxMatrix3VMultiplyBy(&p01, reg->fit2raw);

   data_word_length = dmtxGetSymbolAttribute(DmtxSymAttribSymbolDataWords, reg->sizeIdx);
   if (opt->verbose == DmtxTrue) {

      rotate = (2 * M_PI) + (atan2(reg->fit2raw[0][1], reg->fit2raw[1][1]) -
            atan2(reg->fit2raw[1][0], reg->fit2raw[0][0])) / 2.0;

      rotate_int = (int)(rotate * 180/M_PI + 0.5);
      if(rotate_int >= 360)
         rotate_int -= 360;

      fprintf(stdout, "--------------------------------------------------\n");
      fprintf(stdout, "       Matrix Size: %d x %d\n",
            dmtxGetSymbolAttribute(DmtxSymAttribSymbolRows, reg->sizeIdx),
            dmtxGetSymbolAttribute(DmtxSymAttribSymbolCols, reg->sizeIdx));
      fprintf(stdout, "    Data Codewords: %d (capacity %d)\n",
            data_word_length - msg->padCount, data_word_length);
      fprintf(stdout, "   Error Codewords: %d\n",
            dmtxGetSymbolAttribute(DmtxSymAttribSymbolErrorWords, reg->sizeIdx));
      fprintf(stdout, "      Data Regions: %d x %d\n",
            dmtxGetSymbolAttribute(DmtxSymAttribHorizDataRegions, reg->sizeIdx),
            dmtxGetSymbolAttribute(DmtxSymAttribVertDataRegions, reg->sizeIdx));
      fprintf(stdout, "Interleaved Blocks: %d\n",
            dmtxGetSymbolAttribute(DmtxSymAttribInterleavedBlocks, reg->sizeIdx));
      fprintf(stdout, "    Rotation Angle: %d\n", rotate_int);
      fprintf(stdout, "          Corner 0: (%0.1f, %0.1f)\n", p00.X, height - 1 - p00.Y);
      fprintf(stdout, "          Corner 1: (%0.1f, %0.1f)\n", p10.X, height - 1 - p10.Y);
      fprintf(stdout, "          Corner 2: (%0.1f, %0.1f)\n", p11.X, height - 1 - p11.Y);
      fprintf(stdout, "          Corner 3: (%0.1f, %0.1f)\n", p01.X, height - 1 - p01.Y);
      fprintf(stdout, "--------------------------------------------------\n");
   }

   if (opt->page_numbers == DmtxTrue)
      fprintf(stdout, "%d:", img_page_index + 1);

   if (opt->corners == DmtxTrue) {
      fprintf(stdout, "%d,%d:", (int)(p00.X + 0.5), height - 1 - (int)(p00.Y + 0.5));
      fprintf(stdout, "%d,%d:", (int)(p10.X + 0.5), height - 1 - (int)(p10.Y + 0.5));
      fprintf(stdout, "%d,%d:", (int)(p11.X + 0.5), height - 1 - (int)(p11.Y + 0.5));
      fprintf(stdout, "%d,%d:", (int)(p01.X + 0.5), height - 1 - (int)(p01.Y + 0.5));
   }

   return DmtxPass;
}

DmtxPassFail
print_message(DmtxRegion *reg, DmtxMessage *msg, DecodeOptions *opt, const char *outfile)
{
   unsigned int i;
   int remaining_data_words;
   int data_word_length;

    FILE *fp;

    fp = fopen(outfile, "wb");
    if (fp == NULL) {
        perror("failed to open output file");
        return EXIT_FAILURE;
    }

   if (opt->codewords == DmtxTrue) {
      data_word_length = dmtxGetSymbolAttribute(DmtxSymAttribSymbolDataWords, reg->sizeIdx);
      for (i = 0; i < msg->codeSize; i++) {
         remaining_data_words = data_word_length - i;
         if (remaining_data_words > msg->padCount)
            fprintf(fp, "%c:%03d\n", 'd', msg->code[i]);
         else if (remaining_data_words > 0)
            fprintf(fp, "%c:%03d\n", 'p', msg->code[i]);
         else
            fprintf(fp, "%c:%03d\n", 'e', msg->code[i]);
      }
   } else {
        if (opt->unicode == DmtxTrue) {
                for (i = 0; i < msg->outputIdx; i++) {
                        if (msg->output[i] < 128) {
                                fputc(msg->output[i], fp);
                        } else if (msg->output[i] < 192) {
                                fputc(0xc2, fp);
                                fputc(msg->output[i], fp);
                        } else {
                                fputc(0xc3, fp);
                                fputc(msg->output[i] - 64, fp);
                        }
                }
      } else {
                (void ) fwrite(msg->output, sizeof(char), msg->outputIdx, fp);
      }

      if (opt->newline)
         fputc('\n', fp);
   }

   fclose(fp);
   return DmtxPass;
}

void write_diagnostic_image(DmtxDecode *dec, char *imagePath)
{
   int total_bytes, header_bytes;
   int bytes_written;
   unsigned char *pnm;
   FILE *fp;

   fp = fopen(imagePath, "wb");
   if (fp == NULL) {
      perror(programName);
      fatal_error(EX_CANTCREAT, _("Unable to create image \"%s\""), imagePath);
   }

   pnm = dmtxDecodeCreateDiagnostic(dec, &total_bytes, &header_bytes, 0);
   if (pnm == NULL)
      fatal_error(EX_OSERR, _("Unable to create diagnostic image"));

   bytes_written = fwrite(pnm, sizeof(unsigned char), total_bytes, fp);
   if (bytes_written != total_bytes)
      fatal_error(EX_IOERR, _("Unable to write diagnostic image"));

   free(pnm);
   fclose(fp);
}

int scale_number_string(char *s, int extent)
{
   int err;
   int numValue;
   int scaled_value;
   char *terminate;

   assert(s != NULL);

   err = string_to_int(&numValue, s, &terminate);
   if (err != DmtxPass)
      fatal_error(EX_USAGE, _("Integer value required"));

   scaled_value = (*terminate == '%') ? (int)(0.01 * numValue * extent + 0.5) : numValue;

   if (scaled_value < 0)
      scaled_value = 0;

   if(scaled_value >= extent)
      scaled_value = extent - 1;

   return scaled_value;
}

void cleanup_magick(MagickWand **wand, int magickError)
{
   char *exc_message;
   ExceptionType exc_severity;

   if(magickError == DmtxTrue) {
      exc_message = MagickGetException(*wand, &exc_severity);
      fprintf(stderr, "%s %s %lu %s\n", GetMagickModule(), exc_message);
      MagickRelinquishMemory(exc_message);
   }

   if(*wand != NULL) {
      DestroyMagickWand(*wand);
      *wand = NULL;
   }
}
