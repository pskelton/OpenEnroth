#pragma once
#include <memory>

#include "Engine/Graphics/Nuklear.h"
#include "Engine/Graphics/HWLContainer.h"
#include "Engine/Graphics/RenderBase.h"
#include "Engine/MM7.h"

#ifdef __APPLE__
#define NK_SHADER_VERSION "#version 150\n"
#else
#define NK_SHADER_VERSION "#version 300 es\n"
#endif

#include "shader.h"

class RenderOpenGL : public RenderBase {
 public:
    RenderOpenGL(
        std::shared_ptr<OSWindow> window,
        DecalBuilder* decal_builder,
        LightmapBuilder* lightmap_builder,
        SpellFxRenderer* spellfx,
        std::shared_ptr<ParticleEngine> particle_engine,
        Vis* vis,
        Log* logger
    );
    virtual ~RenderOpenGL();

    virtual bool Initialize();

    virtual bool NuklearInitialize(struct nk_tex_font *tfont);
    virtual bool NuklearCreateDevice();
    virtual bool NuklearRender(enum nk_anti_aliasing AA, int max_vertex_buffer, int max_element_buffer);
    virtual void NuklearRelease();
    virtual struct nk_tex_font *NuklearFontLoad(const char *font_path, size_t font_size);
    virtual void NuklearFontFree(struct nk_tex_font *tfont);
    virtual struct nk_image NuklearImageLoad(Image *img);
    virtual void NuklearImageFree(Image *img);

    virtual Texture *CreateTexture_ColorKey(const String &name, uint16_t colorkey);
    virtual Texture *CreateTexture_Solid(const String &name);
    virtual Texture *CreateTexture_Alpha(const String &name);

    virtual Texture *CreateTexture_PCXFromFile(const String &name);
    virtual Texture *CreateTexture_PCXFromIconsLOD(const String &name);
    virtual Texture *CreateTexture_PCXFromNewLOD(const String &name);
    virtual Texture *CreateTexture_PCXFromLOD(void *pLOD, const String &name);

    virtual Texture *CreateTexture_Blank(unsigned int width, unsigned int height,
        IMAGE_FORMAT format, const void *pixels = nullptr);

    virtual Texture *CreateTexture(const String &name);
    virtual Texture *CreateSprite(
        const String &name, unsigned int palette_id,
        /*refactor*/ unsigned int lod_sprite_id);

    virtual void ClearBlack();
    virtual void PresentBlackScreen();

    virtual void SaveWinnersCertificate(const char *a1);
    virtual void ClearTarget(unsigned int uColor);
    virtual void Present();

    virtual bool InitializeFullscreen();

    virtual void CreateZBuffer();
    virtual void Release();

    virtual bool SwitchToWindow();
    virtual void RasterLine2D(signed int uX, signed int uY, signed int uZ,
                              signed int uW, unsigned __int16 uColor);
    
    void Drawlines();
    void drawtwodverts();
    void drawbillbverts(int* i);
    
    virtual void ClearZBuffer();
    virtual void RestoreFrontBuffer();
    virtual void RestoreBackBuffer();
    virtual void BltBackToFontFast(int a2, int a3, Rect *pSrcRect);
    virtual void BeginSceneD3D();

    virtual unsigned int GetActorTintColor(int DimLevel, int tint, float WorldViewX, int a5, RenderBillboard *Billboard);

    virtual void DrawPolygon(struct Polygon *a3);
    virtual void DrawTerrainPolygon(struct Polygon *a4, bool transparent,
                                    bool clampAtTextureBorders);
    void DrawIndoorBSP();
    virtual void DrawIndoorPolygon(unsigned int uNumVertices,
                                   struct BLVFace *a3, int uPackedID,
                                   unsigned int uColor, int a8);

    virtual void DrawBillboards_And_MaybeRenderSpecialEffects_And_EndScene();
    virtual void DrawBillboard_Indoor(SoftwareBillboard *pSoftBillboard,
                                      RenderBillboard *);
    virtual void _4A4CC9_AddSomeBillboard(struct SpellFX_Billboard *a1, int diffuse);
    virtual void DrawBillboardList_BLV();

    virtual void DrawProjectile(float srcX, float srcY, float a3, float a4,
                                float dstX, float dstY, float a7, float a8,
                                Texture *texture);

    virtual void RemoveTextureFromDevice(Texture* texture);
    virtual bool MoveTextureToDevice(Texture *texture);

    virtual void Update_Texture(Texture *texture);

    virtual void DeleteTexture(Texture *texture);

    virtual void BeginScene();
    virtual void EndScene();
    virtual void ScreenFade(unsigned int color, float t);

    virtual void SetUIClipRect(unsigned int uX, unsigned int uY,
                               unsigned int uZ, unsigned int uW);
    virtual void ResetUIClipRect();

    virtual void DrawTextureNew(float u, float v, class Image*);
    virtual void DrawTextureAlphaNew(float u, float v, class Image*);

    virtual void DrawTextureCustomHeight(float u, float v, class Image*,
        int height);
    virtual void DrawTextureOffset(int x, int y, int offset_x, int offset_y,
        Image*);
    virtual void DrawImage(Image*, const Rect& rect);

    virtual void ZBuffer_Fill_2(signed int a2, signed int a3, Image* pTexture,
        int a5);
    virtual void ZDrawTextureAlpha(float u, float v, Image* pTexture, int zVal);
    virtual void BlendTextures(int a2, int a3, Image* a4, Image* a5, int t,
        int start_opacity, int end_opacity);
    virtual void TexturePixelRotateDraw(float u, float v, Image* img, int time);
    virtual void DrawMonsterPortrait(Rect rc, SpriteFrame *Portrait_Sprite, int Y_Offset);


    virtual void MaskGameViewport();

