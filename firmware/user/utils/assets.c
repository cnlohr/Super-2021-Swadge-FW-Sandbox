#include <stdarg.h>
#include <osapi.h>
#include <mem.h>
#include "assets.h"
#include "oled.h"
#include "fastlz.h"
#include "user_main.h"
#include "printControl.h"
#if defined(EMU)
    #include <stdio.h>
    #ifdef ANDROID
        #include <asset_manager.h>
        #include <asset_manager_jni.h>
        #include <android_native_app_glue.h>
        struct android_app* gapp;
    #endif
    uint32_t* assets = NULL;
#endif

#if defined(FEATURE_OLED)

const uint32_t sin1024[] RODATA_ATTR =
{
    0, 18, 36, 54, 71, 89, 107, 125, 143, 160, 178, 195, 213,
    230, 248, 265, 282, 299, 316, 333, 350, 367, 384, 400, 416, 433, 449, 465, 481,
    496, 512, 527, 543, 558, 573, 587, 602, 616, 630, 644, 658, 672, 685, 698, 711,
    724, 737, 749, 761, 773, 784, 796, 807, 818, 828, 839, 849, 859, 868, 878, 887,
    896, 904, 912, 920, 928, 935, 943, 949, 956, 962, 968, 974, 979, 984, 989, 994,
    998, 1002, 1005, 1008, 1011, 1014, 1016, 1018, 1020, 1022, 1023, 1023, 1024,
    1024, 1024, 1023, 1023, 1022, 1020, 1018, 1016, 1014, 1011, 1008, 1005, 1002, 998,
    994, 989, 984, 979, 974, 968, 962, 956, 949, 943, 935, 928, 920, 912, 904, 896,
    887, 878, 868, 859, 849, 839, 828, 818, 807, 796, 784, 773, 761, 749, 737, 724,
    711, 698, 685, 672, 658, 644, 630, 616, 602, 587, 573, 558, 543, 527, 512, 496,
    481, 465, 449, 433, 416, 400, 384, 367, 350, 333, 316, 299, 282, 265, 248, 230,
    213, 195, 178, 160, 143, 125, 107, 89, 71, 54, 36, 18, 0, -18, -36, -54, -71,
    -89, -107, -125, -143, -160, -178, -195, -213, -230, -248, -265, -282, -299, -316,
    -333, -350, -367, -384, -400, -416, -433, -449, -465, -481, -496, -512, -527,
    -543, -558, -573, -587, -602, -616, -630, -644, -658, -672, -685, -698, -711,
    -724, -737, -749, -761, -773, -784, -796, -807, -818, -828, -839, -849, -859, -868,
    -878, -887, -896, -904, -912, -920, -928, -935, -943, -949, -956, -962, -968,
    -974, -979, -984, -989, -994, -998, -1002, -1005, -1008, -1011, -1014, -1016,
    -1018, -1020, -1022, -1023, -1023, -1024, -1024, -1024, -1023, -1023, -1022, -1020,
    -1018, -1016, -1014, -1011, -1008, -1005, -1002, -998, -994, -989, -984, -979,
    -974, -968, -962, -956, -949, -943, -935, -928, -920, -912, -904, -896, -887,
    -878, -868, -859, -849, -839, -828, -818, -807, -796, -784, -773, -761, -749,
    -737, -724, -711, -698, -685, -672, -658, -644, -630, -616, -602, -587, -573, -558,
    -543, -527, -512, -496, -481, -465, -449, -433, -416, -400, -384, -367, -350,
    -333, -316, -299, -282, -265, -248, -230, -213, -195, -178, -160, -143, -125,
    -107, -89, -71, -54, -36, -18
};

