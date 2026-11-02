/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code ("Doom 3 Source Code").

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#ifndef __RENDERAPI_H__
#define __RENDERAPI_H__

/*
================================================================================

	spec 004 (Q2) render API selection and the classic scene seam.

	This header is included by engine translation units, including ones whose
	precompiled header redefines snprintf/vsnprintf (neo/idlib/Str.h:70-73).
	It therefore includes no standard header and names no standard-library type.

	The neutral command stream, the material plan and the light plan stay
	port-owned (src/renderer/rhi, src/renderer/d3d12). This header is the engine
	half only:

	  * r_renderApi selection and the backend-neutral "render system running"
	    predicate (engine-integration.md 5.1-5.3),
	  * a typed initialization failure that is reported instead of being
	    converted into an automatic switch (engine-integration.md 5.3.6),
	  * the value-only sink through which the engine hands its ordered frame to
	    the port (T035/T080-T082).

	No GL enum, idImage*, idMaterial* or vertex-cache handle is named in a sink
	callback: the engine translates at this boundary, exactly as the adapter
	translates the legacy RC_* structs.

================================================================================
*/

enum renderApi_t {
	RENDER_API_GL = 0,		// the retained OpenGL reference path
	RENDER_API_D3D12 = 1	// the D3D12 classic path
};

// A failure stage is reported, never inferred from a message.
enum renderApiStage_t {
	RA_STAGE_NONE = 0,
	RA_STAGE_SELECTION,		// r_renderApi holds a value the engine does not accept
	RA_STAGE_COMMAND,		// a legacy RC_* command could not be translated
	RA_STAGE_DEVICE,		// adapter/device creation
	RA_STAGE_SURFACE,		// window surface creation or resize
	RA_STAGE_PIPELINE,		// shader/pipeline creation
	RA_STAGE_SCENE_SINK,	// no backend-neutral scene consumer is installed
	RA_STAGE_DEVICE_LOST	// device removed after a successful initialization
};

typedef struct {
	renderApiStage_t	stage;
	char				detail[512];
} renderApiResult_t;

// The classic material stages whose texture/sampler facts the port needs to
// upload and sample. A DOOM 3 material has at most MAX_SHADER_STAGES stages and
// a classic interaction uses at most four; eight is a bounded value-only window,
// never an engine array.
#define Q2_MAX_STAGE_TEXTURES 8

// Per-stage texture binding facts (T081). `imageId` is the engine image registry
// identity assigned by R_Q2ImageIdentity(), not an idImage*; 0 means the stage has
// no image. `filter`/`repeat` are the engine's textureFilter_t/textureRepeat_t
// values, forwarded unchanged so the port can choose its sampler without a GL
// enum crossing the boundary.
typedef struct {
	int					numStages;
	unsigned int		imageId[Q2_MAX_STAGE_TEXTURES];
	int					filter[Q2_MAX_STAGE_TEXTURES];
	int					repeat[Q2_MAX_STAGE_TEXTURES];
	bool				hasImage[Q2_MAX_STAGE_TEXTURES];
} q2TextureStageFacts_t;

// Material facts the port's classic material plan classifies. Values only; `name`
// points into engine memory that stays alive for the duration of the call.
typedef struct {
	const char *		name;
	float				sort;			// drawSurf_t.sort, already offset by the front end
	int					numStages;
	int					coverage;		// materialCoverage_t (1 opaque, 2 perforated, 3 translucent)
	bool				hasTexture;
	bool				hasProgram;
	bool				alphaTest;
	bool				translucent;
	bool				fogLight;
	bool				blendLight;
	bool				noShadows;
	bool				noSelfShadow;
	bool				castsShadow;
	q2TextureStageFacts_t textures;
} q2MaterialFacts_t;

// One viewLight chain as the back end sees it: CP-02..CP-12 and CP-19..CP-21.
// The engine emits one lightPacket per chain, in the classic order (global
// shadows, local interactions, local shadows, global interactions, translucent
// interactions), so the port never has to guess which chain a packet means.
// `chainKind` is 0 for a light with no shadow volume (ambient/fog/blend or a
// noShadows light), 1 for the global chain and 2 for the local chain. The
// exclusion fields state why no volume may be emitted; they are facts about the
// engine's decision, not a shader hint.
typedef struct {
	bool				fogLight;
	bool				blendLight;
	bool				pointLight;
	bool				parallel;
	bool				hasProjection;
	bool				noShadows;
	float				falloffRadius;
	int					numShadowFrustums;
	int					globalShadows;
	int					localInteractions;
	int					localShadows;
	int					globalInteractions;
	int					translucentInteractions;

	int					chainKind;				// 0 none, 1 global, 2 local
	int					chainSurfaceCount;
	bool				chainMaterialNoShadows;
	bool				chainMaterialNoSelfShadow;
	bool				chainSameEntity;		// the local chain shadows the lit entity itself
	bool				excluded;				// no shadow volume may be emitted
	int					exclusionKind;			// 0 none, 1 light noShadows, 2 ambient/fog/blend,
												// 3 material noShadows, 4 no chain remains
} q2LightFacts_t;

