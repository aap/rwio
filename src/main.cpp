#include "dffimp.h"

HINSTANCE hInstance;

static int rwInitialized;

void
initRW(void)
{
	if(rwInitialized)
		return;
	//rw::version = 0x33002;
	rw::version = 0x35000;
	rw::platform = rw::PLATFORM_D3D8;
	//rw::platform = rw::PLATFORM_PS2;
	rw::streamAppendFrames = 1;
	rw::Engine::init();
	gta::attachPlugins();
	rw::Engine::open(nil);
	rw::Engine::start();
	rwInitialized = 1;
	rw::Texture::setCreateDummies(1);
	rw::Texture::setLoadTextures(0);
}

const char*
getAsciiStr(const TCHAR *str)
{
#ifdef _UNICODE
	static char fuckmax[MAX_PATH];
	wcstombs(fuckmax, str, MAX_PATH);
	return fuckmax;
#else
	return str;
#endif
}

const TCHAR*
getMaxStr(const char *str)
{
#ifdef _UNICODE
	static TCHAR fuckmax[MAX_PATH];
	mbstowcs(fuckmax, str, MAX_PATH);
	return fuckmax;
#else
	return str;
#endif
}



// Find node highest up in hierarchy that isn't the scene root
INode*
getRootOf(INode *node)
{
	while(!node->GetParentNode()->IsRootNode())
		node = node->GetParentNode();
	return node;
}

INode*
getRootOfSelection(Interface *ifc)
{
	// Find root of the node hierarchy we're exporting,
	// i.e. first in selection.
	// TODO: could we be smarter about this?
	int numSelected = ifc->GetSelNodeCount();
	for(int i = 0; i < numSelected; i++)
		return getRootOf(ifc->GetSelNode(i));
	return nil;
}

void
extendAnimRange(float duration)
{
	Interval range = GetCOREInterface()->GetAnimRange();
	TimeValue end = SecToTicks(duration);
	if(end > range.End()){
		range.SetEnd(end);
		GetCOREInterface()->SetAnimRange(range);
	}
}

// check for explicitly set ID
int
getID(INode *node, int *id)
{
	int tag;
	if(node->GetUserPropInt(_T("tag"), tag)){
		*id = tag;
		return 1;
	}
	*id = -1;
	return 0;
}

// check for explicit hierarchy sorting number
int
getChildNum(INode *node)
{
	int num;
	if(node->GetUserPropInt(_T("childNum"), num))
		return num;
	return -1;
}

int
sortByID(const void *a, const void *b)
{
	SortNode *na = (SortNode*)a;
	SortNode *nb = (SortNode*)b;
	// sort nodes with no or negative tag to the back
	if(na->id < 0) return 1;
	if(nb->id < 0) return -1;
	return na->id - nb->id;
}

int
sortByChildNum(const void *a, const void *b)
{
	SortNode *na = (SortNode*)a;
	SortNode *nb = (SortNode*)b;
	// sort nodes with no or negative num to the back
	if(na->childNum < 0) return 1;
	if(nb->childNum < 0) return -1;
	return na->childNum - nb->childNum;
}

// Importer

static Value *getConvertHierarchy() { return Integer::intern(DFFImport::convertHierarchy); }
static Value *setConvertHierarchy(Value *val) { DFFImport::convertHierarchy = val->to_int(); return val; }

static Value *getAutoSmooth() { return Integer::intern(DFFImport::autoSmooth); }
static Value *setAutoSmooth(Value *val) { DFFImport::autoSmooth = val->to_int(); return val; }

static Value *getSmoothingAngle() { return Float::intern(DFFImport::smoothingAngle); }
static Value *setSmoothingAngle(Value *val) { DFFImport::smoothingAngle = val->to_float(); return val; }

static Value *getPrepend() { return Integer::intern(DFFImport::prepend); }
static Value *setPrepend(Value *val) { DFFImport::prepend = val->to_int(); return val; }

static Value *getImpStdMat() { return Integer::intern(DFFImport::importStdMaterials); }
static Value *setImpStdMat(Value *val) { DFFImport::importStdMaterials = val->to_int(); return val; }

static Value *getFixKam() { return Integer::intern(DFFImport::fixKam); }
static Value *setFixKam(Value *val) { DFFImport::fixKam = val->to_int(); return val; }

// Exporter

static Value *getLighting() { return Integer::intern(DFFExport::exportLit); }
static Value *setLighting(Value *val) { DFFExport::exportLit = val->to_int(); return val; }