    virtual void DrawMasked(float u, float v, class Image *img,
                            unsigned int color_dimming_level,
                            unsigned __int16 mask);
    virtual void DrawTextureGrayShade(float u, float v, class Image *a4);
    virtual void DrawTransparentRedShade(float u, float v, class Image *a4);
    virtual void DrawTransparentGreenShade(float u, float v,
                                           class Image *pTexture);
    virtual void DrawFansTransparent(const RenderVertexD3D3 *vertices,
                                     unsigned int num_vertices);

    virtual void DrawTextAlpha(int x, int y, unsigned char *font_pixels, int a5,
                               unsigned int uFontHeight, uint8_t *pPalette,
                               bool present_time_transparency);
    virtual void DrawText(int uOutX, int uOutY, uint8_t *pFontPixels,
                          unsigned int uCharWidth, unsigned int uCharHeight,
                          uint8_t *pFontPalette, uint16_t uFaceColor,
                          uint16_t uShadowColor);

    virtual void FillRectFast(unsigned int uX, unsigned int uY,
                              unsigned int uWidth, unsigned int uHeight,
                              unsigned int uColor16);

    virtual void DrawBuildingsD3D();

    virtual void DrawIndoorSky(unsigned int uNumVertices, unsigned int uFaceID);
    virtual void DrawOutdoorSkyD3D();

    virtual void PrepareDecorationsRenderList_ODM();

    virtual void RenderTerrainD3D();

    virtual bool AreRenderSurfacesOk();

    unsigned short *MakeScreenshot(int width, int height);
    virtual Image *TakeScreenshot(unsigned int width, unsigned int height);
    virtual void SaveScreenshot(const String &filename, unsigned int width,
                                unsigned int height);
    virtual void PackScreenshot(unsigned int width, unsigned int height,
                                void *out_data, unsigned int data_size,
                                unsigned int *screenshot_size);
    virtual void SavePCXScreenshot();

    virtual int GetActorsInViewport(int pDepth);

    virtual void BeginLightmaps();
    virtual void EndLightmaps();
    virtual void BeginLightmaps2();
    virtual void EndLightmaps2();
    virtual bool DrawLightmap(struct Lightmap *pLightmap,
                              struct Vec3_float_ *pColorMult, float z_bias);

    virtual void BeginDecals();
    virtual void EndDecals();
    virtual void DrawDecal(struct Decal *pDecal, float z_bias);

    virtual void do_draw_debug_line_d3d(const RenderVertexD3D3 *pLineBegin,
                                        signed int sDiffuseBegin,
                                        const RenderVertexD3D3 *pLineEnd,
                                        signed int sDiffuseEnd, float z_stuff);
    virtual void DrawLines(const RenderVertexD3D3 *vertices,
                           unsigned int num_vertices);

    virtual void DrawSpecialEffectsQuad(const RenderVertexD3D3 *vertices,
                                        Texture *texture);

    virtual void am_Blt_Chroma(Rect *pSrcRect, Point *pTargetPoint, int a3,
                               int blend_mode);

    virtual void ReleaseTerrain();

    virtual void ReleaseBSP();



 public:
    virtual void WritePixel16(int x, int y, uint16_t color);

    virtual unsigned int GetRenderWidth() const;
    virtual unsigned int GetRenderHeight() const;

 protected:
    void DoRenderBillboards_D3D();
    void SetBillboardBlendOptions(RenderBillboardD3D::OpacityType a1);

    void DrawOutdoorSkyPolygon(struct Polygon *pSkyPolygon);
    void DrawIndoorSkyPolygon(signed int uNumVertices,
                              struct Polygon *pSkyPolygon);

    
    int GPU_MAX_TEX_SIZE;
    int GPU_MAX_TEX_LAYERS;
    int GPU_MAX_TEX_UNITS;
    int GPU_MAX_UNIFORM_COMP;
    int GPU_MAX_TOTAL_TEXTURES;

    bool InitShaders();
    
    Shader terrainshader;
    Shader outbuildshader;
    Shader bspshader;
    Shader passthroughshader;
    Shader lineshader;

    
    GLuint terrainVBO, terrainVAO;
    // all terrain textures are square
    GLuint terraintextures[8];
    uint numterraintexloaded[8];
    uint terraintexturesizes[8];
    std::map<std::string, int> terraintexmap;

    
    GLuint outbuildVBO[16], outbuildVAO[16];
    GLuint outbuildtextures[16];
    uint numoutbuildtexloaded[16];
    uint outbuildtexturewidths[16];
    uint outbuildtextureheights[16];
    std::map<std::string, int> outbuildtexmap;

    GLuint bspVBO, bspVAO;
    GLuint bsptextures[16];
    uint bsptexloaded[16];
    uint bsptexturewidths[16];
    uint bsptextureheights[16];
    std::map<std::string, int> bsptexmap;

    GLuint lineVBO, lineVAO;

    GLuint twodVBO, twodVAO;

    GLuint billbVBO, billbVAO;



    int clip_x, clip_y;
    int clip_z, clip_w;
    uint32_t *render_target_rgb;  // now 32 - draw to in format A8R8G8B8 - endian swivel means BGRA

    


    int GL_lastboundtex;
    struct nk_vertex {
        float position[2];
        float uv[2];
        nk_byte col[4];
    } nk_vertex;
    struct nk_device {
        struct nk_buffer cmds;
        struct nk_draw_null_texture null;
        struct nk_font_atlas atlas;
        uint32_t vbo, vao, ebo;
        uint32_t prog;
        uint32_t vert_shdr;
        uint32_t frag_shdr;
        int32_t attrib_pos;
        int32_t attrib_uv;
        int32_t attrib_col;
        int32_t uniform_tex;
        int32_t uniform_proj;
    } nk_dev;
};


