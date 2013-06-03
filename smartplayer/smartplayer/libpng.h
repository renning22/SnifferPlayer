#pragma once

bool load_png(const char *name, int &outWidth, int &outHeight, bool &outHasAlpha , int &outDepth , int &outPitch , unsigned char **outData);