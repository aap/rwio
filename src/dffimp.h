#define _CRT_SECURE_NO_WARNINGS
#include "Max.h"
#include <stdio.h>
#include <direct.h>
#include <notify.h>
#include <commdlg.h>
#include <3dsmaxport.h>
#include <stdmat.h>
#include <iparamb2.h>
#include <dummy.h>
#include <iskin.h>
#include <modstack.h>
#include <icustattribcontainer.h>
#include <meshnormalspec.h>
#include <custattrib.h>
#if MAX_API_NUM <= 33	// 2010
#include <maxscrpt/maxscrpt.h>
#include <maxscrpt/Listener.h>
#include <maxscrpt/numbers.h>
#include <maxscrpt/definsfn.h>
#else
#include <maxscript/maxscript.h>
#include <maxscript/util/Listener.h>
#include <maxscript/foundation/numbers.h>
#include <maxscript/macros/define_instantiation_functions.h>
#endif
#include <vector>

#include <rw.h>
#include <rwgta.h>
#include <collisions.h>

#include "dffimp_res.h"

#define MAP_EXTRACOLORS -1
#define MAP_TEXCOORD0 1
#define MAP_EXTRAALPHA 9

#define lprintf the_listener->edit_stream->printf
#define lflush the_listener->edit_stream->flush

#define STR_DFFCLASSNAME              "RenderWare model"
#define STR_ANMCLASSNAME              "RenderWare animation"
#define STR_LIBDESCRIPTION            "DFF File Im-/Exporter"
#define STR_DFFFILE                   "RenderWare .DFF File"
#define STR_ANMFILE                   "RenderWare .ANM File"
#define STR_AUTHOR                    "aap"
#define STR_SCENEIMPORT               "Scene Import"
#define STR_COPYRIGHT                 "Copyright 2016-2020 aap"
#define STR_SCENEEXPORT               "Scene Export"

#define VERSION 340

//TCHAR *GetStringT(int id);

//rw::Geometry *convertGeometry(INode *node, int **vertexmap);
//void convertSkin(rw::Geometry *geo, INode *node, Modifier *mod, int *map);

const char *getAsciiStr(const TCHAR *str);
const TCHAR *getMaxStr(const char *str);

INode *getRootOf(INode *node);
INode *getRootOfSelection(Interface *ifc);
void extendAnimRange(float duration);
int getID(INode *node, int *id);
int getChildNum(INode *node);

struct SortNode
{
	int id;
	const MCHAR *name;
	int childNum;
	INode *node;
};
int sortByID(const void *a, const void *b);
int sortByChildNum(const void *a, const void *b);

Matrix3 getLocalMatrix(INode *node);
Matrix3 getObjectToLocalMatrix(INode *node);

void transformGeometry(rw::Geometry *geo, rw::Matrix *mat);

enum RwMat {
	mat_color = 0, mat_coloralpha,
	mat_sp_ambient, mat_sp_diffuse, mat_sp_specular,
	mat_texmap_texture,

	/* matfx */
	mat_matfxeffect,
	mat_texmap_envmap, mat_envmap_amount,
	mat_texmap_bumpmap, mat_bumpmap_amount,
	mat_texmap_pass2, mat_pass2_srcblend, mat_pass2_destblend
};

enum RsExt {
	mat_enEnv = 0, mat_enSpec,
	mat_scaleX, mat_scaleY, mat_transScaleX, mat_transScaleY, mat_shininess,
	mat_specularity, mat_specMap
};

class DFFImport : public SceneImport
{
	Interface *ifc;
	ImpInterface *impifc;

	int isBiped;
	int isWorldSpace;
public:
	static std::vector<INode*> lastImported;

	static int convertHierarchy;
	static int autoSmooth;
	static int prepend;
	static float smoothingAngle;
	static int explicitNormals;
	static int importStdMaterials;
	static int fixKam;

	Mtl *MakeGTAMaterial(rw::Material *m);
	Mtl *MakeStdMaterial(rw::Material *m);
	void makeMaterials(rw::Atomic *a, INode *inode);
	void makeMesh(rw::Atomic *a, Mesh *maxmesh);
	void axesHeuristics(rw::Frame *f);
	void saveNodes(INode *node);
	BOOL dffFileRead(const TCHAR *filename);

	DFFImport();
	~DFFImport();
	int ExtCount();				// Number of extensions supported
	const TCHAR *Ext(int n);		// Extension #n (i.e. "3DS")
	const TCHAR *LongDesc();		// Long ASCII description (i.e. "Autodesk 3D Studio File")
	const TCHAR *ShortDesc();		// Short ASCII description (i.e. "3D Studio")
	const TCHAR *AuthorName();		// ASCII Author name
	const TCHAR *CopyrightMessage();	// ASCII Copyright message
	const TCHAR *OtherMessage1();		// Other message #1
	const TCHAR *OtherMessage2();		// Other message #2
	unsigned int Version();			// Version number * 100 (i.e. v3.01 = 301)
	void ShowAbout(HWND hWnd);		// Show DLL's "About..." box
	int DoImport(const TCHAR *name, ImpInterface *ii, Interface *i, BOOL suppressPrompts=FALSE);	// Import file
};

struct RWNode
{
	rw::Frame *frame;
	INode *node;
	int flags;
	int id;
};

class DFFExport : public SceneExport
{
private:
	Interface *ifc;
	ExpInterface *expifc;

	int numNodes;
	int maxNodes;
	int nextId;
	int lastChild;
	RWNode *nodearray;
	int inHierarchy;

	INode *rootnode;

