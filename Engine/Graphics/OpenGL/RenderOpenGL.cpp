// TODO

// debug picked face shading - outdoor building
// 
// indoors - adjust shader to 4.1
// 
// outdoors sky
// 
// perception face shading
// distance fog
// blt chroma blue mask
// remove all glbegin
// get gl params on load (max texture slots/ avaiable vram)
// missing functions
// contain data stores
// batch and shaders fillrectfast
// batch and shaders drawtexturealphanew
// batch and shaders billboards
// batch particles

// billboard zdepth not quite right

// fireball sphere draw as 3d rather than billboard

// text not drawing on d3d
// draw monster portrait squashes

// ui stuff
// draw fucntions -> nuklear??
// remove writepixel - need to sort text drawing for this









#ifdef _WINDOWS
    #define NOMINMAX
    #include <Windows.h>

    #pragma comment(lib, "opengl32.lib")
    #pragma comment(lib, "glu32.lib")

    //  on windows, this is required in gl/glu.h
    #if !defined(APIENTRY)
        #define APIENTRY __stdcall
    #endif

    #if !defined(WINGDIAPI)
        #define WINGDIAPI
    #endif

    #if !defined(CALLBACK)
        #define CALLBACK __stdcall
    #endif
#endif

#include <filesystem>
#include "glad/gl.h"

#include <glm/glm/glm.hpp>
#include <glm/glm/gtc/matrix_transform.hpp>

#ifdef __APPLE__
#include <OpenGL/glu.h>
#else
#include <GL/glu.h>
#endif

#include <algorithm>
#include <memory>

#include "Engine/Engine.h"
#include "Engine/Graphics/Image.h"
#include "Engine/Graphics/ImageLoader.h"
#include "Engine/Graphics/LightmapBuilder.h"
#include "Engine/Graphics/DecalBuilder.h"
#include "Engine/Graphics/Level/Decoration.h"
#include "Engine/Graphics/DecorationList.h"
#include "Engine/Graphics/Lights.h"
#include "Engine/Graphics/Nuklear.h"
#include "Engine/Graphics/OpenGL/RenderOpenGL.h"
#include "Engine/Graphics/OpenGL/TextureOpenGL.h"
#include "Engine/Graphics/Outdoor.h"
#include "Engine/Graphics/ParticleEngine.h"
#include "Engine/Graphics/PCX.h"
#include "Engine/Graphics/Sprites.h"
#include "Engine/Graphics/Viewport.h"
#include "Engine/Graphics/Weather.h"
#include "Engine/Objects/Actor.h"
#include "Engine/Objects/ObjectList.h"
#include "Engine/Objects/SpriteObject.h"
#include "Engine/OurMath.h"
#include "Engine/Party.h"
#include "Engine/SpellFxRenderer.h"
#include "Arcomage/Arcomage.h"

#include "Arcomage/Arcomage.h"

#include "Engine/Graphics/OpenGL/shader.h"

#include "Platform/Api.h"
#include "Platform/OSWindow.h"



void checkglerror(bool breakonerr = true) {
    GLenum err = glGetError();

    while (err != GL_NO_ERROR) {
        static String error;

        switch (err) {
            case GL_INVALID_OPERATION:      error = "INVALID_OPERATION";      break;
            case GL_INVALID_ENUM:           error = "INVALID_ENUM";           break;
            case GL_INVALID_VALUE:          error = "INVALID_VALUE";          break;
            case GL_OUT_OF_MEMORY:          error = "OUT_OF_MEMORY";          break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:  error = "INVALID_FRAMEBUFFER_OPERATION";  break;
            default:                        error = "Unknown Error";  break;
        }

        logger->Warning("OpenGL error: %s", error.c_str());
        if (breakonerr) __debugbreak();

        err = glGetError();
    }
}


glm::mat4 projmat;
glm::mat4 viewmat;


RenderVertexSoft VertexRenderList[50];  // array_50AC10
RenderVertexD3D3 d3d_vertex_buffer[50];

void Polygon::_normalize_v_18() {
    float len = sqrt((double)this->v_18.z * (double)this->v_18.z +
                     (double)this->v_18.y * (double)this->v_18.y +
                     (double)this->v_18.x * (double)this->v_18.x);
    if (fabsf(len) < 1e-6f) {
        v_18.x = 0;
        v_18.y = 0;
        v_18.z = 65536;
    } else {
        v_18.x = round_to_int((double)this->v_18.x / len * 65536.0);
        v_18.y = round_to_int((double)this->v_18.y / len * 65536.0);
        v_18.z = round_to_int((double)this->v_18.z / len * 65536.0);
    }
}

bool IsBModelVisible(BSPModel *model, int *reachable) {
    // checks if model is visible in FOV cone
    float halfangle = (pODMRenderParams->uCameraFovInDegrees * pi_double / 180.0f) /2.0 ;
    float rayx = model->vBoundingCenter.x - pIndoorCameraD3D->vPartyPos.x;
    float rayy = model->vBoundingCenter.y - pIndoorCameraD3D->vPartyPos.y;

    // approx distance
    int dist = int_get_vector_length(abs(rayx), abs(rayy), 0);
    *reachable = false;
    if (dist < model->sBoundingRadius + 256) *reachable = true;

    // dot product of camvec and ray - size in forward
    float frontvec = rayx * pIndoorCameraD3D->fRotationZCosine + rayy * pIndoorCameraD3D->fRotationZSine;
    if (pIndoorCameraD3D->sRotationX) { frontvec *= pIndoorCameraD3D->fRotationXCosine;}

    // dot product of camvec and ray - size in left
    float leftvec = rayy * pIndoorCameraD3D->fRotationZCosine - rayx * pIndoorCameraD3D->fRotationZSine;

    // which half fov is ray in direction of - compare slopes
    float sloperem = 0.0;
    if (leftvec >= 0) {  // acute - left
        sloperem = frontvec * sin(halfangle) - leftvec * cos(halfangle);
    } else {  // obtuse - right
        sloperem = frontvec * sin(halfangle) + leftvec * cos(halfangle);
    }

    // view range check
    if (dist <= pIndoorCameraD3D->GetFarClip() + 2048) {
        // boudning point inside cone
        if (sloperem >= 0) return true;
        // bounding radius inside cone
        if (abs(sloperem) < model->sBoundingRadius + 512) return true;
    }

    // not visible
    return false;
}

int GetActorTintColor(int max_dimm, int min_dimm, float distance, int a4, RenderBillboard *a5) {

    //signed int v6;   // edx@1
    //int isnight;          // eax@3
    double v9;       // st7@12
    int v11;         // ecx@28
    double v15;      // st7@44
    int v18;         // ST14_4@44
    signed int v20;  // [sp+10h] [bp-4h]@10
    float a3c;       // [sp+1Ch] [bp+8h]@44
    int a5a;         // [sp+24h] [bp+10h]@44

    // v5 = a2;
    signed int v6 = 0;

    if (uCurrentlyLoadedLevelType == LEVEL_Indoor)
        return 8 * (31 - max_dimm) | ((8 * (31 - max_dimm) | ((31 - max_dimm) << 11)) << 8);

    if (pParty->armageddon_timer) return 0xFFFF0000;

    int isnight = pWeather->bNight;
    if (engine->IsUnderwater())
        isnight = 0;

    if (isnight) {
        v20 = 1;
        if (pParty->pPartyBuffs[PARTY_BUFF_TORCHLIGHT].Active())
            v20 = pParty->pPartyBuffs[PARTY_BUFF_TORCHLIGHT].uPower;
        v9 = (double)v20 * 1024.0;

        if (a4) {
            v6 = 216;
            goto LABEL_20;
        }

        if (distance <= v9) {
            if (distance > 0.0) {
                // a4b = distance * 216.0 / device_caps;
                // v10 = a4b + 6.7553994e15;
                // v6 = LODWORD(v10);
                v6 = floorf(0.5f + distance * 216.0 / v9);
                if (v6 > 216) {
                    v6 = 216;
                    goto LABEL_20;
                }
            }
        } else {
            v6 = 216;
        }

        if (distance != 0.0) {
        LABEL_20:
            if (a5) v6 = 8 * _43F55F_get_billboard_light_level(a5, v6 >> 3);
            if (v6 > 216) v6 = 216;
            return (255 - v6) | ((255 - v6) << 16) | ((255 - v6) << 8);
        }
        // LABEL_19:
        v6 = 216;
        goto LABEL_20;
    }

    // daytime

    if (fabsf(distance) < 1.0e-6f) return 0xFFF8F8F8;

    // dim in measured in 8-steps
    v11 = 8 * (max_dimm - min_dimm);
    // v12 = v11;
    if (v11 >= 0) {
        if (v11 > 216) v11 = 216;
    } else {
        v11 = 0;
    }

    float fog_density_mult = 216.0f;
    if (a4)
        fog_density_mult +=
            distance / (double)pODMRenderParams->shading_dist_shade * 32.0;

    v6 = v11 + floorf(pOutdoor->fFogDensity * fog_density_mult + 0.5f);

    if (a5) v6 = 8 * _43F55F_get_billboard_light_level(a5, v6 >> 3);
    if (v6 > 216) v6 = 216;
    if (v6 < v11) v6 = v11;
    if (v6 > 8 * pOutdoor->max_terrain_dimming_level)
        v6 = 8 * pOutdoor->max_terrain_dimming_level;

    if (!engine->IsUnderwater()) {
        return (255 - v6) | ((255 - v6) << 16) | ((255 - v6) << 8);
    } else {
        v15 = (double)(255 - v6) * 0.0039215689;
        a3c = v15;
        // a4c = v15 * 16.0;
        // v16 = a4c + 6.7553994e15;
        a5a = floorf(v15 * 16.0 + 0.5f);  // LODWORD(v16);
                                          // a4d = a3c * 194.0;
                                          // v17 = a4d + 6.7553994e15;
        v18 = floorf(a3c * 194.0 + 0.5f);  // LODWORD(v17);
                                           // a3d = a3c * 153.0;
                                           // v19 = a3d + 6.7553994e15;
        return (int)floorf(a3c * 153.0 + 0.5f) /*LODWORD(v19)*/ |
               ((v18 | (a5a << 8)) << 8);
    }
}

std::shared_ptr<IRender> render;
int uNumDecorationsDrawnThisFrame;
RenderBillboard pBillboardRenderList[500];
unsigned int uNumBillboardsToDraw;
int uNumSpritesDrawnThisFrame;

RenderVertexSoft array_73D150[20];

void _46E889_collide_against_bmodels(unsigned int ecx0) {
    int v8;            // eax@19
    int v9;            // ecx@20
    int v10;           // eax@24
    // unsigned int v14;  // eax@28
    int v15;           // eax@30
    int v16;           // ecx@31
    // unsigned int v17;  // eax@36
    int v21;           // eax@42
    // unsigned int v22;  // eax@43
    int a2;            // [sp+84h] [bp-4h]@23
    BLVFace face;      // [sp+Ch] [bp-7Ch]@1

    for (BSPModel &model : pOutdoor->pBModels) {
        if (_actor_collision_struct.sMaxX <= model.sMaxX &&
            _actor_collision_struct.sMinX >= model.sMinX &&
            _actor_collision_struct.sMaxY <= model.sMaxY &&
            _actor_collision_struct.sMinY >= model.sMinY &&
            _actor_collision_struct.sMaxZ <= model.sMaxZ &&
            _actor_collision_struct.sMinZ >= model.sMinZ) {
            for (ODMFace &mface : model.pFaces) {
                if (_actor_collision_struct.sMaxX <= mface.pBoundingBox.x2 &&
                    _actor_collision_struct.sMinX >= mface.pBoundingBox.x1 &&
                    _actor_collision_struct.sMaxY <= mface.pBoundingBox.y2 &&
                    _actor_collision_struct.sMinY >= mface.pBoundingBox.y1 &&
                    _actor_collision_struct.sMaxZ <= mface.pBoundingBox.z2 &&
                    _actor_collision_struct.sMinZ >= mface.pBoundingBox.z1) {
                    face.pFacePlane_old.vNormal.x = mface.pFacePlane.vNormal.x;
                    face.pFacePlane_old.vNormal.y = mface.pFacePlane.vNormal.y;
                    face.pFacePlane_old.vNormal.z = mface.pFacePlane.vNormal.z;

                    face.pFacePlane_old.dist =
                        mface.pFacePlane.dist;  // incorrect

                    face.uAttributes = mface.uAttributes;

                    face.pBounding.x1 = mface.pBoundingBox.x1;
                    face.pBounding.y1 = mface.pBoundingBox.y1;
                    face.pBounding.z1 = mface.pBoundingBox.z1;

                    face.pBounding.x2 = mface.pBoundingBox.x2;
                    face.pBounding.y2 = mface.pBoundingBox.y2;
                    face.pBounding.z2 = mface.pBoundingBox.z2;

                    face.zCalc1 = mface.zCalc1;
                    face.zCalc2 = mface.zCalc2;
                    face.zCalc3 = mface.zCalc3;

                    face.pXInterceptDisplacements =
                        mface.pXInterceptDisplacements;
                    face.pYInterceptDisplacements =
                        mface.pYInterceptDisplacements;
                    face.pZInterceptDisplacements =
                        mface.pZInterceptDisplacements;

                    face.uPolygonType = (PolygonType)mface.uPolygonType;

                    face.uNumVertices = mface.uNumVertices;

                    // face.uBitmapID = model.pFaces[j].uTextureID;
                    face.resource = mface.resource;

                    face.pVertexIDs = mface.pVertexIDs;

                    if (!face.Ethereal() && !face.Portal()) {
                        v8 = (face.pFacePlane_old.dist +
                              face.pFacePlane_old.vNormal.x *
                                  _actor_collision_struct.normal.x +
                              face.pFacePlane_old.vNormal.y *
                                  _actor_collision_struct.normal.y +
                              face.pFacePlane_old.vNormal.z *
                                  _actor_collision_struct.normal.z) >>
                             16;
                        if (v8 > 0) {
                            v9 = (face.pFacePlane_old.dist +
                                  face.pFacePlane_old.vNormal.x *
                                      _actor_collision_struct.normal2.x +
                                  face.pFacePlane_old.vNormal.y *
                                      _actor_collision_struct.normal2.y +
                                  face.pFacePlane_old.vNormal.z *
                                      _actor_collision_struct.normal2.z) >>
                                 16;
                            if (v8 <= _actor_collision_struct.prolly_normal_d ||
                                v9 <= _actor_collision_struct.prolly_normal_d) {
                                if (v9 <= v8) {
                                    a2 = _actor_collision_struct.field_6C;
                                    if (sub_4754BF(_actor_collision_struct.prolly_normal_d,
                                                   &a2, _actor_collision_struct.normal.x,
                                                   _actor_collision_struct.normal.y,
                                                   _actor_collision_struct.normal.z,
                                                   _actor_collision_struct.direction.x,
                                                   _actor_collision_struct.direction.y,
                                                   _actor_collision_struct.direction.z,
                                                   &face, model.index, ecx0)) {
                                        v10 = a2;
                                    } else {
                                        a2 = _actor_collision_struct.prolly_normal_d +
                                             _actor_collision_struct.field_6C;
                                        if (!sub_475F30(&a2, &face,
                                                        _actor_collision_struct.normal.x,
                                                        _actor_collision_struct.normal.y,
                                                        _actor_collision_struct.normal.z,
                                                        _actor_collision_struct.direction.x,
                                                        _actor_collision_struct.direction.y,
                                                        _actor_collision_struct.direction.z,
                                                        model.index))
                                            goto LABEL_29;
                                        v10 = a2 - _actor_collision_struct.prolly_normal_d;
                                        a2 -= _actor_collision_struct.prolly_normal_d;
                                    }
                                    if (v10 < _actor_collision_struct.field_7C) {
                                        _actor_collision_struct.field_7C = v10;
                                        _actor_collision_struct.pid = PID(
                                            OBJECT_BModel,
                                            (mface.index | (model.index << 6)));
                                    }
                                }
                            }
                        }
                    LABEL_29:
                        if (_actor_collision_struct.field_0 & 1) {
                            v15 = (face.pFacePlane_old.dist +
                                   face.pFacePlane_old.vNormal.x *
                                       _actor_collision_struct.position.x +
                                   face.pFacePlane_old.vNormal.y *
                                       _actor_collision_struct.position.y +
                                   face.pFacePlane_old.vNormal.z *
                                       _actor_collision_struct.position.z) >>
                                  16;
                            if (v15 > 0) {
                                v16 = (face.pFacePlane_old.dist +
                                       face.pFacePlane_old.vNormal.x *
                                           _actor_collision_struct.field_4C +
                                       face.pFacePlane_old.vNormal.y *
                                           _actor_collision_struct.field_50 +
                                       face.pFacePlane_old.vNormal.z *
                                           _actor_collision_struct.field_54) >>
                                      16;
                                if (v15 <= _actor_collision_struct.prolly_normal_d ||
                                    v16 <= _actor_collision_struct.prolly_normal_d) {
                                    if (v16 <= v15) {
                                        a2 = _actor_collision_struct.field_6C;
                                        if (sub_4754BF(
                                                _actor_collision_struct.field_8_radius, &a2,
                                                _actor_collision_struct.position.x,
                                                _actor_collision_struct.position.y,
                                                _actor_collision_struct.position.z,
                                                _actor_collision_struct.direction.x,
                                                _actor_collision_struct.direction.y,
                                                _actor_collision_struct.direction.z, &face,
                                                model.index, ecx0)) {
                                            if (a2 < _actor_collision_struct.field_7C) {
                                                _actor_collision_struct.field_7C = a2;
                                                _actor_collision_struct.pid =
                                                    PID(OBJECT_BModel,
                                                        (mface.index |
                                                         (model.index << 6)));
                                            }
                                        } else {
                                            a2 = _actor_collision_struct.field_6C +
                                                 _actor_collision_struct.field_8_radius;
                                            if (sub_475F30(
                                                    &a2, &face,
                                                    _actor_collision_struct.position.x,
                                                    _actor_collision_struct.position.y,
                                                    _actor_collision_struct.position.z,
                                                    _actor_collision_struct.direction.x,
                                                    _actor_collision_struct.direction.y,
                                                    _actor_collision_struct.direction.z,
                                                    model.index)) {
                                                v21 =
                                                    a2 -
                                                    _actor_collision_struct.prolly_normal_d;
                                                a2 -=
                                                    _actor_collision_struct.prolly_normal_d;
                                                if (a2 < _actor_collision_struct.field_7C) {
                                                    _actor_collision_struct.field_7C = v21;
                                                    _actor_collision_struct.pid = PID(
                                                        OBJECT_BModel,
                                                        (mface.index |
                                                         (model.index << 6)));
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void RenderOpenGL::MaskGameViewport() {
    // do not want in opengl mode
}


int _46EF01_collision_chech_player(int a1) {
    int result;  // eax@1
    int v3;      // ebx@7
    int v4;      // esi@7
    int v5;      // edi@8
    int v6;      // ecx@9
    int v7;      // edi@12
    int v10;     // [sp+14h] [bp-8h]@7
    int v11;     // [sp+18h] [bp-4h]@7

    result = pParty->vPosition.x;
    // device_caps = pParty->uPartyHeight;
    if (_actor_collision_struct.sMaxX <=
            pParty->vPosition.x + (2 * pParty->field_14_radius) &&
        _actor_collision_struct.sMinX >=
            pParty->vPosition.x - (2 * pParty->field_14_radius) &&
        _actor_collision_struct.sMaxY <=
            pParty->vPosition.y + (2 * pParty->field_14_radius) &&
        _actor_collision_struct.sMinY >=
            pParty->vPosition.y - (2 * pParty->field_14_radius) &&
        _actor_collision_struct.sMinZ/*sMaxZ*/ <= (pParty->vPosition.z + (int)pParty->uPartyHeight) &&
        _actor_collision_struct.sMaxZ/*sMinZ*/ >= pParty->vPosition.z) {
        v3 = _actor_collision_struct.prolly_normal_d + (2 * pParty->field_14_radius);
        v11 = pParty->vPosition.x - _actor_collision_struct.normal.x;
        v4 = ((pParty->vPosition.x - _actor_collision_struct.normal.x) *
                  _actor_collision_struct.direction.y -
              (pParty->vPosition.y - _actor_collision_struct.normal.y) *
                  _actor_collision_struct.direction.x) >>
             16;
        v10 = pParty->vPosition.y - _actor_collision_struct.normal.y;
        result = abs(((pParty->vPosition.x - _actor_collision_struct.normal.x) *
                          _actor_collision_struct.direction.y -
                      (pParty->vPosition.y - _actor_collision_struct.normal.y) *
                          _actor_collision_struct.direction.x) >>
                     16);
        if (result <=
            _actor_collision_struct.prolly_normal_d + (2 * pParty->field_14_radius)) {
            result = v10 * _actor_collision_struct.direction.y;
            v5 = (v10 * _actor_collision_struct.direction.y +
                  v11 * _actor_collision_struct.direction.x) >>
                 16;
            if (v5 > 0) {
                v6 = fixpoint_mul(_actor_collision_struct.direction.z, v5) +
                     _actor_collision_struct.normal.z;
                result = pParty->vPosition.z;
                if (v6 >= pParty->vPosition.z) {
                    result = pParty->uPartyHeight + pParty->vPosition.z;
                    if (v6 <= (signed int)(pParty->uPartyHeight +
                                           pParty->vPosition.z) ||
                        a1) {
                        result = integer_sqrt(v3 * v3 - v4 * v4);
                        v7 = v5 - integer_sqrt(v3 * v3 - v4 * v4);
                        if (v7 < 0) v7 = 0;
                        if (v7 < _actor_collision_struct.field_7C) {
                            _actor_collision_struct.field_7C = v7;
                            _actor_collision_struct.pid = 4;
                        }
                    }
                }
            }
        }
    }
    return result;
}


void _46E0B2_collide_against_decorations() {
    BLVSector *sector = &pIndoor->pSectors[_actor_collision_struct.uSectorID];
    for (unsigned int i = 0; i < sector->uNumDecorations; ++i) {
        LevelDecoration *decor = &pLevelDecorations[sector->pDecorationIDs[i]];
        if (!(decor->uFlags & LEVEL_DECORATION_INVISIBLE)) {
            DecorationDesc *decor_desc = pDecorationList->GetDecoration(decor->uDecorationDescID);
            if (!decor_desc->CanMoveThrough()) {
                if (_actor_collision_struct.sMaxX <= decor->vPosition.x + decor_desc->uRadius &&
                    _actor_collision_struct.sMinX >= decor->vPosition.x - decor_desc->uRadius &&
                    _actor_collision_struct.sMaxY <= decor->vPosition.y + decor_desc->uRadius &&
                    _actor_collision_struct.sMinY >= decor->vPosition.y - decor_desc->uRadius &&
                    _actor_collision_struct.sMaxZ <= decor->vPosition.z + decor_desc->uDecorationHeight &&
                    _actor_collision_struct.sMinZ >= decor->vPosition.z) {
                    int v16 = decor->vPosition.x - _actor_collision_struct.normal.x;
                    int v15 = decor->vPosition.y - _actor_collision_struct.normal.y;
                    int v8 = _actor_collision_struct.prolly_normal_d + decor_desc->uRadius;
                    int v17 = ((decor->vPosition.x - _actor_collision_struct.normal.x) * _actor_collision_struct.direction.y -
                               (decor->vPosition.y - _actor_collision_struct.normal.y) * _actor_collision_struct.direction.x) >> 16;
                    if (abs(v17) <= _actor_collision_struct.prolly_normal_d + decor_desc->uRadius) {
                        int v9 = (v16 * _actor_collision_struct.direction.x + v15 * _actor_collision_struct.direction.y) >> 16;
                        if (v9 > 0) {
                            int v11 = _actor_collision_struct.normal.z + fixpoint_mul(_actor_collision_struct.direction.z, v9);
                            if (v11 >= decor->vPosition.z) {
                                if (v11 <= decor_desc->uDecorationHeight + decor->vPosition.z) {
                                    int v12 = v9 - integer_sqrt(v8 * v8 - v17 * v17);
                                    if (v12 < 0) v12 = 0;
                                    if (v12 < _actor_collision_struct.field_7C) {
                                        _actor_collision_struct.field_7C = v12;
                                        _actor_collision_struct.pid = PID(OBJECT_Decoration, sector->pDecorationIDs[i]);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}


int _46F04E_collide_against_portals() {
    int a3;             // [sp+Ch] [bp-8h]@13
    int v12 = 0;            // [sp+10h] [bp-4h]@15

    unsigned int v1 = 0xFFFFFF;
    unsigned int v10 = 0xFFFFFF;
    for (unsigned int i = 0; i < pIndoor->pSectors[_actor_collision_struct.uSectorID].uNumPortals; ++i) {
        if (pIndoor->pSectors[_actor_collision_struct.uSectorID].pPortals[i] !=
            _actor_collision_struct.field_80) {
            BLVFace *face = &pIndoor->pFaces[pIndoor->pSectors[_actor_collision_struct.uSectorID].pPortals[i]];
            if (_actor_collision_struct.sMaxX <= face->pBounding.x2 &&
                _actor_collision_struct.sMinX >= face->pBounding.x1 &&
                _actor_collision_struct.sMaxY <= face->pBounding.y2 &&
                _actor_collision_struct.sMinY >= face->pBounding.y1 &&
                _actor_collision_struct.sMaxZ <= face->pBounding.z2 &&
                _actor_collision_struct.sMinZ >= face->pBounding.z1) {
                int v4 = (_actor_collision_struct.normal.x * face->pFacePlane_old.vNormal.x +
                          face->pFacePlane_old.dist +
                          _actor_collision_struct.normal.y * face->pFacePlane_old.vNormal.y +
                          _actor_collision_struct.normal.z * face->pFacePlane_old.vNormal.z) >> 16;
                int v5 = (_actor_collision_struct.normal2.z * face->pFacePlane_old.vNormal.z +
                          face->pFacePlane_old.dist +
                          _actor_collision_struct.normal2.x * face->pFacePlane_old.vNormal.x +
                          _actor_collision_struct.normal2.y * face->pFacePlane_old.vNormal.y) >> 16;
                if ((v4 < _actor_collision_struct.prolly_normal_d || v5 < _actor_collision_struct.prolly_normal_d) &&
                    (v4 > -_actor_collision_struct.prolly_normal_d || v5 > -_actor_collision_struct.prolly_normal_d) &&
                    (a3 = _actor_collision_struct.field_6C, sub_475D85(&_actor_collision_struct.normal, &_actor_collision_struct.direction, &a3, face)) && a3 < (int)v10) {
                    v10 = a3;
                    v12 = pIndoor->pSectors[_actor_collision_struct.uSectorID].pPortals[i];
                }
            }
        }
    }

    v1 = v10;

    int result = 1;

    if (_actor_collision_struct.field_7C >= (int)v1 && (int)v1 <= _actor_collision_struct.field_6C) {
        _actor_collision_struct.field_80 = v12;
        if (pIndoor->pFaces[v12].uSectorID == _actor_collision_struct.uSectorID) {
            _actor_collision_struct.uSectorID = pIndoor->pFaces[v12].uBackSectorID;
        } else {
            _actor_collision_struct.uSectorID = pIndoor->pFaces[v12].uSectorID;
        }
        _actor_collision_struct.field_7C = 268435455;  // 0xFFFFFFF
        result = 0;
    }

    return result;
}

int _46E44E_collide_against_faces_and_portals(unsigned int b1) {
    BLVSector *pSector;   // edi@1
    int v2;        // ebx@1
    BLVFace *pFace;       // esi@2
    __int16 pNextSector;  // si@10
    int pArrayNum;        // ecx@12
    unsigned __int8 v6;   // sf@12
    unsigned __int8 v7;   // of@12
    int result;           // eax@14
    // int v10; // ecx@15
    int pFloor;             // eax@16
    int v15;                // eax@24
    int v16;                // edx@25
    int v17;                // eax@29
    unsigned int v18;       // eax@33
    int v21;                // eax@35
    int v22;                // ecx@36
    int v23;                // eax@40
    unsigned int v24;       // eax@44
    int a3;                 // [sp+10h] [bp-48h]@28
    int v26;                // [sp+14h] [bp-44h]@15
    int i;                  // [sp+18h] [bp-40h]@1
    int a10;                // [sp+1Ch] [bp-3Ch]@1
    int v29;                // [sp+20h] [bp-38h]@14
    int v32;                // [sp+2Ch] [bp-2Ch]@15
    int pSectorsArray[10];  // [sp+30h] [bp-28h]@1

    pSector = &pIndoor->pSectors[_actor_collision_struct.uSectorID];
    i = 1;
    a10 = b1;
    pSectorsArray[0] = _actor_collision_struct.uSectorID;
    for (v2 = 0; v2 < pSector->uNumPortals; ++v2) {
        pFace = &pIndoor->pFaces[pSector->pPortals[v2]];
        if (_actor_collision_struct.sMaxX <= pFace->pBounding.x2 &&
            _actor_collision_struct.sMinX >= pFace->pBounding.x1 &&
            _actor_collision_struct.sMaxY <= pFace->pBounding.y2 &&
            _actor_collision_struct.sMinY >= pFace->pBounding.y1 &&
            _actor_collision_struct.sMaxZ <= pFace->pBounding.z2 &&
            _actor_collision_struct.sMinZ >= pFace->pBounding.z1 &&
            abs((pFace->pFacePlane_old.dist +
                 _actor_collision_struct.normal.x * pFace->pFacePlane_old.vNormal.x +
                 _actor_collision_struct.normal.y * pFace->pFacePlane_old.vNormal.y +
                 _actor_collision_struct.normal.z * pFace->pFacePlane_old.vNormal.z) >>
                16) <= _actor_collision_struct.field_6C + 16) {
            pNextSector = pFace->uSectorID == _actor_collision_struct.uSectorID
                              ? pFace->uBackSectorID
                              : pFace->uSectorID;  // FrontSectorID
            pArrayNum = i++;
            v7 = i < 10;
            v6 = i - 10 < 0;
            pSectorsArray[pArrayNum] = pNextSector;
            if (!(v6 ^ v7)) break;
        }
    }
    result = 0;
    for (v29 = 0; v29 < i; v29++) {
        pSector = &pIndoor->pSectors[pSectorsArray[v29]];
        v32 = pSector->uNumFloors + pSector->uNumWalls + pSector->uNumCeilings;
        for (v26 = 0; v26 < v32; v26++) {
            pFloor = pSector->pFloors[v26];
            pFace = &pIndoor->pFaces[pSector->pFloors[v26]];
            if (!pFace->Portal() && _actor_collision_struct.sMaxX <= pFace->pBounding.x2 &&
                _actor_collision_struct.sMinX >= pFace->pBounding.x1 &&
                _actor_collision_struct.sMaxY <= pFace->pBounding.y2 &&
                _actor_collision_struct.sMinY >= pFace->pBounding.y1 &&
                _actor_collision_struct.sMaxZ <= pFace->pBounding.z2 &&
                _actor_collision_struct.sMinZ >= pFace->pBounding.z1 &&
                pFloor != _actor_collision_struct.field_84) {
                v15 =
                    (pFace->pFacePlane_old.dist +
                     _actor_collision_struct.normal.x * pFace->pFacePlane_old.vNormal.x +
                     _actor_collision_struct.normal.y * pFace->pFacePlane_old.vNormal.y +
                     _actor_collision_struct.normal.z * pFace->pFacePlane_old.vNormal.z) >>
                    16;
                if (v15 > 0) {
                    v16 = (pFace->pFacePlane_old.dist +
                           _actor_collision_struct.normal2.x *
                               pFace->pFacePlane_old.vNormal.x +
                           _actor_collision_struct.normal2.y *
                               pFace->pFacePlane_old.vNormal.y +
                           _actor_collision_struct.normal2.z *
                               pFace->pFacePlane_old.vNormal.z) >>
                          16;
                    if (v15 <= _actor_collision_struct.prolly_normal_d ||
                        v16 <= _actor_collision_struct.prolly_normal_d) {
                        if (v16 <= v15) {
                            a3 = _actor_collision_struct.field_6C;
                            if (sub_47531C(
                                    _actor_collision_struct.prolly_normal_d, &a3,
                                    _actor_collision_struct.normal.x, _actor_collision_struct.normal.y,
                                    _actor_collision_struct.normal.z,
                                    _actor_collision_struct.direction.x,
                                    _actor_collision_struct.direction.y,
                                    _actor_collision_struct.direction.z, pFace, a10)) {
                                v17 = a3;
                            } else {
                                a3 = _actor_collision_struct.field_6C +
                                     _actor_collision_struct.prolly_normal_d;
                                if (!sub_475D85(&_actor_collision_struct.normal,
                                                &_actor_collision_struct.direction, &a3,
                                                pFace))
                                    goto LABEL_34;
                                v17 = a3 - _actor_collision_struct.prolly_normal_d;
                                a3 -= _actor_collision_struct.prolly_normal_d;
                            }
                            if (v17 < _actor_collision_struct.field_7C) {
                                _actor_collision_struct.field_7C = v17;
                                v18 = 8 * pSector->pFloors[v26];
                                v18 |= 6;
                                _actor_collision_struct.pid = v18;
                            }
                        }
                    }
                }
            LABEL_34:
                if (!(_actor_collision_struct.field_0 & 1) ||
                    (v21 = (pFace->pFacePlane_old.dist +
                            _actor_collision_struct.position.x *
                                pFace->pFacePlane_old.vNormal.x +
                            _actor_collision_struct.position.y *
                                pFace->pFacePlane_old.vNormal.y +
                            _actor_collision_struct.position.z *
                                pFace->pFacePlane_old.vNormal.z) >>
                           16,
                     v21 <= 0) ||
                    (v22 = (pFace->pFacePlane_old.dist +
                            _actor_collision_struct.field_4C *
                                pFace->pFacePlane_old.vNormal.x +
                            _actor_collision_struct.field_50 *
                                pFace->pFacePlane_old.vNormal.y +
                            _actor_collision_struct.field_54 *
                                pFace->pFacePlane_old.vNormal.z) >>
                           16,
                     v21 > _actor_collision_struct.prolly_normal_d) &&
                        v22 > _actor_collision_struct.prolly_normal_d ||
                    v22 > v21)
                    continue;
                a3 = _actor_collision_struct.field_6C;
                if (sub_47531C(_actor_collision_struct.field_8_radius, &a3,
                               _actor_collision_struct.position.x, _actor_collision_struct.position.y,
                               _actor_collision_struct.position.z, _actor_collision_struct.direction.x,
                               _actor_collision_struct.direction.y, _actor_collision_struct.direction.z,
                               pFace, a10)) {
                    v23 = a3;
                    goto LABEL_43;
                }
                a3 = _actor_collision_struct.field_6C + _actor_collision_struct.field_8_radius;
                if (sub_475D85(&_actor_collision_struct.position, &_actor_collision_struct.direction,
                               &a3, pFace)) {
                    v23 = a3 - _actor_collision_struct.prolly_normal_d;
                    a3 -= _actor_collision_struct.prolly_normal_d;
                LABEL_43:
                    if (v23 < _actor_collision_struct.field_7C) {
                        _actor_collision_struct.field_7C = v23;
                        v24 = 8 * pSector->pFloors[v26];
                        v24 |= 6;
                        _actor_collision_struct.pid = v24;
                    }
                }
            }
        }
        result = v29 + 1;
    }
    return result;
}



int _43F5C8_get_point_light_level_with_respect_to_lights(unsigned int uBaseLightLevel, int uSectorID, float x, float y, float z) {
    signed int v6;     // edi@1
    int v8;            // eax@6
    int v9;            // ebx@6
    unsigned int v10;  // ecx@6
    unsigned int v11;  // edx@9
    unsigned int v12;  // edx@11
    signed int v13;    // ecx@12
    BLVLightMM7 *v16;  // esi@20
    int v17;           // ebx@21
    signed int v24;    // ecx@30
    int v26;           // ebx@35
    int v37;           // [sp+Ch] [bp-18h]@37
    int v39;           // [sp+10h] [bp-14h]@23
    int v40;           // [sp+10h] [bp-14h]@36
    int v42;           // [sp+14h] [bp-10h]@22
    unsigned int v43;  // [sp+18h] [bp-Ch]@12
    unsigned int v44;  // [sp+18h] [bp-Ch]@30
    unsigned int v45;  // [sp+18h] [bp-Ch]@44

    v6 = uBaseLightLevel;
    for (uint i = 0; i < pMobileLightsStack->uNumLightsActive; ++i) {
        MobileLight *p = &pMobileLightsStack->pLights[i];

        float distX = abs(p->vPosition.x - x);
        if (distX <= p->uRadius) {
            float distY = abs(p->vPosition.y - y);
            if (distY <= p->uRadius) {
                float distZ = abs(p->vPosition.z - z);
                if (distZ <= p->uRadius) {
                    v8 = distX;
                    v9 = distY;
                    v10 = distZ;
                    if (distX < distY) {
                        v8 = distY;
                        v9 = distX;
                    }
                    if (v8 < distZ) {
                        v11 = v8;
                        v8 = distZ;
                        v10 = v11;
                    }
                    if (v9 < (signed int)v10) {
                        v12 = v10;
                        v10 = v9;
                        v9 = v12;
                    }
                    v43 = ((unsigned int)(11 * v9) / 32) + (v10 / 4) + v8;
                    v13 = p->uRadius;
                    if ((signed int)v43 < v13)
         //* ORIGONAL */v6 += ((unsigned __int64)(30i64 *(signed int)(v43 << 16) / v13) >> 16) - 30;
                        v6 += ((unsigned __int64)(30ll * (signed int)(v43 << 16) / v13) >> 16) - 30;
                }
            }
        }
    }

    if (uCurrentlyLoadedLevelType == LEVEL_Indoor) {
        BLVSector *pSector = &pIndoor->pSectors[uSectorID];

        for (uint i = 0; i < pSector->uNumLights; ++i) {
            v16 = pIndoor->pLights + pSector->pLights[i];
            if (~v16->uAtributes & 8) {
                v17 = abs(v16->vPosition.x - x);
                if (v17 <= v16->uRadius) {
                    v42 = abs(v16->vPosition.y - y);
                    if (v42 <= v16->uRadius) {
                        v39 = abs(v16->vPosition.z - z);
                        if (v39 <= v16->uRadius) {
                            v44 = int_get_vector_length(v17, v42, v39);
                            v24 = v16->uRadius;
                            if ((signed int)v44 < v24)
                                v6 += ((unsigned __int64)(30ll * (signed int)(v44 << 16) / v24) >> 16) - 30;
                        }
                    }
                }
            }
        }
    }

    for (uint i = 0; i < pStationaryLightsStack->uNumLightsActive; ++i) {
        // StationaryLight* p = &pStationaryLightsStack->pLights[i];
        v26 = abs(pStationaryLightsStack->pLights[i].vPosition.x - x);
        if (v26 <= pStationaryLightsStack->pLights[i].uRadius) {
            v40 = abs(pStationaryLightsStack->pLights[i].vPosition.y - y);
            if (v40 <= pStationaryLightsStack->pLights[i].uRadius) {
                v37 = abs(pStationaryLightsStack->pLights[i].vPosition.z - z);
                if (v37 <= pStationaryLightsStack->pLights[i].uRadius) {
                    v45 = int_get_vector_length(v26, v40, v37);
                    // v33 = pStationaryLightsStack->pLights[i].uRadius;
                    if ((signed int)v45 <
                        pStationaryLightsStack->pLights[i].uRadius)
                        v6 += ((unsigned __int64)(30ll * (signed int)(v45 << 16) / pStationaryLightsStack->pLights[i].uRadius) >> 16) - 30;
                }
            }
        }
    }

    if (v6 <= 31) {
        if (v6 < 0) v6 = 0;
    } else {
        v6 = 31;
    }
    return v6;
}


int collide_against_floor(int x, int y, int z, unsigned int *pSectorID, unsigned int *pFaceID) {
    uint uFaceID = -1;
    int floor_level = BLV_GetFloorLevel(x, y, z, *pSectorID, &uFaceID);

    if (floor_level != -30000 && floor_level <= z + 50) {
        *pFaceID = uFaceID;
        return floor_level;
    }

    uint uSectorID = pIndoor->GetSector(x, y, z);
    *pSectorID = uSectorID;

    floor_level = BLV_GetFloorLevel(x, y, z, uSectorID, &uFaceID);
    if (uSectorID && floor_level != -30000)
        *pFaceID = uFaceID;
    else
        return -30000;
    return floor_level;
}

void _46ED8A_collide_against_sprite_objects(unsigned int _this) {
    ObjectDesc *object;  // edx@4
    int v10;             // ecx@12
    int v11;             // esi@13

    for (uint i = 0; i < uNumSpriteObjects; ++i) {
        if (pSpriteObjects[i].uObjectDescID) {
            object = &pObjectList->pObjects[pSpriteObjects[i].uObjectDescID];
            if (!(object->uFlags & OBJECT_DESC_NO_COLLISION)) {
                if (_actor_collision_struct.sMaxX <=
                        pSpriteObjects[i].vPosition.x + object->uRadius &&
                    _actor_collision_struct.sMinX >=
                        pSpriteObjects[i].vPosition.x - object->uRadius &&
                    _actor_collision_struct.sMaxY <=
                        pSpriteObjects[i].vPosition.y + object->uRadius &&
                    _actor_collision_struct.sMinY >=
                        pSpriteObjects[i].vPosition.y - object->uRadius &&
                    _actor_collision_struct.sMaxZ <=
                        pSpriteObjects[i].vPosition.z + object->uHeight &&
                    _actor_collision_struct.sMinZ >= pSpriteObjects[i].vPosition.z) {
                    if (abs(((pSpriteObjects[i].vPosition.x -
                              _actor_collision_struct.normal.x) *
                                 _actor_collision_struct.direction.y -
                             (pSpriteObjects[i].vPosition.y -
                              _actor_collision_struct.normal.y) *
                                 _actor_collision_struct.direction.x) >>
                            16) <=
                        object->uHeight + _actor_collision_struct.prolly_normal_d) {
                        v10 = ((pSpriteObjects[i].vPosition.x -
                                _actor_collision_struct.normal.x) *
                                   _actor_collision_struct.direction.x +
                               (pSpriteObjects[i].vPosition.y -
                                _actor_collision_struct.normal.y) *
                                   _actor_collision_struct.direction.y) >>
                              16;
                        if (v10 > 0) {
                            v11 = _actor_collision_struct.normal.z +
                                  ((unsigned __int64)(_actor_collision_struct.direction.z *
                                                      (signed __int64)v10) >>
                                   16);
                            if (v11 >= pSpriteObjects[i].vPosition.z -
                                           _actor_collision_struct.prolly_normal_d) {
                                if (v11 <= object->uHeight +
                                               _actor_collision_struct.prolly_normal_d +
                                               pSpriteObjects[i].vPosition.z) {
                                    if (v10 < _actor_collision_struct.field_7C) {
                                        sub_46DEF2(_this, i);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}


void UpdateObjects() {
    int v5;   // ecx@6
    int v7;   // eax@9
    int v11;  // eax@17
    int v12;  // edi@27
    int v18;  // [sp+4h] [bp-10h]@27
    int v19;  // [sp+8h] [bp-Ch]@27

    for (uint i = 0; i < uNumSpriteObjects; ++i) {
        if (pSpriteObjects[i].uAttributes & OBJECT_40) {
            pSpriteObjects[i].uAttributes &= ~OBJECT_40;
        } else {
            ObjectDesc *object =
                &pObjectList->pObjects[pSpriteObjects[i].uObjectDescID];
            if (pSpriteObjects[i].AttachedToActor()) {
                v5 = PID_ID(pSpriteObjects[i].spell_target_pid);
                pSpriteObjects[i].vPosition.x = pActors[v5].vPosition.x;
                pSpriteObjects[i].vPosition.y = pActors[v5].vPosition.y;
                pSpriteObjects[i].vPosition.z =
                    pActors[v5].vPosition.z + pActors[v5].uActorHeight;
                if (!pSpriteObjects[i].uObjectDescID) continue;
                pSpriteObjects[i].uSpriteFrameID += pEventTimer->uTimeElapsed;
                if (!(object->uFlags & OBJECT_DESC_TEMPORARY)) continue;
                if (pSpriteObjects[i].uSpriteFrameID >= 0) {
                    v7 = object->uLifetime;
                    if (pSpriteObjects[i].uAttributes & ITEM_BROKEN)
                        v7 = pSpriteObjects[i].field_20;
                    if (pSpriteObjects[i].uSpriteFrameID < v7) continue;
                }
                SpriteObject::OnInteraction(i);
                continue;
            }
            if (pSpriteObjects[i].uObjectDescID) {
                pSpriteObjects[i].uSpriteFrameID += pEventTimer->uTimeElapsed;
                if (object->uFlags & OBJECT_DESC_TEMPORARY) {
                    if (pSpriteObjects[i].uSpriteFrameID < 0) {
                        SpriteObject::OnInteraction(i);
                        continue;
                    }
                    v11 = object->uLifetime;
                    if (pSpriteObjects[i].uAttributes & ITEM_BROKEN)
                        v11 = pSpriteObjects[i].field_20;
                }
                if (!(object->uFlags & OBJECT_DESC_TEMPORARY) ||
                    pSpriteObjects[i].uSpriteFrameID < v11) {
                    if (uCurrentlyLoadedLevelType == LEVEL_Indoor)
                        SpriteObject::UpdateObject_fn0_BLV(i);
                    else
                        SpriteObject::UpdateObject_fn0_ODM(i);
                    if (!pParty->bTurnBasedModeOn || !(pSpriteObjects[i].uSectorID & 4)) {
                        continue;
                    }
                    v12 = abs(pParty->vPosition.x -
                              pSpriteObjects[i].vPosition.x);
                    v18 = abs(pParty->vPosition.y -
                              pSpriteObjects[i].vPosition.y);
                    v19 = abs(pParty->vPosition.z -
                              pSpriteObjects[i].vPosition.z);
                    if (int_get_vector_length(v12, v18, v19) <= 5120) continue;
                    SpriteObject::OnInteraction(i);
                    continue;
                }
                if (!(object->uFlags & OBJECT_DESC_INTERACTABLE)) {
                    SpriteObject::OnInteraction(i);
                    continue;
                }
                _46BFFA_update_spell_fx(i, PID(OBJECT_Item, i));
            }
        }
    }
}

int _43F55F_get_billboard_light_level(RenderBillboard *a1,
                                      int uBaseLightLevel) {
    int v3 = 0;

    if (uCurrentlyLoadedLevelType == LEVEL_Indoor) {
        v3 = pIndoor->pSectors[a1->uIndoorSectorID].uMinAmbientLightLevel;
    } else {
        if (uBaseLightLevel == -1) {
            v3 = a1->dimming_level;
        } else {
            v3 = uBaseLightLevel;
        }
    }

    return _43F5C8_get_point_light_level_with_respect_to_lights(
        v3, a1->uIndoorSectorID, a1->world_x, a1->world_y, a1->world_z);
}

unsigned int sub_46DEF2(signed int a2, unsigned int uLayingItemID) {
    unsigned int result = uLayingItemID;
    if (pObjectList->pObjects[pSpriteObjects[uLayingItemID].uObjectDescID].uFlags & 0x10) {
        result = _46BFFA_update_spell_fx(uLayingItemID, a2);
    }
    return result;
}

bool sub_47531C(int a1, int* a2, int pos_x, int pos_y, int pos_z, int dir_x,
    int dir_y, int dir_z, BLVFace* face, int a10) {
    int v11;          // ST1C_4@3
    int v12;          // edi@3
    int v13;          // esi@3
    int v14;          // edi@4
    __int64 v15;      // qtt@6
                      // __int16 v16; // si@7
    int a7a;          // [sp+30h] [bp+18h]@7
    int a9b;          // [sp+38h] [bp+20h]@3
    int a9a;          // [sp+38h] [bp+20h]@3
    int a10b;         // [sp+3Ch] [bp+24h]@3
    signed int a10a;  // [sp+3Ch] [bp+24h]@4
    int a10c;         // [sp+3Ch] [bp+24h]@5

    if (a10 && face->Ethereal()) return 0;
    v11 = fixpoint_mul(dir_x, face->pFacePlane_old.vNormal.x);
    a10b = fixpoint_mul(dir_y, face->pFacePlane_old.vNormal.y);
    a9b = fixpoint_mul(dir_z, face->pFacePlane_old.vNormal.z);
    v12 = v11 + a9b + a10b;
    a9a = v11 + a9b + a10b;
    v13 = (a1 << 16) - pos_x * face->pFacePlane_old.vNormal.x -
        pos_y * face->pFacePlane_old.vNormal.y -
        pos_z * face->pFacePlane_old.vNormal.z - face->pFacePlane_old.dist;
    if (abs((a1 << 16) - pos_x * face->pFacePlane_old.vNormal.x -
        pos_y * face->pFacePlane_old.vNormal.y -
        pos_z * face->pFacePlane_old.vNormal.z -
        face->pFacePlane_old.dist) >= a1 << 16) {
        a10c = abs(v13) >> 14;
        if (a10c > abs(v12)) return 0;
        HEXRAYS_LODWORD(v15) = v13 << 16;
        HEXRAYS_HIDWORD(v15) = v13 >> 16;
        v14 = a1;
        a10a = v15 / a9a;
    } else {
        a10a = 0;
        v14 = abs(v13) >> 16;
    }
    // v16 = pos_y + ((unsigned int)fixpoint_mul(a10a, dir_y) >> 16);
    HEXRAYS_LOWORD(a7a) = (short)pos_x +
        ((unsigned int)fixpoint_mul(a10a, dir_x) >> 16) -
        fixpoint_mul(v14, face->pFacePlane_old.vNormal.x);
    HEXRAYS_HIWORD(a7a) = pos_y +
        ((unsigned int)fixpoint_mul(a10a, dir_y) >> 16) -
        fixpoint_mul(v14, face->pFacePlane_old.vNormal.y);
    if (!sub_475665(face, a7a,
        (short)pos_z +
    ((unsigned int)fixpoint_mul(a10a, dir_z) >> 16) -
        fixpoint_mul(v14, face->pFacePlane_old.vNormal.z)))
        return 0;
    *a2 = a10a >> 16;
    if (a10a >> 16 < 0) *a2 = 0;
    return 1;
}

bool sub_4754BF(int a1, int* a2, int X, int Y, int Z, int dir_x, int dir_y,
    int dir_z, BLVFace* face, int a10, int a11) {
    int v12;      // ST1C_4@3
    int v13;      // edi@3
    int v14;      // esi@3
    int v15;      // edi@4
    int64_t v16;  // qtt@6
                  // __int16 v17; // si@7
    int a7a;      // [sp+30h] [bp+18h]@7
    int a1b;      // [sp+38h] [bp+20h]@3
    int a1a;      // [sp+38h] [bp+20h]@3
    int a11b;     // [sp+40h] [bp+28h]@3
    int a11a;     // [sp+40h] [bp+28h]@4
    int a11c;     // [sp+40h] [bp+28h]@5

    if (a11 && face->Ethereal()) return false;
    v12 = fixpoint_mul(dir_x, face->pFacePlane_old.vNormal.x);
    a11b = fixpoint_mul(dir_y, face->pFacePlane_old.vNormal.y);
    a1b = fixpoint_mul(dir_z, face->pFacePlane_old.vNormal.z);
    v13 = v12 + a1b + a11b;
    a1a = v12 + a1b + a11b;
    v14 = (a1 << 16) - X * face->pFacePlane_old.vNormal.x -
        Y * face->pFacePlane_old.vNormal.y -
        Z * face->pFacePlane_old.vNormal.z - face->pFacePlane_old.dist;
    if (abs((a1 << 16) - X * face->pFacePlane_old.vNormal.x -
        Y * face->pFacePlane_old.vNormal.y -
        Z * face->pFacePlane_old.vNormal.z - face->pFacePlane_old.dist) >=
        a1 << 16) {
        a11c = abs(v14) >> 14;
        if (a11c > abs(v13)) return false;
        HEXRAYS_LODWORD(v16) = v14 << 16;
        HEXRAYS_HIDWORD(v16) = v14 >> 16;
        v15 = a1;
        a11a = v16 / a1a;
    } else {
        a11a = 0;
        v15 = abs(v14) >> 16;
    }
    // v17 = Y + ((unsigned int)fixpoint_mul(a11a, dir_y) >> 16);
    HEXRAYS_LOWORD(a7a) = (short)X +
        ((unsigned int)fixpoint_mul(a11a, dir_x) >> 16) -
        fixpoint_mul(v15, face->pFacePlane_old.vNormal.x);
    HEXRAYS_HIWORD(a7a) = Y + ((unsigned int)fixpoint_mul(a11a, dir_y) >> 16) -
        fixpoint_mul(v15, face->pFacePlane_old.vNormal.y);
    if (!sub_4759C9(face, a10, a7a,
        (short)Z + ((unsigned int)fixpoint_mul(a11a, dir_z) >> 16) -
        fixpoint_mul(v15, face->pFacePlane_old.vNormal.z)))
        return false;
    *a2 = a11a >> 16;
    if (a11a >> 16 < 0) *a2 = 0;
    return true;
}

int sub_475665(BLVFace* face, int a2, __int16 a3) {
    bool v16;     // edi@14
    int v20;      // ebx@18
    int v21;      // edi@20
    int v22;      // ST14_4@22
    __int64 v23;  // qtt@22
    int result;   // eax@25
    int v25;      // [sp+14h] [bp-10h]@14
    int v26;      // [sp+1Ch] [bp-8h]@2
    int v27;      // [sp+20h] [bp-4h]@2
    int v28;      // [sp+30h] [bp+Ch]@2
    int v29;      // [sp+30h] [bp+Ch]@7
    int v30;      // [sp+30h] [bp+Ch]@11
    int v31;      // [sp+30h] [bp+Ch]@14

    if (face->uAttributes & FACE_XY_PLANE) {
        v26 = (signed __int16)a2;
        v27 = HEXRAYS_SHIWORD(a2);
        if (face->uNumVertices) {
            for (v28 = 0; v28 < face->uNumVertices; v28++) {
                word_720C10_intercepts_xs[2 * v28] =
                    face->pXInterceptDisplacements[v28] +
                    pIndoor->pVertices[face->pVertexIDs[v28]].x;
                word_720B40_intercepts_zs[2 * v28] =
                    face->pYInterceptDisplacements[v28] +
                    pIndoor->pVertices[face->pVertexIDs[v28]].y;
                word_720C10_intercepts_xs[2 * v28 + 1] =
                    face->pXInterceptDisplacements[v28 + 1] +
                    pIndoor->pVertices[face->pVertexIDs[v28 + 1]].x;
                word_720B40_intercepts_zs[2 * v28 + 1] =
                    face->pYInterceptDisplacements[v28 + 1] +
                    pIndoor->pVertices[face->pVertexIDs[v28 + 1]].y;
            }
        }
    } else {
        if (face->uAttributes & FACE_XZ_PLANE) {
            v26 = (signed __int16)a2;
            v27 = a3;
            if (face->uNumVertices) {
                for (v29 = 0; v29 < face->uNumVertices; v29++) {
                    word_720C10_intercepts_xs[2 * v29] =
                        face->pXInterceptDisplacements[v29] +
                        pIndoor->pVertices[face->pVertexIDs[v29]].x;
                    word_720B40_intercepts_zs[2 * v29] =
                        face->pZInterceptDisplacements[v29] +
                        pIndoor->pVertices[face->pVertexIDs[v29]].z;
                    word_720C10_intercepts_xs[2 * v29 + 1] =
                        face->pXInterceptDisplacements[v29 + 1] +
                        pIndoor->pVertices[face->pVertexIDs[v29 + 1]].x;
                    word_720B40_intercepts_zs[2 * v29 + 1] =
                        face->pZInterceptDisplacements[v29 + 1] +
                        pIndoor->pVertices[face->pVertexIDs[v29 + 1]].z;
                }
            }
        } else {
            v26 = HEXRAYS_SHIWORD(a2);
            v27 = a3;
            if (face->uNumVertices) {
                for (v30 = 0; v30 < face->uNumVertices; v30++) {
                    word_720C10_intercepts_xs[2 * v30] =
                        face->pYInterceptDisplacements[v30] +
                        pIndoor->pVertices[face->pVertexIDs[v30]].y;
                    word_720B40_intercepts_zs[2 * v30] =
                        face->pZInterceptDisplacements[v30] +
                        pIndoor->pVertices[face->pVertexIDs[v30]].z;
                    word_720C10_intercepts_xs[2 * v30 + 1] =
                        face->pYInterceptDisplacements[v30 + 1] +
                        pIndoor->pVertices[face->pVertexIDs[v30 + 1]].y;
                    word_720B40_intercepts_zs[2 * v30 + 1] =
                        face->pZInterceptDisplacements[v30 + 1] +
                        pIndoor->pVertices[face->pVertexIDs[v30 + 1]].z;
                }
            }
        }
    }
    v31 = 0;
    word_720C10_intercepts_xs[2 * face->uNumVertices] =
        word_720C10_intercepts_xs[0];
    word_720B40_intercepts_zs[2 * face->uNumVertices] =
        word_720B40_intercepts_zs[0];
    v16 = word_720B40_intercepts_zs[0] >= v27;
    if (2 * face->uNumVertices <= 0) return 0;
    for (v25 = 0; v25 < 2 * face->uNumVertices; ++v25) {
        if (v31 >= 2) break;
        if (v16 ^ (word_720B40_intercepts_zs[v25 + 1] >= v27)) {
            if (word_720C10_intercepts_xs[v25 + 1] >= v26)
                v20 = 0;
            else
                v20 = 2;
            v21 = v20 | (word_720C10_intercepts_xs[v25] < v26);
            if (v21 != 3) {
                v22 = word_720C10_intercepts_xs[v25 + 1] -
                    word_720C10_intercepts_xs[v25];
                HEXRAYS_LODWORD(v23) = v22 << 16;
                HEXRAYS_HIDWORD(v23) = v22 >> 16;
                if (!v21 ||
                    (word_720C10_intercepts_xs[v25] +
                        ((signed int)(((unsigned __int64)(v23 /
                            (word_720B40_intercepts_zs
                                [v25 + 1] -
                                word_720B40_intercepts_zs
                                [v25]) *
                            ((v27 -
                                (signed int)
                                word_720B40_intercepts_zs
                                [v25])
                                << 16)) >>
                            16) +
                            32768) >>
                            16) >=
                        v26))
                    ++v31;
            }
        }
        v16 = word_720B40_intercepts_zs[v25 + 1] >= v27;
    }
    result = 1;
    if (v31 != 1) result = 0;
    return result;
}

bool sub_4759C9(BLVFace* face, int a2, int a3, __int16 a4) {
    bool v12;            // edi@14
    signed int v16;      // ebx@18
    int v17;             // edi@20
    signed int v18;      // ST14_4@22
    signed __int64 v19;  // qtt@22
    bool result;         // eax@25
    int v21;             // [sp+14h] [bp-10h]@14
    signed int v22;      // [sp+18h] [bp-Ch]@1
    int v23;             // [sp+1Ch] [bp-8h]@2
    signed int v24;      // [sp+20h] [bp-4h]@2
    signed int a4d;      // [sp+30h] [bp+Ch]@14

    if (face->uAttributes & FACE_XY_PLANE) {
        v23 = (signed __int16)a3;
        v24 = HEXRAYS_SHIWORD(a3);
        if (face->uNumVertices) {
            for (v22 = 0; v22 < face->uNumVertices; ++v22) {
                word_720A70_intercepts_xs_plus_xs[2 * v22] =
                    face->pXInterceptDisplacements[v22] +
                    LOWORD(pOutdoor->pBModels[a2]
                        .pVertices.pVertices[face->pVertexIDs[v22]]
                        .x);
                word_7209A0_intercepts_ys_plus_ys[2 * v22] =
                    face->pYInterceptDisplacements[v22] +
                    LOWORD(pOutdoor->pBModels[a2]
                        .pVertices.pVertices[face->pVertexIDs[v22]]
                        .y);
                word_720A70_intercepts_xs_plus_xs[2 * v22 + 1] =
                    face->pXInterceptDisplacements[v22 + 1] +
                    LOWORD(pOutdoor->pBModels[a2]
                        .pVertices.pVertices[face->pVertexIDs[v22 + 1]]
                        .x);
                word_7209A0_intercepts_ys_plus_ys[2 * v22 + 1] =
                    face->pYInterceptDisplacements[v22 + 1] +
                    LOWORD(pOutdoor->pBModels[a2]
                        .pVertices.pVertices[face->pVertexIDs[v22 + 1]]
                        .y);
            }
        }
    } else {
        if (face->uAttributes & FACE_XZ_PLANE) {
            v23 = (signed __int16)a3;
            v24 = a4;
            if (face->uNumVertices) {
                for (v22 = 0; v22 < face->uNumVertices; ++v22) {
                    word_720A70_intercepts_xs_plus_xs[2 * v22] =
                        face->pXInterceptDisplacements[v22] +
                        LOWORD(pOutdoor->pBModels[a2]
                            .pVertices.pVertices[face->pVertexIDs[v22]]
                            .x);
                    word_7209A0_intercepts_ys_plus_ys[2 * v22] =
                        face->pZInterceptDisplacements[v22] +
                        LOWORD(pOutdoor->pBModels[a2]
                            .pVertices.pVertices[face->pVertexIDs[v22]]
                            .z);
                    word_720A70_intercepts_xs_plus_xs[2 * v22 + 1] =
                        face->pXInterceptDisplacements[v22 + 1] +
                        LOWORD(
                            pOutdoor->pBModels[a2]
                            .pVertices.pVertices[face->pVertexIDs[v22 + 1]]
                            .x);
                    word_7209A0_intercepts_ys_plus_ys[2 * v22 + 1] =
                        face->pZInterceptDisplacements[v22 + 1] +
                        LOWORD(
                            pOutdoor->pBModels[a2]
                            .pVertices.pVertices[face->pVertexIDs[v22 + 1]]
                            .z);
                }
            }
        } else {
            v23 = HEXRAYS_SHIWORD(a3);
            v24 = a4;
            if (face->uNumVertices) {
                for (v22 = 0; v22 < face->uNumVertices; ++v22) {
                    word_720A70_intercepts_xs_plus_xs[2 * v22] =
                        face->pYInterceptDisplacements[v22] +
                        LOWORD(pOutdoor->pBModels[a2]
                            .pVertices.pVertices[face->pVertexIDs[v22]]
                            .y);
                    word_7209A0_intercepts_ys_plus_ys[2 * v22] =
                        face->pZInterceptDisplacements[v22] +
                        LOWORD(pOutdoor->pBModels[a2]
                            .pVertices.pVertices[face->pVertexIDs[v22]]
                            .z);
                    word_720A70_intercepts_xs_plus_xs[2 * v22 + 1] =
                        face->pYInterceptDisplacements[v22 + 1] +
                        LOWORD(
                            pOutdoor->pBModels[a2]
                            .pVertices.pVertices[face->pVertexIDs[v22 + 1]]
                            .y);
                    word_7209A0_intercepts_ys_plus_ys[2 * v22 + 1] =
                        face->pZInterceptDisplacements[v22 + 1] +
                        LOWORD(
                            pOutdoor->pBModels[a2]
                            .pVertices.pVertices[face->pVertexIDs[v22 + 1]]
                            .z);
                }
            }
        }
    }
    a4d = 0;
    word_720A70_intercepts_xs_plus_xs[2 * face->uNumVertices] =
        word_720A70_intercepts_xs_plus_xs[0];
    word_7209A0_intercepts_ys_plus_ys[2 * face->uNumVertices] =
        word_7209A0_intercepts_ys_plus_ys[0];
    v12 = word_7209A0_intercepts_ys_plus_ys[0] >= v24;
    if (2 * face->uNumVertices <= 0) return 0;
    for (v21 = 0; v21 < 2 * face->uNumVertices; ++v21) {
        if (a4d >= 2) break;
        if (v12 ^ (word_7209A0_intercepts_ys_plus_ys[v21 + 1] >= v24)) {
            if (word_720A70_intercepts_xs_plus_xs[v21 + 1] >= v23)
                v16 = 0;
            else
                v16 = 2;
            v17 = v16 | (word_720A70_intercepts_xs_plus_xs[v21] < v23);
            if (v17 != 3) {
                v18 = word_720A70_intercepts_xs_plus_xs[v21 + 1] -
                    word_720A70_intercepts_xs_plus_xs[v21];
                HEXRAYS_LODWORD(v19) = v18 << 16;
                HEXRAYS_HIDWORD(v19) = v18 >> 16;
                if (!v17 ||
                    (word_720A70_intercepts_xs_plus_xs[v21] +
                        ((signed int)(((unsigned __int64)(v19 /
                            (word_7209A0_intercepts_ys_plus_ys
                                [v21 + 1] -
                                word_7209A0_intercepts_ys_plus_ys
                                [v21]) *
                            ((v24 -
                                (signed int)
                                word_7209A0_intercepts_ys_plus_ys
                                [v21])
                                << 16)) >>
                            16) +
                            0x8000) >>
                            16) >=
                        v23))
                    ++a4d;
            }
        }
        v12 = word_7209A0_intercepts_ys_plus_ys[v21 + 1] >= v24;
    }
    result = 1;
    if (a4d != 1) result = 0;
    return result;
}

bool sub_475D85(Vec3_int_* a1, Vec3_int_* a2, int* a3, BLVFace* a4) {
    BLVFace* v4;         // ebx@1
    int v5;              // ST24_4@2
    int v6;              // ST28_4@2
    int v7;              // edi@2
    int v8;              // eax@5
    signed int v9;       // esi@5
    signed __int64 v10;  // qtt@10
    Vec3_int_* v11;      // esi@11
    int v12;             // ST14_4@11
    Vec3_int_* v14;      // [sp+Ch] [bp-18h]@1
    Vec3_int_* v15;      // [sp+14h] [bp-10h]@1
    int v17;             // [sp+20h] [bp-4h]@10
    int a4b;             // [sp+30h] [bp+Ch]@2
    int a4c;             // [sp+30h] [bp+Ch]@9
    int a4a;             // [sp+30h] [bp+Ch]@10

    v4 = a4;
    v15 = a2;
    v14 = a1;
    v5 = fixpoint_mul(a2->x, a4->pFacePlane_old.vNormal.x);
    a4b = fixpoint_mul(a2->y, a4->pFacePlane_old.vNormal.y);
    v6 = fixpoint_mul(a2->z, v4->pFacePlane_old.vNormal.z);
    v7 = v5 + v6 + a4b;
    // (v16 = v5 + v6 + a4b) == 0;
    if (a4->uAttributes & FACE_ETHEREAL || !v7 || v7 > 0 && !v4->Portal())
        return 0;
    v8 = v4->pFacePlane_old.vNormal.z * a1->z;
    v9 = -(v4->pFacePlane_old.dist + v8 + a1->y * v4->pFacePlane_old.vNormal.y +
        a1->x * v4->pFacePlane_old.vNormal.x);
    if (v7 <= 0) {
        if (v4->pFacePlane_old.dist + v8 +
            a1->y * v4->pFacePlane_old.vNormal.y +
            a1->x * v4->pFacePlane_old.vNormal.x <
            0)
            return 0;
    } else {
        if (v9 < 0) return 0;
    }
    a4c = abs(-(v4->pFacePlane_old.dist + v8 +
        a1->y * v4->pFacePlane_old.vNormal.y +
        a1->x * v4->pFacePlane_old.vNormal.x)) >>
        14;
    v11 = v14;
    HEXRAYS_LODWORD(v10) = v9 << 16;
    HEXRAYS_HIDWORD(v10) = v9 >> 16;
    a4a = v10 / v7;
    v17 = v10 / v7;
    HEXRAYS_LOWORD(v12) =
        HEXRAYS_LOWORD(v14->x) +
        (((unsigned int)fixpoint_mul(v17, v15->x) + 0x8000) >> 16);
    HEXRAYS_HIWORD(v12) =
        HEXRAYS_LOWORD(v11->y) +
        (((unsigned int)fixpoint_mul(v17, v15->y) + 0x8000) >> 16);
    if (a4c > abs(v7) || (v17 > * a3 << 16) ||
        !sub_475665(
            v4, v12,
            LOWORD(v11->z) +
        (((unsigned int)fixpoint_mul(v17, v15->z) + 0x8000) >> 16)))
        return 0;
    *a3 = a4a >> 16;
    return 1;
}

bool sub_475F30(int* a1, BLVFace* a2, int a3, int a4, int a5, int a6, int a7,
    int a8, int a9) {
    int v10 = fixpoint_mul(a6, a2->pFacePlane_old.vNormal.x);
    int v11 = fixpoint_mul(a7, a2->pFacePlane_old.vNormal.y);
    int v12 = fixpoint_mul(a8, a2->pFacePlane_old.vNormal.z);
    int v13 = v10 + v12 + v11;
    int v14 = v10 + v12 + v11;
    int v22 = v10 + v12 + v11;
    if (a2->Ethereal() || !v13 || v14 > 0 && !a2->Portal()) {
        return false;
    }
    int v16 = -(a2->pFacePlane_old.dist + a4 * a2->pFacePlane_old.vNormal.y +
        a3 * a2->pFacePlane_old.vNormal.x +
        a5 * a2->pFacePlane_old.vNormal.z);
    if (v14 <= 0) {
        if (a2->pFacePlane_old.dist + a4 * a2->pFacePlane_old.vNormal.y +
            a3 * a2->pFacePlane_old.vNormal.x +
            a5 * a2->pFacePlane_old.vNormal.z <
            0)
            return 0;
    } else {
        if (v16 < 0) {
            return 0;
        }
    }
    int v17 =
        abs(-(a2->pFacePlane_old.dist + a4 * a2->pFacePlane_old.vNormal.y +
            a3 * a2->pFacePlane_old.vNormal.x +
            a5 * a2->pFacePlane_old.vNormal.z)) >>
        14;
    int64_t v18;
    HEXRAYS_LODWORD(v18) = v16 << 16;
    HEXRAYS_HIDWORD(v18) = v16 >> 16;
    int v24 = v18 / v22;
    int v23 = v18 / v22;
    int v19;
    HEXRAYS_LOWORD(v19) =
        a3 + (((unsigned int)fixpoint_mul(v23, a6) + 0x8000) >> 16);
    HEXRAYS_HIWORD(v19) =
        a4 + (((unsigned int)fixpoint_mul(v23, a7) + 0x8000) >> 16);
    if (v17 > abs(v14) || v23 > * a1 << 16 ||
        !sub_4759C9(
            a2, a9, v19,
            a5 + (((unsigned int)fixpoint_mul(v23, a8) + 0x8000) >> 16)))
        return 0;
    *a1 = v24 >> 16;
    return 1;
}


RenderVertexSoft array_507D30[50];

// sky billboard stuff

void SkyBillboardStruct::CalcSkyFrustumVec(int x1, int y1, int z1, int x2, int y2, int z2) {
    // 6 0 0 0 6 0

    //<< 16

    // transform to odd axis??


    float cosz = pIndoorCameraD3D->fRotationZCosine;  // int_cosine_Z;
    float cosx = pIndoorCameraD3D->fRotationXCosine;  // int_cosine_x;
    float sinz = pIndoorCameraD3D->fRotationZSine;  // int_sine_Z;
    float sinx = pIndoorCameraD3D->fRotationXSine;  // int_sine_x;

    // positions all minus ?
    float v11 = cosz * -pIndoorCameraD3D->vPartyPos.x + sinz * -pIndoorCameraD3D->vPartyPos.y;
    float v24 = cosz * -pIndoorCameraD3D->vPartyPos.y - sinz * -pIndoorCameraD3D->vPartyPos.x;

    // cam position transform
    if (pIndoorCameraD3D->sRotationX) {
        this->field_0_party_dir_x = (v11 * cosx) + (-pIndoorCameraD3D->vPartyPos.z * sinx);
        this->field_4_party_dir_y = v24;
        this->field_8_party_dir_z = (-pIndoorCameraD3D->vPartyPos.z * cosx) /*-*/ + (v11 * sinx);
    } else {
        this->field_0_party_dir_x = v11;
        this->field_4_party_dir_y = v24;
        this->field_8_party_dir_z = (-pIndoorCameraD3D->vPartyPos.z);
    }

    // set 1 position transfrom (6 0 0) looks like cam left vector
    if (pIndoorCameraD3D->sRotationX) {
        float v17 = (x1 * cosz) + (y1 * sinz);

        this->CamVecLeft_Z = (v17 * cosx) + (z1 * sinx);  // dz
        this->CamVecLeft_X = (y1 * cosz) - (x1 * sinz);  // dx
        this->CamVecLeft_Y = (z1 * cosx) /*-*/ + (v17 * sinx);  // dy
    } else {
        this->CamVecLeft_Z = (x1 * cosz) + (y1 * sinz);  // dz
        this->CamVecLeft_X = (y1 * cosz) - (x1 * sinz);  // dx
        this->CamVecLeft_Y = z1;  // dy
    }

    // set 2 position transfrom (0 6 0) looks like cam front vector
    if (pIndoorCameraD3D->sRotationX) {
        float v19 = (x2 * cosz) + (y2 * sinz);

        this->CamVecFront_Z = (v19 * cosx) + (z2 * sinx);  // dz
        this->CamVecFront_X = (y2 * cosz) - (x2 * sinz);  // dx
        this->CamVecFront_Y = (z2 * cosx) /*-*/ + (v19 * sinx);  // dy
    } else {
        this->CamVecFront_Z = (x2 * cosz) + (y2 * sinz);  // dz
        this->CamVecFront_X = (y2 * cosz) - (x2 * sinz);  // dx
        this->CamVecFront_Y = z2;  // dy
    }

    this->CamLeftDot =
        (this->CamVecLeft_X * this->field_0_party_dir_x) +
        (this->CamVecLeft_Z * this->field_4_party_dir_y) +
        (this->CamVecLeft_Y * this->field_8_party_dir_z);
    this->CamFrontDot =
        (this->CamVecFront_X * this->field_0_party_dir_x) +
        (this->CamVecFront_Z * this->field_4_party_dir_y) +
        (this->CamVecFront_Y * this->field_8_party_dir_z);
}

RenderOpenGL::RenderOpenGL(
    std::shared_ptr<OSWindow> window,
    DecalBuilder* decal_builder,
    LightmapBuilder* lightmap_builder,
    SpellFxRenderer* spellfx,
    std::shared_ptr<ParticleEngine> particle_engine,
    Vis* vis,
    Log* logger
) : RenderBase(window, decal_builder, lightmap_builder, spellfx, particle_engine, vis, logger) {
    clip_w = 0;
    clip_x = 0;
    clip_y = 0;
    clip_z = 0;
    render_target_rgb = nullptr;
}

RenderOpenGL::~RenderOpenGL() { /*__debugbreak();*/ }

void RenderOpenGL::Release() { __debugbreak(); }

void RenderOpenGL::SaveWinnersCertificate(const char *a1) { __debugbreak(); }

bool RenderOpenGL::InitializeFullscreen() {
    __debugbreak();
    return 0;
}

unsigned int RenderOpenGL::GetActorTintColor(int DimLevel, int tint, float WorldViewX, int a5, RenderBillboard *Billboard) {
    // GetActorTintColor(int max_dimm, int min_dimm, float distance, int a4, RenderBillboard *a5)
    return ::GetActorTintColor(DimLevel, tint, WorldViewX, a5, Billboard);
}


// when losing and regaining window focus - not required for OGL??
void RenderOpenGL::RestoreFrontBuffer() { /*__debugbreak();*/ }
void RenderOpenGL::RestoreBackBuffer() { /*__debugbreak();*/ }





unsigned int RenderOpenGL::GetRenderWidth() const { return window->GetWidth(); }
unsigned int RenderOpenGL::GetRenderHeight() const { return window->GetHeight(); }

void RenderOpenGL::ClearBlack() {  // used only at start and in game over win
    ClearZBuffer();
    ClearTarget(0);
}

void RenderOpenGL::ClearTarget(unsigned int uColor) {
    memset32(render_target_rgb, Color32(uColor), (window->GetWidth() * window->GetHeight()));
}



void RenderOpenGL::CreateZBuffer() {
    if (!pActiveZBuffer) {
        pActiveZBuffer = (int*)malloc(window->GetWidth() * window->GetHeight() * sizeof(int));
        ClearZBuffer();
    }
}


void RenderOpenGL::ClearZBuffer() {
    memset32(this->pActiveZBuffer, 0xFFFF0000, window->GetWidth() * window->GetHeight());
}

struct linesverts {
    GLfloat x;
    GLfloat y;
    GLfloat r;
    GLfloat g;
    GLfloat b;
};

linesverts lineshaderstore[500] = {};
int linevertscnt = 0;

void RenderOpenGL::RasterLine2D(signed int uX, signed int uY, signed int uZ,
                                signed int uW, unsigned __int16 uColor) {
    unsigned int b = (uColor & 0x1F)*8;
    unsigned int g = ((uColor >> 5) & 0x3F)*4;
    unsigned int r = ((uColor >> 11) & 0x1F)*8;

    // pixel centers around 0.5 so tweak to avoid gaps and squashing
    if (uZ == uX) {
       uW += 1;
    }

    lineshaderstore[linevertscnt].x = uX;
    lineshaderstore[linevertscnt].y = uY;
    lineshaderstore[linevertscnt].r = r / 255.;
    lineshaderstore[linevertscnt].g = g / 255.;
    lineshaderstore[linevertscnt].b = b / 255.;
    linevertscnt++;

    lineshaderstore[linevertscnt].x = uZ + 0.5;
    lineshaderstore[linevertscnt].y = uW + 0.5;
    lineshaderstore[linevertscnt].r = r / 255.;
    lineshaderstore[linevertscnt].g = g / 255.;
    lineshaderstore[linevertscnt].b = b / 255.;
    linevertscnt++;

    if (linevertscnt == 500) Drawlines();
    //glLineWidth(1);
}

void RenderOpenGL::Drawlines() {
    if (!linevertscnt) return;
    // draw lines from store

    if (lineVAO == 0) {
        glGenVertexArrays(1, &lineVAO);
        glGenBuffers(1, &lineVBO);

        glBindVertexArray(lineVAO);
        glBindBuffer(GL_ARRAY_BUFFER, lineVBO);

        glBufferData(GL_ARRAY_BUFFER, sizeof(lineshaderstore), lineshaderstore, GL_DYNAMIC_DRAW);

        // position attribute
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, (5 * sizeof(GLfloat)), (void*)0);
        glEnableVertexAttribArray(0);
        // colour attribute
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, (5 * sizeof(GLfloat)), (void*)(2 * sizeof(GLfloat)));
        glEnableVertexAttribArray(1);

        checkglerror();
    }

    // update buffer
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);

    // glBufferData(GL_ARRAY_BUFFER, sizeof(shaderverts)* numBSPverts, BSPshaderstore, GL_STREAM_DRAW);  // GL_STATIC_DRAW
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(linesverts) * linevertscnt, lineshaderstore);

    checkglerror();

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(lineVAO);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);


    glUseProgram(lineshader.ID);

    // set sampler to texure0
    //glUniform1i(0, 0); ??? 

    //GLfloat projmat[16];
    //glGetFloatv(GL_PROJECTION_MATRIX, projmat);
    // GLfloat viewmat[16];
    //glGetFloatv(GL_MODELVIEW_MATRIX, viewmat);


    //logger->Info("after");
    //// set projection
    glUniformMatrix4fv(glGetUniformLocation(lineshader.ID, "projection"), 1, GL_FALSE, &projmat[0][0]);
    //// set view
    glUniformMatrix4fv(glGetUniformLocation(lineshader.ID, "view"), 1, GL_FALSE, &viewmat[0][0]);
    
    
    
    glDrawArrays(GL_LINES, 0, (linevertscnt));
    drawcalls++;

    glUseProgram(0);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(0);

    linevertscnt = 0;
    checkglerror();
}

void RenderOpenGL::BeginSceneD3D() {
    // Setup for 3D

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    glClearColor(0, 0, 0, 0/*0.9f, 0.5f, 0.1f, 1.0f*/);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    render->uNumBillboardsToDraw = 0;  // moved from drawbillboards - cant reset this until mouse picking finished
    checkglerror();
}

extern unsigned int BlendColors(unsigned int a1, unsigned int a2);

void RenderOpenGL::DrawBillboard_Indoor(SoftwareBillboard *pSoftBillboard,
                                        RenderBillboard *billboard) {
    int v11;     // eax@9
    int v12;     // eax@9
    double v15;  // st5@12
    double v16;  // st4@12
    double v17;  // st3@12
    double v18;  // st2@12
    int v19;     // ecx@14
    double v20;  // st3@14
    int v21;     // ecx@16
    double v22;  // st3@16
    float v27;   // [sp+24h] [bp-Ch]@5
    float v29;   // [sp+2Ch] [bp-4h]@5
    float v31;   // [sp+3Ch] [bp+Ch]@5
    float a1;    // [sp+40h] [bp+10h]@5

    // if (this->uNumD3DSceneBegins == 0) {
    //    return;
    //}

    Sprite *pSprite = billboard->hwsprite;
    int dimming_level = billboard->dimming_level;

    // v4 = pSoftBillboard;
    // v5 = (double)pSoftBillboard->zbuffer_depth;
    // pSoftBillboarda = pSoftBillboard->zbuffer_depth;
    // v6 = pSoftBillboard->zbuffer_depth;
    unsigned int v7 = Billboard_ProbablyAddToListAndSortByZOrder(
        pSoftBillboard->screen_space_z);
    // v8 = dimming_level;
    // device_caps = v7;
    int v28 = dimming_level & 0xFF000000;
    if (dimming_level & 0xFF000000) {
        pBillboardRenderListD3D[v7].opacity = RenderBillboardD3D::Opaque_3;
    } else {
        pBillboardRenderListD3D[v7].opacity = RenderBillboardD3D::Transparent;
    }
    // v10 = a3;
    pBillboardRenderListD3D[v7].field_90 = pSoftBillboard->field_44;
    pBillboardRenderListD3D[v7].screen_space_z = pSoftBillboard->screen_space_z;
    pBillboardRenderListD3D[v7].object_pid = pSoftBillboard->object_pid;
    pBillboardRenderListD3D[v7].sParentBillboardID =
        pSoftBillboard->sParentBillboardID;
    // v25 = pSoftBillboard->uScreenSpaceX;
    // v24 = pSoftBillboard->uScreenSpaceY;
    a1 = pSoftBillboard->screenspace_projection_factor_x;
    v29 = pSoftBillboard->screenspace_projection_factor_y;
    v31 = (double)((pSprite->uBufferWidth >> 1) - pSprite->uAreaX);
    v27 = (double)(pSprite->uBufferHeight - pSprite->uAreaY);
    if (pSoftBillboard->uFlags & 4) {
        v31 = v31 * -1.0;
    }
    if (config->is_tinting && pSoftBillboard->sTintColor) {
        v11 = ::GetActorTintColor(dimming_level, 0,
            pSoftBillboard->screen_space_z, 0, 0);
        v12 = BlendColors(pSoftBillboard->sTintColor, v11);
        if (v28)
            v12 =
            (uint64_t)((char *)&array_77EC08[1852].pEdgeList1[17] + 3) &
            ((unsigned int)v12 >> 1);
    } else {
        v12 = ::GetActorTintColor(dimming_level, 0,
            pSoftBillboard->screen_space_z, 0, 0);
    }
    // v13 = (double)v25;
    pBillboardRenderListD3D[v7].pQuads[0].specular = 0;
    pBillboardRenderListD3D[v7].pQuads[0].diffuse = v12;
    pBillboardRenderListD3D[v7].pQuads[0].pos.x =
        pSoftBillboard->screen_space_x - v31 * a1;
    // v14 = (double)v24;
    // v32 = v14;
    pBillboardRenderListD3D[v7].pQuads[0].pos.y =
        pSoftBillboard->screen_space_y - v27 * v29;
    v15 = 1.0 - 1.0 / (pSoftBillboard->screen_space_z * 0.061758894);
    pBillboardRenderListD3D[v7].pQuads[0].pos.z = v15;
    v16 = 1.0 / pSoftBillboard->screen_space_z;
    pBillboardRenderListD3D[v7].pQuads[0].rhw =
        1.0 / pSoftBillboard->screen_space_z;
    pBillboardRenderListD3D[v7].pQuads[0].texcoord.x = 0.0;
    pBillboardRenderListD3D[v7].pQuads[0].texcoord.y = 0.0;
    v17 = (double)((pSprite->uBufferWidth >> 1) - pSprite->uAreaX);
    v18 = (double)(pSprite->uBufferHeight - pSprite->uAreaY -
        pSprite->uAreaHeight);
    if (pSoftBillboard->uFlags & 4) {
        v17 = v17 * -1.0;
    }
    pBillboardRenderListD3D[v7].pQuads[1].specular = 0;
    pBillboardRenderListD3D[v7].pQuads[1].diffuse = v12;
    pBillboardRenderListD3D[v7].pQuads[1].pos.x =
        pSoftBillboard->screen_space_x - v17 * a1;
    pBillboardRenderListD3D[v7].pQuads[1].pos.y =
        pSoftBillboard->screen_space_y - v18 * v29;
    pBillboardRenderListD3D[v7].pQuads[1].pos.z = v15;
    pBillboardRenderListD3D[v7].pQuads[1].rhw = v16;
    pBillboardRenderListD3D[v7].pQuads[1].texcoord.x = 0.0;
    pBillboardRenderListD3D[v7].pQuads[1].texcoord.y = 1.0;
    v19 = pSprite->uBufferHeight - pSprite->uAreaY - pSprite->uAreaHeight;
    v20 = (double)(pSprite->uAreaX + pSprite->uAreaWidth +
        (pSprite->uBufferWidth >> 1) - pSprite->uBufferWidth);
    if (pSoftBillboard->uFlags & 4) {
        v20 = v20 * -1.0;
    }
    pBillboardRenderListD3D[v7].pQuads[2].specular = 0;
    pBillboardRenderListD3D[v7].pQuads[2].diffuse = v12;
    pBillboardRenderListD3D[v7].pQuads[2].pos.x =
        v20 * a1 + pSoftBillboard->screen_space_x;
    pBillboardRenderListD3D[v7].pQuads[2].pos.y =
        pSoftBillboard->screen_space_y - (double)v19 * v29;
    pBillboardRenderListD3D[v7].pQuads[2].pos.z = v15;
    pBillboardRenderListD3D[v7].pQuads[2].rhw = v16;
    pBillboardRenderListD3D[v7].pQuads[2].texcoord.x = 1.0;
    pBillboardRenderListD3D[v7].pQuads[2].texcoord.y = 1.0;
    v21 = pSprite->uBufferHeight - pSprite->uAreaY;
    v22 = (double)(pSprite->uAreaX + pSprite->uAreaWidth +
        (pSprite->uBufferWidth >> 1) - pSprite->uBufferWidth);
    if (pSoftBillboard->uFlags & 4) {
        v22 = v22 * -1.0;
    }
    pBillboardRenderListD3D[v7].pQuads[3].specular = 0;
    pBillboardRenderListD3D[v7].pQuads[3].diffuse = v12;
    pBillboardRenderListD3D[v7].pQuads[3].pos.x =
        v22 * a1 + pSoftBillboard->screen_space_x;
    pBillboardRenderListD3D[v7].pQuads[3].pos.y =
        pSoftBillboard->screen_space_y - (double)v21 * v29;
    pBillboardRenderListD3D[v7].pQuads[3].pos.z = v15;
    pBillboardRenderListD3D[v7].pQuads[3].rhw = v16;
    pBillboardRenderListD3D[v7].pQuads[3].texcoord.x = 1.0;
    pBillboardRenderListD3D[v7].pQuads[3].texcoord.y = 0.0;
    // v23 = pSprite->pTexture;
    pBillboardRenderListD3D[v7].uNumVertices = 4;
    pBillboardRenderListD3D[v7].z_order = pSoftBillboard->screen_space_z;
    pBillboardRenderListD3D[v7].texture = pSprite->texture;
}

void RenderOpenGL::_4A4CC9_AddSomeBillboard(struct SpellFX_Billboard *a1, int diffuse) {
    // fireball
    //__debugbreak();

    // could draw in rather than convert to billboard for ogl 

    if (a1->uNumVertices < 3) {
        return;
    }

    float depth = 1000000.0;
    for (uint i = 0; i < (unsigned int)a1->uNumVertices; ++i) {
        if (a1->field_104[i].z < depth) {
            depth = a1->field_104[i].z;
        }
    }

    unsigned int v5 = Billboard_ProbablyAddToListAndSortByZOrder(depth);
    pBillboardRenderListD3D[v5].field_90 = 0;
    pBillboardRenderListD3D[v5].sParentBillboardID = -1;
    pBillboardRenderListD3D[v5].opacity = RenderBillboardD3D::Opaque_2;
    pBillboardRenderListD3D[v5].texture = 0;
    pBillboardRenderListD3D[v5].uNumVertices = a1->uNumVertices;
    pBillboardRenderListD3D[v5].z_order = depth;

    for (unsigned int i = 0; i < (unsigned int)a1->uNumVertices; ++i) {
        pBillboardRenderListD3D[v5].pQuads[i].pos.x = a1->field_104[i].x;
        pBillboardRenderListD3D[v5].pQuads[i].pos.y = a1->field_104[i].y;
        pBillboardRenderListD3D[v5].pQuads[i].pos.z = a1->field_104[i].z;


        // if (a1->field_104[i].z < 17) a1->field_104[i].z = 17;
        float rhw = 1.f / a1->field_104[i].z;
        float z = 1.f - 1.f / (a1->field_104[i].z * 1000.f / pIndoorCameraD3D->GetFarClip());



        double v10 = a1->field_104[i].z;
        if (uCurrentlyLoadedLevelType == LEVEL_Indoor) {
            v10 *= 1000.f / 16192.f;
        }
        else {
            v10 *= 1000.f / pIndoorCameraD3D->GetFarClip();
        }


        //pBillboardRenderListD3D[v5].pQuads[i].pos.z = z;  // 1.0 - 1.0 / v10;
        pBillboardRenderListD3D[v5].pQuads[i].rhw = rhw;  // 1.0 / a1->field_104[i].z;

        int v12;
        if (diffuse & 0xFF000000) {
            v12 = a1->field_104[i].diffuse;
        }
        else {
            v12 = diffuse;
        }
        pBillboardRenderListD3D[v5].pQuads[i].diffuse = v12;
        pBillboardRenderListD3D[v5].pQuads[i].specular = 0;

        pBillboardRenderListD3D[v5].pQuads[i].texcoord.x = 0.0;
        pBillboardRenderListD3D[v5].pQuads[i].texcoord.y = 0.0;
    }


}

void RenderOpenGL::DrawBillboardList_BLV() {
    SoftwareBillboard soft_billboard = { 0 };
    soft_billboard.sParentBillboardID = -1;
    //  soft_billboard.pTarget = pBLVRenderParams->pRenderTarget;
    soft_billboard.pTargetZ = pBLVRenderParams->pTargetZBuffer;
    //  soft_billboard.uTargetPitch = uTargetSurfacePitch;
    soft_billboard.uViewportX = pBLVRenderParams->uViewportX;
    soft_billboard.uViewportY = pBLVRenderParams->uViewportY;
    soft_billboard.uViewportZ = pBLVRenderParams->uViewportZ - 1;
    soft_billboard.uViewportW = pBLVRenderParams->uViewportW;

    pODMRenderParams->uNumBillboards = ::uNumBillboardsToDraw;
    for (uint i = 0; i < ::uNumBillboardsToDraw; ++i) {
        RenderBillboard *p = &pBillboardRenderList[i];
        if (p->hwsprite) {
            soft_billboard.screen_space_x = p->screen_space_x;
            soft_billboard.screen_space_y = p->screen_space_y;
            soft_billboard.screen_space_z = p->screen_space_z;
            soft_billboard.sParentBillboardID = i;
            soft_billboard.screenspace_projection_factor_x =
                p->screenspace_projection_factor_x;
            soft_billboard.screenspace_projection_factor_y =
                p->screenspace_projection_factor_y;
            soft_billboard.object_pid = p->object_pid;
            soft_billboard.uFlags = p->field_1E;
            soft_billboard.sTintColor = p->sTintColor;

            DrawBillboard_Indoor(&soft_billboard, p);
        }
    }
}


void RenderOpenGL::DrawProjectile(float srcX, float srcY, float a3, float a4,
                                  float dstX, float dstY, float a7, float a8,
                                  Texture *texture) {
    // a3 - src worldviewx
    // a4 - src fov / worldview
    // a7 - dst worldview
    // a8 - dst fov / worldview
    // 
    // 
    //__debugbreak();


    // billboards projectile

    double v20;  // st4@8
    double v21;  // st4@10
    double v22;  // st4@10
    double v23;  // st4@10
    double v25;  // st4@11
    double v26;  // st4@13
    double v28;  // st4@13

    
    TextureOpenGL* textured3d = (TextureOpenGL*)texture;

    int xDifference = bankersRounding(dstX - srcX);
    int yDifference = bankersRounding(dstY - srcY);
    int absYDifference = abs(yDifference);
    int absXDifference = abs(xDifference);
    unsigned int smallerabsdiff = std::min(absXDifference, absYDifference);
    unsigned int largerabsdiff = std::max(absXDifference, absYDifference);

    // distance approx
    int v32 = (11 * smallerabsdiff >> 5) + largerabsdiff;

    double v16 = 1.0 / (double)v32;
    double srcxmod = (double)yDifference * v16 * a4;
    double srcymod = (double)xDifference * v16 * a4;

    if (uCurrentlyLoadedLevelType == LEVEL_Outdoor) {
        v20 = a3 * 1000.0 / pIndoorCameraD3D->GetFarClip();
        v25 = a7 * 1000.0 / pIndoorCameraD3D->GetFarClip();
    }
    else {
        v20 = a3 * 0.061758894;
        v25 = a7 * 0.061758894;
    }

    v21 = 1.0 / a3;
    v22 = (double)yDifference * v16 * a8;
    v23 = (double)xDifference * v16 * a8;
    v26 = 1.0 - 1.0 / v25;
    v28 = 1.0 / a7;

    RenderVertexD3D3 v29[4];
    v29[0].pos.x = srcX + srcxmod;
    v29[0].pos.y = srcY - srcymod;
    v29[0].pos.z = 1.0 - 1.0 / v20;
    v29[0].rhw = v21;
    v29[0].diffuse = -1;
    v29[0].specular = 0;
    v29[0].texcoord.x = 1.0;
    v29[0].texcoord.y = 0.0;

    v29[1].pos.x = v22 + dstX;
    v29[1].pos.y = dstY - v23;
    v29[1].pos.z = v26;
    v29[1].rhw = v28;
    v29[1].diffuse = -16711936;
    v29[1].specular = 0;
    v29[1].texcoord.x = 1.0;
    v29[1].texcoord.y = 1.0;

    v29[2].pos.x = dstX - v22;
    v29[2].pos.y = v23 + dstY;
    v29[2].pos.z = v26;
    v29[2].rhw = v28;
    v29[2].diffuse = -1;
    v29[2].specular = 0;
    v29[2].texcoord.x = 0.0;
    v29[2].texcoord.y = 1.0;

    v29[3].pos.x = srcX - srcxmod;
    v29[3].pos.y = srcymod + srcY;
    v29[3].pos.z = v29[0].pos.z;
    v29[3].rhw = v21;
    v29[3].diffuse = -1;
    v29[3].specular = 0;
    v29[3].texcoord.x = 0.0;
    v29[3].texcoord.y = 0.0;


    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    
    // ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DITHERENABLE, FALSE));
    glDepthMask(FALSE);
    glDisable(GL_CULL_FACE);

    if (textured3d) {
        glBindTexture(GL_TEXTURE_2D, textured3d->GetOpenGlTexture());
    } else {
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    
    glBegin(GL_TRIANGLE_FAN);


        for (uint i = 0; i < 4; ++i) {
            


            glColor4f(1,1,1, 1.0f);  // ????
            glTexCoord2f(v29[i].texcoord.x, v29[i].texcoord.y);
            glVertex3f(v29[i].pos.x, v29[i].pos.y, v29[i].pos.z);


        }
        

    
    glEnd();


    drawcalls++;

    glDisable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ZERO);


    //ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DITHERENABLE, TRUE));
    glDepthMask(TRUE);
    glEnable(GL_CULL_FACE);
    
    checkglerror();
}

void RenderOpenGL::ScreenFade(unsigned int color, float t) { __debugbreak(); }


void RenderOpenGL::DrawTextureOffset(int pX, int pY, int move_X, int move_Y, Image *pTexture) {
    DrawTextureNew((pX - move_X), (pY - move_Y), pTexture);
}


struct twodverts {
    GLfloat x;
    GLfloat y;
    GLfloat z;
    GLfloat u;
    GLfloat v;
    GLfloat r;
    GLfloat g;
    GLfloat b;
    GLfloat a;
    GLfloat texid;
};

twodverts twodshaderstore[500] = {};
int twodvertscnt = 0;

void RenderOpenGL::DrawImage(Image *img, const Rect &rect) {
    if (!img) __debugbreak();

    int width = img->GetWidth();
    int height = img->GetHeight();

    int x = rect.x;
    int y = rect.y;
    int z = rect.z;
    int w = rect.w;

    // check bounds
    if (x >= (int)window->GetWidth() || x >= this->clip_z || y >= (int)window->GetHeight() || y >= this->clip_w) return;
    // check for overlap
    if (!(this->clip_x < z && this->clip_z > x && this->clip_y < w && this->clip_w > y)) return;

    auto texture = (TextureOpenGL*)img;
    float gltexid = texture->GetOpenGlTexture();

    int drawx = std::max(x, this->clip_x);
    int drawy = std::max(y, this->clip_y);
    int draww = std::min(w, this->clip_w);
    int drawz = std::min(z, this->clip_z);

    float texx = (drawx - x) / float(width);
    float texy = (drawy - y) / float(height);
    float texz = (width - (z - drawz)) / float(width);
    float texw = (height - (w - draww)) / float(height);

    // 0 1 2 / 0 2 3

    twodshaderstore[twodvertscnt].x = drawx;
    twodshaderstore[twodvertscnt].y = drawy;
    twodshaderstore[twodvertscnt].z = 0;
    twodshaderstore[twodvertscnt].u = texx;
    twodshaderstore[twodvertscnt].v = texy;
    twodshaderstore[twodvertscnt].r = 1;
    twodshaderstore[twodvertscnt].g = 1;
    twodshaderstore[twodvertscnt].b = 1;
    twodshaderstore[twodvertscnt].a = 1;
    twodshaderstore[twodvertscnt].texid = gltexid;
    twodvertscnt++;

    twodshaderstore[twodvertscnt].x = drawz;
    twodshaderstore[twodvertscnt].y = drawy;
    twodshaderstore[twodvertscnt].z = 0;
    twodshaderstore[twodvertscnt].u = texz;
    twodshaderstore[twodvertscnt].v = texy;
    twodshaderstore[twodvertscnt].r = 1;
    twodshaderstore[twodvertscnt].g = 1;
    twodshaderstore[twodvertscnt].b = 1;
    twodshaderstore[twodvertscnt].a = 1;
    twodshaderstore[twodvertscnt].texid = gltexid;
    twodvertscnt++;

    twodshaderstore[twodvertscnt].x = drawz;
    twodshaderstore[twodvertscnt].y = draww;
    twodshaderstore[twodvertscnt].z = 0;
    twodshaderstore[twodvertscnt].u = texz;
    twodshaderstore[twodvertscnt].v = texw;
    twodshaderstore[twodvertscnt].r = 1;
    twodshaderstore[twodvertscnt].g = 1;
    twodshaderstore[twodvertscnt].b = 1;
    twodshaderstore[twodvertscnt].a = 1;
    twodshaderstore[twodvertscnt].texid = gltexid;
    twodvertscnt++;

    ////////////////////////////////

    twodshaderstore[twodvertscnt].x = drawx;
    twodshaderstore[twodvertscnt].y = drawy;
    twodshaderstore[twodvertscnt].z = 0;
    twodshaderstore[twodvertscnt].u = texx;
    twodshaderstore[twodvertscnt].v = texy;
    twodshaderstore[twodvertscnt].r = 1;
    twodshaderstore[twodvertscnt].g = 1;
    twodshaderstore[twodvertscnt].b = 1;
    twodshaderstore[twodvertscnt].a = 1;
    twodshaderstore[twodvertscnt].texid = gltexid;
    twodvertscnt++;

    twodshaderstore[twodvertscnt].x = drawz;
    twodshaderstore[twodvertscnt].y = draww;
    twodshaderstore[twodvertscnt].z = 0;
    twodshaderstore[twodvertscnt].u = texz;
    twodshaderstore[twodvertscnt].v = texw;
    twodshaderstore[twodvertscnt].r = 1;
    twodshaderstore[twodvertscnt].g = 1;
    twodshaderstore[twodvertscnt].b = 1;
    twodshaderstore[twodvertscnt].a = 1;
    twodshaderstore[twodvertscnt].texid = gltexid;
    twodvertscnt++;

    twodshaderstore[twodvertscnt].x = drawx;
    twodshaderstore[twodvertscnt].y = draww;
    twodshaderstore[twodvertscnt].z = 0;
    twodshaderstore[twodvertscnt].u = texx;
    twodshaderstore[twodvertscnt].v = texw;
    twodshaderstore[twodvertscnt].r = 1;
    twodshaderstore[twodvertscnt].g = 1;
    twodshaderstore[twodvertscnt].b = 1;
    twodshaderstore[twodvertscnt].a = 1;
    twodshaderstore[twodvertscnt].texid = gltexid;
    twodvertscnt++;


    checkglerror();

    // blank over same bit of this render_target_rgb to stop text overlaps
    for (int ys = drawy; ys < draww; ys++) {
        memset(this->render_target_rgb + (ys * window->GetWidth() + drawx), 0x00000000, (drawz - drawx) * 4);
    }

    if (twodvertscnt > 490) drawtwodverts();

    return;



    //glEnable(GL_TEXTURE_2D);
    //glColor3f(1, 1, 1);
    //bool blendFlag = 1;

    //// check if loaded
    //auto texture = (TextureOpenGL *)img;
    //glBindTexture(GL_TEXTURE_2D, texture->GetOpenGlTexture());

    //if (blendFlag) {
    //    glEnable(GL_BLEND);
    //    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //}

    //float depth = 0;

    //GLfloat Vertices[] = { (float)rect.x, (float)rect.y, depth,
    //    (float)rect.z, (float)rect.y, depth,
    //    (float)rect.z, (float)rect.w, depth,
    //    (float)rect.x, (float)rect.w, depth };

    //GLfloat TexCoord[] = { 0, 0,
    //    1, 0,
    //    1, 1,
    //    0, 1 };

    //GLubyte indices[] = { 0, 1, 2,  // first triangle (bottom left - top left - top right)
    //    0, 2, 3 };  // second triangle (bottom left - top right - bottom right)

    //glEnableClientState(GL_VERTEX_ARRAY);
    //glVertexPointer(3, GL_FLOAT, 0, Vertices);

    //glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    //glTexCoordPointer(2, GL_FLOAT, 0, TexCoord);

    //glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, indices);
    //drawcalls++;

    //glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    //glDisableClientState(GL_VERTEX_ARRAY);

    //glDisable(GL_BLEND);

    //checkglerror();
}





void RenderOpenGL::ZDrawTextureAlpha(float u, float v, Image *img, int zVal) {
    if (!img) return;

    int winwidth = this->window->GetWidth();
    int uOutX = u * winwidth;
    int uOutY = v * this->window->GetHeight();
    unsigned int imgheight = img->GetHeight();
    unsigned int imgwidth = img->GetWidth();
    auto pixels = (uint32_t *)img->GetPixels(IMAGE_FORMAT_A8R8G8B8);

    if (uOutX < 0)
        uOutX = 0;
    if (uOutY < 0)
        uOutY = 0;

    for (int xs = 0; xs < imgwidth; xs++) {
        for (int ys = 0; ys < imgheight; ys++) {
            if (pixels[xs + imgwidth * ys] & 0xFF000000) {
                this->pActiveZBuffer[uOutX + xs + winwidth * (uOutY + ys)] = zVal;
            }
        }
    }
}




void RenderOpenGL::BlendTextures(int x, int y, Image* imgin, Image* imgblend, int time, int start_opacity,
    int end_opacity) {
    // thrown together as a crude estimate of the enchaintg
                          // effects

      // leaves gap where it shouldnt on dark pixels currently
      // doesnt use opacity params

    const uint32_t* pixelpoint;
    const uint32_t* pixelpointblend;

    if (imgin && imgblend) {  // 2 images to blend
        pixelpoint = (const uint32_t*)imgin->GetPixels(IMAGE_FORMAT_A8R8G8B8);
        pixelpointblend =
            (const uint32_t*)imgblend->GetPixels(IMAGE_FORMAT_A8R8G8B8);

        int Width = imgin->GetWidth();
        int Height = imgin->GetHeight();
        // Image* temp = Image::Create(Width, Height, IMAGE_FORMAT_A8R8G8B8);
        // uint32_t* temppix = (uint32_t*)temp->GetPixels(IMAGE_FORMAT_A8R8G8B8);

        uint32_t c = *(pixelpointblend + 2700);  // guess at brightest pixel
        unsigned int bmax = (c & 0xFF);
        unsigned int gmax = ((c >> 8) & 0xFF);
        unsigned int rmax = ((c >> 16) & 0xFF);

        unsigned int bmin = bmax / 10;
        unsigned int gmin = gmax / 10;
        unsigned int rmin = rmax / 10;

        unsigned int bstep = (bmax - bmin) / 128;
        unsigned int gstep = (gmax - gmin) / 128;
        unsigned int rstep = (rmax - rmin) / 128;

        for (int ydraw = 0; ydraw < Height; ++ydraw) {
            for (int xdraw = 0; xdraw < Width; ++xdraw) {
                // should go blue -> black -> blue reverse
                // patchy -> solid -> patchy

                if (*pixelpoint) {  // check orig item not got blakc pixel
                    uint32_t nudge =
                        (xdraw % imgblend->GetWidth()) +
                        (ydraw % imgblend->GetHeight()) * imgblend->GetWidth();
                    uint32_t pixcol = *(pixelpointblend + nudge);

                    unsigned int bcur = (pixcol & 0xFF);
                    unsigned int gcur = ((pixcol >> 8) & 0xFF);
                    unsigned int rcur = ((pixcol >> 16) & 0xFF);

                    int steps = (time) % 128;

                    if ((time) % 256 >= 128) {  // step down
                        bcur += bstep * (128 - steps);
                        gcur += gstep * (128 - steps);
                        rcur += rstep * (128 - steps);
                    } else {  // step up
                        bcur += bstep * steps;
                        gcur += gstep * steps;
                        rcur += rstep * steps;
                    }

                    if (bcur > bmax) bcur = bmax;  // limit check
                    if (gcur > gmax) gcur = gmax;
                    if (rcur > rmax) rcur = rmax;
                    if (bcur < bmin) bcur = bmin;
                    if (gcur < gmin) gcur = gmin;
                    if (rcur < rmin) rcur = rmin;

                    // temppix[xdraw + ydraw * Width] = Color32(rcur, gcur, bcur);
                    render_target_rgb[x + xdraw + (render->GetRenderWidth() * (y + ydraw))] = Color32(rcur, gcur, bcur);
                }

                pixelpoint++;
            }

            pixelpoint += imgin->GetWidth() - Width;
        }
        // draw image
        // render->DrawTextureAlphaNew(x / float(window->GetWidth()), y / float(window->GetHeight()), temp);
        // temp->Release();
    }
}


void RenderOpenGL::TexturePixelRotateDraw(float u, float v, Image *img, int time) {
    if (img) {
        auto pixelpoint = (const uint32_t *)img->GetPixels(IMAGE_FORMAT_A8R8G8B8);
        int width = img->GetWidth();
        int height = img->GetHeight();

        u *= render->GetRenderWidth();
        v *= render->GetRenderHeight();

        int brightloc = -1;
        int brightval = 0;
        int darkloc = -1;
        int darkval = 765;

        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                int nudge = x + y * width;
                // Test the brightness against the threshold
                int bright = (*(pixelpoint + nudge) & 0xFF) + ((*(pixelpoint + nudge) >> 8) & 0xFF) + ((*(pixelpoint + nudge) >> 16) & 0xFF);
                if (bright == 0) continue;

                if (bright > brightval) {
                    brightval = bright;
                    brightloc = nudge;
                }
                if (bright < darkval) {
                    darkval = bright;
                    darkloc = nudge;
                }
            }
        }

        // find brightest
        unsigned int bmax = (*(pixelpoint + brightloc) & 0xFF);
        unsigned int gmax = ((*(pixelpoint + brightloc) >> 8) & 0xFF);
        unsigned int rmax = ((*(pixelpoint + brightloc) >> 16) & 0xFF);

        // find darkest not black
        unsigned int bmin = (*(pixelpoint + darkloc) & 0xFF);
        unsigned int gmin = ((*(pixelpoint + darkloc) >> 8) & 0xFF);
        unsigned int rmin = ((*(pixelpoint + darkloc) >> 16) & 0xFF);

        // steps pixels
        float bstep = (bmax - bmin) / 128.;
        float gstep = (gmax - gmin) / 128.;
        float rstep = (rmax - rmin) / 128.;

        int timestep = time % 256;

        // loop through
        for (int ydraw = 0; ydraw < height; ++ydraw) {
            for (int xdraw = 0; xdraw < width; ++xdraw) {
                if (*pixelpoint) {  // check orig item not got blakc pixel
                    unsigned int bcur = (*(pixelpoint) & 0xFF);
                    unsigned int gcur = ((*(pixelpoint) >> 8) & 0xFF);
                    unsigned int rcur = ((*(pixelpoint) >> 16) & 0xFF);
                    int pixstepb = (bcur - bmin) / bstep + timestep;
                    if (pixstepb > 255) pixstepb = pixstepb - 256;
                    if (pixstepb >= 0 && pixstepb < 128)  // 0-127
                        bcur = bmin + pixstepb * bstep;
                    if (pixstepb >= 128 && pixstepb < 256) {  // 128-255
                        pixstepb = pixstepb - 128;
                        bcur = bmax - pixstepb * bstep;
                    }
                    int pixstepr = (rcur - rmin) / rstep + timestep;
                    if (pixstepr > 255) pixstepr = pixstepr - 256;
                    if (pixstepr >= 0 && pixstepr < 128)  // 0-127
                        rcur = rmin + pixstepr * rstep;
                    if (pixstepr >= 128 && pixstepr < 256) {  // 128-255
                        pixstepr = pixstepr - 128;
                        rcur = rmax - pixstepr * rstep;
                    }
                    int pixstepg = (gcur - gmin) / gstep + timestep;
                    if (pixstepg > 255) pixstepg = pixstepg - 256;
                    if (pixstepg >= 0 && pixstepg < 128)  // 0-127
                        gcur = gmin + pixstepg * gstep;
                    if (pixstepg >= 128 && pixstepg < 256) {  // 128-255
                        pixstepg = pixstepg - 128;
                        gcur = gmax - pixstepg * gstep;
                    }
                    // out pixel
                    // temppix[xdraw + ydraw * width] = (rcur << 24) | (gcur << 16) | (bcur << 8) | 0xFF;//Color32(rcur, gcur, bcur);
                    render_target_rgb[int((u)+xdraw + window->GetWidth() *(v+ydraw))] = Color32(rcur, gcur, bcur);
                }
                pixelpoint++;
            }
        }
        // draw image
        // render->Update_Texture(img);
        // render->DrawTextureAlphaNew(u, v, img);
        // temp->Release();
    }
}



void RenderOpenGL::DrawMonsterPortrait(Rect rc, SpriteFrame *Portrait, int Y_Offset) {
    Rect rct;
    rct.x = rc.x + 64 + Portrait->hw_sprites[0]->uAreaX - Portrait->hw_sprites[0]->uBufferWidth / 2;
    rct.y = rc.y + Y_Offset + Portrait->hw_sprites[0]->uAreaY;
    rct.z = rct.x + Portrait->hw_sprites[0]->uAreaWidth;
    rct.w = rct.y + Portrait->hw_sprites[0]->uAreaHeight;

    render->SetUIClipRect(rc.x, rc.y, rc.z, rc.w);
    render->DrawImage(Portrait->hw_sprites[0]->texture, rct);
    render->ResetUIClipRect();
}

void RenderOpenGL::DrawTransparentRedShade(float u, float v, Image *a4) {
    DrawMasked(u, v, a4, 0, 0xF800);
}

void RenderOpenGL::DrawTransparentGreenShade(float u, float v, Image *pTexture) {
    DrawMasked(u, v, pTexture, 0, 0x07E0);
}

void RenderOpenGL::DrawFansTransparent(const RenderVertexD3D3 *vertices,
                                       unsigned int num_vertices) {
    __debugbreak();
}

inline uint32_t PixelDim(uint32_t pix, int dimming) {
    return Color32((((pix >> 16) & 0xFF) >> dimming),
        (((pix >> 8) & 0xFF) >> dimming),
        ((pix & 0xFF) >> dimming));
}

void RenderOpenGL::DrawMasked(float u, float v, Image *pTexture, unsigned int color_dimming_level,
                              unsigned __int16 mask) {
    if (!pTexture) {
        return;
    }
    uint32_t width = pTexture->GetWidth();
    uint32_t *pixels = (uint32_t *)pTexture->GetPixels(IMAGE_FORMAT_A8R8G8B8);
    // Image *temp = Image::Create(width, pTexture->GetHeight(), IMAGE_FORMAT_A8R8G8B8);
    // uint32_t *temppix = (uint32_t *)temp->GetPixels(IMAGE_FORMAT_A8R8G8B8);
    int x = window->GetWidth() * u;
    int y = window->GetHeight() * v;

    for (unsigned int dy = 0; dy < pTexture->GetHeight(); ++dy) {
        for (unsigned int dx = 0; dx < width; ++dx) {
            if (*pixels & 0xFF000000) {
                if ((x + dx) < window->GetWidth() && (y + dy) < window->GetHeight()) {
                    render_target_rgb[x + dx + window->GetWidth() * (y + dy)] = PixelDim(*pixels, color_dimming_level) & Color32(mask);
                }
            }
            ++pixels;
        }
    }
    // render->DrawTextureAlphaNew(u, v, temp);
    // temp->Release();;
}



void RenderOpenGL::DrawTextureGrayShade(float a2, float a3, Image *a4) {
    DrawMasked(a2, a3, a4, 1, 0x7BEF);
}

void RenderOpenGL::DrawIndoorSky(unsigned int uNumVertices,
                                 unsigned int uFaceID) {
    __debugbreak();
}

void RenderOpenGL::DrawIndoorSkyPolygon(signed int uNumVertices,
                                        struct Polygon *pSkyPolygon) {
    __debugbreak();
}

bool RenderOpenGL::AreRenderSurfacesOk() { return true; }

unsigned short *RenderOpenGL::MakeScreenshot(int width, int height) {
    GLubyte* sPixels = new GLubyte[3 * window->GetWidth() * window->GetHeight()];

    if (uCurrentlyLoadedLevelType == LEVEL_Indoor) {
        pIndoor->Draw();
    } else if (uCurrentlyLoadedLevelType == LEVEL_Outdoor) {
        pOutdoor->Draw();
    }

    if (uCurrentlyLoadedLevelType != LEVEL_null)
        DrawBillboards_And_MaybeRenderSpecialEffects_And_EndScene();

    glReadPixels(0, 0, window->GetWidth(), window->GetHeight(), GL_RGB, GL_UNSIGNED_BYTE, sPixels);

    uint16_t *for_pixels;  // ebx@1

    float interval_x = game_viewport_width / (double)width;
    float interval_y = game_viewport_height / (double)height;

    uint16_t *pPixels = (uint16_t *)malloc(sizeof(uint16_t) * height * width);
    memset(pPixels, 0, sizeof(uint16_t) * height * width);

    for_pixels = pPixels;

    if (uCurrentlyLoadedLevelType == LEVEL_null) {
        memset(&for_pixels, 0, sizeof(for_pixels));
    } else {
        for (uint y = 0; y < (unsigned int)height; ++y) {
            for (uint x = 0; x < (unsigned int)width; ++x) {
                unsigned __int8 *p;

                p = sPixels + 3 * (int)(x * interval_x + 8.0) + 3 * (int)(window->GetHeight() - (y * interval_y) - 8.0) * window->GetWidth();

                *for_pixels = Color16(*p & 255, *(p + 1) & 255, *(p + 2) & 255);
                ++for_pixels;
            }
        }
    }

    checkglerror();
    return pPixels;
}

Image *RenderOpenGL::TakeScreenshot(unsigned int width, unsigned int height) {
    auto pixels = MakeScreenshot(width, height);
    Image *image = Image::Create(width, height, IMAGE_FORMAT_R5G6B5, pixels);
    free(pixels);
    return image;
}

void RenderOpenGL::SaveScreenshot(const String &filename, unsigned int width, unsigned int height) {
    auto pixels = MakeScreenshot(width, height);

    FILE *result = fopen(filename.c_str(), "wb");
    if (result == nullptr) {
        return;
    }

    unsigned int pcx_data_size = width * height * 5;
    uint8_t *pcx_data = new uint8_t[pcx_data_size];
    unsigned int pcx_data_real_size = 0;
    PCX::Encode16(pixels, width, height, pcx_data, pcx_data_size, &pcx_data_real_size);
    fwrite(pcx_data, pcx_data_real_size, 1, result);
    delete[] pcx_data;
    fclose(result);
}

void RenderOpenGL::PackScreenshot(unsigned int width, unsigned int height,
                                  void *out_data, unsigned int data_size,
                                  unsigned int *screenshot_size) {
    auto pixels = MakeScreenshot(width, height);
    SaveScreenshot("save.pcx", width, height);
    PCX::Encode16(pixels, 150, 112, out_data, 1000000, screenshot_size);
    free(pixels);
}

void RenderOpenGL::SavePCXScreenshot() { __debugbreak(); }
int RenderOpenGL::GetActorsInViewport(int pDepth) {
    __debugbreak();
    return 0;
}

void RenderOpenGL::BeginLightmaps() {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    auto effpar03 = assets->GetBitmap("effpar03");
    auto texture = (TextureOpenGL*)effpar03;
    glBindTexture(GL_TEXTURE_2D, texture->GetOpenGlTexture());
}

void RenderOpenGL::EndLightmaps() {
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, GL_lastboundtex);
}


void RenderOpenGL::BeginLightmaps2() {
    glDisable(GL_CULL_FACE);
    glDepthMask(false);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    auto effpar03 = assets->GetBitmap("effpar03");
    auto texture = (TextureOpenGL*)effpar03;
    glBindTexture(GL_TEXTURE_2D, texture->GetOpenGlTexture());
}


void RenderOpenGL::EndLightmaps2() {
    glDisable(GL_BLEND);
    glDepthMask(true);
    glEnable(GL_CULL_FACE);
    glBindTexture(GL_TEXTURE_2D, GL_lastboundtex);
}

bool RenderOpenGL::DrawLightmap(struct Lightmap *pLightmap, struct Vec3_float_ *pColorMult, float z_bias) {
    // For outdoor terrain and indoor light (VII)(VII)
    if (pLightmap->NumVertices < 3) {
        log->Warning("Lightmap uNumVertices < 3");
        return false;
    }

    unsigned int uLightmapColorMaskR = (pLightmap->uColorMask >> 16) & 0xFF;
    unsigned int uLightmapColorMaskG = (pLightmap->uColorMask >> 8) & 0xFF;
    unsigned int uLightmapColorMaskB = pLightmap->uColorMask & 0xFF;

    unsigned int uLightmapColorR = floorf(
        uLightmapColorMaskR * pLightmap->fBrightness * pColorMult->x + 0.5f);
    unsigned int uLightmapColorG = floorf(
        uLightmapColorMaskG * pLightmap->fBrightness * pColorMult->y + 0.5f);
    unsigned int uLightmapColorB = floorf(
        uLightmapColorMaskB * pLightmap->fBrightness * pColorMult->z + 0.5f);

    RenderVertexD3D3 pVerticesD3D[64];

    glBegin(GL_TRIANGLE_FAN);

    for (uint i = 0; i < pLightmap->NumVertices; ++i) {
        glColor4f((uLightmapColorR) / 255.0f, (uLightmapColorG) / 255.0f, (uLightmapColorB) / 255.0f, 1.0f);
        glTexCoord2f(pLightmap->pVertices[i].u, pLightmap->pVertices[i].v);
        glVertex3f(pLightmap->pVertices[i].vWorldPosition.x, pLightmap->pVertices[i].vWorldPosition.y, pLightmap->pVertices[i].vWorldPosition.z);
    }

    glEnd();

    drawcalls++;
    return true;
}

void RenderOpenGL::BeginDecals() {
    auto texture = (TextureOpenGL*)assets->GetBitmap("hwsplat04");
    glBindTexture(GL_TEXTURE_2D, texture->GetOpenGlTexture());

    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
}

void RenderOpenGL::EndDecals() {
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void RenderOpenGL::DrawDecal(struct Decal *pDecal, float z_bias) {
    // need to add z biasing
    RenderVertexD3D3 pVerticesD3D[64];

    if (pDecal->uNumVertices < 3) {
        log->Warning("Decal has < 3 vertices");
        return;
    }

    float color_mult = pDecal->Fade_by_time();
    if (color_mult == 0.0) return;

    // temp - bloodsplat persistance
    // color_mult = 1;

    glBegin(GL_TRIANGLE_FAN);

    for (uint i = 0; i < (unsigned int)pDecal->uNumVertices; ++i) {
        uint uTint =
            GetActorTintColor(pDecal->DimmingLevel, 0, pDecal->pVertices[i].vWorldViewPosition.x, 0, nullptr);

        uint uTintR = (uTint >> 16) & 0xFF, uTintG = (uTint >> 8) & 0xFF,
            uTintB = uTint & 0xFF;

        uint uDecalColorMultR = (pDecal->uColorMultiplier >> 16) & 0xFF,
            uDecalColorMultG = (pDecal->uColorMultiplier >> 8) & 0xFF,
            uDecalColorMultB = pDecal->uColorMultiplier & 0xFF;

        uint uFinalR =
            floorf(uTintR / 255.0 * color_mult * uDecalColorMultR + 0.0f),
            uFinalG =
            floorf(uTintG / 255.0 * color_mult * uDecalColorMultG + 0.0f),
            uFinalB =
            floorf(uTintB / 255.0 * color_mult * uDecalColorMultB + 0.0f);

        glColor4f((uFinalR) / 255.0f, (uFinalG) / 255.0f, (uFinalB) / 255.0f, 1.0f);
        glTexCoord2f(pDecal->pVertices[i].u, pDecal->pVertices[i].v);
        glVertex3f(pDecal->pVertices[i].vWorldPosition.x, pDecal->pVertices[i].vWorldPosition.y, pDecal->pVertices[i].vWorldPosition.z);

        drawcalls++;
    }
    glEnd();
}

void RenderOpenGL::do_draw_debug_line_d3d(const RenderVertexD3D3 *pLineBegin,
                                          signed int sDiffuseBegin,
                                          const RenderVertexD3D3 *pLineEnd,
                                          signed int sDiffuseEnd,
                                          float z_stuff) {
    __debugbreak();
}
void RenderOpenGL::DrawLines(const RenderVertexD3D3 *vertices,
                             unsigned int num_vertices) {
    __debugbreak();
}
void RenderOpenGL::DrawSpecialEffectsQuad(const RenderVertexD3D3 *vertices,
                                          Texture *texture) {
    __debugbreak();
}

void RenderOpenGL::am_Blt_Chroma(Rect *pSrcRect, Point *pTargetPoint, int a3, int blend_mode) {
    // want to draw psrcrect section @ point
    // __debugbreak();


    // need to add blue masking


    glEnable(GL_TEXTURE_2D);
    float col = (blend_mode == 2) ? 1 : 0.5;

    glColor3f(col, col, col);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    auto texture = (TextureOpenGL*)pArcomageGame->pSprites;
    glBindTexture(GL_TEXTURE_2D, texture->GetOpenGlTexture());

    int clipx = this->clip_x;
    int clipy = this->clip_y;
    int clipw = this->clip_w;
    int clipz = this->clip_z;

    int texwidth = texture->GetWidth();
    int texheight = texture->GetHeight();

    int width = pSrcRect->z - pSrcRect->x;
    int height = pSrcRect->w - pSrcRect->y;

    int x = pTargetPoint->x;  // u* window->GetWidth();
    int y = pTargetPoint->y;  // v* window->GetHeight();
    int z = x + width;
    int w = y + height;

    // check bounds
    if (x >= (int)window->GetWidth() || x >= clipz || y >= (int)window->GetHeight() || y >= clipw) return;
    // check for overlap
    if (!(clipx < z && clipz > x && clipy < w && clipw > y)) return;

    int drawx = x;  // std::max(x, clipx);
    int drawy = y;  // std::max(y, clipy);
    int draww = w;  // std::min(w, clipw);
    int drawz = z;  // std::min(z, clipz);

    float depth = 0;

    GLfloat Vertices[] = { (float)drawx, (float)drawy, depth,
        (float)drawz, (float)drawy, depth,
        (float)drawz, (float)draww, depth,
        (float)drawx, (float)draww, depth };

    float texx = pSrcRect->x / float(texwidth);  // (drawx - x) / float(width);
    float texy = pSrcRect->y / float(texheight);  //  (drawy - y) / float(height);
    float texz = pSrcRect->z / float(texwidth);  //  (width - (z - drawz)) / float(width);
    float texw = pSrcRect->w / float(texheight);  // (height - (w - draww)) / float(height);

    GLfloat TexCoord[] = { texx, texy,
        texz, texy,
        texz, texw,
        texx, texw,
    };

    GLubyte indices[] = { 0, 1, 2,  // first triangle (bottom left - top left - top right)
        0, 2, 3 };  // second triangle (bottom left - top right - bottom right)

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, Vertices);

    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, 0, TexCoord);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, indices);
    drawcalls++;

    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);

    glDisable(GL_BLEND);


    // blank over same bit of this render_target_rgb to stop text overlaps
    for (int ys = drawy; ys < draww; ys++) {
        memset(this->render_target_rgb + (ys * window->GetWidth() + drawx), 0x00000000, (drawz - drawx) * 4);
    }

    return;
}

   
void RenderOpenGL::PrepareDecorationsRenderList_ODM() {
    unsigned int v6;        // edi@9
    int v7;                 // eax@9
    SpriteFrame *frame;     // eax@9
    unsigned __int16 *v10;  // eax@9
    int v13;                // ecx@9
    char r;                 // ecx@20
    char g;                 // dl@20
    char b_;                // eax@20
    Particle_sw local_0;    // [sp+Ch] [bp-98h]@7
    unsigned __int16 *v37;  // [sp+84h] [bp-20h]@9
    int v38;                // [sp+88h] [bp-1Ch]@9

    for (unsigned int i = 0; i < uNumLevelDecorations; ++i) {
        // LevelDecoration* decor = &pLevelDecorations[i];
        if ((!(pLevelDecorations[i].uFlags & LEVEL_DECORATION_OBELISK_CHEST) ||
            pLevelDecorations[i].IsObeliskChestActive()) &&
            !(pLevelDecorations[i].uFlags & LEVEL_DECORATION_INVISIBLE)) {
            DecorationDesc *decor_desc = pDecorationList->GetDecoration(pLevelDecorations[i].uDecorationDescID);
            if (!(decor_desc->uFlags & 0x80)) {
                if (!(decor_desc->uFlags & 0x22)) {
                    v6 = pMiscTimer->uTotalGameTimeElapsed;
                    v7 = abs(pLevelDecorations[i].vPosition.x +
                        pLevelDecorations[i].vPosition.y);

                    frame = pSpriteFrameTable->GetFrame(decor_desc->uSpriteID,
                        v6 + v7);

                    if (engine->config->seasons_change) {
                        frame = LevelDecorationChangeSeason(decor_desc, v6 + v7, pParty->uCurrentMonth);
                    }

                    if (!frame || frame->texture_name == "null" || frame->hw_sprites[0] == NULL) {
                        continue;
                    }

                    // v8 = pSpriteFrameTable->GetFrame(decor_desc->uSpriteID,
                    // v6 + v7);

                    v10 = (unsigned __int16 *)TrigLUT->Atan2(
                        pLevelDecorations[i].vPosition.x -
                        pIndoorCameraD3D->vPartyPos.x,
                        pLevelDecorations[i].vPosition.y -
                        pIndoorCameraD3D->vPartyPos.y);
                    v38 = 0;
                    v13 = ((signed int)(TrigLUT->uIntegerPi +
                        ((signed int)TrigLUT->uIntegerPi >>
                            3) +
                        pLevelDecorations[i].field_10_y_rot -
                        (int64_t)v10) >>
                        8) &
                        7;
                    v37 = (unsigned __int16 *)v13;
                    if (frame->uFlags & 2) v38 = 2;
                    if ((256 << v13) & frame->uFlags) v38 |= 4;
                    if (frame->uFlags & 0x40000) v38 |= 0x40;
                    if (frame->uFlags & 0x20000) v38 |= 0x80;

                    // for light
                    if (frame->uGlowRadius) {
                        r = 255;
                        g = 255;
                        b_ = 255;
                        if (render->config->is_using_colored_lights) {
                            r = decor_desc->uColoredLightRed;
                            g = decor_desc->uColoredLightGreen;
                            b_ = decor_desc->uColoredLightBlue;
                        }
                        pStationaryLightsStack->AddLight(
                            pLevelDecorations[i].vPosition.x,
                            pLevelDecorations[i].vPosition.y,
                            pLevelDecorations[i].vPosition.z +
                            decor_desc->uDecorationHeight / 2,
                            frame->uGlowRadius, r, g, b_, _4E94D0_light_type);
                    }  // for light

                       // v17 = (pLevelDecorations[i].vPosition.x -
                       // pIndoorCameraD3D->vPartyPos.x) << 16; v40 =
                       // (pLevelDecorations[i].vPosition.y -
                       // pIndoorCameraD3D->vPartyPos.y) << 16;
                    int party_to_decor_x = pLevelDecorations[i].vPosition.x -
                        pIndoorCameraD3D->vPartyPos.x;
                    int party_to_decor_y = pLevelDecorations[i].vPosition.y -
                        pIndoorCameraD3D->vPartyPos.y;
                    int party_to_decor_z = pLevelDecorations[i].vPosition.z -
                        pIndoorCameraD3D->vPartyPos.z;

                    int view_x = 0;
                    int view_y = 0;
                    int view_z = 0;
                    bool visible = pIndoorCameraD3D->ViewClip(
                        pLevelDecorations[i].vPosition.x,
                        pLevelDecorations[i].vPosition.y,
                        pLevelDecorations[i].vPosition.z, &view_x, &view_y,
                        &view_z);

                    if (visible) {
                        if (2 * abs(view_x) >= abs(view_y)) {
                            int projected_x = 0;
                            int projected_y = 0;
                            pIndoorCameraD3D->Project(view_x, view_y, view_z,
                                &projected_x,
                                &projected_y);

                            float _v41 =
                                frame->scale *
                                (pODMRenderParams->int_fov_rad) /
                                (view_x);

                            int screen_space_half_width = _v41 * frame->hw_sprites[(int64_t)v37]->uBufferWidth / 2;

                            if (projected_x + screen_space_half_width >=
                                (signed int)pViewport->uViewportTL_X &&
                                projected_x - screen_space_half_width <=
                                (signed int)pViewport->uViewportBR_X) {
                                if (::uNumBillboardsToDraw >= 500) return;
                                ::uNumBillboardsToDraw++;
                                ++uNumDecorationsDrawnThisFrame;

                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .hwsprite = frame->hw_sprites[(int64_t)v37];
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .world_x = pLevelDecorations[i].vPosition.x;
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .world_y = pLevelDecorations[i].vPosition.y;
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .world_z = pLevelDecorations[i].vPosition.z;
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .screen_space_x = projected_x;
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .screen_space_y = projected_y;
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .screen_space_z = view_x;
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .screenspace_projection_factor_x = _v41;
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .screenspace_projection_factor_y = _v41;
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .uPalette = frame->uPaletteIndex;
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .field_1E = v38 | 0x200;
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .uIndoorSectorID = 0;
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .object_pid = PID(OBJECT_Decoration, i);
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .dimming_level = 0;
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .pSpriteFrame = frame;
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .sTintColor = 0;
                            }
                        }
                    }
                }
            } else {
                memset(&local_0, 0, sizeof(Particle_sw));
                local_0.type = ParticleType_Bitmap | ParticleType_Rotating |
                    ParticleType_8;
                local_0.uDiffuse = 0xFF3C1E;
                local_0.x = (double)pLevelDecorations[i].vPosition.x;
                local_0.y = (double)pLevelDecorations[i].vPosition.y;
                local_0.z = (double)pLevelDecorations[i].vPosition.z;
                local_0.r = 0.0;
                local_0.g = 0.0;
                local_0.b = 0.0;
                local_0.particle_size = 1.0;
                local_0.timeToLive = (rand() & 0x80) + 128;
                local_0.texture = spell_fx_renderer->effpar01;
                particle_engine->AddParticle(&local_0);
            }
        }
    }
}

/*#pragma pack(push, 1)
typedef struct {
        char  idlength;
        char  colourmaptype;
        char  datatypecode;
        short int colourmaporigin;
        short int colourmaplength;
        char  colourmapdepth;
        short int x_origin;
        short int y_origin;
        short width;
        short height;
        char  bitsperpixel;
        char  imagedescriptor;
} tga;
#pragma pack(pop)

FILE *CreateTga(const char *filename, int image_width, int image_height)
{
        auto f = fopen(filename, "w+b");

        tga tga_header;
        memset(&tga_header, 0, sizeof(tga_header));

        tga_header.colourmaptype = 0;
        tga_header.datatypecode = 2;
        //tga_header.colourmaporigin = 0;
        //tga_header.colourmaplength = image_width * image_height;
        //tga_header.colourmapdepth = 32;
        tga_header.x_origin = 0;
        tga_header.y_origin = 0;
        tga_header.width = image_width;
        tga_header.height = image_height;
        tga_header.bitsperpixel = 32;
        tga_header.imagedescriptor = 32; // top-down
        fwrite(&tga_header, 1, sizeof(tga_header), f);

        return f;
}*/

Texture *RenderOpenGL::CreateTexture_ColorKey(const String &name, uint16_t colorkey) {
    return TextureOpenGL::Create(new ColorKey_LOD_Loader(pIcons_LOD, name, colorkey));
}

Texture *RenderOpenGL::CreateTexture_Solid(const String &name) {
    return TextureOpenGL::Create(new Image16bit_LOD_Loader(pIcons_LOD, name));
}

Texture *RenderOpenGL::CreateTexture_Alpha(const String &name) {
    return TextureOpenGL::Create(new Alpha_LOD_Loader(pIcons_LOD, name));
}

Texture *RenderOpenGL::CreateTexture_PCXFromIconsLOD(const String &name) {
    return TextureOpenGL::Create(new PCX_LOD_Compressed_Loader(pIcons_LOD, name));
}

Texture *RenderOpenGL::CreateTexture_PCXFromNewLOD(const String &name) {
    return TextureOpenGL::Create(new PCX_LOD_Compressed_Loader(pNew_LOD, name));
}

Texture *RenderOpenGL::CreateTexture_PCXFromFile(const String &name) {
    return TextureOpenGL::Create(new PCX_File_Loader(name));
}

Texture *RenderOpenGL::CreateTexture_PCXFromLOD(void *pLOD, const String &name) {
    return TextureOpenGL::Create(new PCX_LOD_Raw_Loader((LOD::File *)pLOD, name));
}

Texture *RenderOpenGL::CreateTexture_Blank(unsigned int width, unsigned int height,
    IMAGE_FORMAT format, const void *pixels) {

    return TextureOpenGL::Create(width, height, format, pixels);
}


Texture *RenderOpenGL::CreateTexture(const String &name) {
    return TextureOpenGL::Create(new Bitmaps_LOD_Loader(pBitmaps_LOD, name));
}

Texture *RenderOpenGL::CreateSprite(const String &name, unsigned int palette_id,
                                    /*refactor*/ unsigned int lod_sprite_id) {
    return TextureOpenGL::Create(
        new Sprites_LOD_Loader(pSprites_LOD, palette_id, name, lod_sprite_id));
}

void RenderOpenGL::Update_Texture(Texture *texture) {
    // takes care of endian flip from literals here - hence BGRA

    auto t = (TextureOpenGL *)texture;
    glBindTexture(GL_TEXTURE_2D, t->GetOpenGlTexture());
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, t->GetWidth(), t->GetHeight(), GL_BGRA, GL_UNSIGNED_BYTE, t->GetPixels(IMAGE_FORMAT_A8R8G8B8));
    glBindTexture(GL_TEXTURE_2D, NULL);

    checkglerror();
}

void RenderOpenGL::DeleteTexture(Texture *texture) {
    // crash here when assets not loaded as texture

    auto t = (TextureOpenGL *)texture;
    GLuint texid = t->GetOpenGlTexture();
    if (texid != -1) {
        glDeleteTextures(1, &texid);
    }

    checkglerror();
}

void RenderOpenGL::RemoveTextureFromDevice(Texture* texture) { __debugbreak(); }

bool RenderOpenGL::MoveTextureToDevice(Texture *texture) {
    auto t = (TextureOpenGL *)texture;
    auto native_format = t->GetFormat();
    int gl_format = GL_RGB;
        // native_format == IMAGE_FORMAT_A1R5G5B5 ? GL_RGBA : GL_RGB;

    unsigned __int8 *pixels = nullptr;
    if (native_format == IMAGE_FORMAT_R5G6B5 || native_format == IMAGE_FORMAT_A1R5G5B5 || native_format == IMAGE_FORMAT_A8R8G8B8 || native_format == IMAGE_FORMAT_R8G8B8A8) {
        pixels = (unsigned __int8 *)t->GetPixels(IMAGE_FORMAT_R8G8B8A8);  // rgba
        gl_format = GL_RGBA;
    } else {
        log->Warning("Image not loaded!");
    }

    if (pixels) {
        GLuint texid;
        glGenTextures(1, &texid);
        t->SetOpenGlTexture(texid);

        glBindTexture(GL_TEXTURE_2D, texid);
        glTexImage2D(GL_TEXTURE_2D, 0, gl_format, t->GetWidth(), t->GetHeight(),
                     0, gl_format, GL_UNSIGNED_BYTE, pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glBindTexture(GL_TEXTURE_2D, 0);

        checkglerror();
        return true;
    }
    return false;
}

void _set_3d_projection_matrix() {
    float near_clip = pIndoorCameraD3D->GetNearClip();
    float far_clip = pIndoorCameraD3D->GetFarClip();




    // outdoors 60 - should be 75?
    // indoors 65?/
    // something to do with ratio of screenwidth to viewport width

    float fov = 60;
    if (uCurrentlyLoadedLevelType == LEVEL_Indoor) fov = 50;

    //gluPerspective(fov, double(game_viewport_width/double(game_viewport_height))  // 65.0f
    //               /*(GLfloat)window->GetWidth() / (GLfloat)window->GetHeight()*/,
    //               near_clip, far_clip);
    float screenratio = 1.0;
    
    if (game_viewport_height) {
        screenratio = float(game_viewport_width) / float(game_viewport_height);
    }

    float fov2 = fov * (pi / 180.0);

    projmat = glm::perspective(fov2, screenratio, near_clip, far_clip);

    checkglerror();
}

void _set_3d_modelview_matrix() {
    int camera_x = pParty->vPosition.x - pParty->y_rotation_granularity * cosf(2 * pi_double * pParty->sRotationZ / 2048.0);
    int camera_y = pParty->vPosition.y - pParty->y_rotation_granularity * sinf(2 * pi_double * pParty->sRotationZ / 2048.0);
    int camera_z = pParty->vPosition.z + pParty->sEyelevel;

    glm::vec3 campos = glm::vec3((float)camera_x, (float)camera_y, (float)camera_z);
    glm::vec3 eyepos = glm::vec3((float)camera_x - pParty->y_rotation_granularity * cosf(2 * 3.14159 * pParty->sRotationZ / 2048.0) /*- 5*/,
        (float)camera_y - pParty->y_rotation_granularity * sinf(2 * 3.14159 * pParty->sRotationZ / 2048.0),
        (float)camera_z - pParty->y_rotation_granularity * sinf(2 * 3.14159 * (-pParty->sRotationX - 20) / 2048.0));
    glm::vec3 upvec = glm::vec3(0.0, 0.0, 1.0);

    viewmat = glm::lookAtLH(campos, eyepos, upvec);

    checkglerror();
}

void _set_ortho_projection(bool gameviewport = false) {
    if (!gameviewport) {  // project over entire window
        glViewport(0, 0, window->GetWidth(), window->GetHeight());
        projmat = glm::ortho((float)0, (float)window->GetWidth(), (float)window->GetHeight(), (float)0, (float)-1, (float)1);
    } else {  // project to game viewport
        glViewport(game_viewport_x, window->GetHeight()-game_viewport_w-1, game_viewport_width, game_viewport_height);
        projmat = glm::ortho((float)game_viewport_x, (float)game_viewport_z, (float)game_viewport_w, (float)game_viewport_y, (float)1, (float)-1);
    }
    checkglerror();
}


void _set_ortho_modelview() {
    viewmat = glm::mat4(1);
    checkglerror();
}

const int terrain_block_scale = 512;
const int terrain_height_scale = 32;

struct shaderverts {
    GLfloat x;
    GLfloat y;
    GLfloat z;
    GLfloat u;
    GLfloat v;
    GLfloat texunit;
    GLfloat texturelayer;
    GLfloat normx;
    GLfloat normy;
    GLfloat normz;
    GLfloat attribs;
};



shaderverts terrshaderstore[127 * 127 * 6] = {};

void RenderOpenGL::RenderTerrainD3D() {
    // face culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // camera matrices
    _set_3d_projection_matrix();
    _set_3d_modelview_matrix();

    //GLfloat projmat[16];
    //glGetFloatv(GL_PROJECTION_MATRIX, projmat);
    //GLfloat viewmat[16];
    //glGetFloatv(GL_MODELVIEW_MATRIX, viewmat);

    







    // generate array and populate data - needs to be moved to on map load
    if (terrainVAO == 0) {

        static RenderVertexSoft pTerrainVertices[128 * 128];
        int blockScale = 512;
        int heightScale = 32;

        for (unsigned int y = 0; y < 128; ++y) {
            for (unsigned int x = 0; x < 128; ++x) {
                pTerrainVertices[y * 128 + x].vWorldPosition.x = (-64 + (signed)x) * blockScale;
                pTerrainVertices[y * 128 + x].vWorldPosition.y = (64 - (signed)y) * blockScale;
                pTerrainVertices[y * 128 + x].vWorldPosition.z = heightScale* pOutdoor->pTerrain.pHeightmap[y * 128 + x];
            }
        }

        // reserve first 7 layers for water tiles in unit 0
        auto wtrtexture = this->hd_water_tile_anim[0];
        terraintexturesizes[0] = wtrtexture->GetWidth();

        for (int buff = 0; buff < 7; buff++) {
            char container_name[64];
            sprintf(container_name, "HDWTR%03u", buff);
            
            terraintexmap.insert(std::make_pair(container_name, terraintexmap.size()));
            numterraintexloaded[0]++;
        }


    // tile culling maths
    int camx = WorldPosToGridCellX(pIndoorCameraD3D->vPartyPos.x);
    int camy = WorldPosToGridCellY(pIndoorCameraD3D->vPartyPos.y);
    int tilerange = (pIndoorCameraD3D->GetFarClip() / terrain_block_scale)+1;


        for (int y = 0; y < 127; ++y) {
            for (int x = 0; x < 127; ++x) {
                // map is 127 x 127 squares
                // each square has two triangles
                // each tri has 3 verts

                auto tile = pOutdoor->DoGetTile(x, y);

                int tileunit = 0;
                int tilelayer = 0;

                // check if tile->name is already in list
                auto mapiter = terraintexmap.find(tile->name);
                if (mapiter != terraintexmap.end()) {
                    // if so, extract unit and layer
                    int unitlayer = mapiter->second;
                    tilelayer = unitlayer & 0xFF;
                    tileunit = (unitlayer & 0xFF00) >> 8;
                } else if (tile->name == "wtrtyl") {
                    // water tile
                    tileunit = 0;
                    tilelayer = 0;
                } else {
                // else need to add it
                    auto thistexture = assets->GetBitmap(tile->name);
                    int width = thistexture->GetWidth();
                    // check size to see what unit it needs
                    int i;
                    for (i = 0; i < 8; i++) {
                        if (terraintexturesizes[i] == width || terraintexturesizes[i] == 0) break;
                    }
                    if (i == 8) __debugbreak();

                    if (terraintexturesizes[i] == 0) terraintexturesizes[i] = width;

                    tileunit = i;
                    tilelayer = numterraintexloaded[i];

                    // encode unit and layer together
                    int encode = (tileunit << 8) | tilelayer;

                    // intsert into tex map
                    terraintexmap.insert(std::make_pair(tile->name, encode));

                    numterraintexloaded[i]++;
                    if (numterraintexloaded[i] == 256) __debugbreak();
                }

                
                // vertices

                uint norm_idx = pTerrainNormalIndices[(2 * x * 128) + (2 * y) + 2 /*+ 1*/];  // 2 is top tri // 3 is bottom
                uint bottnormidx = pTerrainNormalIndices[(2 * x * 128) + (2 * y) + 3];
                assert(norm_idx < uNumTerrainNormals);
                assert(bottnormidx < uNumTerrainNormals);
                Vec3_float_* norm = &pTerrainNormals[norm_idx];
                Vec3_float_* norm2 = &pTerrainNormals[bottnormidx];

                // calc each vertex
                // [0] - x,y        n1
                terrshaderstore[6 * (x + (127 * y))].x = pTerrainVertices[y * 128 + x].vWorldPosition.x;
                terrshaderstore[6 * (x + (127 * y))].y = pTerrainVertices[y * 128 + x].vWorldPosition.y;
                terrshaderstore[6 * (x + (127 * y))].z = pTerrainVertices[y * 128 + x].vWorldPosition.z;
                terrshaderstore[6 * (x + (127 * y))].u = 0;
                terrshaderstore[6 * (x + (127 * y))].v = 0;
                terrshaderstore[6 * (x + (127 * y))].texunit = tileunit;
                terrshaderstore[6 * (x + (127 * y))].texturelayer = tilelayer;
                terrshaderstore[6 * (x + (127 * y))].normx = norm->x;
                terrshaderstore[6 * (x + (127 * y))].normy = norm->y;
                terrshaderstore[6 * (x + (127 * y))].normz = norm->z;
                terrshaderstore[6 * (x + (127 * y))].attribs = 0;

                // [1] - x+1,y+1    n1
                terrshaderstore[6 * (x + (127 * y)) + 1].x = pTerrainVertices[(y + 1) * 128 + x + 1].vWorldPosition.x;
                terrshaderstore[6 * (x + (127 * y)) + 1].y = pTerrainVertices[(y + 1) * 128 + x + 1].vWorldPosition.y;
                terrshaderstore[6 * (x + (127 * y)) + 1].z = pTerrainVertices[(y + 1) * 128 + x + 1].vWorldPosition.z;
                terrshaderstore[6 * (x + (127 * y)) + 1].u = 1;
                terrshaderstore[6 * (x + (127 * y)) + 1].v = 1;
                terrshaderstore[6 * (x + (127 * y)) + 1].texunit = tileunit;
                terrshaderstore[6 * (x + (127 * y)) + 1].texturelayer = tilelayer;
                terrshaderstore[6 * (x + (127 * y)) + 1].normx = norm->x;
                terrshaderstore[6 * (x + (127 * y)) + 1].normy = norm->y;
                terrshaderstore[6 * (x + (127 * y)) + 1].normz = norm->z;
                terrshaderstore[6 * (x + (127 * y)) + 1].attribs = 0;

                // [2] - x+1,y      n1
                terrshaderstore[6 * (x + (127 * y)) + 2].x = pTerrainVertices[y * 128 + x + 1].vWorldPosition.x;
                terrshaderstore[6 * (x + (127 * y)) + 2].y = pTerrainVertices[y * 128 + x + 1].vWorldPosition.y;
                terrshaderstore[6 * (x + (127 * y)) + 2].z = pTerrainVertices[y * 128 + x + 1].vWorldPosition.z;
                terrshaderstore[6 * (x + (127 * y)) + 2].u = 1;
                terrshaderstore[6 * (x + (127 * y)) + 2].v = 0;
                terrshaderstore[6 * (x + (127 * y)) + 2].texunit = tileunit;
                terrshaderstore[6 * (x + (127 * y)) + 2].texturelayer = tilelayer;
                terrshaderstore[6 * (x + (127 * y)) + 2].normx = norm->x;
                terrshaderstore[6 * (x + (127 * y)) + 2].normy = norm->y;
                terrshaderstore[6 * (x + (127 * y)) + 2].normz = norm->z;
                terrshaderstore[6 * (x + (127 * y)) + 2].attribs = 0;

                // [3] - x,y        n2
                terrshaderstore[6 * (x + (127 * y)) + 3].x = pTerrainVertices[y * 128 + x].vWorldPosition.x;
                terrshaderstore[6 * (x + (127 * y)) + 3].y = pTerrainVertices[y * 128 + x].vWorldPosition.y;
                terrshaderstore[6 * (x + (127 * y)) + 3].z = pTerrainVertices[y * 128 + x].vWorldPosition.z;
                terrshaderstore[6 * (x + (127 * y)) + 3].u = 0;
                terrshaderstore[6 * (x + (127 * y)) + 3].v = 0;
                terrshaderstore[6 * (x + (127 * y)) + 3].texunit = tileunit;
                terrshaderstore[6 * (x + (127 * y)) + 3].texturelayer = tilelayer;
                terrshaderstore[6 * (x + (127 * y)) + 3].normx = norm2->x;
                terrshaderstore[6 * (x + (127 * y)) + 3].normy = norm2->y;
                terrshaderstore[6 * (x + (127 * y)) + 3].normz = norm2->z;
                terrshaderstore[6 * (x + (127 * y)) + 3].attribs = 0;

                // [4] - x,y+1      n2
                terrshaderstore[6 * (x + (127 * y)) + 4].x = pTerrainVertices[(y + 1) * 128 + x].vWorldPosition.x;
                terrshaderstore[6 * (x + (127 * y)) + 4].y = pTerrainVertices[(y + 1) * 128 + x].vWorldPosition.y;
                terrshaderstore[6 * (x + (127 * y)) + 4].z = pTerrainVertices[(y + 1) * 128 + x].vWorldPosition.z;
                terrshaderstore[6 * (x + (127 * y)) + 4].u = 0;
                terrshaderstore[6 * (x + (127 * y)) + 4].v = 1;
                terrshaderstore[6 * (x + (127 * y)) + 4].texunit = tileunit;
                terrshaderstore[6 * (x + (127 * y)) + 4].texturelayer = tilelayer;
                terrshaderstore[6 * (x + (127 * y)) + 4].normx = norm2->x;
                terrshaderstore[6 * (x + (127 * y)) + 4].normy = norm2->y;
                terrshaderstore[6 * (x + (127 * y)) + 4].normz = norm2->z;
                terrshaderstore[6 * (x + (127 * y)) + 4].attribs = 0;

                // [5] - x+1,y+1    n2
                terrshaderstore[6 * (x + (127 * y)) + 5].x = pTerrainVertices[(y + 1) * 128 + x + 1].vWorldPosition.x;
                terrshaderstore[6 * (x + (127 * y)) + 5].y = pTerrainVertices[(y + 1) * 128 + x + 1].vWorldPosition.y;
                terrshaderstore[6 * (x + (127 * y)) + 5].z = pTerrainVertices[(y + 1) * 128 + x + 1].vWorldPosition.z;
                terrshaderstore[6 * (x + (127 * y)) + 5].u = 1;
                terrshaderstore[6 * (x + (127 * y)) + 5].v = 1;
                terrshaderstore[6 * (x + (127 * y)) + 5].texunit = tileunit;
                terrshaderstore[6 * (x + (127 * y)) + 5].texturelayer = tilelayer;
                terrshaderstore[6 * (x + (127 * y)) + 5].normx = norm2->x;
                terrshaderstore[6 * (x + (127 * y)) + 5].normy = norm2->y;
                terrshaderstore[6 * (x + (127 * y)) + 5].normz = norm2->z;
                terrshaderstore[6 * (x + (127 * y)) + 5].attribs = 0;

                // if (terrshaderstore[6 * (x + (127 * y)) + 5].texturelayer != 0) __debugbreak();


            }
        }

        glGenVertexArrays(1, &terrainVAO);
        glGenBuffers(1, &terrainVBO);

        glBindVertexArray(terrainVAO);
        glBindBuffer(GL_ARRAY_BUFFER, terrainVBO);

        glBufferData(GL_ARRAY_BUFFER, sizeof(terrshaderstore), terrshaderstore, GL_STATIC_DRAW);

        // position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, (11 * sizeof(GLfloat)), (void*)0);
        glEnableVertexAttribArray(0);
        // tex uv attribute
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, (11 * sizeof(GLfloat)), (void*)(3 * sizeof(GLfloat)));
        glEnableVertexAttribArray(1);
        // tex unit attribute
        // tex array layer attribute
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, (11 * sizeof(GLfloat)), (void*)(5 * sizeof(GLfloat)));
        glEnableVertexAttribArray(2);
        // normals
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, (11 * sizeof(GLfloat)), (void*)(7 * sizeof(GLfloat)));
        glEnableVertexAttribArray(3);
        // attribs - not used here yet
        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, (11 * sizeof(GLfloat)), (void*)(10 * sizeof(GLfloat)));
        glEnableVertexAttribArray(4);

        checkglerror();

        // texture set up

        // loop over all units
        for (int unit = 0; unit < 8; unit++) {

            assert(numterraintexloaded[unit] <= 256);
            // skip if textures are empty
            if (numterraintexloaded[unit] == 0) continue;

            glGenTextures(1, &terraintextures[unit]);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D_ARRAY, terraintextures[unit]);

            glTexStorage3D(GL_TEXTURE_2D_ARRAY, 4, GL_RGBA8, terraintexturesizes[unit], terraintexturesizes[unit], numterraintexloaded[unit]);

            std::map<std::string, int>::iterator it = terraintexmap.begin();
            while (it != terraintexmap.end()) {
                // skip if wtrtyl
                //if ((it->first).substr(0, 6) == "wtrtyl") {
                //    std::cout << "skipped  " << it->first << std::endl;
                 //   it++;
                 //   continue;
                //}

                int comb = it->second;
                int tlayer = comb & 0xFF;
                int tunit = (comb & 0xFF00) >> 8;

                if (tunit == unit) {

                    // get texture
                    auto texture = assets->GetBitmap(it->first);

                    std::cout << "loading  " << it->first << "   into unit " << tunit << " and pos " << tlayer << std::endl;

                    glTexSubImage3D(GL_TEXTURE_2D_ARRAY,
                        0,
                        0, 0, tlayer,
                        terraintexturesizes[unit], terraintexturesizes[unit], 1,
                        GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        texture->GetPixels(IMAGE_FORMAT_R8G8B8A8));

                    //numterraintexloaded[0]++;
                }

                it++;
            }

            //iterate through terrain tex map
            //ignore wtrtyl
            //laod in

            //auto tile = pOutdoor->DoGetTile(0, 0);
            //bool border = tile->IsWaterBorderTile();


            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glGenerateMipmap(GL_TEXTURE_2D_ARRAY);



            checkglerror();
        }


    }
    
    //
    //unsigned int VBO;
    
  

 
/////////////////////////////////////////////////////
    // actual drawing

    
    // terrain debug
    if (engine->config->debug_terrain)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);


    ////
    for (int unit = 0; unit < 8; unit++) {

        
        // skip if textures are empty
        if (numterraintexloaded[unit] > 0) {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D_ARRAY, terraintextures[unit]);
        }
        checkglerror();

    }
    //logger->Info("vefore");

    glBindVertexArray(terrainVAO);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
    glEnableVertexAttribArray(4);

    glUseProgram(terrainshader.ID);

    checkglerror();

    // set sampler to texure0
    //glUniform1i(0, 0); ??? 

    //logger->Info("after");
    //// set projection
    glUniformMatrix4fv(glGetUniformLocation(terrainshader.ID, "projection"), 1, GL_FALSE, &projmat[0][0]);
    //// set view
    glUniformMatrix4fv(glGetUniformLocation(terrainshader.ID, "view"), 1, GL_FALSE, &viewmat[0][0]);

    glUniform1i(glGetUniformLocation(terrainshader.ID, "waterframe"), GLint(this->hd_water_current_frame));

    glUniform1i(glGetUniformLocation(terrainshader.ID, "textureArray0"), GLint(0));
    glUniform1i(glGetUniformLocation(terrainshader.ID, "textureArray1"), GLint(1));

    GLfloat camera[3];
    camera[0] = (float)(pParty->vPosition.x - pParty->y_rotation_granularity * cosf(2 * pi_double * pParty->sRotationZ / 2048.0));
    camera[1] = (float)(pParty->vPosition.y - pParty->y_rotation_granularity * sinf(2 * pi_double * pParty->sRotationZ / 2048.0));
    camera[2] = (float)(pParty->vPosition.z + pParty->sEyelevel);
    glUniform3fv(glGetUniformLocation(terrainshader.ID, "CameraPos"), 1, &camera[0]);


    // lighting stuff
    GLfloat sunvec[3];
    sunvec[0] = (float)pOutdoor->vSunlight.x / 65536.0;
    sunvec[1] = (float)pOutdoor->vSunlight.y / 65536.0;
    sunvec[2] = (float)pOutdoor->vSunlight.z / 65536.0;

    float ambient = pParty->uCurrentMinute + pParty->uCurrentHour * 60.0;  // 0 - > 1439
    ambient = 0.15 + (sinf(((ambient - 360.0) * 2 * pi_double) / 1440) + 1) * 0.27;

    float diffuseon = pWeather->bNight ? 0 : 1;



    glUniform3fv(glGetUniformLocation(terrainshader.ID, "sun.direction"), 1, &sunvec[0]);
    glUniform3f(glGetUniformLocation(terrainshader.ID, "sun.ambient"), ambient, ambient, ambient);
    glUniform3f(glGetUniformLocation(terrainshader.ID, "sun.diffuse"), diffuseon * (ambient + 0.3), diffuseon* (ambient + 0.3), diffuseon* (ambient + 0.3));
    glUniform3f(glGetUniformLocation(terrainshader.ID, "sun.specular"), diffuseon * 1.0, diffuseon * 0.8, 0.0);

    if (pParty->armageddon_timer) {
        glUniform3f(glGetUniformLocation(terrainshader.ID, "sun.ambient"), 1.0, 0, 0);
        glUniform3f(glGetUniformLocation(terrainshader.ID, "sun.diffuse"), 1.0, 0, 0);
        glUniform3f(glGetUniformLocation(terrainshader.ID, "sun.specular"), 0, 0, 0);
    }

    // 48

    // point lights - fspointlights
    /*   
        lightingShader.setVec3("pointLights[0].position", pointLightPositions[0]);
        lightingShader.setVec3("pointLights[0].ambient", 0.05f, 0.05f, 0.05f);
        lightingShader.setVec3("pointLights[0].diffuse", 0.8f, 0.8f, 0.8f);
        lightingShader.setVec3("pointLights[0].specular", 1.0f, 1.0f, 1.0f);
        lightingShader.setFloat("pointLights[0].constant", 1.0f);
        lightingShader.setFloat("pointLights[0].linear", 0.09);
        lightingShader.setFloat("pointLights[0].quadratic", 0.032);
    */

    // test torchlight
    float torchradius = 0;
    if (!diffuseon) {
        int rangemult = 1;
        if (pParty->pPartyBuffs[PARTY_BUFF_TORCHLIGHT].Active())
            rangemult = pParty->pPartyBuffs[PARTY_BUFF_TORCHLIGHT].uPower;
        torchradius = float(rangemult) * 1024.0;
    }

    glUniform3fv(glGetUniformLocation(terrainshader.ID, "fspointlights[0].position"), 1, &camera[0]);
    glUniform3f(glGetUniformLocation(terrainshader.ID, "fspointlights[0].ambient"), 0.85, 0.85, 0.85);
    glUniform3f(glGetUniformLocation(terrainshader.ID, "fspointlights[0].diffuse"), 0.85, 0.85, 0.85);
    glUniform3f(glGetUniformLocation(terrainshader.ID, "fspointlights[0].specular"), 0, 0, 1);
    //glUniform1f(glGetUniformLocation(terrainshader.ID, "fspointlights[0].constant"), .81);
    //glUniform1f(glGetUniformLocation(terrainshader.ID, "fspointlights[0].linear"), 0.003);
    //glUniform1f(glGetUniformLocation(terrainshader.ID, "fspointlights[0].quadratic"), 0.000007);
    glUniform1f(glGetUniformLocation(terrainshader.ID, "fspointlights[0].radius"), torchradius);

    
    // rest of lights stacking
    GLuint num_lights = 1;

    for (int i = 0; i < pMobileLightsStack->uNumLightsActive; ++i) {
        if (num_lights >= 20) break;

        String slotnum = std::to_string(num_lights);
        auto test = pMobileLightsStack->pLights[i];

        float x = pMobileLightsStack->pLights[i].vPosition.x;
        float y = pMobileLightsStack->pLights[i].vPosition.y;
        float z = pMobileLightsStack->pLights[i].vPosition.z;

        float r = pMobileLightsStack->pLights[i].uLightColorR / 255.0;
        float g = pMobileLightsStack->pLights[i].uLightColorG / 255.0;
        float b = pMobileLightsStack->pLights[i].uLightColorB / 255.0;

        float lightrad = pMobileLightsStack->pLights[i].uRadius;

        glUniform1f(glGetUniformLocation(terrainshader.ID, ("fspointlights[" + slotnum + "].type").c_str()), 2.0);
        glUniform3f(glGetUniformLocation(terrainshader.ID, ("fspointlights[" + slotnum + "].position").c_str()), x, y, z);
        glUniform3f(glGetUniformLocation(terrainshader.ID, ("fspointlights[" + slotnum + "].ambient").c_str()), r, g, b);
        glUniform3f(glGetUniformLocation(terrainshader.ID, ("fspointlights[" + slotnum + "].diffuse").c_str()), r, g, b);
        glUniform3f(glGetUniformLocation(terrainshader.ID, ("fspointlights[" + slotnum + "].specular").c_str()), r, g, b);
        //glUniform1f(glGetUniformLocation(terrainshader.ID, "fspointlights[0].constant"), .81);
        //glUniform1f(glGetUniformLocation(terrainshader.ID, "fspointlights[0].linear"), 0.003);
        //glUniform1f(glGetUniformLocation(terrainshader.ID, "fspointlights[0].quadratic"), 0.000007);
        glUniform1f(glGetUniformLocation(terrainshader.ID, ("fspointlights[" + slotnum + "].radius").c_str()), lightrad);

        num_lights++;

    //    StackLight_TerrainFace(
    //        (StationaryLight*)&pMobileLightsStack->pLights[i], pNormal,
    //        Light_tile_dist, VertList, uStripType, bLightBackfaces, &num_lights);
    }





    for (int i = 0; i < pStationaryLightsStack->uNumLightsActive; ++i) {
        if (num_lights >= 20) break;

        String slotnum = std::to_string(num_lights);
        auto test = pStationaryLightsStack->pLights[i];

        float x = test.vPosition.x;
        float y = test.vPosition.y;
        float z = test.vPosition.z;

        float r = test.uLightColorR / 255.0;
        float g = test.uLightColorG / 255.0;
        float b = test.uLightColorB / 255.0;

        float lightrad = test.uRadius;

        glUniform1f(glGetUniformLocation(terrainshader.ID, ("fspointlights[" + slotnum + "].type").c_str()), 1.0);
        glUniform3f(glGetUniformLocation(terrainshader.ID, ("fspointlights[" + slotnum + "].position").c_str()), x, y, z);
        glUniform3f(glGetUniformLocation(terrainshader.ID, ("fspointlights[" + slotnum + "].ambient").c_str()), r, g, b);
        glUniform3f(glGetUniformLocation(terrainshader.ID, ("fspointlights[" + slotnum + "].diffuse").c_str()), r, g, b);
        glUniform3f(glGetUniformLocation(terrainshader.ID, ("fspointlights[" + slotnum + "].specular").c_str()), r, g, b);
        //glUniform1f(glGetUniformLocation(terrainshader.ID, "fspointlights[0].constant"), .81);
        //glUniform1f(glGetUniformLocation(terrainshader.ID, "fspointlights[0].linear"), 0.003);
        //glUniform1f(glGetUniformLocation(terrainshader.ID, "fspointlights[0].quadratic"), 0.000007);
        glUniform1f(glGetUniformLocation(terrainshader.ID, ("fspointlights[" + slotnum + "].radius").c_str()), lightrad);

        num_lights++;



    //    StackLight_TerrainFace(&pStationaryLightsStack->pLights[i], pNormal,
    //        Light_tile_dist, VertList, uStripType, bLightBackfaces,
    //        &num_lights);
    }


    // blank lights

    for (int blank = num_lights; blank < 20; blank++) {
        String slotnum = std::to_string(blank);
        glUniform1f(glGetUniformLocation(terrainshader.ID, ("fspointlights[" + slotnum + "].type").c_str()), 0.0);
    }

    checkglerror();


    glDrawArrays(GL_TRIANGLES, 0, (127 * 127 * 6));
    drawcalls++;
    checkglerror();

    glUseProgram(0);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glDisableVertexAttribArray(3);
    glDisableVertexAttribArray(4);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(0);



    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, NULL);



    //end terrain debug
    if (engine->config->debug_terrain)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);


    checkglerror();

    // return;


  
    // stack new decals onto terrain faces ////////////////////////////////////////////////
    // cludge for now


    if(!decal_builder->bloodsplat_container->uNumBloodsplats) return;
    unsigned int NumBloodsplats = decal_builder->bloodsplat_container->uNumBloodsplats;

    // loop over blood to lay
    for (uint i = 0; i < NumBloodsplats; ++i) {
        // approx location of bloodsplat
        int splatx = decal_builder->bloodsplat_container->pBloodsplats_to_apply[i].x;
        int splaty = decal_builder->bloodsplat_container->pBloodsplats_to_apply[i].y;
        int splatz = decal_builder->bloodsplat_container->pBloodsplats_to_apply[i].z;
        int testx = WorldPosToGridCellX(splatx);
        int testy = WorldPosToGridCellY(splaty);
        // use terrain squares in block surrounding to try and stack faces

        for (int loopy = (testy - 1); loopy < (testy + 1); ++loopy) {
            for (int loopx = (testx - 1); loopx < (testx + 1); ++loopx) {
                if (loopy < 0) continue;
                if (loopy > 126) continue;
                if (loopx < 0) continue;
                if (loopx > 126) continue;



                struct Polygon* pTilePolygon = &array_77EC08[pODMRenderParams->uNumPolygons];
                pTilePolygon->flags = pOutdoor->GetSomeOtherTileInfo(loopx, loopy);



                uint norm_idx = pTerrainNormalIndices[(2 * loopx * 128) + (2 * loopy) + 2];  // 2 is top tri // 3 is bottom
                uint bottnormidx = pTerrainNormalIndices[(2 * loopx * 128) + (2 * loopy) + 3];

                assert(norm_idx < uNumTerrainNormals);
                assert(bottnormidx < uNumTerrainNormals);

                Vec3_float_* norm = &pTerrainNormals[norm_idx];
                Vec3_float_* norm2 = &pTerrainNormals[bottnormidx];

                float v51 = (-pOutdoor->vSunlight.x * norm->x) / 65536.0;
                float v53 = (-pOutdoor->vSunlight.y * norm->y) / 65536.0;
                float v52 = (-pOutdoor->vSunlight.z * norm->z) / 65536.0;
                pTilePolygon->dimming_level = 20 - 20 * (v51 + v53 + v52);

                if (pTilePolygon->dimming_level < 0) pTilePolygon->dimming_level = 0;
                if (pTilePolygon->dimming_level > 31) pTilePolygon->dimming_level = 31;

                float Light_tile_dist = 0.0;

                int blockScale = 512;
                int heightScale = 32;

                static stru154 static_sub_0048034E_stru_154;

                // top tri
                // x, y
                VertexRenderList[0].vWorldPosition.x = terrshaderstore[6 * (loopx + (127 * loopy))].x;
                VertexRenderList[0].vWorldPosition.y = terrshaderstore[6 * (loopx + (127 * loopy))].y;
                VertexRenderList[0].vWorldPosition.z = terrshaderstore[6 * (loopx + (127 * loopy))].z;
                // x + 1, y + 1
                VertexRenderList[1].vWorldPosition.x = terrshaderstore[6 * (loopx + (127 * loopy)) + 1].x;
                VertexRenderList[1].vWorldPosition.y = terrshaderstore[6 * (loopx + (127 * loopy)) + 1].y;
                VertexRenderList[1].vWorldPosition.z = terrshaderstore[6 * (loopx + (127 * loopy)) + 1].z;
                // x + 1, y
                VertexRenderList[2].vWorldPosition.x = terrshaderstore[6 * (loopx + (127 * loopy)) + 2].x;
                VertexRenderList[2].vWorldPosition.y = terrshaderstore[6 * (loopx + (127 * loopy)) + 2].y;
                VertexRenderList[2].vWorldPosition.z = terrshaderstore[6 * (loopx + (127 * loopy)) + 2].z;

                decal_builder->ApplyBloodSplatToTerrain(pTilePolygon, norm, &Light_tile_dist, VertexRenderList, 3, 1);
                static_sub_0048034E_stru_154.ClassifyPolygon(norm, Light_tile_dist);
                if (decal_builder->uNumSplatsThisFace > 0)
                    decal_builder->BuildAndApplyDecals(31 - pTilePolygon->dimming_level, 4, &static_sub_0048034E_stru_154, 3, VertexRenderList, 0/**(float*)&uClipFlag*/, -1);

                //bottom tri
                v51 = (-pOutdoor->vSunlight.x * norm2->x) / 65536.0;
                v53 = (-pOutdoor->vSunlight.y * norm2->y) / 65536.0;
                v52 = (-pOutdoor->vSunlight.z * norm2->z) / 65536.0;
                pTilePolygon->dimming_level = 20 - 20 * (v51 + v53 + v52);

                if (pTilePolygon->dimming_level < 0) pTilePolygon->dimming_level = 0;
                if (pTilePolygon->dimming_level > 31) pTilePolygon->dimming_level = 31;


                
                // x, y
                VertexRenderList[0].vWorldPosition.x = terrshaderstore[6 * (loopx + (127 * loopy)) + 3].x;
                VertexRenderList[0].vWorldPosition.y = terrshaderstore[6 * (loopx + (127 * loopy)) + 3].y;
                VertexRenderList[0].vWorldPosition.z = terrshaderstore[6 * (loopx + (127 * loopy)) + 3].z;
                // x, y + 1
                VertexRenderList[1].vWorldPosition.x = terrshaderstore[6 * (loopx + (127 * loopy)) + 4].x;
                VertexRenderList[1].vWorldPosition.y = terrshaderstore[6 * (loopx + (127 * loopy)) + 4].y;
                VertexRenderList[1].vWorldPosition.z = terrshaderstore[6 * (loopx + (127 * loopy)) + 4].z;
                // x + 1, y + 1
                VertexRenderList[2].vWorldPosition.x = terrshaderstore[6 * (loopx + (127 * loopy)) + 5].x;
                VertexRenderList[2].vWorldPosition.y = terrshaderstore[6 * (loopx + (127 * loopy)) + 5].y;
                VertexRenderList[2].vWorldPosition.z = terrshaderstore[6 * (loopx + (127 * loopy)) + 5].z;

                decal_builder->ApplyBloodSplatToTerrain(pTilePolygon, norm2, &Light_tile_dist, VertexRenderList, 3, 0);
                static_sub_0048034E_stru_154.ClassifyPolygon(norm2, Light_tile_dist);
                if (decal_builder->uNumSplatsThisFace > 0)
                    decal_builder->BuildAndApplyDecals(31 - pTilePolygon->dimming_level, 4, &static_sub_0048034E_stru_154, 3, VertexRenderList, 0/**(float*)&uClipFlag_2*/, -1);




            }
        }

    }

    // end of new system test

    return;


        // tile culling maths
    int camx = WorldPosToGridCellX(pIndoorCameraD3D->vPartyPos.x);
    int camy = WorldPosToGridCellY(pIndoorCameraD3D->vPartyPos.y);
    int tilerange = (pIndoorCameraD3D->GetFarClip() / terrain_block_scale) + 1;

    int camfacing = 2048 - pIndoorCameraD3D->sRotationZ;
    int right = int(camfacing - (TrigLUT->uIntegerPi / 2));
    int left = int(camfacing + (TrigLUT->uIntegerPi / 2));
    if (left > 2048) left -= 2048;
    if (right < 0) right += 2048;


    float direction = (float)(pIndoorCameraD3D->sRotationZ /
        256);  // direction of the camera(напрвление
               // камеры) 0-East(B) 1-NorthEast(CB)
               // 2-North(C)
               // 3-WestNorth(CЗ)
               // 4-West(З)
               // 5-SouthWest(ЮЗ)
               // 6-South(Ю)
               // 7-SouthEast(ЮВ)
    unsigned int Start_X, End_X, Start_Y, End_Y;
    if (direction >= 0 && direction < 1.0) {  // East(B) - NorthEast(CB)
        Start_X = camx - 2, End_X = 127;
        Start_Y = 0, End_Y = 127;
    }
    else if (direction >= 1.0 &&
        direction < 3.0) {  // NorthEast(CB) - WestNorth(CЗ)
        Start_X = 0, End_X = 127;
        Start_Y = 0, End_Y = camy + 2;
    }
    else if (direction >= 3.0 &&
        direction < 5.0) {  // WestNorth(CЗ) - SouthWest(ЮЗ)
        Start_X = 0, End_X = camx + 2;
        Start_Y = 0, End_Y = 127;
    }
    else if (direction >= 5.0 &&
        direction < 7.0) {  // SouthWest(ЮЗ) - //SouthEast(ЮВ)
        Start_X = 0, End_X = 127;
        Start_Y = camy - 2, End_Y = 127;
    }
    else {  // SouthEast(ЮВ) - East(B)
        Start_X = camx - 2, End_X = 127;
        Start_Y = 0, End_Y = 127;
    }




    //decal_builder->ApplyBloodSplatToTerrain(pTilePolygon, norm, &Light_tile_dist, VertexRenderList, 3, 1);

    float convert = 2048.0 / (2 * pi);

    for (int y = Start_Y; y < End_Y; ++y) {
        for (int x = Start_X; x < End_X; ++x) {
            // tile culling
            int xdist = camx - x;
            int ydist = camy - y;

            if (xdist > tilerange || ydist > tilerange) continue;

            int dist = sqrt((xdist)*(xdist) + (ydist)*(ydist));
            if (dist > tilerange) continue;  // crude distance culling

            // could do further x + z culling by camera direction see dx

            // 2 pi = 2048

            int tiledir = TrigLUT->Atan2(xdist, ydist)+1024;
            //int tiledir = atan2(xdist, ydist) * convert + 1024;

            if (tiledir > 2048) {
                tiledir -= 2048;
            }

            if (dist > 2) {  // dont cull near feet
                if (left > right) {  // crude fov culling
                    if ((tiledir > left) || (tiledir < right)) continue;
                } else {
                    if (!(tiledir < left || tiledir > right)) continue;
                }
            }

            struct Polygon* pTilePolygon = &array_77EC08[pODMRenderParams->uNumPolygons];
            pTilePolygon->flags = pOutdoor->GetSomeOtherTileInfo(x, y);
            pTilePolygon->dimming_level = 20.0;

            uint norm_idx = pTerrainNormalIndices[(2 * x * 128) + (2 * y) + 2];  // 2 is top tri // 3 is bottom
            uint bottnormidx = pTerrainNormalIndices[(2 * x * 128) + (2 * y) + 3];

            assert(norm_idx < uNumTerrainNormals);
            assert(bottnormidx < uNumTerrainNormals);

            Vec3_float_* norm = &pTerrainNormals[norm_idx];
            Vec3_float_* norm2 = &pTerrainNormals[bottnormidx];

            float Light_tile_dist = 0.0;

            int blockScale = 512;
            int heightScale = 32;

            static stru154 static_sub_0048034E_stru_154;

            // top tri
            // x, y
            VertexRenderList[0].vWorldPosition.x = terrshaderstore[6 * (x + (127 * y))].x;
            VertexRenderList[0].vWorldPosition.y = terrshaderstore[6 * (x + (127 * y))].y;
            VertexRenderList[0].vWorldPosition.z = terrshaderstore[6 * (x + (127 * y))].z;
            // x + 1, y + 1
            VertexRenderList[1].vWorldPosition.x = terrshaderstore[6 * (x + (127 * y)) + 1].x;
            VertexRenderList[1].vWorldPosition.y = terrshaderstore[6 * (x + (127 * y)) + 1].y;
            VertexRenderList[1].vWorldPosition.z = terrshaderstore[6 * (x + (127 * y)) + 1].z;
            // x + 1, y
            VertexRenderList[2].vWorldPosition.x = terrshaderstore[6 * (x + (127 * y)) + 2].x;
            VertexRenderList[2].vWorldPosition.y = terrshaderstore[6 * (x + (127 * y)) + 2].y;
            VertexRenderList[2].vWorldPosition.z = terrshaderstore[6 * (x + (127 * y)) + 2].z;

            decal_builder->ApplyBloodSplatToTerrain(pTilePolygon, norm, &Light_tile_dist, VertexRenderList, 3, 1);
            static_sub_0048034E_stru_154.ClassifyPolygon(norm, Light_tile_dist);
            if (decal_builder->uNumSplatsThisFace > 0)
                decal_builder->BuildAndApplyDecals(31 - pTilePolygon->dimming_level, 4, &static_sub_0048034E_stru_154, 3, VertexRenderList, 0/**(float*)&uClipFlag*/, -1);

            //bottom tri
            // x, y
            VertexRenderList[0].vWorldPosition.x = terrshaderstore[6 * (x + (127 * y)) + 3].x;
            VertexRenderList[0].vWorldPosition.y = terrshaderstore[6 * (x + (127 * y)) + 3].y;
            VertexRenderList[0].vWorldPosition.z = terrshaderstore[6 * (x + (127 * y)) + 3].z;
            // x, y + 1
            VertexRenderList[1].vWorldPosition.x = terrshaderstore[6 * (x + (127 * y)) + 4].x;
            VertexRenderList[1].vWorldPosition.y = terrshaderstore[6 * (x + (127 * y)) + 4].y;
            VertexRenderList[1].vWorldPosition.z = terrshaderstore[6 * (x + (127 * y)) + 4].z;
            // x + 1, y + 1
            VertexRenderList[2].vWorldPosition.x = terrshaderstore[6 * (x + (127 * y)) + 5].x;
            VertexRenderList[2].vWorldPosition.y = terrshaderstore[6 * (x + (127 * y)) + 5].y;
            VertexRenderList[2].vWorldPosition.z = terrshaderstore[6 * (x + (127 * y)) + 5].z;

            decal_builder->ApplyBloodSplatToTerrain(pTilePolygon, norm2, &Light_tile_dist, VertexRenderList, 3, 0);
            static_sub_0048034E_stru_154.ClassifyPolygon(norm2, Light_tile_dist);
            if (decal_builder->uNumSplatsThisFace > 0)
                decal_builder->BuildAndApplyDecals(31 - pTilePolygon->dimming_level, 4, &static_sub_0048034E_stru_154, 3, VertexRenderList, 0/**(float*)&uClipFlag_2*/, -1);

        }
    }



    return;

}



void RenderOpenGL::DrawOutdoorSkyD3D() {
    double rot_to_rads = ((2 * pi_double) / 2048);

    // lowers clouds as party goes up
    float  horizon_height_offset = ((double)(pODMRenderParams->int_fov_rad * pIndoorCameraD3D->vPartyPos.z)
        / ((double)pODMRenderParams->int_fov_rad + 8192.0)
        + (double)(pViewport->uScreenCenterY));

    // magnitude in up direction
    float cam_vec_up = cos((double)pIndoorCameraD3D->sRotationX * rot_to_rads) * pIndoorCameraD3D->GetFarClip();

    float bot_y_proj = ((double)(pViewport->uScreenCenterY) -
        (double)pODMRenderParams->int_fov_rad /
        (cam_vec_up + 0.0000001) *
        (sin((double)pIndoorCameraD3D->sRotationX * rot_to_rads)
            *
            pIndoorCameraD3D->GetFarClip() -
            (double)pIndoorCameraD3D->vPartyPos.z));

    struct Polygon pSkyPolygon;
    pSkyPolygon.texture = nullptr;
    pSkyPolygon.ptr_38 = &SkyBillboard;


    // if ( pParty->uCurrentHour > 20 || pParty->uCurrentHour < 5 )
    // pSkyPolygon.uTileBitmapID = pOutdoor->New_SKY_NIGHT_ID;
    // else
    // pSkyPolygon.uTileBitmapID = pOutdoor->sSky_TextureID;//179(original 166)
    // pSkyPolygon.pTexture = (Texture_MM7 *)(pSkyPolygon.uTileBitmapID != -1 ?
    // (int)&pBitmaps_LOD->pTextures[pSkyPolygon.uTileBitmapID] : 0);

    pSkyPolygon.texture = pOutdoor->sky_texture;
    if (pSkyPolygon.texture) {
        pSkyPolygon.dimming_level = 0;
        pSkyPolygon.uNumVertices = 4;

        // centering(центруем)-----------------------------------------------------------------
        // plane of sky polygon rotation vector
        float v18x, v18y, v18z;
        /*pSkyPolygon.v_18.x*/ v18x = -TrigLUT->Sin(-pIndoorCameraD3D->sRotationX + 16) / 65536.0;
        /*pSkyPolygon.v_18.y*/ v18y = 0;
        /*pSkyPolygon.v_18.z*/ v18z = -TrigLUT->Cos(pIndoorCameraD3D->sRotationX + 16) / 65536.0;

        // sky wiew position(положение неба на
        // экране)------------------------------------------
        //                X
        // 0._____________________________.3
        //  |8,8                    468,8 |
        //  |                             |
        //  |                             |
        // Y|                             |
        //  |                             |
        //  |8,351                468,351 |
        // 1._____________________________.2
        //
        VertexRenderList[0].vWorldViewProjX = (double)(signed int)pViewport->uViewportTL_X;  // 8
        VertexRenderList[0].vWorldViewProjY = (double)(signed int)pViewport->uViewportTL_Y;  // 8

        VertexRenderList[1].vWorldViewProjX = (double)(signed int)pViewport->uViewportTL_X;   // 8
        VertexRenderList[1].vWorldViewProjY = (double)bot_y_proj + 1;  // 247

        VertexRenderList[2].vWorldViewProjX = (double)(signed int)pViewport->uViewportBR_X;   // 468
        VertexRenderList[2].vWorldViewProjY = (double)bot_y_proj + 1;  // 247

        VertexRenderList[3].vWorldViewProjX = (double)(signed int)pViewport->uViewportBR_X;  // 468
        VertexRenderList[3].vWorldViewProjY = (double)(signed int)pViewport->uViewportTL_Y;  // 8

        double half_fov_angle_rads = ((pODMRenderParams->uCameraFovInDegrees - 1) * pi_double) / 360;

        // far width per pixel??
        // 1 / d where d is the distance from the camera to the image plane
        float widthperpixel = 1 /
            (((double)(pViewport->uViewportBR_X - pViewport->uViewportTL_X) / 2)
                / tan(half_fov_angle_rads) +
                0.5);

        for (uint i = 0; i < pSkyPolygon.uNumVertices; ++i) {
            // rotate skydome(вращение купола
            // неба)--------------------------------------
            // В игре принята своя система измерения углов. Полный угол (180).
            // Значению угла 0 соответствует направление на север и/или юг (либо
            // на восток и/или запад), значению 65536 еденицам(0х10000)
            // соответствует угол 90. две переменные хранят данные по углу
            // обзора. field_14 по западу и востоку. field_20 по югу и северу от
            // -25080 до 25080

            float v13 = widthperpixel * (pViewport->uScreenCenterX - VertexRenderList[i].vWorldViewProjX);


            float v39 = (pSkyPolygon.ptr_38->CamVecLeft_Y * widthperpixel * (horizon_height_offset - floor(VertexRenderList[i].vWorldViewProjY + 0.5)));
            float v35 = v39 + pSkyPolygon.ptr_38->CamVecLeft_Z;

            float skyfinalleft = v35 + (pSkyPolygon.ptr_38->CamVecLeft_X * v13);

            v39 = (pSkyPolygon.ptr_38->CamVecFront_Y * widthperpixel * (horizon_height_offset - floor(VertexRenderList[i].vWorldViewProjY + 0.f)));
            float v36 = v39 + pSkyPolygon.ptr_38->CamVecFront_Z;

            float finalskyfront = v36 + (pSkyPolygon.ptr_38->CamVecFront_X * v13);


            float v9 = (/*pSkyPolygon.v_18.z*/v18z * widthperpixel * (horizon_height_offset - floor(VertexRenderList[i].vWorldViewProjY + 0.5)));




            float top_y_proj = /*pSkyPolygon.v_18.x*/v18x + v9;
            if (top_y_proj > 0) top_y_proj = 0;

            /* v32 = (signed __int64)VertexRenderList[i].vWorldViewProjY - 1.0; */
            // v14 = widthperpixel * (horizon_height_offset - v32);
            // while (1) {
            //    if (top_y_proj) {
            //        v37 = 0.03125;  // abs((int)cam_vec_up >> 14);
            //        v15 = abs(top_y_proj);
            //        if (v37 <= v15 ||
            //            v32 <= (signed int)pViewport->uViewportTL_Y) {
            //            if (top_y_proj <= 0) break;
            //        }
            //    }
            //    v16 = (/*pSkyPolygon.v_18.z*/v18z * v14);  // does this bit ever get called?
            //    --v32;
            //    v14 += widthperpixel;
            //    top_y_proj = /*pSkyPolygon.v_18.x*/v18x + v16;
            // }

            float worldviewdepth = -64.0 / top_y_proj;
            if (worldviewdepth < 0) worldviewdepth = pIndoorCameraD3D->GetFarClip();

            float texoffset_U = (float(pMiscTimer->uTotalGameTimeElapsed) / 128.0) + ((skyfinalleft * worldviewdepth));
            VertexRenderList[i].u = texoffset_U / ((float)pSkyPolygon.texture->GetWidth());

            float texoffset_V = (float(pMiscTimer->uTotalGameTimeElapsed) / 128.0) + ((finalskyfront * worldviewdepth));
            VertexRenderList[i].v = texoffset_V / ((float)pSkyPolygon.texture->GetHeight());

            VertexRenderList[i].vWorldViewPosition.x = pIndoorCameraD3D->GetFarClip();
            VertexRenderList[i]._rhw = 1.0 / (double)(worldviewdepth);
        }

        _set_ortho_projection(1);
        _set_ortho_modelview();

        DrawOutdoorSkyPolygon(&pSkyPolygon);

        // adjust and draw again to fill gap below horizon
        // could mirror over??

        // VertexRenderList[0].vWorldViewProjY += 60;
        // VertexRenderList[1].vWorldViewProjY += 60;
        // VertexRenderList[2].vWorldViewProjY += 60;
        // VertexRenderList[3].vWorldViewProjY += 60;

        // DrawOutdoorSkyPolygon(&pSkyPolygon);
    }
}

//----- (004A2DA3) --------------------------------------------------------
void RenderOpenGL::DrawOutdoorSkyPolygon(struct Polygon *pSkyPolygon) {

    return;

    // needs sorting


    checkglerror();
    auto texture = (TextureOpenGL *)pSkyPolygon->texture;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture->GetOpenGlTexture());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    

    //VertexRenderList[0].u = 0 - (float)pParty->sRotationZ / (512*5.0);
    //VertexRenderList[1].u = 0 - (float)pParty->sRotationZ / 512;
    //VertexRenderList[2].u = 1 - (float)pParty->sRotationZ / 512;
    //VertexRenderList[3].u = 1 - (float)pParty->sRotationZ / (512*5.0);

    //if (pParty->sRotationX > 0) {
    //    VertexRenderList[0].v = 0 - (float)pParty->sRotationX / 1024;
    //    VertexRenderList[1].v = 1 - (float)pParty->sRotationX / 1024;
    //    VertexRenderList[2].v = 1 - (float)pParty->sRotationX / 1024;
    //    VertexRenderList[3].v = 0 - (float)pParty->sRotationX / 1024;
    //} else {
    //    VertexRenderList[0].v = 0 - (float)pParty->sRotationX / 256;
    //    VertexRenderList[1].v = 1 - (float)pParty->sRotationX / 256;
    //    VertexRenderList[2].v = 1 - (float)pParty->sRotationX / 256;
    //    VertexRenderList[3].v = 0 - (float)pParty->sRotationX / 256;
    //}

    // x' = cos(a) * x - sin(a) * y
    // y' = sin(a) * x + cos(a) * y

    //float midx = 1.0;
    //float midy = 0.0;

    //VertexRenderList[0].u = (pIndoorCameraD3D->fRotationZCosine * (1.0-midx) - pIndoorCameraD3D->fRotationZSine * (1.0-midy) + midx) / 5.0; //-(float)pParty->sRotationZ / (512 * 5.0);
    //VertexRenderList[1].u = pIndoorCameraD3D->fRotationZCosine * (1.0-midx) - pIndoorCameraD3D->fRotationZSine * (2.0-midy) + midx;
    //VertexRenderList[2].u = pIndoorCameraD3D->fRotationZCosine * (2.0-midx) - pIndoorCameraD3D->fRotationZSine * (2.0-midy) + midx;
    //VertexRenderList[3].u = (pIndoorCameraD3D->fRotationZCosine * (2.0-midx) - pIndoorCameraD3D->fRotationZSine * (1.0-midy) + midx) / 5.0;

    //VertexRenderList[0].v = (pIndoorCameraD3D->fRotationZSine * (1.0-midy) + pIndoorCameraD3D->fRotationZCosine * (1.0-midx) + midy) / 5.0; //-(float)pParty->sRotationZ / (512 * 5.0);
    //VertexRenderList[1].v = pIndoorCameraD3D->fRotationZSine * (1.0-midy) + pIndoorCameraD3D->fRotationZCosine * (2.0-midx) + midy;
    //VertexRenderList[2].v = pIndoorCameraD3D->fRotationZSine * (2.0-midy) + pIndoorCameraD3D->fRotationZCosine * (2.0-midx) + midy;
    //VertexRenderList[3].v = (pIndoorCameraD3D->fRotationZSine * (2.0-midy) + pIndoorCameraD3D->fRotationZCosine * (1.0-midx) + midy)  / 5.0;


    //VertexRenderList[0].u = -0.2;
    //VertexRenderList[1].u = -2.0;
    //VertexRenderList[2].u = 2.0;
    //VertexRenderList[3].u = 0.2;

    //VertexRenderList[0].v = 0;
    //VertexRenderList[1].v = 1;
    //VertexRenderList[2].v = 1;
    //VertexRenderList[3].v = 0;

    //GLfloat projmat[16];
    //glGetFloatv(GL_PROJECTION_MATRIX, projmat);
    //GLfloat viewmat[16];
    //glGetFloatv(GL_MODELVIEW_MATRIX, viewmat);


    glUseProgram(passthroughshader.ID);

    // set sampler to texure0
    //glUniform1i(0, 0); ??? 

    //logger->Info("after");
    //// set projection
    glUniformMatrix4fv(glGetUniformLocation(passthroughshader.ID, "projection"), 1, GL_FALSE, &projmat[0][0]);
    //// set view
    glUniformMatrix4fv(glGetUniformLocation(passthroughshader.ID, "view"), 1, GL_FALSE, &viewmat[0][0]);


    unsigned int diffuse = ::GetActorTintColor(31, 0, VertexRenderList[0].vWorldViewPosition.x, 1, 0);

    float col = (diffuse & 0xFF) / 255.0f;

    float vertices[] = {
        // positions          // colors           // texture coords
        VertexRenderList[0].vWorldViewProjX, VertexRenderList[0].vWorldViewProjY, 0.99989998, col, col, col, VertexRenderList[0].u, VertexRenderList[0].v,
        VertexRenderList[1].vWorldViewProjX, VertexRenderList[1].vWorldViewProjY, 0.99989998, col, col, col, VertexRenderList[1].u, VertexRenderList[1].v,
        VertexRenderList[2].vWorldViewProjX, VertexRenderList[2].vWorldViewProjY, 0.99989998, col, col, col, VertexRenderList[2].u, VertexRenderList[2].v,

        VertexRenderList[0].vWorldViewProjX, VertexRenderList[0].vWorldViewProjY, 0.99989998, col, col, col, VertexRenderList[0].u, VertexRenderList[0].v,
        VertexRenderList[2].vWorldViewProjX, VertexRenderList[2].vWorldViewProjY, 0.99989998, col, col, col, VertexRenderList[2].u, VertexRenderList[2].v,
        VertexRenderList[3].vWorldViewProjX, VertexRenderList[3].vWorldViewProjY, 0.99989998, col, col, col, VertexRenderList[3].u, VertexRenderList[3].v,
    };


    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);


    glBindVertexArray(VAO);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // texture coord attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);


    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);

    glUseProgram(0);

    drawcalls++;

    checkglerror();
}

void RenderOpenGL::DrawBillboards_And_MaybeRenderSpecialEffects_And_EndScene() {
    engine->draw_debug_outlines();
    this->DoRenderBillboards_D3D();
    spell_fx_renderer->RenderSpecialEffects();
}


twodverts billbshaderstore[500] = {};
int billbvertscnt = 0;
int billbopac[500] = {};

//----- (004A1C1E) --------------------------------------------------------
void RenderOpenGL::DoRenderBillboards_D3D() {
    glEnable(GL_BLEND);

    glDepthMask(GL_FALSE);  // in theory billboards all sorted by depth so dont cull by depth test

    glDisable(GL_CULL_FACE);  // some quads are reversed to reuse sprites opposite hand
    // glEnable(GL_TEXTURE_2D);

    _set_ortho_projection(1);
    _set_ortho_modelview();

    // flush twod
    // drawtwodverts();

    // fill twod

    int lastex = 0;
    int changes = 0;

    checkglerror();

    int handledboards = uNumBillboardsToDraw - 1;

    for (int i = uNumBillboardsToDraw - 1; i >= 0; --i) {
        auto billboard = &pBillboardRenderListD3D[i];
        // texture
        auto texture = (TextureOpenGL*)billboard->texture;
        float gltexid = texture->GetOpenGlTexture();

        // z pos
        float oneoz = 1. / (billboard->screen_space_z);
        float oneon = 1. / (pIndoorCameraD3D->GetNearClip() + 4.0);
        float oneof = 1. / pIndoorCameraD3D->GetFarClip();
        float zdepth = (oneoz - oneon) / (oneof - oneon);  // depth is  non linear  proportional to reciprocal of distance
    


        // 0 1 2 / 0 2 3
        billbopac[billbvertscnt] = billboard->opacity;
        billbshaderstore[billbvertscnt].x = billboard->pQuads[0].pos.x;
        billbshaderstore[billbvertscnt].y = billboard->pQuads[0].pos.y;
        billbshaderstore[billbvertscnt].z = zdepth;
        billbshaderstore[billbvertscnt].u = billboard->pQuads[0].texcoord.x;
        billbshaderstore[billbvertscnt].v = billboard->pQuads[0].texcoord.y;
        billbshaderstore[billbvertscnt].r = ((billboard->pQuads[0].diffuse >> 16) & 0xFF) / 255.0f;
        billbshaderstore[billbvertscnt].g = ((billboard->pQuads[0].diffuse >> 8) & 0xFF) / 255.0f;
        billbshaderstore[billbvertscnt].b = ((billboard->pQuads[0].diffuse >> 0) & 0xFF) / 255.0f;
        billbshaderstore[billbvertscnt].a = 1;
        billbshaderstore[billbvertscnt].texid = gltexid;
        billbvertscnt++;

        billbshaderstore[billbvertscnt].x = billboard->pQuads[1].pos.x;
        billbshaderstore[billbvertscnt].y = billboard->pQuads[1].pos.y;
        billbshaderstore[billbvertscnt].z = zdepth;
        billbshaderstore[billbvertscnt].u = billboard->pQuads[1].texcoord.x;
        billbshaderstore[billbvertscnt].v = billboard->pQuads[1].texcoord.y;
        billbshaderstore[billbvertscnt].r = ((billboard->pQuads[1].diffuse >> 16) & 0xFF) / 255.0f;
        billbshaderstore[billbvertscnt].g = ((billboard->pQuads[1].diffuse >> 8) & 0xFF) / 255.0f;
        billbshaderstore[billbvertscnt].b = ((billboard->pQuads[1].diffuse >> 0) & 0xFF) / 255.0f;
        billbshaderstore[billbvertscnt].a = 1;
        billbshaderstore[billbvertscnt].texid = gltexid;
        billbvertscnt++;

        billbshaderstore[billbvertscnt].x = billboard->pQuads[2].pos.x;
        billbshaderstore[billbvertscnt].y = billboard->pQuads[2].pos.y;
        billbshaderstore[billbvertscnt].z = zdepth;
        billbshaderstore[billbvertscnt].u = billboard->pQuads[2].texcoord.x;
        billbshaderstore[billbvertscnt].v = billboard->pQuads[2].texcoord.y;
        billbshaderstore[billbvertscnt].r = ((billboard->pQuads[2].diffuse >> 16) & 0xFF) / 255.0f;
        billbshaderstore[billbvertscnt].g = ((billboard->pQuads[2].diffuse >> 8) & 0xFF) / 255.0f;
        billbshaderstore[billbvertscnt].b = ((billboard->pQuads[2].diffuse >> 0) & 0xFF) / 255.0f;
        billbshaderstore[billbvertscnt].a = 1;
        billbshaderstore[billbvertscnt].texid = gltexid;
        billbvertscnt++;

        ////////////////////////////////

        billbshaderstore[billbvertscnt].x = billboard->pQuads[0].pos.x;
        billbshaderstore[billbvertscnt].y = billboard->pQuads[0].pos.y;
        billbshaderstore[billbvertscnt].z = zdepth;
        billbshaderstore[billbvertscnt].u = billboard->pQuads[0].texcoord.x;
        billbshaderstore[billbvertscnt].v = billboard->pQuads[0].texcoord.y;
        billbshaderstore[billbvertscnt].r = ((billboard->pQuads[0].diffuse >> 16) & 0xFF) / 255.0f;
        billbshaderstore[billbvertscnt].g = ((billboard->pQuads[0].diffuse >> 8) & 0xFF) / 255.0f;
        billbshaderstore[billbvertscnt].b = ((billboard->pQuads[0].diffuse >> 0) & 0xFF) / 255.0f;
        billbshaderstore[billbvertscnt].a = 1;
        billbshaderstore[billbvertscnt].texid = gltexid;
        billbvertscnt++;

        billbshaderstore[billbvertscnt].x = billboard->pQuads[2].pos.x;
        billbshaderstore[billbvertscnt].y = billboard->pQuads[2].pos.y;
        billbshaderstore[billbvertscnt].z = zdepth;
        billbshaderstore[billbvertscnt].u = billboard->pQuads[2].texcoord.x;
        billbshaderstore[billbvertscnt].v = billboard->pQuads[2].texcoord.y;
        billbshaderstore[billbvertscnt].r = ((billboard->pQuads[2].diffuse >> 16) & 0xFF) / 255.0f;
        billbshaderstore[billbvertscnt].g = ((billboard->pQuads[2].diffuse >> 8) & 0xFF) / 255.0f;
        billbshaderstore[billbvertscnt].b = ((billboard->pQuads[2].diffuse >> 0) & 0xFF) / 255.0f;
        billbshaderstore[billbvertscnt].a = 1;
        billbshaderstore[billbvertscnt].texid = gltexid;
        billbvertscnt++;

        billbshaderstore[billbvertscnt].x = billboard->pQuads[3].pos.x;
        billbshaderstore[billbvertscnt].y = billboard->pQuads[3].pos.y;
        billbshaderstore[billbvertscnt].z = zdepth;
        billbshaderstore[billbvertscnt].u = billboard->pQuads[3].texcoord.x;
        billbshaderstore[billbvertscnt].v = billboard->pQuads[3].texcoord.y;
        billbshaderstore[billbvertscnt].r = ((billboard->pQuads[3].diffuse >> 16) & 0xFF) / 255.0f;
        billbshaderstore[billbvertscnt].g = ((billboard->pQuads[3].diffuse >> 8) & 0xFF) / 255.0f;
        billbshaderstore[billbvertscnt].b = ((billboard->pQuads[3].diffuse >> 0) & 0xFF) / 255.0f;
        billbshaderstore[billbvertscnt].a = 1;
        billbshaderstore[billbvertscnt].texid = gltexid;
        billbvertscnt++;

        // if full then draw
        if (billbvertscnt > 490) drawbillbverts(&handledboards);
    }
    checkglerror();
    //////////

    drawbillbverts(0);

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}

void RenderOpenGL::drawbillbverts(int* upto) {
    if (!billbvertscnt) return;

    if (billbVAO == 0) {
        glGenVertexArrays(1, &billbVAO);
        glGenBuffers(1, &billbVBO);

        glBindVertexArray(billbVAO);
        glBindBuffer(GL_ARRAY_BUFFER, billbVBO);
        
        glBufferData(GL_ARRAY_BUFFER, sizeof(billbshaderstore), billbshaderstore, GL_DYNAMIC_DRAW);

        // position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, (10 * sizeof(GLfloat)), (void*)0);
        glEnableVertexAttribArray(0);
        // tex uv
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, (10 * sizeof(GLfloat)), (void*)(3 * sizeof(GLfloat)));
        glEnableVertexAttribArray(1);
        // colour
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, (10 * sizeof(GLfloat)), (void*)(5 * sizeof(GLfloat)));
        glEnableVertexAttribArray(2);
        // texid
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, (10 * sizeof(GLfloat)), (void*)(9 * sizeof(GLfloat)));
        glEnableVertexAttribArray(3);

        checkglerror();

    }

    // update buffer
    glBindBuffer(GL_ARRAY_BUFFER, billbVBO);

    checkglerror();

    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(twodverts) * billbvertscnt, billbshaderstore);
    checkglerror();

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(billbVAO);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);

    glUseProgram(passthroughshader.ID);
    checkglerror();
    // set sampler to texure0
    glUniform1i(glGetUniformLocation(passthroughshader.ID, "texture0"), GLint(0));

    //// set projection
    glUniformMatrix4fv(glGetUniformLocation(passthroughshader.ID, "projection"), 1, GL_FALSE, &projmat[0][0]);
    //// set view
    glUniformMatrix4fv(glGetUniformLocation(passthroughshader.ID, "view"), 1, GL_FALSE, &viewmat[0][0]);
    checkglerror();


    //num to draw i billbvertscnt / 6
    //int start = uNumBillboardsToDraw - 1 - billbdrawn;

    for (int offset = 0; offset < billbvertscnt; offset += 6) {
        //int offset = (start - i) * 6;
        //if (offset > billbvertscnt) break;

        // blending
        //if (pBillboardRenderListD3D[i].opacity != RenderBillboardD3D::NoBlend) {
            SetBillboardBlendOptions(RenderBillboardD3D::OpacityType(billbopac[offset]));
        //}
        // set texture
        glBindTexture(GL_TEXTURE_2D, billbshaderstore[offset].texid);
        // draw
        glDrawArrays(GL_TRIANGLES, offset, 6);
        checkglerror();
        drawcalls++;
    }


    checkglerror();
    glUseProgram(0);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glDisableVertexAttribArray(3);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(0);

    billbvertscnt = 0;
    checkglerror();




    //for (int i = uNumBillboardsToDraw - 1; i >= 0; --i) {
    //    if (pBillboardRenderListD3D[i].opacity != RenderBillboardD3D::NoBlend) {
    //        SetBillboardBlendOptions(pBillboardRenderListD3D[i].opacity);
    //    }

    //    auto texture = (TextureOpenGL*)pBillboardRenderListD3D[i].texture;

    //    int thistex = 0;
    //    int texwidth = 1, texheight = 1;

    //    if (texture) {
    //        thistex = texture->GetOpenGlTexture();
    //        texwidth = texture->GetWidth();
    //        texheight = texture->GetHeight();
    //    }

    //    if (thistex != lastex) {
    //        glBindTexture(GL_TEXTURE_2D, thistex);
    //        lastex = thistex;
    //        changes++;
    //    }

    //    glBegin(GL_TRIANGLE_FAN);
    //    {
    //        auto billboard = &pBillboardRenderListD3D[i];
    //        auto b = &pBillboardRenderList[i];

    //        // since OpenGL 1.0 can't mirror texture borders, we should offset
    //        // UV to avoid black edges
    //        //billboard->pQuads[0].texcoord.x += 0.5f / texwidth;
    //        //billboard->pQuads[0].texcoord.y += 0.5f / texheight;
    //        //billboard->pQuads[1].texcoord.x += 0.5f / texwidth;
    //        //billboard->pQuads[1].texcoord.y -= 0.5f / texheight;
    //        //billboard->pQuads[2].texcoord.x -= 0.5f / texwidth;
    //        //billboard->pQuads[2].texcoord.y -= 0.5f / texheight;
    //        //billboard->pQuads[3].texcoord.x -= 0.5f / texwidth;
    //        //billboard->pQuads[3].texcoord.y += 0.5f / texheight;

    //        for (unsigned int j = 0; j < billboard->uNumVertices; ++j) {
    //            glColor4f(
    //                ((billboard->pQuads[j].diffuse >> 16) & 0xFF) / 255.0f,
    //                ((billboard->pQuads[j].diffuse >> 8) & 0xFF) / 255.0f,
    //                ((billboard->pQuads[j].diffuse >> 0) & 0xFF) / 255.0f,
    //                1.0f);

    //            glTexCoord2f(billboard->pQuads[j].texcoord.x,
    //                billboard->pQuads[j].texcoord.y);

    //            float oneoz = 1. / (billboard->screen_space_z);
    //            float oneon = 1. / (pIndoorCameraD3D->GetNearClip() + 4.0);
    //            float oneof = 1. / pIndoorCameraD3D->GetFarClip();

    //            glVertex3f(
    //                billboard->pQuads[j].pos.x,
    //                billboard->pQuads[j].pos.y,
    //                (oneoz - oneon) / (oneof - oneon));  // depth is  non linear  proportional to reciprocal of distance
    //        }
    //        drawcalls++;
    //    }
    //    drawcalls++;
    //    glEnd();

    //}

    //checkglerror();

    // uNumBillboardsToDraw = 0;


    //if (config->is_using_fog) {
    //    SetUsingFog(false);
    //    glEnable(GL_FOG);
    //    glFogi(GL_FOG_MODE, GL_EXP);

    //    GLfloat fog_color[] = {((GetLevelFogColor() >> 16) & 0xFF) / 255.0f,
    //                           ((GetLevelFogColor() >> 8) & 0xFF) / 255.0f,
    //                           ((GetLevelFogColor() >> 0) & 0xFF) / 255.0f,
    //                           1.0f};
    //    glFogfv(GL_FOG_COLOR, fog_color);
    //}



    checkglerror();

}

//----- (004A1DA8) --------------------------------------------------------
void RenderOpenGL::SetBillboardBlendOptions(RenderBillboardD3D::OpacityType a1) {
    checkglerror();
    switch (a1) {
        case RenderBillboardD3D::Transparent: {
            /*if (config->is_using_fog) {
                SetUsingFog(false);
                glEnable(GL_FOG);
                glFogi(GL_FOG_MODE, GL_EXP);

                GLfloat fog_color[] = {
                    ((GetLevelFogColor() >> 16) & 0xFF) / 255.0f,
                    ((GetLevelFogColor() >> 8) & 0xFF) / 255.0f,
                    ((GetLevelFogColor() >> 0) & 0xFF) / 255.0f, 1.0f};
                glFogfv(GL_FOG_COLOR, fog_color);
            }*/

            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        } break;

        case RenderBillboardD3D::Opaque_1:
        case RenderBillboardD3D::Opaque_2:
        case RenderBillboardD3D::Opaque_3: {
            /*if (config->is_using_specular) {
                if (!config->is_using_fog) {
                    SetUsingFog(true);
                    glDisable(GL_FOG);
                }
            }*/

            glBlendFunc(GL_ONE, GL_ONE);  // 1 0
        } break;

        default:
            log->Warning(
                "SetBillboardBlendOptions: invalid opacity type (%u)", a1);
            assert(false);
            break;
    }

    checkglerror();
}

void RenderOpenGL::SetUIClipRect(unsigned int x, unsigned int y, unsigned int z,
                                 unsigned int w) {
    this->clip_x = x;
    this->clip_y = y;
    this->clip_z = z;
    this->clip_w = w;
    glScissor(x, this->window->GetHeight() -w, z-x, w-y);  // invert glscissor co-ords 0,0 is BL

    checkglerror();
}

void RenderOpenGL::ResetUIClipRect() {
    this->SetUIClipRect(0, 0, this->window->GetWidth(), this->window->GetHeight());
}

void RenderOpenGL::PresentBlackScreen() {
    BeginScene();
    ClearBlack();
    EndScene();
    Present();
}

void RenderOpenGL::BeginScene() {
    // Setup for 2D

    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    _set_ortho_projection();
    _set_ortho_modelview();

    checkglerror();
}

void RenderOpenGL::EndScene() {
    // blank in d3d
}



void RenderOpenGL::DrawTextureAlphaNew(float inx, float iny, Image *img) {
    DrawTextureNew(inx, iny, img);
    return;
}



void RenderOpenGL::DrawTextureNew(float inx, float iny, Image *tex) {
    if (!tex) __debugbreak();

    int width = tex->GetWidth();
    int height = tex->GetHeight();

    int x = inx * window->GetWidth();
    int y = iny * window->GetHeight();
    int z = x + width;
    int w = y + height;

    // check bounds
    if (x >= (int)window->GetWidth() || x >= this->clip_z || y >= (int)window->GetHeight() || y >= this->clip_w) return;
    // check for overlap
    if (!(this->clip_x < z && this->clip_z > x && this->clip_y < w && this->clip_w > y)) return;

    auto texture = (TextureOpenGL*)tex;
    float gltexid = texture->GetOpenGlTexture();

    int drawx = std::max(x, this->clip_x);
    int drawy = std::max(y, this->clip_y);
    int draww = std::min(w, this->clip_w);
    int drawz = std::min(z, this->clip_z);

    float texx = (drawx - x) / float(width);
    float texy = (drawy - y) / float(height);
    float texz = (width - (z - drawz)) / float(width);
    float texw = (height - (w - draww)) / float(height);

    // 0 1 2 / 0 2 3

    twodshaderstore[twodvertscnt].x = drawx;
    twodshaderstore[twodvertscnt].y = drawy;
    twodshaderstore[twodvertscnt].z = 0;
    twodshaderstore[twodvertscnt].u = texx;
    twodshaderstore[twodvertscnt].v = texy;
    twodshaderstore[twodvertscnt].r = 1;
    twodshaderstore[twodvertscnt].g = 1;
    twodshaderstore[twodvertscnt].b = 1;
    twodshaderstore[twodvertscnt].a = 1;
    twodshaderstore[twodvertscnt].texid = gltexid;
    twodvertscnt++;

    twodshaderstore[twodvertscnt].x = drawz;
    twodshaderstore[twodvertscnt].y = drawy;
    twodshaderstore[twodvertscnt].z = 0;
    twodshaderstore[twodvertscnt].u = texz;
    twodshaderstore[twodvertscnt].v = texy;
    twodshaderstore[twodvertscnt].r = 1;
    twodshaderstore[twodvertscnt].g = 1;
    twodshaderstore[twodvertscnt].b = 1;
    twodshaderstore[twodvertscnt].a = 1;
    twodshaderstore[twodvertscnt].texid = gltexid;
    twodvertscnt++;

    twodshaderstore[twodvertscnt].x = drawz;
    twodshaderstore[twodvertscnt].y = draww;
    twodshaderstore[twodvertscnt].z = 0;
    twodshaderstore[twodvertscnt].u = texz;
    twodshaderstore[twodvertscnt].v = texw;
    twodshaderstore[twodvertscnt].r = 1;
    twodshaderstore[twodvertscnt].g = 1;
    twodshaderstore[twodvertscnt].b = 1;
    twodshaderstore[twodvertscnt].a = 1;
    twodshaderstore[twodvertscnt].texid = gltexid;
    twodvertscnt++;

    ////////////////////////////////

    twodshaderstore[twodvertscnt].x = drawx;
    twodshaderstore[twodvertscnt].y = drawy;
    twodshaderstore[twodvertscnt].z = 0;
    twodshaderstore[twodvertscnt].u = texx;
    twodshaderstore[twodvertscnt].v = texy;
    twodshaderstore[twodvertscnt].r = 1;
    twodshaderstore[twodvertscnt].g = 1;
    twodshaderstore[twodvertscnt].b = 1;
    twodshaderstore[twodvertscnt].a = 1;
    twodshaderstore[twodvertscnt].texid = gltexid;
    twodvertscnt++;

    twodshaderstore[twodvertscnt].x = drawz;
    twodshaderstore[twodvertscnt].y = draww;
    twodshaderstore[twodvertscnt].z = 0;
    twodshaderstore[twodvertscnt].u = texz;
    twodshaderstore[twodvertscnt].v = texw;
    twodshaderstore[twodvertscnt].r = 1;
    twodshaderstore[twodvertscnt].g = 1;
    twodshaderstore[twodvertscnt].b = 1;
    twodshaderstore[twodvertscnt].a = 1;
    twodshaderstore[twodvertscnt].texid = gltexid;
    twodvertscnt++;

    twodshaderstore[twodvertscnt].x = drawx;
    twodshaderstore[twodvertscnt].y = draww;
    twodshaderstore[twodvertscnt].z = 0;
    twodshaderstore[twodvertscnt].u = texx;
    twodshaderstore[twodvertscnt].v = texw;
    twodshaderstore[twodvertscnt].r = 1;
    twodshaderstore[twodvertscnt].g = 1;
    twodshaderstore[twodvertscnt].b = 1;
    twodshaderstore[twodvertscnt].a = 1;
    twodshaderstore[twodvertscnt].texid = gltexid;
    twodvertscnt++;


    checkglerror();

    // blank over same bit of this render_target_rgb to stop text overlaps
    for (int ys = drawy; ys < draww; ys++) {
        memset(this->render_target_rgb +(ys * window->GetWidth() + drawx), 0x00000000, (drawz - drawx) * 4);
    }

    if (twodvertscnt > 490) drawtwodverts();

    return;
}

void RenderOpenGL::drawtwodverts() {
    if (!twodvertscnt) return;
    int savex = this->clip_x;
    int savey = this->clip_y;
    int savez = this->clip_z;
    int savew = this->clip_w;
    render->ResetUIClipRect();

    if (twodVAO == 0) {
        glGenVertexArrays(1, &twodVAO);
        glGenBuffers(1, &twodVBO);

        glBindVertexArray(twodVAO);
        glBindBuffer(GL_ARRAY_BUFFER, twodVBO);

        glBufferData(GL_ARRAY_BUFFER, sizeof(twodshaderstore), twodshaderstore, GL_DYNAMIC_DRAW);

        // position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, (10 * sizeof(GLfloat)), (void*)0);
        glEnableVertexAttribArray(0);
        // tex uv
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, (10 * sizeof(GLfloat)), (void*)(3 * sizeof(GLfloat)));
        glEnableVertexAttribArray(1);
        // colour
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, (10 * sizeof(GLfloat)), (void*)(5 * sizeof(GLfloat)));
        glEnableVertexAttribArray(2);
        // texid
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, (10 * sizeof(GLfloat)), (void*)(9 * sizeof(GLfloat)));
        glEnableVertexAttribArray(3);


        checkglerror();
    }

    checkglerror();

    // update buffer
    glBindBuffer(GL_ARRAY_BUFFER, twodVBO);

    checkglerror();

    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(twodverts) * twodvertscnt, twodshaderstore);
    checkglerror();

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(twodVAO);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);

    glUseProgram(passthroughshader.ID);
    checkglerror();

    // glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    checkglerror();
    // set sampler to texure0
    glUniform1i(glGetUniformLocation(passthroughshader.ID, "texture0"), GLint(0));

    //// set projection
    glUniformMatrix4fv(glGetUniformLocation(passthroughshader.ID, "projection"), 1, GL_FALSE, &projmat[0][0]);
    //// set view
    glUniformMatrix4fv(glGetUniformLocation(passthroughshader.ID, "view"), 1, GL_FALSE, &viewmat[0][0]);
    checkglerror();
    for (int offset = 0; offset < twodvertscnt; offset += 6) {
        // set texture
        glBindTexture(GL_TEXTURE_2D, twodshaderstore[offset].texid);
        glDrawArrays(GL_TRIANGLES, offset, 6);
        drawcalls++;
    }
    checkglerror();
    glUseProgram(0);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glDisableVertexAttribArray(3);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(0);

    twodvertscnt = 0;
    checkglerror();
    render->SetUIClipRect(savex, savey, savez, savew);
}




void RenderOpenGL::DrawTextureCustomHeight(float inx, float iny, class Image *img, int custom_height) {
    unsigned __int16 *v6;  // esi@3
    unsigned int v8;       // eax@5
    unsigned int v11;      // eax@7
    unsigned int v12;      // ebx@8
    unsigned int v15;      // eax@14
    int v19;               // [sp+10h] [bp-8h]@3

    if (!img) return;

    unsigned int uOutX = inx * window->GetWidth();
    unsigned int uOutY = iny * window->GetHeight();

    int width = img->GetWidth();
    int height = std::min((int)img->GetHeight(), custom_height);
    v6 = (unsigned __int16 *)img->GetPixels(IMAGE_FORMAT_R5G6B5);

    // v5 = &this->pTargetSurface[uOutX + uOutY * this->uTargetSurfacePitch];
    v19 = width;
    // if (this->bClip)
    {
        if ((signed int)uOutX < (signed int)this->clip_x) {
            v8 = this->clip_x - uOutX;
            unsigned int v9 = uOutX - this->clip_x;
            v8 *= 2;
            width += v9;
            v6 = (unsigned __int16 *)((char *)v6 + v8);
            // v5 = (unsigned __int16 *)((char *)v5 + v8);
        }
        if ((signed int)uOutY < (signed int)this->clip_y) {
            v11 = this->clip_y - uOutY;
            v6 += v19 * v11;
            height += uOutY - this->clip_y;
            // v5 += this->uTargetSurfacePitch * v11;
        }
        v12 = std::max((unsigned int)this->clip_x, uOutX);
        if ((signed int)(width + v12) > (signed int)this->clip_z) {
            width = this->clip_z - std::max(this->clip_x, (int)uOutX);
        }
        v15 = std::max((unsigned int)this->clip_y, uOutY);
        if ((signed int)(v15 + height) > (signed int)this->clip_w) {
            height = this->clip_w - std::max(this->clip_y, (int)uOutY);
        }
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            WritePixel16(uOutX + x, uOutY + y, *v6);
            // *v5 = *v6;
            // ++v5;
            ++v6;
        }
        v6 += v19 - width;
        // v5 += this->uTargetSurfacePitch - v4;
    }
}

void RenderOpenGL::DrawText(int uOutX, int uOutY, uint8_t* pFontPixels,
                            unsigned int uCharWidth, unsigned int uCharHeight,
                            uint8_t* pFontPalette, uint16_t uFaceColor,
                            uint16_t uShadowColor) {
    // needs limits checks adding

    // Image *fonttemp = Image::Create(uCharWidth, uCharHeight, IMAGE_FORMAT_A8R8G8B8);
    // uint32_t *fontpix = (uint32_t*)fonttemp->GetPixels(IMAGE_FORMAT_A8R8G8B8);

    for (uint y = 0; y < uCharHeight; ++y) {
        for (uint x = 0; x < uCharWidth; ++x) {
            if (*pFontPixels) {
                uint16_t color = uShadowColor;
                if (*pFontPixels != 1) {
                    color = uFaceColor;
                }
                // fontpix[x + y * uCharWidth] = Color32(color);
                this->render_target_rgb[(uOutX+x)+(uOutY+y)*window->GetWidth()] = Color32(color);
            }
            ++pFontPixels;
        }
    }
    // render->DrawTextureAlphaNew(uOutX / 640., uOutY / 480., fonttemp);
    // fonttemp->Release();
}

void RenderOpenGL::DrawTextAlpha(int x, int y, unsigned char *font_pixels,
                                 int uCharWidth, unsigned int uFontHeight,
                                 uint8_t *pPalette,
                                 bool present_time_transparency) {
    // needs limits checks adding

    // Image *fonttemp = Image::Create(uCharWidth, uFontHeight, IMAGE_FORMAT_A8R8G8B8);
    // uint32_t *fontpix = (uint32_t *)fonttemp->GetPixels(IMAGE_FORMAT_A8R8G8B8);

    if (present_time_transparency) {
        for (unsigned int dy = 0; dy < uFontHeight; ++dy) {
            for (unsigned int dx = 0; dx < uCharWidth; ++dx) {
                uint16_t color = (*font_pixels)
                    ? pPalette[*font_pixels]
                    : teal_mask_16;  // transparent color 16bit
                              // render->uTargetGMask |
                              // render->uTargetBMask;
                this->render_target_rgb[(x + dx) + (y + dy) * window->GetWidth()] = Color32(color);
                // fontpix[dx + dy * uCharWidth] = Color32(color);
                ++font_pixels;
            }
        }
    } else {
        for (unsigned int dy = 0; dy < uFontHeight; ++dy) {
            for (unsigned int dx = 0; dx < uCharWidth; ++dx) {
                if (*font_pixels) {
                    uint8_t index = *font_pixels;
                    uint8_t r = pPalette[index * 3 + 0];
                    uint8_t g = pPalette[index * 3 + 1];
                    uint8_t b = pPalette[index * 3 + 2];
                    this->render_target_rgb[(x + dx) + (y + dy) * window->GetWidth()] = Color32(r, g, b);
                    // fontpix[dx + dy * uCharWidth] = Color32(r, g, b);
                }
                ++font_pixels;
            }
        }
    }
    // render->DrawTextureAlphaNew(x / 640., y / 480., fonttemp);
    // fonttemp->Release();
}

void RenderOpenGL::Present() {
    render->ResetUIClipRect();

    checkglerror();

    drawtwodverts();

    checkglerror();

    // flush 2d lines
    Drawlines();
    // filled rects??

    // 2d elements
    
    checkglerror();

    // screen overlay holds all text and changing images at the moment

    static Texture *screen_text_overlay = 0;

    if (!screen_text_overlay) {
        screen_text_overlay = render->CreateTexture_Blank(window->GetWidth(), window->GetHeight(), IMAGE_FORMAT_A8R8G8B8);


    }

    uint32_t* pix = (uint32_t*)screen_text_overlay->GetPixels(IMAGE_FORMAT_A8R8G8B8);
    unsigned int num_pixels = screen_text_overlay->GetWidth() * screen_text_overlay->GetHeight();
    unsigned int num_pixels_bytes = num_pixels * IMAGE_FORMAT_BytesPerPixel(IMAGE_FORMAT_A8R8G8B8);

    // update pixels
    memcpy(pix, this->render_target_rgb, num_pixels_bytes);

    checkglerror();
    // update texture
    render->Update_Texture(screen_text_overlay);

    checkglerror();
    // draw
    render->DrawTextureAlphaNew(0, 0, screen_text_overlay);
    checkglerror();
    drawtwodverts();
 

    checkglerror();
    window->OpenGlSwapBuffers();

    checkglerror();
}

// RenderVertexSoft ogl_draw_buildings_vertices[20];

/*

struct shaderverts {
    GLfloat x;
    GLfloat y;
    GLfloat z;
    GLfloat u;
    GLfloat v;
    GLfloat texunit;
    GLfloat texturelayer;
    GLfloat normx;
    GLfloat normy;
    GLfloat normz;
    GLfloat attribs;
};



shaderverts terrshaderstore[127 * 127 * 6] = {}

*/

shaderverts* outbuildshaderstore[16] = { nullptr };
int numoutbuildverts[16] = { 0 };

void RenderOpenGL::DrawBuildingsD3D() {

    glEnable(GL_CULL_FACE);

    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    _set_3d_projection_matrix();
    _set_3d_modelview_matrix();

    if (outbuildVAO[0] == 0) {


        // count all triangles
        // make out build shader store
        int verttotals = 0;
        
        for (int i = 0; i < 16; i++) {


            numoutbuildverts[i] = 0;

            //for (BSPModel& model : pOutdoor->pBModels) {
            //    //int reachable;
            //    //if (IsBModelVisible(&model, &reachable)) {
            //    //model.field_40 |= 1;
            //    if (!model.pFaces.empty()) {
            //        for (ODMFace& face : model.pFaces) {
            //            if (!face.Invisible()) {
            //                numoutbuildverts += 3 * (face.uNumVertices - 2);
            //            }
            //        }
            //    }
            //}

            free(outbuildshaderstore[i]);
            outbuildshaderstore[i] = nullptr;
            outbuildshaderstore[i] = (shaderverts*)malloc(sizeof(shaderverts) * 10000);

        }

        // reserve first 7 layers for water tiles in unit 0
        auto wtrtexture = this->hd_water_tile_anim[0];
        //terraintexmap.insert(std::make_pair("wtrtyl", terraintexmap.size()));
        //numterraintexloaded[0]++;
        outbuildtexturewidths[0] = wtrtexture->GetWidth();
        outbuildtextureheights[0] = wtrtexture->GetHeight();

        for (int buff = 0; buff < 7; buff++) {
            char container_name[64];
            sprintf(container_name, "HDWTR%03u", buff);

            outbuildtexmap.insert(std::make_pair(container_name, outbuildtexmap.size()));
            numoutbuildtexloaded[0]++;
        }





        for (BSPModel& model : pOutdoor->pBModels) {
            //int reachable;
            //if (IsBModelVisible(&model, &reachable)) {
                model.field_40 |= 1;
                if (!model.pFaces.empty()) {
                    for (ODMFace& face : model.pFaces) {
                        if (!face.Invisible()) {
                            //v53 = 0;
                            //auto poly = &array_77EC08[pODMRenderParams->uNumPolygons];

                            //poly->flags = 0;
                            //poly->field_32 = 0;
                            auto tex = face.GetTexture();

                            String texname = face.resourcename;
                            // gather up all texture and shaderverts data

                            //auto tile = pOutdoor->DoGetTile(x, y);

                            int texunit = 0;
                            int texlayer = 0;
                            int attribflags = 0;

                            if (face.uAttributes & FACE_IsFluid) attribflags |= 2;
                            if (face.uAttributes & FACE_INDOOR_SKY) attribflags |= 0x400;

                            if (face.uAttributes & FACE_FlowDown)
                                attribflags |= 0x400;
                            else if (face.uAttributes & FACE_FlowUp)
                                attribflags |= 0x800;

                            if (face.uAttributes & FACE_FlowRight)
                                attribflags |= 0x2000;
                            else if (face.uAttributes & FACE_FlowLeft)
                                attribflags |= 0x1000;

                            // check if tile->name is already in list
                            auto mapiter = outbuildtexmap.find(texname);
                            if (mapiter != outbuildtexmap.end()) {
                                // if so, extract unit and layer
                                int unitlayer = mapiter->second;
                                texlayer = unitlayer & 0xFF;
                                texunit = (unitlayer & 0xFF00) >> 8;
                            }
                            else if (texname == "wtrtyl") {
                                // water tile
                                texunit = 0;
                                texlayer = 0;
                            } else {
                                // else need to add it
                                auto thistexture = assets->GetBitmap(texname);
                                int width = thistexture->GetWidth();
                                int height = thistexture->GetHeight();
                                // check size to see what unit it needs
                                int i;
                                for (i = 0; i < 16; i++) {
                                    if ((outbuildtexturewidths[i] == width && outbuildtextureheights[i] == height) || outbuildtexturewidths[i] == 0) break;
                                }
                                if (i == 16) __debugbreak();

                                if (outbuildtexturewidths[i] == 0) {
                                    outbuildtexturewidths[i] = width;
                                    outbuildtextureheights[i] = height;
                                }

                                texunit = i;
                                texlayer = numoutbuildtexloaded[i];

                                // encode unit and layer together
                                int encode = (texunit << 8) | texlayer;

                                // intsert into tex map
                                outbuildtexmap.insert(std::make_pair(texname, encode));

                                numoutbuildtexloaded[i]++;
                                if (numoutbuildtexloaded[i] == 256) __debugbreak();
                            }

                            face.texunit = texunit;
                            face.texlayer = texlayer;

                            // load up verts here

                            for (int z = 0; z < (face.uNumVertices - 2); z++) {
                                // 123, 134, 145, 156..
                                shaderverts* thisvert = &outbuildshaderstore[texunit][numoutbuildverts[texunit]];

                                // copy first
                                thisvert->x = model.pVertices.pVertices[face.pVertexIDs[0]].x;
                                thisvert->y = model.pVertices.pVertices[face.pVertexIDs[0]].y;
                                thisvert->z = model.pVertices.pVertices[face.pVertexIDs[0]].z;
                                thisvert->u = face.pTextureUIDs[0] + face.sTextureDeltaU;
                                thisvert->v = face.pTextureVIDs[0] + face.sTextureDeltaV;
                                thisvert->texunit = texunit;
                                thisvert->texturelayer = texlayer;
                                thisvert->normx = face.pFacePlane.vNormal.x / 65536.0;
                                thisvert->normy = face.pFacePlane.vNormal.y / 65536.0;
                                thisvert->normz = face.pFacePlane.vNormal.z / 65536.0;
                                thisvert->attribs = attribflags;
                                thisvert++;

                                // copy other two (z+1)(z+2)
                                for (uint i = 1; i < 3; ++i) {
                                    thisvert->x = model.pVertices.pVertices[face.pVertexIDs[z + i]].x;
                                    thisvert->y = model.pVertices.pVertices[face.pVertexIDs[z + i]].y;
                                    thisvert->z = model.pVertices.pVertices[face.pVertexIDs[z + i]].z;
                                    thisvert->u = face.pTextureUIDs[z + i] + face.sTextureDeltaU;
                                    thisvert->v = face.pTextureVIDs[z + i] + face.sTextureDeltaV;
                                    thisvert->texunit = texunit;
                                    thisvert->texturelayer = texlayer;
                                    thisvert->normx = face.pFacePlane.vNormal.x / 65536.0;
                                    thisvert->normy = face.pFacePlane.vNormal.y / 65536.0;
                                    thisvert->normz = face.pFacePlane.vNormal.z / 65536.0;
                                    thisvert->attribs = attribflags;
                                    thisvert++;
                                }

                                numoutbuildverts[texunit] += 3;
                                assert(numoutbuildverts[texunit] <= 9999);
                            }




                        }
                    }
                }
            //}
        }

        for (int l = 0; l < 16; l++) {

            glGenVertexArrays(1, &outbuildVAO[l]);
            glGenBuffers(1, &outbuildVBO[l]);

            glBindVertexArray(outbuildVAO[l]);
            glBindBuffer(GL_ARRAY_BUFFER, outbuildVBO[l]);

            glBufferData(GL_ARRAY_BUFFER, sizeof(shaderverts) * 10000, outbuildshaderstore[l], GL_DYNAMIC_DRAW);

            // position attribute
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, (11 * sizeof(GLfloat)), (void*)0);
            glEnableVertexAttribArray(0);
            // tex uv attribute
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, (11 * sizeof(GLfloat)), (void*)(3 * sizeof(GLfloat)));
            glEnableVertexAttribArray(1);
            // tex unit attribute
            // tex array layer attribute
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, (11 * sizeof(GLfloat)), (void*)(5 * sizeof(GLfloat)));
            glEnableVertexAttribArray(2);
            // normals
            glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, (11 * sizeof(GLfloat)), (void*)(7 * sizeof(GLfloat)));
            glEnableVertexAttribArray(3);
            // attribs - not used here yet
            glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, (11 * sizeof(GLfloat)), (void*)(10 * sizeof(GLfloat)));
            glEnableVertexAttribArray(4);


            checkglerror();

        }


        // texture set up

        // loop over all units
        for (int unit = 0; unit < 16; unit++) {

            assert(numoutbuildtexloaded[unit] <= 256);
            // skip if textures are empty
            if (numoutbuildtexloaded[unit] == 0) continue;

            glGenTextures(1, &outbuildtextures[unit]);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D_ARRAY, outbuildtextures[unit]);

            glTexStorage3D(GL_TEXTURE_2D_ARRAY, 4, GL_RGBA8, outbuildtexturewidths[unit], outbuildtextureheights[unit], numoutbuildtexloaded[unit]);

            std::map<std::string, int>::iterator it = outbuildtexmap.begin();
            while (it != outbuildtexmap.end()) {
                // skip if wtrtyl
                //if ((it->first).substr(0, 6) == "wtrtyl") {
                //    std::cout << "skipped  " << it->first << std::endl;
                 //   it++;
                 //   continue;
                //}

                int comb = it->second;
                int tlayer = comb & 0xFF;
                int tunit = (comb & 0xFF00) >> 8;

                if (tunit == unit) {

                    // get texture
                    auto texture = assets->GetBitmap(it->first);

                    std::cout << "loading  " << it->first << "   into unit " << tunit << " and pos " << tlayer << std::endl;

                    glTexSubImage3D(GL_TEXTURE_2D_ARRAY,
                        0,
                        0, 0, tlayer,
                        outbuildtexturewidths[unit], outbuildtextureheights[unit], 1,
                        GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        texture->GetPixels(IMAGE_FORMAT_R8G8B8A8));

                    //numterraintexloaded[0]++;
                }

                it++;
            }

            //iterate through terrain tex map
            //ignore wtrtyl
            //laod in

            //auto tile = pOutdoor->DoGetTile(0, 0);
            //bool border = tile->IsWaterBorderTile();


            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

            glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

            checkglerror();


        }




        //outbuildVAO = 1;
        //__debugbreak();
    } else {
        // else update verts - blank store
        for (int i = 0; i < 16; i++) {
            numoutbuildverts[i] = 0;
        }

        //String texname;

        for (BSPModel& model : pOutdoor->pBModels) {
            int reachable;
            if (IsBModelVisible(&model, &reachable)) {
            //if (model.index == 35) continue;
                model.field_40 |= 1;
                if (!model.pFaces.empty()) {
                    for (ODMFace& face : model.pFaces) {
                        if (!face.Invisible()) {

                            //auto tex = face.GetTexture();

                            //texname = face.resourcename;
                            // gather up all texture and shaderverts data

                            //auto tile = pOutdoor->DoGetTile(x, y);

                            int texunit = face.texunit;
                            int texlayer = face.texlayer;
                            int attribflags = 0;

                            if (face.uAttributes & FACE_IsFluid) attribflags |= 2;
                            if (face.uAttributes & FACE_INDOOR_SKY) attribflags |= 0x400;

                            if (face.uAttributes & FACE_FlowDown)
                                attribflags |= 0x400;
                            else if (face.uAttributes & FACE_FlowUp)
                                attribflags |= 0x800;

                            if (face.uAttributes & FACE_FlowRight)
                                attribflags |= 0x2000;
                            else if (face.uAttributes & FACE_FlowLeft)
                                attribflags |= 0x1000;

                            if (face.uAttributes & (FACE_OUTLINED | FACE_IsSecret))
                                attribflags |= FACE_OUTLINED;

                           // load up verts here

                            for (int z = 0; z < (face.uNumVertices - 2); z++) {
                                // 123, 134, 145, 156..
                                shaderverts* thisvert = &outbuildshaderstore[texunit][numoutbuildverts[texunit]];

                                // copy first
                                thisvert->x = model.pVertices.pVertices[face.pVertexIDs[0]].x;
                                thisvert->y = model.pVertices.pVertices[face.pVertexIDs[0]].y;
                                thisvert->z = model.pVertices.pVertices[face.pVertexIDs[0]].z;
                                thisvert->u = face.pTextureUIDs[0] + face.sTextureDeltaU;
                                thisvert->v = face.pTextureVIDs[0] + face.sTextureDeltaV;
                                thisvert->texunit = texunit;
                                thisvert->texturelayer = texlayer;
                                thisvert->normx = face.pFacePlane.vNormal.x / 65536.0;
                                thisvert->normy = face.pFacePlane.vNormal.y / 65536.0;
                                thisvert->normz = face.pFacePlane.vNormal.z / 65536.0;
                                thisvert->attribs = attribflags;
                                thisvert++;

                                // copy other two (z+1)(z+2)
                                for (uint i = 1; i < 3; ++i) {
                                    thisvert->x = model.pVertices.pVertices[face.pVertexIDs[z + i]].x;
                                    thisvert->y = model.pVertices.pVertices[face.pVertexIDs[z + i]].y;
                                    thisvert->z = model.pVertices.pVertices[face.pVertexIDs[z + i]].z;
                                    thisvert->u = face.pTextureUIDs[z + i] + face.sTextureDeltaU;
                                    thisvert->v = face.pTextureVIDs[z + i] + face.sTextureDeltaV;
                                    thisvert->texunit = texunit;
                                    thisvert->texturelayer = texlayer;
                                    thisvert->normx = face.pFacePlane.vNormal.x / 65536.0;
                                    thisvert->normy = face.pFacePlane.vNormal.y / 65536.0;
                                    thisvert->normz = face.pFacePlane.vNormal.z / 65536.0;
                                    thisvert->attribs = attribflags;
                                    thisvert++;
                                }

                                numoutbuildverts[texunit] += 3;
                                assert(numoutbuildverts[texunit] <= 9999);
                            }
                        }
                    }
                }
            }
        }
        checkglerror();

        for (int l = 0; l < 16; l++) {
            // update buffers
            if (numoutbuildverts[l]) {
                
                glBindBuffer(GL_ARRAY_BUFFER, outbuildVBO[l]);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(shaderverts) * numoutbuildverts[l], outbuildshaderstore[l]);
                checkglerror();
            }
        }

        checkglerror();
        glBindBuffer(GL_ARRAY_BUFFER, 0);

    }

    







    // terrain debug
    if (engine->config->debug_terrain)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);


    ////


    glUseProgram(outbuildshader.ID);

    // set sampler to texure0
    //glUniform1i(0, 0); ??? 

    //logger->Info("after");
    //// set projection
    glUniformMatrix4fv(glGetUniformLocation(outbuildshader.ID, "projection"), 1, GL_FALSE, &projmat[0][0]);
    //// set view
    glUniformMatrix4fv(glGetUniformLocation(outbuildshader.ID, "view"), 1, GL_FALSE, &viewmat[0][0]);

    glUniform1i(glGetUniformLocation(outbuildshader.ID, "waterframe"), GLint(this->hd_water_current_frame));
    glUniform1i(glGetUniformLocation(outbuildshader.ID, "flowtimer"), GLint(OS_GetTime() >> 4));

    GLfloat camera[3];
    camera[0] = (float)(pParty->vPosition.x - pParty->y_rotation_granularity * cosf(2 * pi_double * pParty->sRotationZ / 2048.0));
    camera[1] = (float)(pParty->vPosition.y - pParty->y_rotation_granularity * sinf(2 * pi_double * pParty->sRotationZ / 2048.0));
    camera[2] = (float)(pParty->vPosition.z + pParty->sEyelevel);
    glUniform3fv(glGetUniformLocation(outbuildshader.ID, "CameraPos"), 1, &camera[0]);


    // lighting stuff
    GLfloat sunvec[3];
    sunvec[0] = (float)pOutdoor->vSunlight.x / 65536.0;
    sunvec[1] = (float)pOutdoor->vSunlight.y / 65536.0;
    sunvec[2] = (float)pOutdoor->vSunlight.z / 65536.0;

    float ambient = pParty->uCurrentMinute + pParty->uCurrentHour * 60.0;  // 0 - > 1439
    ambient = 0.15 + (sinf(((ambient - 360.0) * 2 * pi_double) / 1440) + 1) * 0.27;

    float diffuseon = pWeather->bNight ? 0 : 1;

    glUniform3fv(glGetUniformLocation(outbuildshader.ID, "sun.direction"), 1, &sunvec[0]);
    glUniform3f(glGetUniformLocation(outbuildshader.ID, "sun.ambient"), ambient, ambient, ambient);
    glUniform3f(glGetUniformLocation(outbuildshader.ID, "sun.diffuse"), diffuseon * (ambient + 0.3), diffuseon * (ambient + 0.3), diffuseon * (ambient + 0.3));
    glUniform3f(glGetUniformLocation(outbuildshader.ID, "sun.specular"), diffuseon * 1.0, diffuseon * 0.8, 0.0);

    if (pParty->armageddon_timer) {
        glUniform3f(glGetUniformLocation(terrainshader.ID, "sun.ambient"), 1.0, 0, 0);
        glUniform3f(glGetUniformLocation(terrainshader.ID, "sun.diffuse"), 1.0, 0, 0);
        glUniform3f(glGetUniformLocation(terrainshader.ID, "sun.specular"), 0, 0, 0);
    }

    // point lights - fspointlights
    /*
        lightingShader.setVec3("pointLights[0].position", pointLightPositions[0]);
        lightingShader.setVec3("pointLights[0].ambient", 0.05f, 0.05f, 0.05f);
        lightingShader.setVec3("pointLights[0].diffuse", 0.8f, 0.8f, 0.8f);
        lightingShader.setVec3("pointLights[0].specular", 1.0f, 1.0f, 1.0f);
        lightingShader.setFloat("pointLights[0].constant", 1.0f);
        lightingShader.setFloat("pointLights[0].linear", 0.09);
        lightingShader.setFloat("pointLights[0].quadratic", 0.032);
    */

    // test torchlight
    float torchradius = 0;
    if (!diffuseon) {
        int rangemult = 1;
        if (pParty->pPartyBuffs[PARTY_BUFF_TORCHLIGHT].Active())
            rangemult = pParty->pPartyBuffs[PARTY_BUFF_TORCHLIGHT].uPower;
        torchradius = float(rangemult) * 1024.0;
    }

    glUniform3fv(glGetUniformLocation(outbuildshader.ID, "fspointlights[0].position"), 1, &camera[0]);
    glUniform3f(glGetUniformLocation(outbuildshader.ID, "fspointlights[0].ambient"), 0.85, 0.85, 0.85);
    glUniform3f(glGetUniformLocation(outbuildshader.ID, "fspointlights[0].diffuse"), 0.85, 0.85, 0.85);
    glUniform3f(glGetUniformLocation(outbuildshader.ID, "fspointlights[0].specular"), 0, 0, 1);
    //glUniform1f(glGetUniformLocation(outbuildshader.ID, "fspointlights[0].constant"), .81);
    //glUniform1f(glGetUniformLocation(outbuildshader.ID, "fspointlights[0].linear"), 0.003);
    //glUniform1f(glGetUniformLocation(outbuildshader.ID, "fspointlights[0].quadratic"), 0.000007);
    glUniform1f(glGetUniformLocation(outbuildshader.ID, "fspointlights[0].radius"), torchradius);


    // rest of lights stacking
    GLuint num_lights = 1;

    for (int i = 0; i < pMobileLightsStack->uNumLightsActive; ++i) {
        if (num_lights >= 20) break;

        String slotnum = std::to_string(num_lights);
        auto test = pMobileLightsStack->pLights[i];

        float x = pMobileLightsStack->pLights[i].vPosition.x;
        float y = pMobileLightsStack->pLights[i].vPosition.y;
        float z = pMobileLightsStack->pLights[i].vPosition.z;

        float r = pMobileLightsStack->pLights[i].uLightColorR / 255.0;
        float g = pMobileLightsStack->pLights[i].uLightColorG / 255.0;
        float b = pMobileLightsStack->pLights[i].uLightColorB / 255.0;

        float lightrad = pMobileLightsStack->pLights[i].uRadius;

        glUniform1f(glGetUniformLocation(outbuildshader.ID, ("fspointlights[" + slotnum + "].type").c_str()), 2.0);
        glUniform3f(glGetUniformLocation(outbuildshader.ID, ("fspointlights[" + slotnum + "].position").c_str()), x, y, z);
        glUniform3f(glGetUniformLocation(outbuildshader.ID, ("fspointlights[" + slotnum + "].ambient").c_str()), r, g, b);
        glUniform3f(glGetUniformLocation(outbuildshader.ID, ("fspointlights[" + slotnum + "].diffuse").c_str()), r, g, b);
        glUniform3f(glGetUniformLocation(outbuildshader.ID, ("fspointlights[" + slotnum + "].specular").c_str()), r, g, b);
        //glUniform1f(glGetUniformLocation(outbuildshader.ID, "fspointlights[0].constant"), .81);
        //glUniform1f(glGetUniformLocation(outbuildshader.ID, "fspointlights[0].linear"), 0.003);
        //glUniform1f(glGetUniformLocation(outbuildshader.ID, "fspointlights[0].quadratic"), 0.000007);
        glUniform1f(glGetUniformLocation(outbuildshader.ID, ("fspointlights[" + slotnum + "].radius").c_str()), lightrad);

        num_lights++;

        //    StackLight_TerrainFace(
        //        (StationaryLight*)&pMobileLightsStack->pLights[i], pNormal,
        //        Light_tile_dist, VertList, uStripType, bLightBackfaces, &num_lights);
    }





    for (int i = 0; i < pStationaryLightsStack->uNumLightsActive; ++i) {
        if (num_lights >= 20) break;

        String slotnum = std::to_string(num_lights);
        auto test = pStationaryLightsStack->pLights[i];

        float x = test.vPosition.x;
        float y = test.vPosition.y;
        float z = test.vPosition.z;

        float r = test.uLightColorR / 255.0;
        float g = test.uLightColorG / 255.0;
        float b = test.uLightColorB / 255.0;

        float lightrad = test.uRadius;

        glUniform1f(glGetUniformLocation(outbuildshader.ID, ("fspointlights[" + slotnum + "].type").c_str()), 1.0);
        glUniform3f(glGetUniformLocation(outbuildshader.ID, ("fspointlights[" + slotnum + "].position").c_str()), x, y, z);
        glUniform3f(glGetUniformLocation(outbuildshader.ID, ("fspointlights[" + slotnum + "].ambient").c_str()), r, g, b);
        glUniform3f(glGetUniformLocation(outbuildshader.ID, ("fspointlights[" + slotnum + "].diffuse").c_str()), r, g, b);
        glUniform3f(glGetUniformLocation(outbuildshader.ID, ("fspointlights[" + slotnum + "].specular").c_str()), r, g, b);
        //glUniform1f(glGetUniformLocation(outbuildshader.ID, "fspointlights[0].constant"), .81);
        //glUniform1f(glGetUniformLocation(outbuildshader.ID, "fspointlights[0].linear"), 0.003);
        //glUniform1f(glGetUniformLocation(outbuildshader.ID, "fspointlights[0].quadratic"), 0.000007);
        glUniform1f(glGetUniformLocation(outbuildshader.ID, ("fspointlights[" + slotnum + "].radius").c_str()), lightrad);

        num_lights++;



        //    StackLight_TerrainFace(&pStationaryLightsStack->pLights[i], pNormal,
        //        Light_tile_dist, VertList, uStripType, bLightBackfaces,
        //        &num_lights);
    }


    // blank lights

    for (int blank = num_lights; blank < 20; blank++) {
        String slotnum = std::to_string(blank);
        glUniform1f(glGetUniformLocation(outbuildshader.ID, ("fspointlights[" + slotnum + "].type").c_str()), 0.0);
    }

    glUniform1i(glGetUniformLocation(outbuildshader.ID, "watertiles"), GLint(1));

    glActiveTexture(GL_TEXTURE0);

    for (int unit = 0; unit < 16; unit++) {


        // skip if textures are empty
        if (numoutbuildtexloaded[unit] > 0) {
            if (unit == 1) {
                glUniform1i(glGetUniformLocation(outbuildshader.ID, "watertiles"), GLint(0));
            }
            
            //if (numoutbuildverts[unit] > 0) {
                
                glBindTexture(GL_TEXTURE_2D_ARRAY, outbuildtextures[unit]);

                glBindVertexArray(outbuildVAO[unit]);
                glDrawArrays(GL_TRIANGLES, 0, (numoutbuildverts[unit]));
                drawcalls++;
            //}
        }

    }
    //logger->Info("vefore");

    //glBindVertexArray(outbuildVAO);
   // glEnableVertexAttribArray(0);
   // glEnableVertexAttribArray(1);
   // glEnableVertexAttribArray(2);
   // glEnableVertexAttribArray(3);
   // glEnableVertexAttribArray(4);




  //  glDrawArrays(GL_TRIANGLES, 0, (numoutbuildverts)); //numoutbuildverts
  //  drawcalls++;

    glUseProgram(0);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glDisableVertexAttribArray(3);
    glDisableVertexAttribArray(4);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    



    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, NULL);



    //end terrain debug
    if (engine->config->debug_terrain)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);



    checkglerror();




    // need to stack decals

    if (!decal_builder->bloodsplat_container->uNumBloodsplats) return;

    for (BSPModel& model : pOutdoor->pBModels) {
        int reachable;
        if (!IsBModelVisible(&model, &reachable)) {
            continue;
        }
        model.field_40 |= 1;
        if (model.pFaces.empty()) {
            continue;
        }

        for (ODMFace& face : model.pFaces) {
            if (face.Invisible()) {
                continue;
            }


            //////////////////////////////////////////////

            struct Polygon* poly = &array_77EC08[pODMRenderParams->uNumPolygons];
            poly->flags = 0;
            poly->field_32 = 0;

            // if (v53 == face.uNumVertices) poly->field_32 |= 1;
            poly->pODMFace = &face;
            poly->uNumVertices = face.uNumVertices;
            poly->field_59 = 5;

            int v51 = fixpoint_mul(-pOutdoor->vSunlight.x, face.pFacePlane.vNormal.x);
            int v53 = fixpoint_mul(-pOutdoor->vSunlight.y, face.pFacePlane.vNormal.y);
            int v52 = fixpoint_mul(-pOutdoor->vSunlight.z, face.pFacePlane.vNormal.z);
            poly->dimming_level = 20 - fixpoint_mul(20, v51 + v53 + v52);

            if (poly->dimming_level < 0) poly->dimming_level = 0;
            if (poly->dimming_level > 31) poly->dimming_level = 31;

            //float v51 = (-pOutdoor->vSunlight.x * face.pFacePlane.vNormal.x / 65536.0) / 65536.0;
            //float v53 = (-pOutdoor->vSunlight.y * face.pFacePlane.vNormal.y / 65536.0) / 65536.0;
            //float v52 = (-pOutdoor->vSunlight.z * face.pFacePlane.vNormal.z / 65536.0) / 65536.0;
            //poly->dimming_level = 20 - 20 * (v51 + v53 + v52);

            //if (poly->dimming_level < 0) poly->dimming_level = 0;
            //if (poly->dimming_level > 31) poly->dimming_level = 31;


            for (uint vertex_id = 1; vertex_id <= face.uNumVertices; vertex_id++) {
                array_73D150[vertex_id - 1].vWorldPosition.x =
                    model.pVertices.pVertices[face.pVertexIDs[vertex_id - 1]].x;
                array_73D150[vertex_id - 1].vWorldPosition.y =
                    model.pVertices.pVertices[face.pVertexIDs[vertex_id - 1]].y;
                array_73D150[vertex_id - 1].vWorldPosition.z =
                    model.pVertices.pVertices[face.pVertexIDs[vertex_id - 1]].z;
            }


            for (int vertex_id = 0; vertex_id < face.uNumVertices; ++vertex_id) {
                memcpy(&VertexRenderList[vertex_id], &array_73D150[vertex_id], sizeof(VertexRenderList[vertex_id]));
                VertexRenderList[vertex_id]._rhw = 1.0 / (array_73D150[vertex_id].vWorldViewPosition.x + 0.0000001);
            }


            float Light_tile_dist = 0.0;

 

            //static stru154 static_sub_0048034E_stru_154;


            if (ODMFace::IsBackfaceNotCulled(array_73D150, poly)) {
                face.bVisible = 1;
                poly->uBModelFaceID = face.index;
                poly->uBModelID = model.index;
                poly->pid =
                    PID(OBJECT_BModel, (face.index | (model.index << 6)));

                static stru154 static_RenderBuildingsD3D_stru_73C834;




                decal_builder->ApplyBloodSplat_OutdoorFace(&face);

                //lightmap_builder->StationaryLightsCount = 0;
                int v31 = 0;
                if (decal_builder->uNumSplatsThisFace > 0) {
                    //v31 = nearclip ? 3 : farclip != 0 ? 5 : 0;

                    // if (face.uAttributes & FACE_OUTLINED) __debugbreak();

                    static_RenderBuildingsD3D_stru_73C834.GetFacePlaneAndClassify(&face, &model.pVertices);
                    if (decal_builder->uNumSplatsThisFace > 0) {
                        decal_builder->BuildAndApplyDecals(
                            31 - poly->dimming_level, 2,
                            &static_RenderBuildingsD3D_stru_73C834,
                            face.uNumVertices, VertexRenderList, (char)v31,
                            -1);
                    }
                }             
            }
        }
    }

    return;
}

shaderverts* BSPshaderstore = NULL;
int numBSPverts = 0;

void RenderOpenGL::DrawIndoorBSP() {


    
    
        glEnable(GL_CULL_FACE);

    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    _set_ortho_projection(1);
    _set_ortho_modelview();

    _set_3d_projection_matrix();
    _set_3d_modelview_matrix();

    if (bspVAO == 0) {

        // lights setup
        int cntnosect = 0;

        for (int lightscnt = 0; lightscnt < pStationaryLightsStack->uNumLightsActive; ++lightscnt) {
            
            
            auto test = pStationaryLightsStack->pLights[lightscnt];


            // kludge for getting lights in  visible sectors
            pStationaryLightsStack->pLights[lightscnt].uSectorID = pIndoor->GetSector(test.vPosition.x, test.vPosition.y, test.vPosition.z);

            if (pStationaryLightsStack->pLights[lightscnt].uSectorID == 0) cntnosect++;
        }

        logger->Warning("%i lights - sector not found", cntnosect);

        
        // count triangles and create store
        int verttotals = 0;
        numBSPverts = 0;

        for (int count = 0; count < pIndoor->uNumFaces; count++) {
            auto face = &pIndoor->pFaces[count];
            if (face->Portal()) continue;
            if (!face->GetTexture()) continue;

            //if (face->uAttributes & FACE_IS_DOOR) continue;

            if (face->uNumVertices) {
                numBSPverts += 3 * (face->uNumVertices - 2);
            }
        }

        free(BSPshaderstore);
        BSPshaderstore = NULL;
        BSPshaderstore = (shaderverts*)malloc(sizeof(shaderverts) * numBSPverts);

        // reserve first 7 layers for water tiles in unit 0
        auto wtrtexture = this->hd_water_tile_anim[0];
        //terraintexmap.insert(std::make_pair("wtrtyl", terraintexmap.size()));
        //numterraintexloaded[0]++;
        bsptexturewidths[0] = wtrtexture->GetWidth();
        bsptextureheights[0] = wtrtexture->GetHeight();

        for (int buff = 0; buff < 7; buff++) {
            char container_name[64];
            sprintf(container_name, "HDWTR%03u", buff);

            bsptexmap.insert(std::make_pair(container_name, bsptexmap.size()));
            bsptexloaded[0]++;
        }


        for (int test = 0; test < pIndoor->uNumFaces; test++) {
            BLVFace* face = &pIndoor->pFaces[test];

            if (face->Portal()) continue;
            if (!face->GetTexture()) continue;
            //if (face->uAttributes & FACE_IS_DOOR) continue;

            //TextureOpenGL* tex = (TextureOpenGL*)face->GetTexture();
            String texname = face->resourcename;

            int texunit = 0;
            int texlayer = 0;
            int attribflags = face->uAttributes;

            // check if tile->name is already in list
            auto mapiter = bsptexmap.find(texname);
            if (mapiter != bsptexmap.end()) {
                // if so, extract unit and layer
                int unitlayer = mapiter->second;
                texlayer = unitlayer & 0xFF;
                texunit = (unitlayer & 0xFF00) >> 8;
            }
            else if (texname == "wtrtyl") {
                // water tile
                texunit = 0;
                texlayer = 0;
            }
            else {
                // else need to add it
                auto thistexture = assets->GetBitmap(texname);
                int width = thistexture->GetWidth();
                int height = thistexture->GetHeight();
                // check size to see what unit it needs
                int i;
                for (i = 0; i < 16; i++) {
                    if ((bsptexturewidths[i] == width && bsptextureheights[i] == height) || bsptexturewidths[i] == 0) break;
                }
                if (i == 16) __debugbreak();

                if (bsptexturewidths[i] == 0) {
                    bsptexturewidths[i] = width;
                    bsptextureheights[i] = height;
                }

                texunit = i;
                texlayer = bsptexloaded[i];

                // encode unit and layer together
                int encode = (texunit << 8) | texlayer;

                // intsert into tex map
                bsptexmap.insert(std::make_pair(texname, encode));

                bsptexloaded[i]++;
                if (bsptexloaded[i] == 256) __debugbreak();
            }


            // load up verts here

            for (int z = 0; z < (face->uNumVertices - 2); z++) {
                // 123, 134, 145, 156..
                shaderverts* thisvert = &BSPshaderstore[verttotals];

                /*
                for (uint i = 0; i < pFace->uNumVertices; ++i) {
            static_vertices_buff_in[i].vWorldPosition.x =
                pIndoor->pVertices[pFace->pVertexIDs[i]].x;
            static_vertices_buff_in[i].vWorldPosition.y =
                pIndoor->pVertices[pFace->pVertexIDs[i]].y;
            static_vertices_buff_in[i].vWorldPosition.z =
                pIndoor->pVertices[pFace->pVertexIDs[i]].z;
            static_vertices_buff_in[i].u = (signed short)pFace->pVertexUIDs[i];
            static_vertices_buff_in[i].v = (signed short)pFace->pVertexVIDs[i];
        }
                
                */


                /*
                Lights.pDeltaUV[0] = pIndoor->pFaceExtras[pIndoor->pFaces[uFaceID].uFaceExtraID].sTextureDeltaU;
    Lights.pDeltaUV[1] = pIndoor->pFaceExtras[pIndoor->pFaces[uFaceID].uFaceExtraID].sTextureDeltaV;
                */

                // copy first
                thisvert->x = pIndoor->pVertices[face->pVertexIDs[0]].x;
                thisvert->y = pIndoor->pVertices[face->pVertexIDs[0]].y;
                thisvert->z = pIndoor->pVertices[face->pVertexIDs[0]].z;
                thisvert->u = face->pVertexUIDs[0] + pIndoor->pFaceExtras[pIndoor->pFaces[test].uFaceExtraID].sTextureDeltaU  /*+ face->sTextureDeltaU*/;
                thisvert->v = face->pVertexVIDs[0] + pIndoor->pFaceExtras[pIndoor->pFaces[test].uFaceExtraID].sTextureDeltaV  /*+ face->sTextureDeltaV*/;
                thisvert->texunit = texunit;
                thisvert->texturelayer = texlayer;
                thisvert->normx = face->pFacePlane.vNormal.x / 65536.0;
                thisvert->normy = face->pFacePlane.vNormal.y / 65536.0;
                thisvert->normz = face->pFacePlane.vNormal.z / 65536.0;
                thisvert->attribs = attribflags;
                thisvert++;

                // copy other two (z+1)(z+2)
                for (uint i = 1; i < 3; ++i) {
                    thisvert->x = pIndoor->pVertices[face->pVertexIDs[z + i]].x;
                    thisvert->y = pIndoor->pVertices[face->pVertexIDs[z + i]].y;
                    thisvert->z = pIndoor->pVertices[face->pVertexIDs[z + i]].z;
                    thisvert->u = face->pVertexUIDs[z + i] + pIndoor->pFaceExtras[pIndoor->pFaces[test].uFaceExtraID].sTextureDeltaU  /*+ face->sTextureDeltaU*/;
                    thisvert->v = face->pVertexVIDs[z + i] + pIndoor->pFaceExtras[pIndoor->pFaces[test].uFaceExtraID].sTextureDeltaV  /*+ face->sTextureDeltaV*/;
                    thisvert->texunit = texunit;
                    thisvert->texturelayer = texlayer;
                    thisvert->normx = face->pFacePlane.vNormal.x / 65536.0;
                    thisvert->normy = face->pFacePlane.vNormal.y / 65536.0;
                    thisvert->normz = face->pFacePlane.vNormal.z / 65536.0;
                    thisvert->attribs = attribflags;
                    thisvert++;
                }

                verttotals += 3;
            }




        }


        glGenVertexArrays(1, &bspVAO);
        glGenBuffers(1, &bspVBO);

        glBindVertexArray(bspVAO);
        glBindBuffer(GL_ARRAY_BUFFER, bspVBO);

        glBufferData(GL_ARRAY_BUFFER, sizeof(shaderverts)* numBSPverts, BSPshaderstore, GL_DYNAMIC_DRAW);  // GL_STATIC_DRAW


        // position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, (11 * sizeof(GLfloat)), (void*)0);
        glEnableVertexAttribArray(0);
        // tex uv attribute
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, (11 * sizeof(GLfloat)), (void*)(3 * sizeof(GLfloat)));
        glEnableVertexAttribArray(1);
        // tex unit attribute
        // tex array layer attribute
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, (11 * sizeof(GLfloat)), (void*)(5 * sizeof(GLfloat)));
        glEnableVertexAttribArray(2);
        // normals
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, (11 * sizeof(GLfloat)), (void*)(7 * sizeof(GLfloat)));
        glEnableVertexAttribArray(3);
        // attribs - not used here yet
        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, (11 * sizeof(GLfloat)), (void*)(10 * sizeof(GLfloat)));
        glEnableVertexAttribArray(4);

        checkglerror();
                 
        // texture set up

        // loop over all units
        for (int unit = 0; unit < 16; unit++) {

            assert(bsptexloaded[unit] <= 256);
            // skip if textures are empty
            if (bsptexloaded[unit] == 0) continue;

            glGenTextures(1, &bsptextures[unit]);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D_ARRAY, bsptextures[unit]);

            glTexStorage3D(GL_TEXTURE_2D_ARRAY, 4, GL_RGBA8, bsptexturewidths[unit], bsptextureheights[unit], bsptexloaded[unit]);

            std::map<std::string, int>::iterator it = bsptexmap.begin();
            while (it != bsptexmap.end()) {
                

                int comb = it->second;
                int tlayer = comb & 0xFF;
                int tunit = (comb & 0xFF00) >> 8;

                if (tunit == unit) {

                    // get texture
                    auto texture = assets->GetBitmap(it->first);

                    std::cout << "loading  " << it->first << "   into unit " << tunit << " and pos " << tlayer << std::endl;

                    glTexSubImage3D(GL_TEXTURE_2D_ARRAY,
                        0,
                        0, 0, tlayer,
                        bsptexturewidths[unit], bsptextureheights[unit], 1,
                        GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        texture->GetPixels(IMAGE_FORMAT_R8G8B8A8));

                    //numterraintexloaded[0]++;
                }

                it++;
            }




            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

            glGenerateMipmap(GL_TEXTURE_2D_ARRAY);


            checkglerror();

        }



    } else {
        // update verts
        int verttotals = 0;
        for (int test = 0; test < pIndoor->uNumFaces; test++) {
            BLVFace* face = &pIndoor->pFaces[test];

            if (face->Portal()) continue;
            if (!face->GetTexture()) continue;
            //if (face->uAttributes & FACE_IS_DOOR) {
            //    int test = rand();
                // continue;
            //}




            // load up verts here

            for (int z = 0; z < (face->uNumVertices - 2); z++) {
                // 123, 134, 145, 156..
                shaderverts* thisvert = &BSPshaderstore[verttotals];

                
                // copy first
                thisvert->x = pIndoor->pVertices[face->pVertexIDs[0]].x;
                thisvert->y = pIndoor->pVertices[face->pVertexIDs[0]].y;
                thisvert->z = pIndoor->pVertices[face->pVertexIDs[0]].z;
                thisvert->u = face->pVertexUIDs[0] + pIndoor->pFaceExtras[pIndoor->pFaces[test].uFaceExtraID].sTextureDeltaU  /*+ face->sTextureDeltaU*/;
                thisvert->v = face->pVertexVIDs[0] + pIndoor->pFaceExtras[pIndoor->pFaces[test].uFaceExtraID].sTextureDeltaV  /*+ face->sTextureDeltaV*/;
                // thisvert->texunit = texunit;
                // thisvert->texturelayer = texlayer;
                // thisvert->normx = face->pFacePlane.vNormal.x / 65536.0;
                // thisvert->normy = face->pFacePlane.vNormal.y / 65536.0;
                // thisvert->normz = face->pFacePlane.vNormal.z / 65536.0;
                // thisvert->attribs = attribflags;
                thisvert++;

                // copy other two (z+1)(z+2)
                for (uint i = 1; i < 3; ++i) {
                    thisvert->x = pIndoor->pVertices[face->pVertexIDs[z + i]].x;
                    thisvert->y = pIndoor->pVertices[face->pVertexIDs[z + i]].y;
                    thisvert->z = pIndoor->pVertices[face->pVertexIDs[z + i]].z;
                    thisvert->u = face->pVertexUIDs[z + i] + pIndoor->pFaceExtras[pIndoor->pFaces[test].uFaceExtraID].sTextureDeltaU  /*+ face->sTextureDeltaU*/;
                    thisvert->v = face->pVertexVIDs[z + i] + pIndoor->pFaceExtras[pIndoor->pFaces[test].uFaceExtraID].sTextureDeltaV  /*+ face->sTextureDeltaV*/;
                    // thisvert->texunit = texunit;
                    // thisvert->texturelayer = texlayer;
                    // thisvert->normx = face->pFacePlane.vNormal.x / 65536.0;
                    // thisvert->normy = face->pFacePlane.vNormal.y / 65536.0;
                    // thisvert->normz = face->pFacePlane.vNormal.z / 65536.0;
                    // thisvert->attribs = attribflags;
                    thisvert++;
                }

                verttotals += 3;
            }




        }


        // update buffer
        glBindBuffer(GL_ARRAY_BUFFER, bspVBO);

        // glBufferData(GL_ARRAY_BUFFER, sizeof(shaderverts)* numBSPverts, BSPshaderstore, GL_STREAM_DRAW);  // GL_STATIC_DRAW
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(shaderverts)* numBSPverts, BSPshaderstore);

        checkglerror();

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }



    

    



    // terrain debug
    if (engine->config->debug_terrain)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);


    ////
    for (int unit = 0; unit < 16; unit++) {


        // skip if textures are empty
        if (bsptexloaded[unit] > 0) {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D_ARRAY, bsptextures[unit]);
        }

    }
    //logger->Info("vefore");

    glBindVertexArray(bspVAO);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
    glEnableVertexAttribArray(4);

    glUseProgram(bspshader.ID);

    // set sampler to texure0
    //glUniform1i(0, 0); ??? 

    //logger->Info("after");
    //// set projection
    glUniformMatrix4fv(glGetUniformLocation(bspshader.ID, "projection"), 1, GL_FALSE, &projmat[0][0]);
    //// set view
    glUniformMatrix4fv(glGetUniformLocation(bspshader.ID, "view"), 1, GL_FALSE, &viewmat[0][0]);

    glUniform1i(glGetUniformLocation(bspshader.ID, "waterframe"), GLint(this->hd_water_current_frame));
    glUniform1i(glGetUniformLocation(bspshader.ID, "flowtimer"), GLint(OS_GetTime() >> 4));

    GLfloat camera[3];
    camera[0] = (float)(pParty->vPosition.x - pParty->y_rotation_granularity * cosf(2 * pi_double * pParty->sRotationZ / 2048.0));
    camera[1] = (float)(pParty->vPosition.y - pParty->y_rotation_granularity * sinf(2 * pi_double * pParty->sRotationZ / 2048.0));
    camera[2] = (float)(pParty->vPosition.z + pParty->sEyelevel);
    glUniform3fv(glGetUniformLocation(bspshader.ID, "CameraPos"), 1, &camera[0]);


    // lighting stuff
    GLfloat sunvec[3];
    sunvec[0] = 0;  // (float)pOutdoor->vSunlight.x / 65536.0;
    sunvec[1] = 0;  // (float)pOutdoor->vSunlight.y / 65536.0;
    sunvec[2] = 0;  // (float)pOutdoor->vSunlight.z / 65536.0;

    
     int16_t mintest = 0;

    for (int i = 0; i < pIndoor->uNumSectors; i++) {
        mintest = std::max(mintest, pIndoor->pSectors[i].uMinAmbientLightLevel);
    }

    Lights.uCurrentAmbientLightLevel = (Lights.uDefaultAmbientLightLevel + mintest);

    float ambient = (248.0 - (Lights.uCurrentAmbientLightLevel << 3)) / 255.0;
        //pParty->uCurrentMinute + pParty->uCurrentHour * 60.0;  // 0 - > 1439
    // ambient = 0.15 + (sinf(((ambient - 360.0) * 2 * pi_double) / 1440) + 1) * 0.27;

    float diffuseon = 0;  // pWeather->bNight ? 0 : 1;

    glUniform3fv(glGetUniformLocation(bspshader.ID, "sun.direction"), 1, &sunvec[0]);
    glUniform3f(glGetUniformLocation(bspshader.ID, "sun.ambient"), ambient, ambient, ambient);
    glUniform3f(glGetUniformLocation(bspshader.ID, "sun.diffuse"), diffuseon * (ambient + 0.3), diffuseon * (ambient + 0.3), diffuseon * (ambient + 0.3));
    glUniform3f(glGetUniformLocation(bspshader.ID, "sun.specular"), diffuseon * 1.0, diffuseon * 0.8, 0.0);

    // point lights - fspointlights
    
        //lightingShader.setVec3("pointLights[0].position", pointLightPositions[0]);
        //lightingShader.setVec3("pointLights[0].ambient", 0.05f, 0.05f, 0.05f);
        //lightingShader.setVec3("pointLights[0].diffuse", 0.8f, 0.8f, 0.8f);
        //lightingShader.setVec3("pointLights[0].specular", 1.0f, 1.0f, 1.0f);
        //lightingShader.setFloat("pointLights[0].constant", 1.0f);
        //lightingShader.setFloat("pointLights[0].linear", 0.09);
        //lightingShader.setFloat("pointLights[0].quadratic", 0.032);
    

    //// test torchlight
    //float torchradius = 0;
    //if (!diffuseon) {
    //    int rangemult = 1;
    //    if (pParty->pPartyBuffs[PARTY_BUFF_TORCHLIGHT].Active())
    //        rangemult = pParty->pPartyBuffs[PARTY_BUFF_TORCHLIGHT].uPower;
    //    torchradius = float(rangemult) * 1024.0;
    //}

    //glUniform3fv(glGetUniformLocation(bspshader.ID, "fspointlights[0].position"), 1, &camera[0]);
    //glUniform3f(glGetUniformLocation(bspshader.ID, "fspointlights[0].ambient"), 0.85, 0.85, 0.85);
    //glUniform3f(glGetUniformLocation(bspshader.ID, "fspointlights[0].diffuse"), 0.85, 0.85, 0.85);
    //glUniform3f(glGetUniformLocation(bspshader.ID, "fspointlights[0].specular"), 0, 0, 1);
    ////glUniform1f(glGetUniformLocation(bspshader.ID, "fspointlights[0].constant"), .81);
    ////glUniform1f(glGetUniformLocation(bspshader.ID, "fspointlights[0].linear"), 0.003);
    ////glUniform1f(glGetUniformLocation(bspshader.ID, "fspointlights[0].quadratic"), 0.000007);
    //glUniform1f(glGetUniformLocation(bspshader.ID, "fspointlights[0].radius"), torchradius);


    // rest of lights stacking
    GLuint num_lights = 0;   // 1;

    for (int i = 0; i < pMobileLightsStack->uNumLightsActive; ++i) {
        if (num_lights >= 40) break;

        // test sector/ distance?

        String slotnum = std::to_string(num_lights);
        auto test = pMobileLightsStack->pLights[i];

        float x = pMobileLightsStack->pLights[i].vPosition.x;
        float y = pMobileLightsStack->pLights[i].vPosition.y;
        float z = pMobileLightsStack->pLights[i].vPosition.z;

        float r = pMobileLightsStack->pLights[i].uLightColorR / 255.0;
        float g = pMobileLightsStack->pLights[i].uLightColorG / 255.0;
        float b = pMobileLightsStack->pLights[i].uLightColorB / 255.0;

        float lightrad = pMobileLightsStack->pLights[i].uRadius;

        glUniform1f(glGetUniformLocation(bspshader.ID, ("fspointlights[" + slotnum + "].type").c_str()), 2.0);
        glUniform3f(glGetUniformLocation(bspshader.ID, ("fspointlights[" + slotnum + "].position").c_str()), x, y, z);
        glUniform3f(glGetUniformLocation(bspshader.ID, ("fspointlights[" + slotnum + "].ambient").c_str()), r, g, b);
        glUniform3f(glGetUniformLocation(bspshader.ID, ("fspointlights[" + slotnum + "].diffuse").c_str()), r, g, b);
        glUniform3f(glGetUniformLocation(bspshader.ID, ("fspointlights[" + slotnum + "].specular").c_str()), r, g, b);
        //glUniform1f(glGetUniformLocation(bspshader.ID, "fspointlights[0].constant"), .81);
        //glUniform1f(glGetUniformLocation(bspshader.ID, "fspointlights[0].linear"), 0.003);
        //glUniform1f(glGetUniformLocation(bspshader.ID, "fspointlights[0].quadratic"), 0.000007);
        glUniform1f(glGetUniformLocation(bspshader.ID, ("fspointlights[" + slotnum + "].radius").c_str()), lightrad);

        num_lights++;

        //    StackLight_TerrainFace(
        //        (StationaryLight*)&pMobileLightsStack->pLights[i], pNormal,
        //        Light_tile_dist, VertList, uStripType, bLightBackfaces, &num_lights);
    }



    // sort stationary by distance ??



    for (int i = 0; i < pStationaryLightsStack->uNumLightsActive; ++i) {
        if (num_lights >= 40) break;

        String slotnum = std::to_string(num_lights);
        auto test = pStationaryLightsStack->pLights[i];


        // kludge for getting lights in  visible sectors
        // int sector = pIndoor->GetSector(test.vPosition.x, test.vPosition.y, test.vPosition.z);
        // 
        // is this on the list
        bool onlist = false;
        for (uint i = 0; i < pBspRenderer->uNumVisibleNotEmptySectors; ++i) {
            int listsector = pBspRenderer->pVisibleSectorIDs_toDrawDecorsActorsEtcFrom[i];
            if (test.uSectorID == listsector) {
                onlist = true;
                break;
            }
        }
        if (!onlist) continue;



        float x = test.vPosition.x;
        float y = test.vPosition.y;
        float z = test.vPosition.z;

        float r = test.uLightColorR / 255.0;
        float g = test.uLightColorG / 255.0;
        float b = test.uLightColorB / 255.0;

        float lightrad = test.uRadius;

        glUniform1f(glGetUniformLocation(bspshader.ID, ("fspointlights[" + slotnum + "].type").c_str()), 1.0);
        glUniform3f(glGetUniformLocation(bspshader.ID, ("fspointlights[" + slotnum + "].position").c_str()), x, y, z);
        glUniform3f(glGetUniformLocation(bspshader.ID, ("fspointlights[" + slotnum + "].ambient").c_str()), r, g, b);
        glUniform3f(glGetUniformLocation(bspshader.ID, ("fspointlights[" + slotnum + "].diffuse").c_str()), r, g, b);
        glUniform3f(glGetUniformLocation(bspshader.ID, ("fspointlights[" + slotnum + "].specular").c_str()), r, g, b);
        //glUniform1f(glGetUniformLocation(bspshader.ID, "fspointlights[0].constant"), .81);
        //glUniform1f(glGetUniformLocation(bspshader.ID, "fspointlights[0].linear"), 0.003);
        //glUniform1f(glGetUniformLocation(bspshader.ID, "fspointlights[0].quadratic"), 0.000007);
        glUniform1f(glGetUniformLocation(bspshader.ID, ("fspointlights[" + slotnum + "].radius").c_str()), lightrad);

        num_lights++;



        //    StackLight_TerrainFace(&pStationaryLightsStack->pLights[i], pNormal,
        //        Light_tile_dist, VertList, uStripType, bLightBackfaces,
        //        &num_lights);
    }

    // logger->Info("Frame");

    // blank lights

    for (int blank = num_lights; blank < 40; blank++) {
        String slotnum = std::to_string(blank);
        glUniform1f(glGetUniformLocation(bspshader.ID, ("fspointlights[" + slotnum + "].type").c_str()), 0.0);
    }







    glDrawArrays(GL_TRIANGLES, 0, (numBSPverts)); //numoutbuildverts
    drawcalls++;

    glUseProgram(0);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glDisableVertexAttribArray(3);
    glDisableVertexAttribArray(4);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(0);



    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, NULL);



    //end terrain debug
    if (engine->config->debug_terrain)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);



    checkglerror();


    // stack decals start

    if (!decal_builder->bloodsplat_container->uNumBloodsplats) return;
    static stru154 FacePlaneHolder;
    static RenderVertexSoft static_vertices_buff_in[64];  // buff in

    // loop over faces
    for (int test = 0; test < pIndoor->uNumFaces; test++) {
        BLVFace* pface = &pIndoor->pFaces[test];

        if (pface->Portal()) continue;
        if (!pface->GetTexture()) continue;

        // check if faces is visible
        bool onlist = false;
        for (uint i = 0; i < pBspRenderer->uNumVisibleNotEmptySectors; ++i) {
            int listsector = pBspRenderer->pVisibleSectorIDs_toDrawDecorsActorsEtcFrom[i];
            if (pface->uSectorID == listsector) {
                onlist = true;
                break;
            }
        }
        if (!onlist) continue;


        decal_builder->ApplyBloodsplatDecals_IndoorFace(test);
        if (!decal_builder->uNumSplatsThisFace) continue;

        FacePlaneHolder.face_plane.vNormal.x = pface->pFacePlane.vNormal.x;
        FacePlaneHolder.polygonType = pface->uPolygonType;
        FacePlaneHolder.face_plane.vNormal.y = pface->pFacePlane.vNormal.y;
        FacePlaneHolder.face_plane.vNormal.z = pface->pFacePlane.vNormal.z;
        FacePlaneHolder.face_plane.dist = pface->pFacePlane.dist;

        // copy to buff in
        for (uint i = 0; i < pface->uNumVertices; ++i) {
            static_vertices_buff_in[i].vWorldPosition.x =
                pIndoor->pVertices[pface->pVertexIDs[i]].x;
            static_vertices_buff_in[i].vWorldPosition.y =
                pIndoor->pVertices[pface->pVertexIDs[i]].y;
            static_vertices_buff_in[i].vWorldPosition.z =
                pIndoor->pVertices[pface->pVertexIDs[i]].z;
            static_vertices_buff_in[i].u = (signed short)pface->pVertexUIDs[i];
            static_vertices_buff_in[i].v = (signed short)pface->pVertexVIDs[i];
        }

        // blood draw
            decal_builder->BuildAndApplyDecals(Lights.uCurrentAmbientLightLevel, 1, &FacePlaneHolder,
                pface->uNumVertices, static_vertices_buff_in,
                0, pface->uSectorID);


        
    }



    ///////////////////////////////////////////////////////


    /////////////////////////////////////



    // stack decals end


   // __debugbreak();


    return;

    


    //if (bspVAO == 0) {
    //    bspVAO = 1;



    //    // reserve first 7 layers for water tiles in unit 0
    //    auto wtrtexture = this->hd_water_tile_anim[0];
    //    //terraintexmap.insert(std::make_pair("wtrtyl", terraintexmap.size()));
    //    //numterraintexloaded[0]++;
    //    bsptexturewidths[0] = wtrtexture->GetWidth();
    //    bsptextureheights[0] = wtrtexture->GetHeight();

    //    for (int buff = 0; buff < 7; buff++) {
    //        char container_name[64];
    //        sprintf(container_name, "HDWTR%03u", buff);

    //        bsptexmap.insert(std::make_pair(container_name, bsptexmap.size()));
    //        bsptexloaded[0]++;
    //    }


    //    for (int test = 0; test < pIndoor->uNumFaces; test++) {
    //        BLVFace* face = &pIndoor->pFaces[test];

    //        //TextureOpenGL* tex = (TextureOpenGL*)face->GetTexture();
    //        String texname = face->resourcename;

    //        int texunit = 0;
    //        int texlayer = 0;

    //        // check if tile->name is already in list
    //        auto mapiter = bsptexmap.find(texname);
    //        if (mapiter != bsptexmap.end()) {
    //            // if so, extract unit and layer
    //            int unitlayer = mapiter->second;
    //            texlayer = unitlayer & 0xFF;
    //            texunit = (unitlayer & 0xFF00) >> 8;
    //        }
    //        else if (texname == "wtrtyl") {
    //            // water tile
    //            texunit = 0;
    //            texlayer = 0;
    //        }
    //        else {
    //            // else need to add it
    //            auto thistexture = assets->GetBitmap(texname);
    //            int width = thistexture->GetWidth();
    //            int height = thistexture->GetHeight();
    //            // check size to see what unit it needs
    //            int i;
    //            for (i = 0; i < 16; i++) {
    //                if ((bsptexturewidths[i] == width && bsptextureheights[i] == height) || bsptexturewidths[i] == 0) break;
    //            }
    //            if (i == 16) __debugbreak();

    //            if (bsptexturewidths[i] == 0) {
    //                bsptexturewidths[i] = width;
    //                bsptextureheights[i] = height;
    //            }

    //            texunit = i;
    //            texlayer = bsptexloaded[i];

    //            // encode unit and layer together
    //            int encode = (texunit << 8) | texlayer;

    //            // intsert into tex map
    //            bsptexmap.insert(std::make_pair(texname, encode));

    //            bsptexloaded[i]++;
    //            if (bsptexloaded[i] == 256) __debugbreak();
    //        }




    //    }






    //    __debugbreak();
    //}
}












bool RenderOpenGL::Initialize() {
    if (!RenderBase::Initialize()) {
        return false;
    }

    if (window != nullptr) {
        window->OpenGlCreate();

        checkglerror();

        // glShadeModel(GL_SMOOTH);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);       // Black Background
        checkglerror();
        glClearDepth(1.0f);
        checkglerror();
        glEnable(GL_DEPTH_TEST);
        checkglerror();
        glDepthFunc(GL_LEQUAL);
        checkglerror();
        // glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
        checkglerror();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        checkglerror();

        glViewport(0, 0, window->GetWidth(), window->GetHeight());
        glScissor(0, 0, window->GetWidth(), window->GetHeight());
        glEnable(GL_SCISSOR_TEST);
        checkglerror();

        // glMatrixMode(GL_PROJECTION);
        // glLoadIdentity();
        // checkglerror();
        
        // Calculate The Aspect Ratio Of The Window
        /*gluPerspective(45.0f,
            (GLfloat)window->GetWidth() / (GLfloat)window->GetHeight(),
            0.1f, 100.0f);*/
        //_set_3d_projection_matrix();
        checkglerror();
        
        // glMatrixMode(GL_MODELVIEW);
         //glLoadIdentity();

        _set_ortho_projection();
        _set_ortho_modelview();

        checkglerror();

        // Swap Buffers (Double Buffering)
        window->OpenGlSwapBuffers();

        this->clip_x = this->clip_y = 0;
        this->clip_z = window->GetWidth();
        this->clip_w = window->GetHeight();
        this->render_target_rgb = new uint32_t[window->GetWidth() * window->GetHeight()];

        checkglerror();

        PostInitialization();

        checkglerror();

        // get GPU capability params
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &GPU_MAX_TEX_SIZE);
        assert(GPU_MAX_TEX_SIZE >= 512);

        glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &GPU_MAX_TEX_LAYERS);
        assert(GPU_MAX_TEX_LAYERS >= 256);

        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &GPU_MAX_TEX_UNITS);
        assert(GPU_MAX_TEX_UNITS >= 16);

        glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS , &GPU_MAX_UNIFORM_COMP);
        assert(GPU_MAX_UNIFORM_COMP >= 1024);

        glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &GPU_MAX_TOTAL_TEXTURES);
        assert(GPU_MAX_TOTAL_TEXTURES >= 80);

        checkglerror();

        // shaders
        if (!InitShaders()) {
            logger->Warning("--- Shader initialisation has failed ---");
            return false;
        }

        checkglerror();


        return true;
    }

    return false;
}

bool RenderOpenGL::InitShaders() {

    logger->Info("Building outdoors terrain shader...");
    Shader buildterrainShader("../../../../Engine/Graphics/Shaders/terrain.vs", "../../../../Engine/Graphics/Shaders/terrain.fs");
    terrainshader.ID = buildterrainShader.ID;

    if (terrainshader.ID == 0)
        return false;

    logger->Info("Building outdoors building shader...");
    Shader buildoutbuildShader("../../../../Engine/Graphics/Shaders/outbuild.vs", "../../../../Engine/Graphics/Shaders/outbuild.fs");
    outbuildshader.ID = buildoutbuildShader.ID;

    if (outbuildshader.ID == 0)
        return false;

    logger->Info("Building indoors map shader...");
    Shader bspbuildShader("../../../../Engine/Graphics/Shaders/bsp.vs", "../../../../Engine/Graphics/Shaders/bsp.fs");
    bspshader.ID = bspbuildShader.ID;

    if (bspshader.ID == 0)
        return false;

    logger->Info("Building passthrough shader...");
    Shader buildpassthroughshader("../../../../Engine/Graphics/Shaders/passthrough.vs", "../../../../Engine/Graphics/Shaders/passthrough.fs");
    passthroughshader.ID = buildpassthroughshader.ID;

    if (passthroughshader.ID == 0)
        return false;

    logger->Info("Building lines shader...");
    Shader buildlineshader("../../../../Engine/Graphics/Shaders/lines.vs", "../../../../Engine/Graphics/Shaders/lines.fs");
    lineshader.ID = buildlineshader.ID;

    lineVAO = 0;
    twodVAO = 0;
    billbVAO = 0;

    // else

    checkglerror();
    return true;
}

void RenderOpenGL::ReleaseTerrain() {

    /*GLuint terrainVBO, terrainVAO;
    GLuint terraintextures[8];
    uint numterraintexloaded[8];
    uint terraintexturesizes[8];
    std::map<std::string, int> terraintexmap;*/

    terraintexmap.clear();

    for (int i = 0; i < 8; i++) {
        glDeleteTextures(1, &terraintextures[i]);
        terraintextures[i] = 0;
        numterraintexloaded[i] = 0;
        terraintexturesizes[i] = 0;
    }

    glDeleteBuffers(1, &terrainVBO);
    glDeleteVertexArrays(1, &terrainVAO);

    terrainVBO = 0;
    terrainVAO = 0;

    /*GLuint outbuildVBO, outbuildVAO;
    GLuint outbuildtextures[8];
    uint numoutbuildtexloaded[8];
    uint outbuildtexturewidths[8];
    uint outbuildtextureheights[8];
    std::map<std::string, int> outbuildtexmap;*/

    outbuildtexmap.clear();

    for (int i = 0; i < 16; i++) {
        glDeleteTextures(1, &outbuildtextures[i]);
        outbuildtextures[i] = 0;
        numoutbuildtexloaded[i] = 0;
        outbuildtexturewidths[i] = 0;
        outbuildtextureheights[i] = 0;


        glDeleteBuffers(1, &outbuildVBO[i]);
        glDeleteVertexArrays(1, &outbuildVAO[i]);

        outbuildVBO[i] = 0;
        outbuildVAO[i] = 0;

        if (outbuildshaderstore[i]) {
            free(outbuildshaderstore[i]);
            outbuildshaderstore[i] = nullptr;
        }

    }
    checkglerror();
}

void RenderOpenGL::ReleaseBSP() {
    /*GLuint bspVBO, bspVAO;
    GLuint bsptextures[16];
    uint bsptexloaded[16];
    uint bsptexturewidths[16];
    uint bsptextureheights[16];
    std::map<std::string, int> bsptexmap;*/

    bsptexmap.clear();

    for (int i = 0; i < 16; i++) {
        glDeleteTextures(1, &bsptextures[i]);
       bsptextures[i] = 0;
        bsptexloaded[i] = 0;
        bsptexturewidths[i] = 0;
        bsptextureheights[i] = 0;
    }

    glDeleteBuffers(1, &bspVBO);
    glDeleteVertexArrays(1, &bspVAO);

    bspVBO = 0;
    bspVAO = 0;


    checkglerror();
}


void RenderOpenGL::WritePixel16(int x, int y, uint16_t color) {
    // render target now 32 bit - format A8R8G8B8
    render_target_rgb[x + window->GetWidth() * y] = Color32(color);
}

void RenderOpenGL::FillRectFast(unsigned int uX, unsigned int uY,
                                unsigned int uWidth, unsigned int uHeight,
                                unsigned int uColor16) {
    int x = uX;
    int y = uY;
    int z = x + uWidth;
    int w = y + uHeight;

    // check bounds
    if (x >= (int)window->GetWidth() || x >= this->clip_z || y >= (int)window->GetHeight() || y >= this->clip_w) return;
    // check for overlap
    if (!(this->clip_x < z && this->clip_z > x && this->clip_y < w && this->clip_w > y)) return;

    int c = Color32(uColor16);
    float b = ((c & 31) * 8) / 255.0;
    float g = (((c >> 5) & 63) * 4) / 255.0;
    float r = (((c >> 11) & 31) * 8) / 255.0;
    

    //auto texture = (TextureOpenGL*)img;
    float gltexid = 0;

    int drawx = std::max(x, this->clip_x);
    int drawy = std::max(y, this->clip_y);
    int draww = std::min(w, this->clip_w);
    int drawz = std::min(z, this->clip_z);

    float texx = 0.5;
    float texy = 0.5;
    float texz = 0.5;
    float texw = 0.5;

    // 0 1 2 / 0 2 3

    twodshaderstore[twodvertscnt].x = drawx;
    twodshaderstore[twodvertscnt].y = drawy;
    twodshaderstore[twodvertscnt].z = 0;
    twodshaderstore[twodvertscnt].u = texx;
    twodshaderstore[twodvertscnt].v = texy;
    twodshaderstore[twodvertscnt].r = r;
    twodshaderstore[twodvertscnt].g = g;
    twodshaderstore[twodvertscnt].b = b;
    twodshaderstore[twodvertscnt].a = 1;
    twodshaderstore[twodvertscnt].texid = gltexid;
    twodvertscnt++;

    twodshaderstore[twodvertscnt].x = drawz;
    twodshaderstore[twodvertscnt].y = drawy;
    twodshaderstore[twodvertscnt].z = 0;
    twodshaderstore[twodvertscnt].u = texz;
    twodshaderstore[twodvertscnt].v = texy;
    twodshaderstore[twodvertscnt].r = r;
    twodshaderstore[twodvertscnt].g = g;
    twodshaderstore[twodvertscnt].b = b;
    twodshaderstore[twodvertscnt].a = 1;
    twodshaderstore[twodvertscnt].texid = gltexid;
    twodvertscnt++;

    twodshaderstore[twodvertscnt].x = drawz;
    twodshaderstore[twodvertscnt].y = draww;
    twodshaderstore[twodvertscnt].z = 0;
    twodshaderstore[twodvertscnt].u = texz;
    twodshaderstore[twodvertscnt].v = texw;
    twodshaderstore[twodvertscnt].r = r;
    twodshaderstore[twodvertscnt].g = g;
    twodshaderstore[twodvertscnt].b = b;
    twodshaderstore[twodvertscnt].a = 1;
    twodshaderstore[twodvertscnt].texid = gltexid;
    twodvertscnt++;

    ////////////////////////////////

    twodshaderstore[twodvertscnt].x = drawx;
    twodshaderstore[twodvertscnt].y = drawy;
    twodshaderstore[twodvertscnt].z = 0;
    twodshaderstore[twodvertscnt].u = texx;
    twodshaderstore[twodvertscnt].v = texy;
    twodshaderstore[twodvertscnt].r = r;
    twodshaderstore[twodvertscnt].g = g;
    twodshaderstore[twodvertscnt].b = b;
    twodshaderstore[twodvertscnt].a = 1;
    twodshaderstore[twodvertscnt].texid = gltexid;
    twodvertscnt++;

    twodshaderstore[twodvertscnt].x = drawz;
    twodshaderstore[twodvertscnt].y = draww;
    twodshaderstore[twodvertscnt].z = 0;
    twodshaderstore[twodvertscnt].u = texz;
    twodshaderstore[twodvertscnt].v = texw;
    twodshaderstore[twodvertscnt].r = r;
    twodshaderstore[twodvertscnt].g = g;
    twodshaderstore[twodvertscnt].b = b;
    twodshaderstore[twodvertscnt].a = 1;
    twodshaderstore[twodvertscnt].texid = gltexid;
    twodvertscnt++;

    twodshaderstore[twodvertscnt].x = drawx;
    twodshaderstore[twodvertscnt].y = draww;
    twodshaderstore[twodvertscnt].z = 0;
    twodshaderstore[twodvertscnt].u = texx;
    twodshaderstore[twodvertscnt].v = texw;
    twodshaderstore[twodvertscnt].r = r;
    twodshaderstore[twodvertscnt].g = g;
    twodshaderstore[twodvertscnt].b = b;
    twodshaderstore[twodvertscnt].a = 1;
    twodshaderstore[twodvertscnt].texid = gltexid;
    twodvertscnt++;


    checkglerror();

    // blank over same bit of this render_target_rgb to stop text overlaps
    for (int ys = drawy; ys < draww; ys++) {
        memset(this->render_target_rgb + (ys * window->GetWidth() + drawx), 0x00000000, (drawz - drawx) * 4);
    }

    if (twodvertscnt > 490) drawtwodverts();

    return;
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// possibly obsolete
//void RenderOpenGL::do_draw_debug_line_d3d(const RenderVertexD3D3* pLineBegin,
//    signed int sDiffuseBegin,
//    const RenderVertexD3D3* pLineEnd,
//    signed int sDiffuseEnd,
//    float z_stuff) {
//    __debugbreak();
//}

//void RenderOpenGL::DrawLines(const RenderVertexD3D3* vertices, unsigned int num_vertices) {
//    __debugbreak();
//}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// obsolete in this GL branch
//void RenderOpenGL::BeginLightmaps() { return; }
//void RenderOpenGL::EndLightmaps() { return; }
//void RenderOpenGL::BeginLightmaps2() { return; }
//void RenderOpenGL::EndLightmaps2() { return; }
//bool RenderOpenGL::DrawLightmap(struct Lightmap* pLightmap, struct Vec3_float_* pColorMult, float z_bias) { return 1; }

void RenderOpenGL::BltBackToFontFast(int a2, int a3, Rect* a4) { return; }
void RenderOpenGL::ZBuffer_Fill_2(signed int a2, signed int a3, Image* pTexture, int a5) { return; }
void RenderOpenGL::DrawTerrainPolygon(struct Polygon* poly, bool transparent, bool clampAtTextureBorders) { return; }


// still things to move across to shader
void RenderOpenGL::DrawIndoorPolygon(unsigned int uNumVertices, BLVFace* pFace, int uPackedID, unsigned int uColor, int a8) {

    // shaders now
    return;


    if (uNumVertices < 3) {
        return;
    }

    _set_ortho_projection(1);
    _set_ortho_modelview();

    _set_3d_projection_matrix();
    _set_3d_modelview_matrix();



    unsigned int sCorrectedColor = uColor;

    TextureOpenGL* texture = (TextureOpenGL*)pFace->GetTexture();

    if (lightmap_builder->StationaryLightsCount) {
        sCorrectedColor = 0xFFFFFFFF/*-1*/;
    }

    engine->AlterGamma_BLV(pFace, &sCorrectedColor);

    if (pFace->uAttributes & FACE_OUTLINED) {
        if (OS_GetTime() % 300 >= 150)
            uColor = sCorrectedColor = 0xFF20FF20;
        else
            uColor = sCorrectedColor = 0xFF109010;
    }


    if (_4D864C_force_sw_render_rules && engine->config->Flag1_1()) {
        /*
        __debugbreak();
        ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE,
        false)); ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0,
        D3DTSS_ADDRESS, D3DTADDRESS_WRAP)); for (uint i = 0; i <
        uNumVertices; ++i)
        {
        d3d_vertex_buffer[i].pos.x = array_507D30[i].vWorldViewProjX;
        d3d_vertex_buffer[i].pos.y = array_507D30[i].vWorldViewProjY;
        d3d_vertex_buffer[i].pos.z = 1.0 - 1.0 /
        (array_507D30[i].vWorldViewPosition.x * 0.061758894);
        d3d_vertex_buffer[i].rhw = 1.0 /
        array_507D30[i].vWorldViewPosition.x; d3d_vertex_buffer[i].diffuse =
        sCorrectedColor; d3d_vertex_buffer[i].specular = 0;
        d3d_vertex_buffer[i].texcoord.x = array_507D30[i].u /
        (double)pFace->GetTexture()->GetWidth();
        d3d_vertex_buffer[i].texcoord.y = array_507D30[i].v /
        (double)pFace->GetTexture()->GetHeight();
        }

        ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_ADDRESS,
        D3DTADDRESS_WRAP)); ErrD3D(pRenderD3D->pDevice->SetTexture(0,
        nullptr));
        ErrD3D(pRenderD3D->pDevice->DrawPrimitive(D3DPT_TRIANGLEFAN,
        D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1,
        d3d_vertex_buffer, uNumVertices, 28));
        lightmap_builder->DrawLightmaps(-1);
        */
    } else {
        if (!lightmap_builder->StationaryLightsCount || _4D864C_force_sw_render_rules && engine->config->Flag1_2()) {
            for (uint i = 0; i < uNumVertices; ++i) {
                d3d_vertex_buffer[i].pos.x = array_507D30[i].vWorldViewProjX;
                d3d_vertex_buffer[i].pos.y = array_507D30[i].vWorldViewProjY;
                d3d_vertex_buffer[i].pos.z =
                    1.0 -
                    1.0 / (array_507D30[i].vWorldViewPosition.x * 0.061758894);
                d3d_vertex_buffer[i].rhw =
                    1.0 / array_507D30[i].vWorldViewPosition.x;
                d3d_vertex_buffer[i].diffuse = sCorrectedColor;
                d3d_vertex_buffer[i].specular = 0;
                d3d_vertex_buffer[i].texcoord.x =
                    array_507D30[i].u / (double)pFace->GetTexture()->GetWidth();
                d3d_vertex_buffer[i].texcoord.y =
                    array_507D30[i].v /
                    (double)pFace->GetTexture()->GetHeight();
            }

            // glEnable(GL_TEXTURE_2D);
            // glDisable(GL_BLEND);
            glBindTexture(GL_TEXTURE_2D, texture->GetOpenGlTexture());

            // glDisable(GL_CULL_FACE);  // testing
            // glDisable(GL_DEPTH_TEST);

            // if (uNumVertices != 3 ) return; //3 ,4, 5 ,6

            glBegin(GL_TRIANGLE_FAN);

            for (uint i = 0; i < pFace->uNumVertices; ++i) {
                // glTexCoord2f(d3d_vertex_buffer[i].texcoord.x, d3d_vertex_buffer[i].texcoord.y);
                glTexCoord2f(((pFace->pVertexUIDs[i] + Lights.pDeltaUV[0]) / (double)pFace->GetTexture()->GetWidth()), ((pFace->pVertexVIDs[i] + Lights.pDeltaUV[1]) / (double)pFace->GetTexture()->GetHeight()));


                 glColor4f(
                ((d3d_vertex_buffer[i].diffuse >> 16) & 0xFF) / 255.0f,
                ((d3d_vertex_buffer[i].diffuse >> 8) & 0xFF) / 255.0f,
                ((d3d_vertex_buffer[i].diffuse >> 0) & 0xFF) / 255.0f,
                1.0f);

                glVertex3f(pIndoor->pVertices[pFace->pVertexIDs[i]].x,
                    pIndoor->pVertices[pFace->pVertexIDs[i]].y,
                    pIndoor->pVertices[pFace->pVertexIDs[i]].z);
            }
            drawcalls++;

            glEnd();
        } else {
            for (uint i = 0; i < uNumVertices; ++i) {
                d3d_vertex_buffer[i].pos.x = array_507D30[i].vWorldViewProjX;
                d3d_vertex_buffer[i].pos.y = array_507D30[i].vWorldViewProjY;
                d3d_vertex_buffer[i].pos.z =
                    1.0 -
                    1.0 / (array_507D30[i].vWorldViewPosition.x * 0.061758894);
                d3d_vertex_buffer[i].rhw = 1.0 / array_507D30[i].vWorldViewPosition.x;
                d3d_vertex_buffer[i].diffuse = uColor;
                d3d_vertex_buffer[i].specular = 0;
                d3d_vertex_buffer[i].texcoord.x =
                    array_507D30[i].u / (double)pFace->GetTexture()->GetWidth();
                d3d_vertex_buffer[i].texcoord.y =
                    array_507D30[i].v /
                    (double)pFace->GetTexture()->GetHeight();
            }

            glDepthMask(false);

            // ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_ADDRESS, D3DTADDRESS_WRAP));
            glBindTexture(GL_TEXTURE_2D, 0);

            glBegin(GL_TRIANGLE_FAN);

            for (uint i = 0; i < pFace->uNumVertices; ++i) {
                // glTexCoord2f(d3d_vertex_buffer[i].texcoord.x, d3d_vertex_buffer[i].texcoord.y);
                glTexCoord2f(((pFace->pVertexUIDs[i] + Lights.pDeltaUV[0]) / (double)pFace->GetTexture()->GetWidth()), ((pFace->pVertexVIDs[i] + Lights.pDeltaUV[1]) / (double)pFace->GetTexture()->GetHeight()));


                glColor4f(
                    ((d3d_vertex_buffer[i].diffuse >> 16) & 0xFF) / 255.0f,
                    ((d3d_vertex_buffer[i].diffuse >> 8) & 0xFF) / 255.0f,
                    ((d3d_vertex_buffer[i].diffuse >> 0) & 0xFF) / 255.0f,
                    1.0f);

                glVertex3f(pIndoor->pVertices[pFace->pVertexIDs[i]].x,
                    pIndoor->pVertices[pFace->pVertexIDs[i]].y,
                    pIndoor->pVertices[pFace->pVertexIDs[i]].z);
            }
            drawcalls++;

            glEnd();
            glDisable(GL_CULL_FACE);

            lightmap_builder->DrawLightmaps(-1 /*, 0*/);

            for (uint i = 0; i < uNumVertices; ++i) {
                d3d_vertex_buffer[i].diffuse = sCorrectedColor;
            }

            glBindTexture(GL_TEXTURE_2D, texture->GetOpenGlTexture());
            // ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_ADDRESS, D3DTADDRESS_WRAP));

            glDepthMask(true);
            glEnable(GL_BLEND);
            glBlendFunc(GL_ZERO, GL_SRC_COLOR);

            glBegin(GL_TRIANGLE_FAN);

            for (uint i = 0; i < pFace->uNumVertices; ++i) {
                // glTexCoord2f(d3d_vertex_buffer[i].texcoord.x, d3d_vertex_buffer[i].texcoord.y);
                glTexCoord2f(((pFace->pVertexUIDs[i] + Lights.pDeltaUV[0]) / (double)pFace->GetTexture()->GetWidth()), ((pFace->pVertexVIDs[i] + Lights.pDeltaUV[1]) / (double)pFace->GetTexture()->GetHeight()));


                glColor4f(
                    ((d3d_vertex_buffer[i].diffuse >> 16) & 0xFF) / 255.0f,
                    ((d3d_vertex_buffer[i].diffuse >> 8) & 0xFF) / 255.0f,
                    ((d3d_vertex_buffer[i].diffuse >> 0) & 0xFF) / 255.0f,
                    1.0f);

                glVertex3f(pIndoor->pVertices[pFace->pVertexIDs[i]].x,
                    pIndoor->pVertices[pFace->pVertexIDs[i]].y,
                    pIndoor->pVertices[pFace->pVertexIDs[i]].z);
            }
            drawcalls++;

            glEnd();

            glDisable(GL_BLEND);
        }
    }
}

bool RenderOpenGL::SwitchToWindow() {
    // pParty->uFlags |= PARTY_FLAGS_1_ForceRedraw;
    pViewport->SetFOV(_6BE3A0_fov);
    CreateZBuffer();

    return true;
}





bool RenderOpenGL::NuklearInitialize(struct nk_tex_font *tfont) {
    struct nk_context* nk_ctx = nuklear->ctx;
    if (!nk_ctx) {
        log->Warning("Nuklear context is not available");
        return false;
    }

    if (!NuklearCreateDevice()) {
        log->Warning("Nuklear device creation failed");
        NuklearRelease();
        return false;
    }

    nk_font_atlas_init_default(&nk_dev.atlas);
    struct nk_tex_font *font = NuklearFontLoad(NULL, 13);
    nk_dev.atlas.default_font = font->font;
    if (!nk_dev.atlas.default_font) {
        log->Warning("Nuklear default font loading failed");
        NuklearRelease();
        return false;
    }

    memcpy(tfont, font, sizeof(struct nk_tex_font));

    if (!nk_init_default(nk_ctx, &nk_dev.atlas.default_font->handle)) {
        log->Warning("Nuklear initialization failed");
        NuklearRelease();
        return false;
    }

    nk_buffer_init_default(&nk_dev.cmds);

    return true;
}

bool RenderOpenGL::NuklearCreateDevice() {
    GLint status;
    static const GLchar* vertex_shader =
        NK_SHADER_VERSION
        "uniform mat4 ProjMtx;\n"
        "in vec2 Position;\n"
        "in vec2 TexCoord;\n"
        "in vec4 Color;\n"
        "out vec2 Frag_UV;\n"
        "out vec4 Frag_Color;\n"
        "void main() {\n"
        "   Frag_UV = TexCoord;\n"
        "   Frag_Color = Color;\n"
        "   gl_Position = ProjMtx * vec4(Position.xy, 0, 1);\n"
        "}\n";
    static const GLchar* fragment_shader =
        NK_SHADER_VERSION
        "precision mediump float;\n"
        "uniform sampler2D Texture;\n"
        "in vec2 Frag_UV;\n"
        "in vec4 Frag_Color;\n"
        "out vec4 Out_Color;\n"
        "void main(){\n"
        "   Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
        "}\n";

    nk_buffer_init_default(&nk_dev.cmds);
    nk_dev.prog = glCreateProgram();
    nk_dev.vert_shdr = glCreateShader(GL_VERTEX_SHADER);
    nk_dev.frag_shdr = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(nk_dev.vert_shdr, 1, &vertex_shader, 0);
    glShaderSource(nk_dev.frag_shdr, 1, &fragment_shader, 0);
    glCompileShader(nk_dev.vert_shdr);
    glCompileShader(nk_dev.frag_shdr);
    glGetShaderiv(nk_dev.vert_shdr, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE)
        return false;
    glGetShaderiv(nk_dev.frag_shdr, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE)
        return false;
    glAttachShader(nk_dev.prog, nk_dev.vert_shdr);
    glAttachShader(nk_dev.prog, nk_dev.frag_shdr);
    glLinkProgram(nk_dev.prog);
    glGetProgramiv(nk_dev.prog, GL_LINK_STATUS, &status);
    if (status != GL_TRUE)
        return false;

    nk_dev.uniform_tex = glGetUniformLocation(nk_dev.prog, "Texture");
    nk_dev.uniform_proj = glGetUniformLocation(nk_dev.prog, "ProjMtx");
    nk_dev.attrib_pos = glGetAttribLocation(nk_dev.prog, "Position");
    nk_dev.attrib_uv = glGetAttribLocation(nk_dev.prog, "TexCoord");
    nk_dev.attrib_col = glGetAttribLocation(nk_dev.prog, "Color");

    {
        GLsizei vs = sizeof(struct nk_vertex);
        size_t vp = offsetof(struct nk_vertex, position);
        size_t vt = offsetof(struct nk_vertex, uv);
        size_t vc = offsetof(struct nk_vertex, col);

        glGenBuffers(1, &nk_dev.vbo);
        glGenBuffers(1, &nk_dev.ebo);
        glGenVertexArrays(1, &nk_dev.vao);

        glBindVertexArray(nk_dev.vao);
        glBindBuffer(GL_ARRAY_BUFFER, nk_dev.vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, nk_dev.ebo);

        glEnableVertexAttribArray((GLuint)nk_dev.attrib_pos);
        glEnableVertexAttribArray((GLuint)nk_dev.attrib_uv);
        glEnableVertexAttribArray((GLuint)nk_dev.attrib_col);

        glVertexAttribPointer((GLuint)nk_dev.attrib_pos, 2, GL_FLOAT, GL_FALSE, vs, (void*)vp);
        glVertexAttribPointer((GLuint)nk_dev.attrib_uv, 2, GL_FLOAT, GL_FALSE, vs, (void*)vt);
        glVertexAttribPointer((GLuint)nk_dev.attrib_col, 4, GL_UNSIGNED_BYTE, GL_TRUE, vs, (void*)vc);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return true;
}

bool RenderOpenGL::NuklearRender(enum nk_anti_aliasing AA, int max_vertex_buffer, int max_element_buffer) {
    struct nk_context *nk_ctx = nuklear->ctx;
    if (!nk_ctx)
        return false;

    int width, height;
    int display_width, display_height;
    struct nk_vec2 scale;
    GLfloat ortho[4][4] = {
        { 2.0f,  0.0f,  0.0f,  0.0f },
        { 0.0f, -2.0f,  0.0f,  0.0f },
        { 0.0f,  0.0f, -1.0f,  0.0f },
        { -1.0f, 1.0f,  0.0f,  1.0f },
    };

    height = window->GetHeight();
    width = window->GetWidth();
    display_height = render->GetRenderHeight();
    display_width = render->GetRenderWidth();

    ortho[0][0] /= (GLfloat)width;
    ortho[1][1] /= (GLfloat)height;

    scale.x = (float)display_width / (float)width;
    scale.y = (float)display_height / (float)height;

    /* setup global state */
    glViewport(0, 0, display_width, display_height);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glActiveTexture(GL_TEXTURE0);

    /* setup program */
    glUseProgram(nk_dev.prog);
    glUniform1i(nk_dev.uniform_tex, 0);
    glUniformMatrix4fv(nk_dev.uniform_proj, 1, GL_FALSE, &ortho[0][0]);
    {
        /* convert from command queue into draw list and draw to screen */
        const struct nk_draw_command *cmd;
        void *vertices, *elements;
        const nk_draw_index *offset = NULL;
        struct nk_buffer vbuf, ebuf;

        /* allocate vertex and element buffer */
        glBindVertexArray(nk_dev.vao);
        glBindBuffer(GL_ARRAY_BUFFER, nk_dev.vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, nk_dev.ebo);

        glBufferData(GL_ARRAY_BUFFER, max_vertex_buffer, NULL, GL_STREAM_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, max_element_buffer, NULL, GL_STREAM_DRAW);

        /* load vertices/elements directly into vertex/element buffer */
        vertices = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
        elements = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
        {
            /* fill convert configuration */
            struct nk_convert_config config;
            struct nk_draw_vertex_layout_element vertex_layout[] = {
                {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_vertex, position)},
                {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_vertex, uv)},
                {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, NK_OFFSETOF(struct nk_vertex, col)},
                {NK_VERTEX_LAYOUT_END}
            };
            memset(&config, 0, sizeof(config));
            config.vertex_layout = vertex_layout;
            config.vertex_size = sizeof(struct nk_vertex);
            config.vertex_alignment = NK_ALIGNOF(struct nk_vertex);
            config.null = nk_dev.null;
            config.circle_segment_count = 22;
            config.curve_segment_count = 22;
            config.arc_segment_count = 22;
            config.global_alpha = 1.0f;
            config.shape_AA = AA;
            config.line_AA = AA;

            /* setup buffers to load vertices and elements */
            nk_buffer_init_fixed(&vbuf, vertices, (nk_size)max_vertex_buffer);
            nk_buffer_init_fixed(&ebuf, elements, (nk_size)max_element_buffer);
            nk_convert(nk_ctx, &nk_dev.cmds, &vbuf, &ebuf, &config);
        }
        glUnmapBuffer(GL_ARRAY_BUFFER);
        glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

        /* iterate over and execute each draw command */
        nk_draw_foreach(cmd, nk_ctx, &nk_dev.cmds) {
            if (!cmd->elem_count) continue;
            glBindTexture(GL_TEXTURE_2D, (GLuint)cmd->texture.id);
            glScissor((GLint)(cmd->clip_rect.x * scale.x),
                (GLint)((height - (GLint)(cmd->clip_rect.y + cmd->clip_rect.h)) * scale.y),
                (GLint)(cmd->clip_rect.w * scale.x),
                (GLint)(cmd->clip_rect.h * scale.y));
            glDrawElements(GL_TRIANGLES, (GLsizei)cmd->elem_count, GL_UNSIGNED_SHORT, offset);
            offset += cmd->elem_count;
        }
        nk_clear(nk_ctx);
        nk_buffer_clear(&nk_dev.cmds);
    }

    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);

    return true;
}

void RenderOpenGL::NuklearRelease() {
    nk_font_atlas_clear(&nk_dev.atlas);

    glDetachShader(nk_dev.prog, nk_dev.vert_shdr);
    glDetachShader(nk_dev.prog, nk_dev.frag_shdr);
    glDeleteShader(nk_dev.vert_shdr);
    glDeleteShader(nk_dev.frag_shdr);
    glDeleteProgram(nk_dev.prog);
    glDeleteBuffers(1, &nk_dev.vbo);
    glDeleteBuffers(1, &nk_dev.ebo);
    glDeleteVertexArrays(1, &nk_dev.vao);

    nk_buffer_free(&nk_dev.cmds);

    memset(&nk_dev, 0, sizeof(nk_dev));
}

struct nk_tex_font *RenderOpenGL::NuklearFontLoad(const char* font_path, size_t font_size) {
    const void *image;
    int w, h;
    GLuint texid;

    struct nk_tex_font *tfont = new (struct nk_tex_font);
    if (!tfont)
        return NULL;

    struct nk_font_config cfg = nk_font_config(font_size);
    cfg.merge_mode = nk_false;
    cfg.coord_type = NK_COORD_UV;
    cfg.spacing = nk_vec2(0, 0);
    cfg.oversample_h = 3;
    cfg.oversample_v = 1;
    cfg.range = nk_font_cyrillic_glyph_ranges();
    cfg.size = font_size;
    cfg.pixel_snap = 0;
    cfg.fallback_glyph = '?';

    nk_font_atlas_begin(&nk_dev.atlas);

    if (!font_path)
        tfont->font = nk_font_atlas_add_default(&nk_dev.atlas, font_size, 0);
    else
        tfont->font = nk_font_atlas_add_from_file(&nk_dev.atlas, font_path, font_size, &cfg);

    image = nk_font_atlas_bake(&nk_dev.atlas, &w, &h, NK_FONT_ATLAS_RGBA32);

    glGenTextures(1, &texid);
    glBindTexture(GL_TEXTURE_2D, texid);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)w, (GLsizei)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);

    tfont->texid = texid;
    nk_font_atlas_end(&nk_dev.atlas, nk_handle_id(texid), &nk_dev.null);

    return tfont;
}

void RenderOpenGL::NuklearFontFree(struct nk_tex_font *tfont) {
    if (tfont)
        glDeleteTextures(1, &tfont->texid);
}

struct nk_image RenderOpenGL::NuklearImageLoad(Image *img) {
    GLuint texid;
    auto t = (TextureOpenGL *)img;
    unsigned __int8 *pixels = (unsigned __int8 *)t->GetPixels(IMAGE_FORMAT_R8G8B8A8);

    glGenTextures(1, &texid);
    t->SetOpenGlTexture(texid);
    glBindTexture(GL_TEXTURE_2D, texid);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t->GetWidth(), t->GetHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);

    return nk_image_id(texid);
}

void RenderOpenGL::NuklearImageFree(Image *img) {
    auto t = (TextureOpenGL *)img;
    GLuint texid = t->GetOpenGlTexture();
    if (texid != -1) {
        glDeleteTextures(1, &texid);
    }
}
