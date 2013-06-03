#include "libpng.h"
#include <png.h>
#include <iostream>

#pragma comment( lib, "libpng15.lib" )
#pragma comment( lib, "zlib.lib" )

bool load_png(const char *name, int &outWidth, int &outHeight, bool &outHasAlpha,int &outDepth,int &outPitch, unsigned char **outData)
{
    png_structp png_ptr;
    png_infop info_ptr;
    unsigned int sig_read = 0;
    int color_type, interlace_type;
    FILE *fp;
 
    if ((fp = fopen(name, "rb")) == NULL)
        return false;
 
    /* Create and initialize the png_struct
     * with the desired error handler
     * functions.  If you want to use the
     * default stderr and longjump method,
     * you can supply NULL for the last
     * three parameters.  We also supply the
     * the compiler header file version, so
     * that we know if the application
     * was compiled with a compatible version
     * of the library.  REQUIRED
     */
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
            NULL, NULL, NULL);
 
    if (png_ptr == NULL) {
        fclose(fp);
        return false;
    }
 
    /* Allocate/initialize the memory
     * for image information.  REQUIRED. */
    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        fclose(fp);
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return false;
    }
 
    /* Set error handling if you are
     * using the setjmp/longjmp method
     * (this is the normal method of
     * doing things with libpng).
     * REQUIRED unless you  set up
     * your own error handlers in
     * the png_create_read_struct()
     * earlier.
     */
  
    /* Set up the output control if
     * you are using standard C streams */
    png_init_io(png_ptr, fp);
 
    /* If we have already
     * read some of the signature */
    //png_set_sig_bytes(png_ptr, sig_read);
 
    /*
     * If you have enough memory to read
     * in the entire image at once, and
     * you need to specify only
     * transforms that can be controlled
     * with one of the PNG_TRANSFORM_*
     * bits (this presently excludes
     * dithering, filling, setting
     * background, and doing gamma
     * adjustment), then you can read the
     * entire image (including pixels)
     * into the info structure with this
     * call
     *
     * PNG_TRANSFORM_STRIP_16 |
     * PNG_TRANSFORM_PACKING  forces 8 bit
     * PNG_TRANSFORM_EXPAND forces to
     *  expand a palette into RGB
     */
    png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND, NULL);

	//png_read_info(png_ptr, info_ptr);

	{
		int color_type;
		int compression_type, filter_method;
		png_get_IHDR(png_ptr, info_ptr, (png_uint_32*)&outWidth, (png_uint_32*)&outHeight,
		&outDepth, &color_type, &interlace_type,
		&compression_type, &filter_method);
			
		switch (color_type)
		{
        case PNG_COLOR_TYPE_RGBA:
            outHasAlpha = true;
			outDepth *= 4;
            break;
        case PNG_COLOR_TYPE_RGB:
            outHasAlpha = false;
			outDepth *= 3;
            break;
        default:
            std::cout << "Color type " << color_type << " not supported" << std::endl;
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
            fclose(fp);
            return false;
		}
	}

    unsigned int row_bytes = png_get_rowbytes(png_ptr, info_ptr);
    *outData = (unsigned char*) malloc(row_bytes * outHeight);

	outPitch = row_bytes;
 
    png_bytepp row_pointers = png_get_rows(png_ptr, info_ptr);
 
	
    for (int i = 0; i < outHeight; i++)
	{
        // note that png is ordered top to
        // bottom, but OpenGL expect it bottom to top
        // so the order or swapped
        //memcpy(*outData+(row_bytes * (outHeight-1-i)), row_pointers[i], row_bytes);
		
		for(int j = 0; j < outWidth; j++)
		{
			float alpha = (float)(row_pointers[i] + j*(outDepth/8))[3]/255;
			// RGBA => BGRA
			(*outData + row_bytes*i + j*(outDepth/8))[0] = (row_pointers[i] + j*(outDepth/8))[3];		//A
			(*outData + row_bytes*i + j*(outDepth/8))[1] = (row_pointers[i] + j*(outDepth/8))[2]*alpha;	//B
			(*outData + row_bytes*i + j*(outDepth/8))[2] = (row_pointers[i] + j*(outDepth/8))[1]*alpha;	//G
			(*outData + row_bytes*i + j*(outDepth/8))[3] = (row_pointers[i] + j*(outDepth/8))[0]*alpha;	//R
		}
		
		//memcpy(*outData + row_bytes*i,row_pointers[i],row_bytes);
    }

    /* Clean up after the read,
     * and free any memory allocated */
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
 
    /* Close the file */
    fclose(fp);
 
    /* That's it */
    return true;
}