const uint32_t tan1024[] RODATA_ATTR =
{
    0, 18, 36, 54, 72, 90, 108, 126, 144, 162, 181, 199, 218, 236, 255, 274, 294,
    313, 333, 353, 373, 393, 414, 435, 456, 477, 499, 522, 544, 568, 591, 615,
    640, 665, 691, 717, 744, 772, 800, 829, 859, 890, 922, 955, 989, 1024, 1060,
    1098, 1137, 1178, 1220, 1265, 1311, 1359, 1409, 1462, 1518, 1577, 1639, 1704,
    1774, 1847, 1926, 2010, 2100, 2196, 2300, 2412, 2534, 2668, 2813, 2974,
    3152, 3349, 3571, 3822, 4107, 4435, 4818, 5268, 5807, 6465, 7286, 8340, 9743,
    11704, 14644, 19539, 29324, 58665, 67108863, -58665, -29324, -19539, -14644,
    -11704, -9743, -8340, -7286, -6465, -5807, -5268, -4818, -4435, -4107, -3822,
    -3571, -3349, -3152, -2974, -2813, -2668, -2534, -2412, -2300, -2196, -2100,
    -2010, -1926, -1847, -1774, -1704, -1639, -1577, -1518, -1462, -1409,
    -1359, -1311, -1265, -1220, -1178, -1137, -1098, -1060, -1024, -989, -955,
    -922, -890, -859, -829, -800, -772, -744, -717, -691, -665, -640, -615, -591,
    -568, -544, -522, -499, -477, -456, -435, -414, -393, -373, -353, -333, -313,
    -294, -274, -255, -236, -218, -199, -181, -162, -144, -126, -108, -90, -72,
    -54, -36, -18, 0, 18, 36, 54, 72, 90, 108, 126, 144, 162, 181, 199, 218,
    236, 255, 274, 294, 313, 333, 353, 373, 393, 414, 435, 456, 477, 499, 522,
    544, 568, 591, 615, 640, 665, 691, 717, 744, 772, 800, 829, 859, 890, 922, 955,
    989, 1024, 1060, 1098, 1137, 1178, 1220, 1265, 1311, 1359, 1409, 1462,
    1518, 1577, 1639, 1704, 1774, 1847, 1926, 2010, 2100, 2196, 2300, 2412, 2534,
    2668, 2813, 2974, 3152, 3349, 3571, 3822, 4107, 4435, 4818, 5268, 5807, 6465,
    7286, 8340, 9743, 11704, 14644, 19539, 29324, 58665, 67108863, -58665, -29324,
    -19539, -14644, -11704, -9743, -8340, -7286, -6465, -5807, -5268, -4818,
    -4435, -4107, -3822, -3571, -3349, -3152, -2974, -2813, -2668, -2534, -2412,
    -2300, -2196, -2100, -2010, -1926, -1847, -1774, -1704, -1639, -1577, -1518,
    -1462, -1409, -1359, -1311, -1265, -1220, -1178, -1137, -1098, -1060, -1024,
    -989, -955, -922, -890, -859, -829, -800, -772, -744, -717, -691, -665,
    -640, -615, -591, -568, -544, -522, -499, -477, -456, -435, -414, -393, -373,
    -353, -333, -313, -294, -274, -255, -236, -218, -199, -181, -162, -144, -126,
    -108, -90, -72, -54, -36, -18
};

void ICACHE_FLASH_ATTR gifTimerFn(void* arg);
void ICACHE_FLASH_ATTR transformPixel(int16_t* x, int16_t* y, int16_t transX,
                                      int16_t transY, bool flipLR, bool flipUD,
                                      int16_t rotateDeg, int16_t width, int16_t height);

/**
 * @brief Get a pointer to an asset
 *
 * @param name   The name of the asset to fetch
 * @param retLen A pointer to a uint32_t where the asset length will be written
 * @return A pointer to the asset, or NULL if not found
 */
uint32_t* ICACHE_FLASH_ATTR getAsset(const char* name, uint32_t* retLen)
{
#if !defined(EMU)
    /* Note assets are placed immediately after irom0
     * See "irom0_0_seg" in "eagle.app.v6.ld" for where this value comes from
     * The makefile flashes ASSETS_FILE to 0x6C000
     */
    uint32_t* assets = (uint32_t*)(0x40200000 + ASSETS_ADDR);
#elif defined( ANDROID )
    if( !assets )
    {
        AAsset* file = AAssetManager_open( gapp->activity->assetManager, "assets.bin", AASSET_MODE_BUFFER );
        if( file )
        {
            assets = AAsset_getBuffer( file );
            //size_t fileLength = AAsset_getLength(file);
            //assets = malloc( fileLength + 1);
            //memcpy( assets, AAsset_getBuffer( file ), fileLength );
        }
        else
        {
            return NULL;
        }
    }
#else
    /* When emulating a swadge, assets are read directly from a file */
    if(NULL == assets)
    {
        FILE* fp = fopen( "assets.bin", "rb" );
        if( NULL == fp )
        {
            fprintf( stderr, "EMU Error: Could not open assets.bin\n" );
            return NULL;
        }
        fseek(fp, 0L, SEEK_END);
        long sz = ftell(fp);
        assets = (uint32_t*)malloc(sz);
        fseek(fp, 0L, SEEK_SET);
        int r = fread(assets, sz, 1, fp);
        if( r != 1 )
        {
            fprintf( stderr, "EMU Error: read error with assets.bin\n" );
            return NULL;
        }
        fclose(fp);
    }
#endif
    uint32_t idx = 0;
    uint32_t numIndexItems = assets[idx++];
    AST_PRINTF("Scanning %d items\n", numIndexItems);

    for(uint32_t ni = 0; ni < numIndexItems; ni++)
    {
        // Read the name from the index
        char assetName[16] = {0};
        ets_memcpy(assetName, &assets[idx], sizeof(uint32_t) * 4);
        idx += 4;

        // Read the address from the index
        uint32_t assetAddress = assets[idx++];

        // Read the length from the index
        uint32_t assetLen = assets[idx++];

        AST_PRINTF("%s, addr: %d, len: %d\n", assetName, assetAddress, assetLen);

        // Compare names
        if(0 == ets_strcmp(name, assetName))
        {
            AST_PRINTF("Found asset\n");
            *retLen = assetLen;
            return &assets[assetAddress / sizeof(uint32_t)];
        }
    }
    *retLen = 0;
    return NULL;
}

