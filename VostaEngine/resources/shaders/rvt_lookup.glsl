// Consumer-side lookup for the paged runtime virtual texture. Included by every
// shader that shades a surface from the baked pages.

uniform vec2 u_RVTWorldMin;
uniform vec2 u_RVTWorldSize;
uniform int  u_PagesX;      // mip-0 page counts; level k is ceil(that / 2^k)
uniform int  u_PagesY;
uniform int  u_MipCount;
uniform int  u_RVTPageSize; // texels a page's interior covers
uniform int  u_RVTBorder;   // baked ring around that interior

uniform usampler2D     u_PageTable;
uniform sampler2DArray u_Physical0;
uniform sampler2DArray u_Physical1;
uniform sampler2DArray u_Physical2;
uniform sampler2DArray u_Physical3;

// Mip-0 page units for a world XZ.
vec2 veRvtPageXY(vec2 worldXZ) {
    vec2 uv = clamp((worldXZ - u_RVTWorldMin) / u_RVTWorldSize, 0.0, 1.0);
    return uv * vec2(float(u_PagesX), float(u_PagesY));
}

// The level whose texels are about one screen pixel across; a level-k texel spans
// 2^k mip-0 texels. Uses derivatives, so it must run in uniform control flow.
int veRvtLod(vec2 page0) {
    vec2  dpx = dFdx(page0);
    vec2  dpy = dFdy(page0);
    float lod = 0.5 * log2(max(1e-8, max(dot(dpx, dpx), dot(dpy, dpy))))
              + log2(float(max(1, u_RVTPageSize)));
    return clamp(int(floor(lod + 0.5)), 0, u_MipCount - 1);
}

// Resolves one virtual location to the finest level at or above `level` whose page is
// resident, plus the UV inside it. Returns -1 when no level holds it (outside the
// baked bounds). `page0` is in mip-0 page units.
int veRvtResolve(vec2 page0, int level, out uint slot, out vec2 inPage) {
    slot   = 0xFFFFu;
    inPage = vec2(0.0);
    for (int k = level; k < u_MipCount; ++k) {
        float scale = exp2(float(k));
        ivec2 dims  = ivec2(ceil(vec2(float(u_PagesX), float(u_PagesY)) / scale));
        vec2  pk    = page0 / scale;
        ivec2 page  = min(ivec2(floor(pk)), dims - ivec2(1));
        uint  s     = texelFetch(u_PageTable, page, k).r;
        if (s != 0xFFFFu) {
            slot   = s;
            inPage = fract(pk);
            return k;
        }
    }
    return -1;
}

// Interior UV -> physical layer UV. The border ring around a page's interior holds the
// neighbouring content, so a bilinear tap near [0,1] stays on real samples.
vec2 veRvtPageToPhysicalUV(vec2 inPage) {
    float interior = float(max(1, u_RVTPageSize));
    float total    = interior + 2.0 * float(u_RVTBorder);
    return (inPage * interior + float(u_RVTBorder)) / total;
}