// One world surface's geometry snapshot (T080). The pointers point into the
// engine's live vertex/index arrays and stay valid only for the callback; the
// port copies what it keeps. The layout is described by offsets and a stride so
// no idDrawVert or GL type crosses the boundary.
typedef struct {
	int					vertexCount;
	int					indexCount;
	const unsigned char *	vertexData;
	int					vertexStride;
	int					positionOffset;
	int					normalOffset;		// -1 when the surface has no normals
	int					texCoordOffset;		// -1 when the surface has no texture coordinates
	const unsigned char *	indexes;			// NULL for a non-indexed surface
	int					indexSize;			// bytes per index (2 or 4; glIndex_t)
	float				transform[16];		// column-major model-to-world (space->modelMatrix)
} q2SurfaceGeometry_t;

// One uploaded image (T081). `pixels` is tightly packed, `bytesPerRow` bytes per
// row, valid only for the callback; NULL for a render target or a precompressed
// upload whose bytes the port cannot consume. The format enum is the portable
// subset the port already uploads:
//   0 RGBA8, 1 R8, 2 BC1, 3 BC3, 4 BC5, 5 BC7.
typedef struct {
	unsigned int		imageId;
	const char *		name;
	int					width;
	int					height;
	int					mipCount;
	int					format;
	bool				sRGB;
	int					minFilter;
	int					magFilter;
	int					wrapS;
	int					wrapT;
	const unsigned char *	pixels;
	int					bytesPerRow;
} q2ImageUpload_t;

// One private skinned pose snapshot (T044). The engine keeps its own copy; the
// callback gives the port the posed positions of one surface for one view, so
// the port's history does not alias the shared dynamic-model cache. `positions`
// is 3 floats per vertex and valid only for the callback.
typedef struct {
	unsigned int		viewId;
	unsigned int		viewKind;			// 0 main, 1 mirror, 2 camera, 3 shadow
	unsigned int		surfaceId;			// stable engine identity, not a draw packet serial
	unsigned int		generation;			// increments when the shared surface is regenerated
	int					vertexCount;
	const float *		positions;
} q2PrivatePose_t;

// Per-surface depth hack, forwarded with the same meaning the GL path reads from
// drawSurf_t.space (draw_common.cpp:749-754, tr_render.cpp:176-206). The view
// projection passed to beginView() is NOT pre-hacked, so the consumer applies the
// hack for the packet and restores the projection afterwards. The weapon hack also
// narrows the depth range to 0..0.5; the model hack keeps 0..1 and subtracts the
// distance from the projection z offset. When both flags are set, the model hack is
// the effective one: the GL path applies the model hack from the original projection
// after the weapon hack, which overwrites it.
enum q2DepthHackKind_t {
	Q2_DEPTH_HACK_NONE = 0,
	Q2_DEPTH_HACK_WEAPON = 1,
	Q2_DEPTH_HACK_MODEL = 2
};