static Value *getNormals() { return Integer::intern(DFFExport::exportNormals); }
static Value *setNormals(Value *val) { DFFExport::exportNormals = val->to_int(); return val; }

static Value *getPrelight() { return Integer::intern(DFFExport::exportPrelit); }
static Value *setPrelight(Value *val) { DFFExport::exportPrelit = val->to_int(); return val; }

static Value *getTristrip() { return Integer::intern(DFFExport::exportTristrip); }
static Value *setTristrip(Value *val) { DFFExport::exportTristrip = val->to_int(); return val; }

static Value *getWorldSpace() { return Integer::intern(DFFExport::worldSpace); }
static Value *setWorldSpace(Value *val) { DFFExport::worldSpace = val->to_int(); return val; }

static Value *getCreateHAnim() { return Integer::intern(DFFExport::exportHAnim); }
static Value *setCreateHAnim(Value *val) { DFFExport::exportHAnim = val->to_int(); return val; }

static Value *getSkinning() { return Integer::intern(DFFExport::exportSkin); }
static Value *setSkinning(Value *val) { DFFExport::exportSkin = val->to_int(); return val; }

static Value *getExtraColors() { return Integer::intern(DFFExport::exportExtraColors); }
static Value *setExtraColors(Value *val) { DFFExport::exportExtraColors = val->to_int(); return val; }

static Value *getNodeNames() { return Integer::intern(DFFExport::exportNames); }
static Value *setNodeNames(Value *val) { DFFExport::exportNames = val->to_int(); return val; }

static Value *getFileVersion() { return Integer::intern(DFFExport::version); }
static Value *setFileVersion(Value *val)
{
	int v = val->to_int();
	if(v < 0x30000 || v >= 0x38000){
		lprintf(_T("invalid version\n"));
		return NULL;
	}
	DFFExport::version = v;
	return val;
}

def_visible_primitive(selectImportedDFF, "selectImportedDFF");
 
Value*
selectImportedDFF_cf(Value **arg_list, int count)
{
	int clear = 1;
	for(int i = 0; i < DFFImport::lastImported.size(); i++){
		GetCOREInterface()->SelectNode(DFFImport::lastImported[i], clear);
		clear = 0;
	}
	DFFImport::lastImported.clear();
	return &ok;
}

static void
initMaxscript(void)
{
	define_struct_global(_T("convertHierarchy"), _T("dffImp"), getConvertHierarchy, setConvertHierarchy);
	define_struct_global(_T("autoSmooth"), _T("dffImp"), getAutoSmooth, setAutoSmooth);
	define_struct_global(_T("smoothingAngle"), _T("dffImp"), getSmoothingAngle, setSmoothingAngle);
	define_struct_global(_T("prepend"), _T("dffImp"), getPrepend, setPrepend);
	define_struct_global(_T("importStdMaterials"), _T("dffImp"), getImpStdMat, setImpStdMat);
	define_struct_global(_T("fixMaterialIDs"), _T("dffImp"), getFixKam, setFixKam);

	define_struct_global(_T("lighting"), _T("dffExp"), getLighting, setLighting);
	define_struct_global(_T("normals"), _T("dffExp"), getNormals, setNormals);
	define_struct_global(_T("prelight"), _T("dffExp"), getPrelight, setPrelight);
	define_struct_global(_T("tristrip"), _T("dffExp"), getTristrip, setTristrip);
	define_struct_global(_T("worldSpace"), _T("dffExp"), getWorldSpace, setWorldSpace);
	define_struct_global(_T("createHAnim"), _T("dffExp"), getCreateHAnim, setCreateHAnim);
	define_struct_global(_T("skinning"), _T("dffExp"), getSkinning, setSkinning);
	define_struct_global(_T("extraColors"), _T("dffExp"), getExtraColors, setExtraColors);
	define_struct_global(_T("nodeNames"), _T("dffExp"), getNodeNames, setNodeNames);
	define_struct_global(_T("fileVersion"), _T("dffExp"), getFileVersion, setFileVersion);

/*	AllocConsole();
	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);*/
}


//TCHAR*
//GetStringT(int id)
//{
//	static TCHAR buf[256];
//	if(hInstance)
//		return LoadString(hInstance, id, buf, sizeof(buf)) ? buf : NULL;
//	return NULL;
//}

static void onStartup(void *param, NotifyInfo *info) { initMaxscript(); }

