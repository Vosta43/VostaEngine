// Single translation unit for stb_truetype so the rest of the engine only sees
// the declarations. Compiled into the engine, like stb_image / ufbx.
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