#if defined(EMU)
void ICACHE_FLASH_ATTR freeAssets(void)
{
#ifndef ANDROID
    os_free(assets);
#endif
}
#endif

/**
 * Transform a pixel's coordinates by rotation around the sprite's center point,
 * then reflection over Y axis, then reflection over X axis, then translation
 *
 * This intentionally does not have ICACHE_FLASH_ATTR because it may be called often
 *
 * @param x The x coordinate of the pixel location to transform
 * @param y The y coordinate of the pixel location to trasform
 * @param transX The number of pixels to translate X by
 * @param transY The number of pixels to translate Y by
 * @param flipLR true to flip over the Y axis, false to do nothing
 * @param flipUD true to flip over the X axis, false to do nothing
 * @param rotateDeg The number of degrees to rotate clockwise, must be 0-359
 * @param width  The width of the image
 * @param height The height of the image
 */
void transformPixel(int16_t* x, int16_t* y, int16_t transX,
                    int16_t transY, bool flipLR, bool flipUD,
                    int16_t rotateDeg, int16_t width, int16_t height)
{
    // First rotate the sprite around the sprite's center point
    if (0 < rotateDeg && rotateDeg < 360)
    {
        // This solves the aliasing problem, but because of tan() it's only safe
        // to rotate by 0 to 90 degrees. So rotate by a multiple of 90 degrees
        // first, which doesn't need trig, then rotate the rest with shears
        // See http://datagenetics.com/blog/august32013/index.html
        // See https://graphicsinterface.org/wp-content/uploads/gi1986-15.pdf

        // Center around (0, 0)
        (*x) -= (width / 2);
        (*y) -= (height / 2);

        // First rotate to the nearest 90 degree boundary, which is trivial
        if(rotateDeg >= 270)
        {
            // (x, y) -> (y, -x)
            int16_t tmp = (*x);
            (*x) = (*y);
            (*y) = -tmp;
        }
        else if(rotateDeg >= 180)
        {
            // (x, y) -> (-x, -y)
            (*x) = -(*x);
            (*y) = -(*y);
        }
        else if(rotateDeg >= 90)
        {
            // (x, y) -> (-y, x)
            int16_t tmp = (*x);
            (*x) = -(*y);
            (*y) = tmp;
        }
        // Now that it's rotated to a 90 degree boundary, find out how much more
        // there is to rotate by shearing
        rotateDeg = rotateDeg % 90;

        // If there's any more to rotate, apply three shear matrices in order
        // if(rotateDeg > 1 && rotateDeg < 89)
        if(rotateDeg > 0)
        {
            // 1st shear
            (*x) = (*x) - (((*y) * tan1024[rotateDeg / 2]) + 512) / 1024;
            // 2nd shear
            (*y) = (((*x) * sin1024[rotateDeg]) + 512) / 1024 + (*y);
            // 3rd shear
            (*x) = (*x) - (((*y) * tan1024[rotateDeg / 2]) + 512) / 1024;
        }

        // Return pixel to original position
        (*x) = (*x) + (width / 2);
        (*y) = (*y) + (height / 2);
    }

    // Then reflect over Y axis
    if (flipLR)
    {
        (*x) = width - 1 - (*x);
    }

    // Then reflect over X axis
    if(flipUD)
    {
        (*y) = height - 1 - (*y);
    }

    // Then translate
    (*x) += transX;
    (*y) += transY;
}

