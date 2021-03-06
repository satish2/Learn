/**
 * imgjpeg.c - interface for libjpeg GIF input routines
 *
 * Copyright (c) 1998
 *      Transvirtual Technologies, Inc.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution 
 * of this file. 
 */

#if NO_NEED

#include "toolkit.h"
#include "config-setjmp.h"

#if defined(HAVE_JPEGLIB_H) && defined(HAVE_LIBJPEG)
#define INCLUDE_JPEG 1
#endif

#if defined(INCLUDE_JPEG)
#include "jpeglib.h"
#include "jerror.h"
#endif


#define MAX_JPEG_COLORS 256

/*
 * Hmmm, libjpeg uses explicit indices to access colormap components. We assume
 * the same values like in rdgif.h (which isn't part of jpeglib-6b anymore).
 * We should add a check for a RGB colormap / organization. 
 */
#define CM_RED		0
#define CM_GREEN	1
#define CM_BLUE		2

/* references to img.c */
Image* createImage ( int width, int height );
void Java_java_awt_Toolkit_imgFreeImage( JNIEnv* env, jclass clazz, Image * img);
void createXImage ( Toolkit* X, Image* img );


#if defined(INCLUDE_JPEG)

/*******************************************************************************************
 * required typedefs and structs
 */

struct error_mgr {
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
};

typedef struct error_mgr * error_ptr;

METHODDEF(void)
error_exit ( j_common_ptr cinfo)
{
  error_ptr myerr = (error_ptr) cinfo->err;
  (*cinfo->err->output_message)(cinfo);
  longjmp(myerr->setjmp_buffer, 1);
}

typedef struct {
  struct jpeg_source_mgr pub;	/* public fields */

  long   buflen;       /* length of external buffer */
  JOCTET * buffer;		/* start of buffer */

  boolean start_of_file;	/* have we gotten any data yet? */
} bufsrc_mgr;

typedef bufsrc_mgr * bufsrc_ptr;


/*******************************************************************************************
 * memory buffer extension for the standard jpeg lib input management (jdatasrc.c)
 */

METHODDEF(void)
init_buf_source (j_decompress_ptr cinfo)
{
  bufsrc_ptr src = (bufsrc_ptr) cinfo->src;

  /* We reset the empty-input-file flag for each image,
   * but we don't clear the input buffer.
   * This is correct behavior for reading a series of images from one source.
   */
  src->start_of_file = TRUE;
}


static JOCTET end_marker[2] = { (JOCTET) 0xFF, (JOCTET) JPEG_EOI };

METHODDEF(boolean)
get_buf_source (j_decompress_ptr cinfo)
{
  bufsrc_ptr src = (bufsrc_ptr) cinfo->src;

  if ( src->pub.next_input_byte && (src->start_of_file == FALSE) ) {
    WARNMS(cinfo, JWRN_JPEG_EOF);
    src->pub.next_input_byte = end_marker;
	src->pub.bytes_in_buffer = sizeof(end_marker);
  }
  else {                    /* we read in one seep (no need to use two buffers) */
	src->pub.next_input_byte = src->buffer;
	src->pub.bytes_in_buffer = src->buflen;
	src->start_of_file = FALSE;
  }

  return TRUE;
}


METHODDEF(void)
skip_buf_source ( j_decompress_ptr cinfo, long num_bytes )
{
  bufsrc_ptr src = (bufsrc_ptr) cinfo->src;

  if ( src->start_of_file == TRUE )
	get_buf_source( cinfo);

  src->pub.next_input_byte += (size_t) num_bytes;
  src->pub.bytes_in_buffer -= (size_t) num_bytes;
}


METHODDEF(void)
term_buf_source (j_decompress_ptr cinfo)
{
  bufsrc_ptr src = (bufsrc_ptr) cinfo->src;

  /*
   * Take care the (external) buffer isn't deleted by jpeg_destroy_decompress.
   * This shouldn't be necessary since jpeg_destroy cleans up by means of the 
   * jpeg memory manager, but we better play it safe
   */
  src->buffer = 0;
}