	INode *skinNodes[256];
	int numSkins;

public:
	// geometry flags
	static int exportLit;
	static int exportNormals;
	static int exportPrelit;
	static int exportTristrip;
	// world space
	static int worldSpace;
	// animation
	static int exportHAnim;
	static int exportSkin;
	// user data
	static int exportUserData;
	static int exportObjNames;
	static TCHAR exportObjNames_Entry[128];
	static int exportObjNames_Geo;
	static int exportObjNames_Frm;
	static int exportObjNames_Cam;
	static int exportObjNames_Lgt;
	static int exportObjNames_Mat;
	static int exportObjNames_Tex;
	// R* extension
	static int exportExtraColors;
	static int exportNames;
	// RW version;
	static int version;
public:
	void convertSkin(rw::Geometry *geo, rw::Frame *frame, INode *node, Modifier *mod, int *map);
	rw::Geometry *convertGeometry(INode *node, int **vertexmap);
	void findSkinnedGeometry(INode *root);
	rw::Frame *findFrameOfNode(INode *node);
	void convertAtomic(rw::Frame *frame, rw::Frame *root, rw::Clump *clump, INode *node);
	void convertLight(rw::Frame *frame, rw::Clump *clump, INode *node);
	void convertCamera(rw::Frame *frame, rw::Clump *clump, INode *node);
	void convertNode(rw::Clump *clump, rw::Frame *frame, INode *node, int flipz = 0);
	BOOL writeDFF(const TCHAR *filename);

	DFFExport();
	~DFFExport();
	int ExtCount();				// Number of extensions supported
	const TCHAR *Ext(int n);		// Extension #n (i.e. "3DS")
	const TCHAR *LongDesc();		// Long ASCII description (i.e. "Autodesk 3D Studio File")
	const TCHAR *ShortDesc();		// Short ASCII description (i.e. "3D Studio")
	const TCHAR *AuthorName();		// ASCII Author name
	const TCHAR *CopyrightMessage();	// ASCII Copyright message
	const TCHAR *OtherMessage1();		// Other message #1
	const TCHAR *OtherMessage2();		// Other message #2
	unsigned int Version();			// Version number * 100 (i.e. v3.01 = 301)
	void ShowAbout(HWND hWnd);		// Show DLL's "About..." box
	int DoExport(const TCHAR *name, ExpInterface *ei, Interface *i, BOOL suppressPrompts=FALSE, DWORD options=0);	// Export file
	BOOL SupportsOptions(int ext, DWORD options);
};

class COLExport : public SceneExport
{
private:
	Interface *ifc;
	ExpInterface *expifc;
public:
	bool writeCOL1(const TCHAR *filename);

	COLExport();
	~COLExport();
	int ExtCount();				// Number of extensions supported
	const TCHAR *Ext(int n);		// Extension #n (i.e. "3DS")
	const TCHAR *LongDesc();		// Long ASCII description (i.e. "Autodesk 3D Studio File")
	const TCHAR *ShortDesc();		// Short ASCII description (i.e. "3D Studio")
	const TCHAR *AuthorName();		// ASCII Author name
	const TCHAR *CopyrightMessage();	// ASCII Copyright message
	const TCHAR *OtherMessage1();		// Other message #1
	const TCHAR *OtherMessage2();		// Other message #2
	unsigned int Version();			// Version number * 100 (i.e. v3.01 = 301)
	void ShowAbout(HWND hWnd);		// Show DLL's "About..." box
	int DoExport(const TCHAR *name, ExpInterface *ei, Interface *i, BOOL suppressPrompts=FALSE, DWORD options=0);	// Export file
	BOOL SupportsOptions(int ext, DWORD options);
};

class ANMImport : public SceneImport
{
	Interface *ifc;

public:
	BOOL anmFileRead(const TCHAR *filename);

	ANMImport();
	~ANMImport();
	int ExtCount();				// Number of extensions supported
	const TCHAR *Ext(int n);		// Extension #n (i.e. "3DS")
	const TCHAR *LongDesc();		// Long ASCII description (i.e. "Autodesk 3D Studio File")
	const TCHAR *ShortDesc();		// Short ASCII description (i.e. "3D Studio")
	const TCHAR *AuthorName();		// ASCII Author name
	const TCHAR *CopyrightMessage();	// ASCII Copyright message
	const TCHAR *OtherMessage1();		// Other message #1
	const TCHAR *OtherMessage2();		// Other message #2
	unsigned int Version();			// Version number * 100 (i.e. v3.01 = 301)
	void ShowAbout(HWND hWnd);		// Show DLL's "About..." box
	int DoImport(const TCHAR *name, ImpInterface *ii, Interface *i, BOOL suppressPrompts=FALSE);	// Import file
};

class ANMExport : public SceneExport
{
	Interface *ifc;
	ExpInterface *expifc;

public:
	BOOL anmFileWrite(const TCHAR *filename);

	ANMExport();
	~ANMExport();
	int ExtCount();				// Number of extensions supported
	const TCHAR *Ext(int n);		// Extension #n (i.e. "3DS")
	const TCHAR *LongDesc();		// Long ASCII description (i.e. "Autodesk 3D Studio File")
	const TCHAR *ShortDesc();		// Short ASCII description (i.e. "3D Studio")
	const TCHAR *AuthorName();		// ASCII Author name
	const TCHAR *CopyrightMessage();	// ASCII Copyright message
	const TCHAR *OtherMessage1();		// Other message #1
	const TCHAR *OtherMessage2();		// Other message #2
	unsigned int Version();			// Version number * 100 (i.e. v3.01 = 301)
	void ShowAbout(HWND hWnd);		// Show DLL's "About..." box
	int DoExport(const TCHAR *name, ExpInterface *ei, Interface *i, BOOL suppressPrompts=FALSE, DWORD options=0);	// Export file
	BOOL SupportsOptions(int ext, DWORD options);
};

void initRW(void);