/**
 * Load a PNG asset from ROM to RAM
 *
 * @param name   The name of the asset to draw
 * @param handle A handle to load the asset into
 * @return true if the asset was allocated, false if it was not
 */
bool ICACHE_FLASH_ATTR allocPngAsset(const char* name, pngHandle* handle)
{
    // Get the image from the packed assets
    uint32_t assetLen = 0;
    uint32_t* assetPtr = getAsset(name, &assetLen);

    // If the asset was found
    if(NULL != assetPtr)
    {
        uint32_t idx = 0;

        // Get the width and height
        handle->width  = assetPtr[idx++];
        handle->height = assetPtr[idx++];
        AST_PRINTF("Width: %d, height: %d\n", handle->width, handle->height);

        // Pad the length to a 32 bit boundary for memcpy
        uint32_t paddedLen = assetLen - (2 * sizeof(uint32_t));
        while(paddedLen % 4 != 0)
        {
            paddedLen++;
        }
        // Allocate RAM, then copy the memory from ROM to RAM
        handle->data = (uint32_t*)os_malloc(paddedLen);
        if(NULL == handle->data)
        {
            handle->dataLen = 0;
            return false;
        }
        os_memcpy(handle->data, &assetPtr[idx], paddedLen);
        handle->dataLen = paddedLen / sizeof(uint32_t);
        return true;
    }
    else
    {
        return false;
    }
}

/**
 * Free a PNG asset from RAM
 *
 * @param handle The handle to free memory from
 */
void ICACHE_FLASH_ATTR freePngAsset(pngHandle* handle)
{
    if(NULL != handle->data)
    {
        os_free(handle->data);
    }
    handle->data = NULL;
    handle->width = 0;
    handle->height = 0;
    handle->dataLen = 0;
}

/**
 * @brief Draw a PNG asset to the OLED
 *
 * @param handle A handle of a PNG to draw
 * @param xp The x coordinate to draw the asset at
 * @param yp The y coordinate to draw the asset at
 * @param flipLR true to flip over the Y axis, false to do nothing
 * @param flipUD true to flip over the X axis, false to do nothing
 * @param rotateDeg The number of degrees to rotate clockwise, must be 0-359
 */
void ICACHE_FLASH_ATTR drawPng(pngHandle* handle, int16_t xp,
                               int16_t yp, bool flipLR, bool flipUD, int16_t rotateDeg)
{
    uint32_t idx = 0;

    // Read 32 bits at a time
    uint32_t chunk = handle->data[idx++];
    uint32_t bitIdx = 0;

    // Draw the image's pixels
    for(int16_t h = 0; h < handle->height; h++)
    {
        for(int16_t w = 0; w < handle->width; w++)
        {
            // Transform this pixel's draw location as necessary
            int16_t x = w;
            int16_t y = h;
            transformPixel(&x, &y, xp, yp, flipLR, flipUD, rotateDeg, handle->width, handle->height);

            // 'Traverse' the huffman tree to find out what to do
            bool isZero = true;
            if(chunk & (0x80000000 >> (bitIdx++)))
            {
                // If it's a one, draw a black pixel
                drawPixel(x, y, BLACK);
                isZero = false;
            }

            // After bitIdx was incremented, check it
            if(bitIdx == 32)
            {
                if(idx >= handle->dataLen)
                {
                    return;
                }
                chunk = handle->data[idx++];
                bitIdx = 0;
            }

            // A zero can be followed by a zero or a one
            if(isZero)
            {
                if(chunk & (0x80000000 >> (bitIdx++)))
                {
                    // zero-one means transparent, so don't do anything
                }
                else
                {
                    // zero-zero means white, draw a pixel
                    drawPixel(x, y, WHITE);
                }

                // After bitIdx was incremented, check it
                if(bitIdx == 32)
                {
                    if(idx >= handle->dataLen)
                    {
                        return;
                    }

                    chunk = handle->data[idx++];
                    bitIdx = 0;
                }
            }
        }
    }
}

/**
 * Draw a png asset directly to memory, not the OLED, without transformations
 * This is useful for loading raycast sprites
 *
 * TODO make the memory accesses safer
 *
 * @param handle The png asset to draw
 * @param buf    The memory to draw to
 */