/*
	The port installs one sink. The engine calls it in the order it already
	draws: the sorted drawSurfs array first (the port may replay it for the
	depth fill, interaction and post-process phases), then each viewLight in
	viewLights order. Every callback returns false to refuse the frame; the
	engine then reports the refusal and never presents a partial frame.

	beginView:
	  `overlay` marks the display HUD: a separate 2D view at display resolution,
	  appended after the 3D scene (O-11).
	  `worldGui` is a view-level flag for a view that is itself a GUI view. The
	  engine has no such view, so it is always false here; an in-world GUI is a
	  surface inside a normal 3D view, which is the per-packet `worldGui` below.
	  `projection` is the unhacked viewDef projection; per-packet depth-hack facts
	  follow separately (see q2DepthHackKind_t).

	surfacePacket:
	  Emitted immediately before drawPacket for the same surfaceId, so the port
	  can upload the ordered world geometry before it classifies the material.
	  The geometry snapshot is value-only and stays valid only for the call.

	drawPacket:
	  `worldGui` is true for an in-world GUI surface. The engine derives it where the
	  material and the space already resolve the GUI (R_AddDrawSurf, the same decision
	  tr_light.cpp:1283-1297 makes) and records it on the drawSurf, so it is NOT an
	  SS_GUI sort comparison: drawSurf_t.sort carries tr.sortOffset
	  (tr_light.cpp:1195-1200), so equality with SS_GUI would only match the first
	  surface added in a frame.
	  `depthHackKind`/`modelDepthHack` are the real per-surface facts; the view
	  projection is not pre-hacked and the consumer applies the hack itself.

	imageUpload:
	  Emitted when the engine generates or scratches an image. The port receives
	  the pixel bytes and the sampler metadata; a render-target upload has NULL
	  pixels. This is a metadata/upload seam, not a promise that the GL upload ran.

	privatePose:
	  Emitted after a skinned surface's posed vertices exist. The engine keeps the
	  private copy; the port receives a snapshot keyed by view and stable surface.
*/
typedef struct q2SceneSink_s {
	void *		user;

	// False is an actionable failure: write a reason into `reason`.
	bool		(*initialize)( void *user, int width, int height, char *reason, int reasonSize );
	void		(*shutdown)( void *user );
	bool		(*imageUpload)( void *user, const q2ImageUpload_t *image );

	bool		(*beginView)( void *user, unsigned int viewId, int x, int y, int width, int height,
							  bool overlay, bool worldGui, const float projection[16] );
	bool		(*surfacePacket)( void *user, unsigned int surfaceId, const q2SurfaceGeometry_t *geometry );
	bool		(*drawPacket)( void *user, unsigned int surfaceId, const q2MaterialFacts_t *material,
							   int vertexCount, int indexCount, int depthHackKind,
							   float modelDepthHack, bool worldGui, bool postProcess );
	bool		(*lightPacket)( void *user, const q2LightFacts_t *light );
	bool		(*privatePose)( void *user, const q2PrivatePose_t *pose );
	bool		(*endView)( void *user );

	bool		(*setColorTarget)( void *user, int selector );	// 0 current, 1 offscreen
	bool		(*copyToImage)( void *user, unsigned int imageId, const char *imageName );
	bool		(*swapBuffers)( void *user );
} q2SceneSink_t;

void					R_SetQ2SceneSink( const q2SceneSink_t *sink );
const q2SceneSink_t *	R_Q2SceneSink( void );

// Selection. R_RenderApiSelected() returns the cvar value without clamping;
// callers must check R_RenderApiSelectionValid() before using it.
renderApi_t				R_RenderApiSelected( void );
bool					R_RenderApiSelectionValid( void );

// GL maps to glConfig.isInitialized; D3D12 maps to "device + surface + resources ready".
// A failed D3D12 initialization leaves this false and r_renderApi untouched.
bool					R_RenderSystemRunning( void );
bool					R_ModernRenderApiRunning( void );

// Replaces the direct R_InitOpenGL()/GLimp_Shutdown() pairing at the three
// lifecycle sites (InitOpenGL, Shutdown, vid_restart).
void					R_InitRenderApi( void );
void					R_ShutdownRenderApi( void );

const renderApiResult_t *R_RenderApiFailure( void );
const char *			R_RenderApiName( renderApi_t api );
const char *			R_RenderApiStageName( renderApiStage_t stage );

// Records a typed failure without touching r_renderApi and without switching paths.
void					R_RenderApiFail( renderApiStage_t stage, const char *detail );

// Per-frame counters for r_showRenderApiTrace. They record the order the engine
// emitted, not a rendered result.
enum q2TraceCounter_t {
	Q2_TRACE_VIEW = 0,
	Q2_TRACE_OVERLAY,
	Q2_TRACE_DRAW,
	Q2_TRACE_SURFACE,
	Q2_TRACE_IMAGE,
	Q2_TRACE_POSE,
	Q2_TRACE_LIGHT,
	Q2_TRACE_REFUSED,
	Q2_TRACE_COPY,
	Q2_TRACE_TARGET,
	Q2_TRACE_SWAP,
	Q2_TRACE_COUNTERS
};

void					R_Q2TraceAdd( q2TraceCounter_t counter );
void					R_Q2TraceReset( void );
void					R_Q2TracePrint( const char *label );

// The engine assigns a stable per-image identity so no idImage* crosses the seam.
void					R_Q2ResetImageRegistry( void );

// spec 004 (Q3) T044: the private previous-pose store. The engine keeps its own
// copy; the port receives the same snapshot through privatePose. The getter is
// value-only so the engine glue can query it without naming idMD5Mesh.
bool					R_Q3PreviousPrivatePose( unsigned int viewId, unsigned int surfaceId,
												 unsigned int generation, int maxVerts,
												 float *positions, int *numVerts );
void					R_Q3ResetPrivatePoses( void );

#endif // __RENDERAPI_H__