GLOBAL(void)
jpeg_buffer_src (j_decompress_ptr cinfo, unsigned char* buf, long len )
{
  bufsrc_ptr src;

  if (cinfo->src == NULL) {	/* first time for this JPEG object? */
    cinfo->src = (struct jpeg_source_mgr *)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
				  sizeof(bufsrc_mgr));
  }

  src = (bufsrc_ptr) cinfo->src;
  src->pub.init_source = init_buf_source;
  src->pub.fill_input_buffer = get_buf_source;
  src->pub.skip_input_data = skip_buf_source;
  src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
  src->pub.term_source = term_buf_source;

  src->pub.bytes_in_buffer = 0; /* forces fill_input_buffer on first read */
  src->pub.next_input_byte = NULL; /* until buffer loaded */

  src->buffer = buf;
  src->buflen = len;
}



/************************************************************************************
 * JPEG auxiliary funcs (production callbacks)
 */

void
jscan_to_img( Image * img, JSAMPROW buf, struct jpeg_decompress_struct * cinfo)
{
  register int col, pix, rgb, idx;
  register JSAMPARRAY colormap = cinfo->colormap;

  for ( col = 0; col < cinfo->output_width; col++) {
    idx = *buf++;
    rgb = (colormap[CM_RED][idx] << 16) | (colormap[CM_GREEN][idx] << 8) | colormap[CM_BLUE][idx];    
    pix = pixelValue( X, rgb);
    XPutPixel( img->xImg, col, cinfo->output_scanline-1, pix);
  }
}


/************************************************************************************
 * Jpeg production
 */

Image*
readJpeg ( struct jpeg_decompress_struct* cinfo, int colors )
{
  struct error_mgr jerr;
  JSAMPARRAY buffer;
  Image      *img = 0;
  int        row_stride;

  cinfo->err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = error_exit;

  if ( setjmp(jerr.setjmp_buffer)) {
	if ( img )
	  Java_java_awt_Toolkit_imgFreeImage( 0, 0, img);
    return 0;
  }

  jpeg_read_header( cinfo, TRUE);

  if ( colors < 8 )
    colors = 8;
  else if ( colors > 256 )
    colors = 256;

  cinfo->desired_number_of_colors = colors;
  cinfo->quantize_colors = TRUE;
  cinfo->out_color_space = JCS_RGB;

  jpeg_start_decompress( cinfo);
  row_stride = cinfo->output_width * cinfo->output_components;

  /* it's time to create the target image */
  img = createImage( cinfo->output_width, cinfo->output_height);
  createXImage( X, img);

  buffer = (*cinfo->mem->alloc_sarray)((j_common_ptr) cinfo, JPOOL_IMAGE, row_stride, 1);

  while (cinfo->output_scanline < cinfo->output_height) {
    jpeg_read_scanlines( cinfo, buffer, 1);
    jscan_to_img( img, buffer[0], cinfo);
  }

  jpeg_finish_decompress( cinfo);

  return img;
}

#endif /* INCLUDE_JPEG */


/**************************************************************************************
 * these are the "exported" production interface functions
 */

Image*
readJpegFile ( FILE* infile )
{
  Image *img = 0;

#if defined(INCLUDE_JPEG)
  struct jpeg_decompress_struct cinfo;

  jpeg_create_decompress( &cinfo);
  jpeg_stdio_src( &cinfo, infile);

  img = readJpeg( &cinfo, MAX_JPEG_COLORS);

  jpeg_destroy_decompress( &cinfo);
#endif /* INCLUDE_JPEG */

  return img;
}


Image*
readJpegData ( unsigned char* buf, long len )
{
  Image *img = 0;

#if defined(INCLUDE_JPEG)
  struct jpeg_decompress_struct cinfo;

  jpeg_create_decompress( &cinfo);
  jpeg_buffer_src( &cinfo, buf, len);

  img = readJpeg( &cinfo, MAX_JPEG_COLORS);

  jpeg_destroy_decompress( &cinfo);
#endif /* INCLUDE_JPEG */

  return img;
}

#endif /* NO_NEED */