void ICACHE_FLASH_ATTR drawPngToBuffer(pngHandle* handle, color* buf)
{
    uint32_t idx = 0;

    // Read 32 bits at a time
    uint32_t chunk = handle->data[idx++];
    uint32_t bitIdx = 0;

    // Draw the image's pixels
    for(int16_t y = 0; y < handle->height; y++)
    {
        for(int16_t x = 0; x < handle->width; x++)
        {
            // 'Traverse' the huffman tree to find out what to do
            bool isZero = true;
            if(chunk & (0x80000000 >> (bitIdx++)))
            {
                // If it's a one, draw a black pixel
                buf[(x * handle->height) + y] = BLACK;
                isZero = false;
            }

            // After bitIdx was incremented, check it
            if(bitIdx == 32)
            {
                chunk = handle->data[idx++];
                bitIdx = 0;
            }

            // A zero can be followed by a zero or a one
            if(isZero)
            {
                if(chunk & (0x80000000 >> (bitIdx++)))
                {
                    // zero-one means transparent
                    buf[(x * handle->height) + y] = TRANSPARENT_COLOR;
                }
                else
                {
                    // zero-zero means white, draw a pixel
                    buf[(x * handle->height) + y] = WHITE;
                }

                // After bitIdx was incremented, check it
                if(bitIdx == 32)
                {
                    chunk = handle->data[idx++];
                    bitIdx = 0;
                }
            }
        }
    }
}

/**
 * Allocate memory for a sequence of PNGs and load them from ROM to RAM
 *
 * @param handle A handle to load PNGs into
 * @param count  The number of PNGs to load
 * @param ...    A list of PNG names
 * @return true if all PNGs were loaded, false if they were not
 */
bool ICACHE_FLASH_ATTR allocPngSequence(pngSequenceHandle* handle, uint16_t count, ...)
{
    // Allocate handles for each png
    handle->handles = os_malloc(sizeof(pngHandle) * count);
    if(NULL == handle->handles)
    {
        return false;
    }
    handle->count = count;

    /* Initialize the argument list. */
    va_list ap;
    va_start(ap, count);
    for (uint16_t i = 0; i < count; i++)
    {
        /* Get the next argument value. */
        if(false == allocPngAsset(va_arg(ap, const char*), &(handle->handles[i])))
        {
            freePngSequence(handle);
            va_end(ap);
            return false;
        }
    }

    /* Clean up. */
    va_end(ap);

    return true;
}

/**
 * Free the memory from a sequence of PNGs
 *
 * @param handle The handle whose memory to free
 */
void ICACHE_FLASH_ATTR freePngSequence(pngSequenceHandle* handle)
{
    for(uint16_t i = 0; i < handle->count; i++)
    {
        freePngAsset(&handle->handles[i]);
    }
    if(NULL != handle->handles)
    {
        os_free(handle->handles);
    }
    handle->handles = NULL;
    handle->cFrame = 0;
    handle->count = 0;

}

/**
 * Draw a PNG in a sequence of PNGs
 *
 * @param handle The handle to draw a png from
 * @param xp The x coordinate to draw the asset at
 * @param yp The y coordinate to draw the asset at
 * @param flipLR true to flip over the Y axis, false to do nothing
 * @param flipUD true to flip over the X axis, false to do nothing
 * @param rotateDeg The number of degrees to rotate clockwise, must be 0-359
 * @param frame     The frame number to draw
 */
void ICACHE_FLASH_ATTR drawPngSequence(pngSequenceHandle* handle, int16_t xp,
                                       int16_t yp, bool flipLR, bool flipUD,
                                       int16_t rotateDeg, int16_t frame)
{
    if(-1 == frame)
    {
        // Draw the PNG
        drawPng(&handle->handles[handle->cFrame], xp, yp, flipLR, flipUD, rotateDeg);
        // Move to the next frame
        handle->cFrame = (handle->cFrame + 1) % handle->count;
    }
    else
    {
        if(frame < handle->count)
        {
            handle->cFrame = frame;
            drawPng(&handle->handles[frame], xp, yp, flipLR, flipUD, rotateDeg);
        }
    }
}

/**
 * Load a gif from assets to a handle
 *
 * @param name The name of the asset to draw
 * @param handle A handle to load the gif to
 */