BOOL WINAPI
DllMain(HINSTANCE hinstDLL, ULONG fdwReason, LPVOID lpvReserved)
{
	hInstance = hinstDLL;
	switch(fdwReason){
	case DLL_PROCESS_ATTACH:
		RegisterNotification(onStartup, NULL, NOTIFY_SYSTEM_STARTUP);
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

class DFFImportDesc : public ClassDesc {
	public:
	int IsPublic(void) { return 1; }
	void *Create(BOOL loading = FALSE) { return new DFFImport; }
	const TCHAR *ClassName(void) { return _T(STR_DFFCLASSNAME); /*GetStringT(IDS_CLASSNAME);*/ }
	SClass_ID SuperClassID(void) { return SCENE_IMPORT_CLASS_ID; }
	Class_ID ClassID(void) { return Class_ID(0xc42486c, 0x6f900bef); }
	const TCHAR *Category(void) { return _T(STR_SCENEIMPORT); /*GetStringT(IDS_SCENEIMPORT);*/  }
};

class DFFExportDesc : public ClassDesc {
	public:
	int IsPublic(void) { return 1; }
	void *Create(BOOL loading = FALSE) { return new DFFExport; }
	const TCHAR *ClassName(void) { return _T(STR_DFFCLASSNAME); /*GetStringT(IDS_CLASSNAME);*/ }
	SClass_ID SuperClassID(void) { return SCENE_EXPORT_CLASS_ID; }
	Class_ID ClassID(void) { return Class_ID(0x6e2a398d, 0x2afe6e70); }
	const TCHAR *Category(void) { return _T(STR_SCENEEXPORT); /*GetStringT(IDS_SCENEEXPORT);*/  }
};

class ANMImportDesc : public ClassDesc {
	public:
	int IsPublic(void) { return 1; }
	void *Create(BOOL loading = FALSE) { return new ANMImport; }
	const TCHAR *ClassName(void) { return _T(STR_ANMCLASSNAME); /*GetStringT(IDS_CLASSNAME);*/ }
	SClass_ID SuperClassID(void) { return SCENE_IMPORT_CLASS_ID; }
	Class_ID ClassID(void) { return Class_ID(0x7de72f47, 0x2d020d73); }
	const TCHAR *Category(void) { return _T(STR_SCENEIMPORT); /*GetStringT(IDS_SCENEEXPORT);*/  }
};

class ANMExportDesc : public ClassDesc {
	public:
	int IsPublic(void) { return 1; }
	void *Create(BOOL loading = FALSE) { return new ANMExport; }
	const TCHAR *ClassName(void) { return _T(STR_ANMCLASSNAME); /*GetStringT(IDS_CLASSNAME);*/ }
	SClass_ID SuperClassID(void) { return SCENE_EXPORT_CLASS_ID; }
	Class_ID ClassID(void) { return Class_ID(0xa9d6559, 0x55eb1212); }
	const TCHAR *Category(void) { return _T(STR_SCENEEXPORT); /*GetStringT(IDS_SCENEEXPORT);*/  }
};

class COLExportDesc : public ClassDesc {
	public:
	int IsPublic(void) { return 1; }
	void *Create(BOOL loading = FALSE) { return new COLExport; }
	const TCHAR *ClassName(void) { return _T("GTA Collision"); /*GetStringT(IDS_CLASSNAME);*/ }
	SClass_ID SuperClassID(void) { return SCENE_EXPORT_CLASS_ID; }
	Class_ID ClassID(void) { return Class_ID(0x12480f60, 0x41820396); }
	const TCHAR *Category(void) { return _T(STR_SCENEEXPORT); /*GetStringT(IDS_SCENEEXPORT);*/  }
};

static DFFImportDesc importDesc;
static DFFExportDesc exportDesc;
static ANMImportDesc animImportDesc;
static ANMExportDesc animExportDesc;
static COLExportDesc colExportDesc;

__declspec(dllexport) const TCHAR*
LibDescription(void) {
//	TCHAR *str = GetStringT(IDS_LIBDESCRIPTION);
//	str = str;
//	return str;
	return _T("DFF File Im-/Exporter");
}

__declspec(dllexport) int
LibNumberClasses(void) { return 5; }

__declspec(dllexport) ClassDesc*
LibClassDesc(int i)
{
	switch(i){
	case 0:
		return &importDesc;
	case 1:
		return &exportDesc;
	case 2:
		return &animImportDesc;
	case 3:
		return &animExportDesc;
	case 4:
		return &colExportDesc;
	default:
		return NULL;
	}
}

// Return version so can detect obsolete DLLs
__declspec(dllexport) ULONG 
LibVersion(void) { return VERSION_3DSMAX; }

// We cannot defer loading. Maxscript variables won't be initialized.
__declspec(dllexport)
ULONG CanAutoDefer(void)
{
	return 0;
}