void ICACHE_FLASH_ATTR loadGifFromAsset(const char* name, gifHandle* handle)
{
    // Only do anything if the handle is uninitialized
    if(NULL == handle->compressed)
    {
        // Get the image from the packed assets
        uint32_t assetLen = 0;
        handle->assetPtr = getAsset(name, &assetLen);

        if(NULL != handle->assetPtr)
        {
            // Read metadata from memory
            handle->idx = 0;
            handle->width    = handle->assetPtr[handle->idx++];
            handle->height   = handle->assetPtr[handle->idx++];
            handle->nFrames  = handle->assetPtr[handle->idx++];
            handle->duration = handle->assetPtr[handle->idx++];

            AST_PRINTF("%s\n  w: %d\n  h: %d\n  f: %d\n  d: %d\n", __func__,
                       handle->width,
                       handle->height,
                       handle->nFrames,
                       handle->duration);

            // Allocate enough space for the compressed data, decompressed data
            // and the actual gif
            handle->allocedSize = ((handle->width * handle->height) + 8) / 8;
            handle->compressed = (uint8_t*)os_malloc(handle->allocedSize);
            handle->decompressed = (uint8_t*)os_malloc(handle->allocedSize);
            handle->frame = (uint8_t*)os_malloc(handle->allocedSize);

            handle->firstFrameLoaded = false;
        }
    }
}

/**
 * Free all the memory allocated for a gif
 *
 * @param handle The gif to free
 */
void ICACHE_FLASH_ATTR freeGifAsset(gifHandle* handle)
{
    os_free(handle->compressed);
    os_free(handle->decompressed);
    os_free(handle->frame);
    handle->compressed = NULL;
    handle->decompressed = NULL;
    handle->frame = NULL;
}

/**
 * Draw a frame of a gif to the screen
 *
 * @param handle A handle to the gif to draw
 * @param xp The x coordinate to draw the asset at
 * @param yp The y coordinate to draw the asset at
 * @param flipLR true to flip over the Y axis, false to do nothing
 * @param flipUD true to flip over the X axis, false to do nothing
 * @param rotateDeg The number of degrees to rotate clockwise, must be 0-359
 * @param drawNext true to draw the next frame, false to draw the same frame again
 */
void ICACHE_FLASH_ATTR drawGifFromAsset(gifHandle* handle, int16_t xp, int16_t yp,
                                        bool flipLR, bool flipUD, int16_t rotateDeg,
                                        bool drawNext)
{
    if(drawNext || false == handle->firstFrameLoaded)
    {
        // Read the compressed length of this frame
        uint32_t compressedLen = handle->assetPtr[handle->idx++];

        // Pad the length to a 32 bit boundary for memcpy
        uint32_t paddedLen = compressedLen;
        while(paddedLen % 4 != 0)
        {
            paddedLen++;
        }
        AST_PRINTF("%s\n  frame: %d\n  cLen: %d\n  pLen: %d\n", __func__,
                   handle->cFrame, compressedLen, paddedLen);

        // Copy the compressed data from flash to RAM
        os_memcpy(handle->compressed, &handle->assetPtr[handle->idx], paddedLen);
        handle->idx += (paddedLen / 4);

        // If this is the first frame
        if(handle->cFrame == 0)
        {
            // Decompress it straight to the frame data
            fastlz_decompress(handle->compressed, compressedLen,
                              handle->frame, handle->allocedSize);
        }
        else
        {
            // Otherwise decompress it to decompressed
            fastlz_decompress(handle->compressed, compressedLen,
                              handle->decompressed, handle->allocedSize);
            // Then apply the changes to the current frame
            uint16_t i;
            for(i = 0; i < handle->allocedSize; i++)
            {
                handle->frame[i] ^= handle->decompressed[i];
            }
        }

        if(drawNext)
        {
            // Increment the frame count, mod the number of frames
            handle->cFrame = (handle->cFrame + 1) % handle->nFrames;
            if(handle->cFrame == 0)
            {
                // Reset the index if we're starting again
                handle->idx = 4;
            }
        }
        handle->firstFrameLoaded = true;
    }

    // Draw the current frame to the OLED
    int16_t h, w;
    for(h = 0; h < handle->height; h++)
    {
        for(w = 0; w < handle->width; w++)
        {
            int16_t x = w;
            int16_t y = h;
            if(yp || flipLR || flipUD || rotateDeg)
            {
                transformPixel(&x, &y, xp, yp, flipLR, flipUD, rotateDeg,
                               handle->width, handle->height);
            }
            else if(xp)
            {
                x += xp;
            }

            if(0 <= x && x < OLED_WIDTH)
            {
                int16_t byteIdx = (w + (h * handle->width)) / 8;
                int16_t bitIdx  = (w + (h * handle->width)) % 8;
                if(handle->frame[byteIdx] & (0x80 >> bitIdx))
                {
                    drawPixel(x, y, WHITE);
                }
                else
                {
                    drawPixel(x, y, BLACK);
                }
            }
        }
    }
}

#endif
