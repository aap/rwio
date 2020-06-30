#include "dffimp.h"

rw::Matrix::Tolerance exportTolerance = { 0.001, 0.001, 0.001 };

static rw::Matrix flipZMat = {
	{-1.0f, 0.0f,  0.0f}, rw::Matrix::TYPEORTHONORMAL,
	{ 0.0f, 1.0f,  0.0f}, 0,
	{ 0.0f, 0.0f, -1.0f}, 0,
	{ 0.0f, 0.0f,  0.0f}, 0
};

static rw::Matrix bipmat = {
	{0.0f, 0.0f, 1.0f}, rw::Matrix::TYPEORTHONORMAL,
	{1.0f, 0.0f, 0.0f}, 0,
	{0.0f, 1.0f, 0.0f}, 0,
	{0.0f, 0.0f, 0.0f}, 0,
};

static rw::Matrix invbipmat = {
	{0.0f, 1.0f, 0.0f}, rw::Matrix::TYPEORTHONORMAL,
	{0.0f, 0.0f, 1.0f}, 0,
	{1.0f, 0.0f, 0.0f}, 0,
	{0.0f, 0.0f, 0.0f}, 0,
};

static rw::Matrix rwworldmat = {
	{1.0f, 0.0f,  0.0f}, rw::Matrix::TYPEORTHONORMAL,
	{0.0f, 0.0f, -1.0f}, 0,
	{0.0f, 1.0f,  0.0f}, 0,
	{0.0f, 0.0f,  0.0f}, 0,
};

static rw::Matrix rwidentmat = {
	{1.0f, 0.0f, 0.0f}, rw::Matrix::IDENTITY | rw::Matrix::TYPEORTHONORMAL,
	{0.0f, 1.0f, 0.0f}, 0,
	{0.0f, 0.0f, 1.0f}, 0,
	{0.0f, 0.0f, 0.0f}, 0,
};

int DFFExport::exportLit = 1;
int DFFExport::exportNormals = 1;
int DFFExport::exportPrelit = 0;
int DFFExport::exportTristrip = 0;

int DFFExport::worldSpace = 0;

int DFFExport::exportHAnim = 0;
int DFFExport::exportSkin = 0;

int DFFExport::exportExtraColors = 0;
int DFFExport::exportNames = 1;

int DFFExport::version = 0x36003;

Matrix3
getLocalMatrix(INode *node)
{
	Matrix3 mat = node->GetNodeTM(0);
	if(!node->GetParentNode()->IsRootNode()){
		Matrix3 parent = node->GetParentTM(0);
		// Leave in scale for translation.
		parent.NoScale();
		mat = mat * Inverse(parent);
	}
	// Remove scale now.
	mat.NoScale();
	return mat;
}

Matrix3
getObjectToLocalMatrix(INode *node)
{
	Matrix3 objectTM = node->GetObjectTM(0);
	Matrix3 nodeTM = node->GetNodeTM(0);
	nodeTM.NoScale();
	objectTM *= Inverse(nodeTM);
	return objectTM;
}

static void
customFrameData(rw::Frame *frame, INode *node)
{
	// R* Node name plugin
	if(DFFExport::exportNames){
		TCHAR *name = (TCHAR*)node->GetName();
		char *rwname = gta::getNodeName(frame);
#ifdef _UNICODE
		char fuckmax[MAX_PATH];
		wcstombs(fuckmax, name, MAX_PATH);
		strncpy(rwname, fuckmax, 24);
#else
		strncpy(rwname, name, 24);
#endif
		rwname[23] = '\0';
	}
}

static void
assignPipelines(rw::Atomic *atomic)
{
	using namespace rw;
	Geometry *g = atomic->geometry;
	int hasSkin = rw::Skin::get(g) != NULL;
	int hasMatFX = 0;
	for(int i = 0; i < g->matList.numMaterials; i++){
		hasMatFX = *PLUGINOFFSET(MatFX*, g->matList.materials[i], matFXGlobals.materialOffset) != NULL;
		if(hasMatFX)
			break;
	}
	if(hasSkin)
		rw::Skin::setPipeline(atomic, 1);
	else if(hasMatFX)
		rw::MatFX::enableEffects(atomic);
}

static void
dumpScene(INode *node, int ind)
{
	for(int i = 0; i < ind; i++)
		lprintf(_T(" "));
	lprintf(_T("%s\n"), node->GetName());
	int numChildren = node->NumberOfChildren();
	for(int i = 0; i < numChildren; i++)
		dumpScene(node->GetChildNode(i), ind+1);
}

// Find node highest up in hierarchy that isn't the scene root
static INode*
getRootOf(INode *node)
{
	while(!node->GetParentNode()->IsRootNode())
		node = node->GetParentNode();
	return node;
}

static int
countNodes(INode *node)
{
	int n = 1;
	int numChildren = node->NumberOfChildren();
	for(int i = 0; i < numChildren; i++)
		n += countNodes(node->GetChildNode(i));
	return n;
}

static void
convertMatrix(rw::Matrix *out, Matrix3 *mat)
{
	MRow *m = mat->GetAddr();
	out->right.x  = m[0][0];
	out->right.y  = m[0][1];
	out->right.z  = m[0][2];
	out->up.x  = m[1][0];
	out->up.y  = m[1][1];
	out->up.z  = m[1][2];
	out->at.x = m[2][0];
	out->at.y = m[2][1];
	out->at.z = m[2][2];
	out->pos.x = m[3][0];
	out->pos.y = m[3][1];
	out->pos.z = m[3][2];
	out->optimize(&exportTolerance);
}

static void
dumpFrameHier(rw::Frame *frame, int ind = 0)
{
	using namespace rw;
	for(int i = 0; i < ind; i++)
		printf(" ");
	char *name = gta::getNodeName(frame);
	HAnimData *hanim = PLUGINOFFSET(HAnimData, frame, hAnimOffset);
	printf("%s %d %d %s\n", name[0] ? name : "---", frame->objectList.count(), hanim->id, hanim->hierarchy ? "HIERARCHY" : "");
	for(Frame *child = frame->child;
	    child; child = child->next)
		dumpFrameHier(child, ind+1);
}

// stolen from RW
static Modifier*
findModifier(INode *nodePtr, Class_ID type)
{
	Object* obj = nodePtr->GetObjectRef();
	while(obj && obj->SuperClassID() == GEN_DERIVOB_CLASS_ID){
		IDerivedObject *deriv = (IDerivedObject*)(obj);
		int numMod = deriv->NumModifiers();
		for(int i = 0; i < numMod; i++){
			Modifier *mod = deriv->GetModifier(i);
			if (mod->ClassID() == type)
				return mod;
		}
		obj = deriv->GetObjRef();
	}
	return NULL;
}

static INode*
getFirstBone(Modifier *mod)
{
	ISkin *skin = (ISkin*)mod->GetInterface(I_SKIN);
	if(skin->GetNumBones() <= 0)
		return NULL;
	return skin->GetBone(0);
}

void
DFFExport::findSkinnedGeometry(INode *root)
{
	Modifier *mod = findModifier(root, SKIN_CLASSID);
	if(mod){
		INode *bone = getFirstBone(mod);
		int visible = root->Renderable() && !root->IsNodeHidden();
		if(bone && getRootOf(bone) == this->rootnode && visible)
			this->skinNodes[this->numSkins++] = root;
	}else{
		int numChildren = root->NumberOfChildren();
		for(int i = 0; i < numChildren; i++)
			findSkinnedGeometry(root->GetChildNode(i));
	}
}

INode*
findSkinRootBone(INode *node)
{
	Modifier *mod = findModifier(node, SKIN_CLASSID);
	if(mod)
		return getFirstBone(mod);
	return nil;
}

void
DFFExport::convertLight(rw::Frame *frame, rw::Clump *clump, INode *node)
{
	using namespace rw;
	GenLight *gl = (GenLight*)node->EvalWorldState(0).obj;
	LightState state;
	Interval valid = FOREVER;
	gl->EvalLightState(0, valid, &state);
	int32 rwtype = rw::Light::POINT;
	switch(state.type){
	case OMNI_LGT:
		rwtype = rw::Light::POINT;
		break;
	case SPOT_LGT:
		rwtype = rw::Light::SPOT;
		break;
	case DIRECT_LGT:
		rwtype = rw::Light::DIRECTIONAL;
		break;
	case AMBIENT_LGT:
		rwtype = rw::Light::AMBIENT;
		break;
	}
	if(rwtype == rw::Light::SPOT && state.fallsize-state.hotsize > 5.0f)
		rwtype = rw::Light::SOFTSPOT;
	if(state.ambientOnly)
		rwtype = rw::Light::AMBIENT;
	rw::Light *rwlight = rw::Light::create(rwtype);
	rwlight->setColor(state.color.r, state.color.g, state.color.b);
	if(rwtype >= rw::Light::POINT)
		rwlight->radius = state.attenEnd;
	if(state.type == SPOT_LGT)
		rwlight->setAngle(DegToRad(state.fallsize/2.0f));
	rwlight->setFrame(frame);
	clump->addLight(rwlight);
}

void
DFFExport::convertCamera(rw::Frame *frame, rw::Clump *clump, INode *node)
{
	using namespace rw;
	GenCamera *gc = (GenCamera*)node->EvalWorldState(0).obj;
	CameraState state;
	Interval valid = FOREVER;
	gc->EvalCameraState(0, valid, &state);

	rw::Camera *rwcam = rw::Camera::create();
	rwcam->projection = !state.isOrtho;
	rwcam->nearPlane = state.hither;
	rwcam->farPlane = state.yon;
	rwcam->fogPlane = rwcam->nearPlane;
	rwcam->viewWindow.x = tan(state.fov/2.0f);
	rwcam->viewWindow.y = rwcam->viewWindow.x*3.0f/4.0f;
	rwcam->setFrame(frame);
	clump->addCamera(rwcam);
}

void
transformGeometry(rw::Geometry *geo, rw::Matrix *mat)
{
	rw::Matrix inv, nmat;
	rw::Matrix::invert(&inv, mat);
	rw::Matrix::transpose(&nmat, &inv);
	// TODO: all MOPRH targets
	rw::V3d *verts = geo->morphTargets[0].vertices;
	rw::V3d *norms = geo->morphTargets[0].normals;

	rw::V3d::transformPoints(verts, verts, geo->numVertices, mat);
	if(norms)
		rw::V3d::transformVectors(norms, norms, geo->numVertices, &nmat);
}

// frame is the frame the atomic will be attached to.
// root is the root of the hierarchy.
void
DFFExport::convertAtomic(rw::Frame *frame, rw::Frame *root, rw::Clump *clump, INode *node)
{
	using namespace rw;
	::Object *obj = node->EvalWorldState(0).obj;
	Geometry *geo;
	int *map;
	geo = convertGeometry(node, &map);
	if(geo == nil)
		return;

	// move vertices from object into node space
	rw::Matrix objectOffset;
	::Matrix3 objectTM = node->GetObjectTM(0);
	::Matrix3 nodeTM = node->GetNodeTM(0);
	nodeTM.NoScale();
	objectTM *= Inverse(nodeTM);
	convertMatrix(&objectOffset, &objectTM);
	transformGeometry(geo, &objectOffset);

	Modifier *mod = findModifier(node, SKIN_CLASSID);
	if(mod){
		convertSkin(geo, frame, node, mod, map);

		// Transform the geometry data of node so that it will
		// be right when attached to frame.
		rw::Matrix mat, tmp;
		rw::Matrix::invert(&tmp, frame->getLTM());
		rw::Matrix::mult(&mat, frame->root->getLTM(), &tmp);
		transformGeometry(geo, &mat);
	}
	delete[] map;
	// do it after geometry transformations
	geo->calculateBoundingSphere();
	Atomic *atomic = Atomic::create();
	atomic->geometry = geo;
	assignPipelines(atomic);

	atomic->setFrame(frame);
	clump->addAtomic(atomic);
}

// check for explicitly set ID
static int
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

static int
getChildNum(INode *node)
{
	int num;
	if(node->GetUserPropInt(_T("childNum"), num))
		return num;
	return -1;
}

static void
flipFrameZ(rw::Frame *frame)
{
	using namespace rw;
	V3d pos;
	// TODO: this is a bit ugly, clean up
	rw::Matrix *m = &frame->matrix;
	pos = m->pos;
	m->pos.set(0.0f, 0.0f, 0.0f);
	m->optimize(&exportTolerance);
	frame->transform(&flipZMat, rw::COMBINEPRECONCAT);
	m->pos = pos;
	m->optimize(&exportTolerance);
	frame->updateObjects();
}

struct SortNode
{
	int id;
	const MCHAR *name;
	int childNum;
	INode *node;
};

static int
sortByID(const void *a, const void *b)
{
	SortNode *na = (SortNode*)a;
	SortNode *nb = (SortNode*)b;
	// sort nodes with no or negative tag to the back
	if(na->id < 0) return 1;
	if(nb->id < 0) return -1;
	return na->id - nb->id;
}

static int
sortByChildNum(const void *a, const void *b)
{
	SortNode *na = (SortNode*)a;
	SortNode *nb = (SortNode*)b;
	// sort nodes with no or negative tag to the back
	if(na->childNum < 0) return 1;
	if(nb->childNum < 0) return -1;
	return na->childNum - nb->childNum;
}

rw::Frame*
DFFExport::findFrameOfNode(INode *node)
{
	int i;
	for(i = 0; i < numNodes; i++)
		if(nodearray[i].node == node)
			return nodearray[i].frame;
	return nil;
}

void
DFFExport::convertNode(rw::Clump *clump, rw::Frame *frame, INode *node, int flip)
{
	using namespace rw;

	::Matrix3 mat = getLocalMatrix(node);
	convertMatrix(&frame->matrix, &mat);
	frame->updateObjects();

	// Lights and Cameras look along Z in RW but along -Z in Max.
	// Rotate around Y to fix this. Since a Light/Camera frame is rotated,
	// all of their children have to be rotated back.
	if(flip)
		flipFrameZ(frame);
	flip = 0;

	::Object *obj = node->EvalWorldState(0).obj;
	if(obj && node->Renderable() && !node->IsNodeHidden()){
		switch(obj->SuperClassID()){
		case GEOMOBJECT_CLASS_ID:
			if(obj->IsRenderable())
				convertAtomic(frame, NULL, clump, node);
			break;
		case CAMERA_CLASS_ID:
			convertCamera(frame, clump, node);
			flip = 1;
			break;
		case LIGHT_CLASS_ID:
			convertLight(frame, clump, node);
			flip = 1;
			break;
		}
	}

	if(flip)
		flipFrameZ(frame);

	customFrameData(frame, node);

	int thisIndex = -1;
	int oldNumNodes = -1;
	RWNode *n = NULL;
	int id, hasid;
	// Ignore branches of the hierarchy that have
	// an explicit negative tag.
	int wasInHierarchy = this->inHierarchy;
	hasid = getID(node, &id);
	if(hasid && id < 0)
		this->inHierarchy = 0;
	if(this->inHierarchy){
		thisIndex = this->numNodes++;
		n = &this->nodearray[thisIndex];
		// Find a tag if none was set.
		if(id < 0)
			id = this->nextId++;
		n->id = id;
		n->flags = HAnimHierarchy::PUSH;
		n->frame = frame;
		n->node = node;
		oldNumNodes = this->numNodes;
//	}else{
//		lprintf(_T("skipping frame %s as node\n"), gta::getNodeName(frame));
	}

	// Sort child nodes by ID
	int numChildren = node->NumberOfChildren();
	SortNode *children = new SortNode[numChildren];
	for(int i = 0; i < numChildren; i++){
		int id;
		children[i].node = node->GetChildNode(i);
		getID(children[i].node, &id);
		children[i].id = id;
		children[i].name = node->GetName();
		children[i].childNum = getChildNum(children[i].node);
	}

	qsort(children, numChildren, sizeof(*children), sortByChildNum);
//	qsort(children, numChildren, sizeof(*children), sortByID);

	for(int i = 0; i < numChildren; i++){
		BOOL noexp;
		if(children[i].node->GetUserPropBool(_T("noexport"), noexp) && noexp)
			continue;
		Frame *childframe = Frame::create();
		frame->addChild(childframe, 1);
		convertNode(clump, childframe, children[i].node, flip);
	}
	delete[] children;

	if(this->inHierarchy){
		// no children -> POP
		if(oldNumNodes == this->numNodes)
			n->flags |= HAnimHierarchy::POP;
		// last child has no PUSH
		else{
			assert(this->lastChild >= 0);
			this->nodearray[this->lastChild].flags &= ~HAnimHierarchy::PUSH;
		}
		this->lastChild = thisIndex;
	}
	this->inHierarchy = wasInHierarchy;
}

BOOL
DFFExport::writeDFF(const TCHAR *filename)
{
	using namespace rw;

	rw::platform = PLATFORM_NULL;

	// Find root of the node hierarchy we're exporting
	int numSelected = this->ifc->GetSelNodeCount();
	INode *rootnode = NULL;
	for(int i = 0; i < numSelected; i++){
		INode *n = this->ifc->GetSelNode(i);
		rootnode = getRootOf(n);
		break;
		//lprintf(_T(": %s : %s\n"), n->GetName(), rootnode->GetName());
	}
	if(rootnode == NULL){
		lprintf(_T("error: nothing selected\n"));
		return 0;
	}
	this->rootnode = rootnode;

	Clump *clump = Clump::create();
	// actual root of the hierarchy, there may be a dummy above this one
	Frame *rootFrame = Frame::create();
	clump->setFrame(rootFrame);

	//lprintf("%d\n", GetTicksPerFrame());
	//lprintf("anim range: %d %d\n", this->ifc->GetAnimRange().Start(), this->ifc->GetAnimRange().End());

	this->inHierarchy = 0;
	this->nodearray = NULL;
	this->numNodes = 0;
	if(this->exportHAnim){
		this->maxNodes = countNodes(rootnode);
		//lprintf("number of nodes: %d\n", this->maxNodes);
		this->nodearray = new RWNode[this->maxNodes];
		this->nextId = 2000;
		this->inHierarchy = 1;
		this->lastChild = -1;
	}

	this->numSkins = 0;

	convertNode(clump, rootFrame, rootnode);

	if(this->exportHAnim){
		this->nodearray[0].flags &= ~HAnimHierarchy::PUSH;

		int32 flags = 0;
		int32 maxFrameSize = 36;
		int32 *nodeFlags = new int32[this->numNodes];
		int32 *nodeIDs = new int32[this->numNodes];
		for(int i = 0; i < this->numNodes; i++){
			RWNode *n = &this->nodearray[i];
			nodeFlags[i] = n->flags;
			nodeIDs[i] = n->id;
			HAnimData *hanim = HAnimData::get(n->frame);
			hanim->id = n->id;
			//lprintf("%d %d %s\n", n->id, n->flags, gta::getNodeName(n->frame));
		}
		HAnimHierarchy *hier = HAnimHierarchy::create(this->numNodes,
			nodeFlags, nodeIDs, flags, maxFrameSize);
		HAnimData *hanim = HAnimData::get(this->nodearray[0].frame);
		hanim->hierarchy = hier;
		delete[] nodeFlags;
		delete[] nodeIDs;
	}

	// Add a dummy frame to carry the world transformation
	if(this->exportHAnim || this->worldSpace){
		Frame *f = Frame::create();
		f->addChild(rootFrame);
		clump->setFrame(f);

		if(DFFExport::version < 0x35000)
			customFrameData(f, rootnode);
	}

	// Fix up frames.
	rw::Matrix tmp;
	BOOL isbiped = FALSE;
	rootnode->GetUserPropBool(_T("fakeBiped"), isbiped);
	if(this->worldSpace){
		rw::Matrix::mult(&tmp, &rootFrame->matrix, &rwworldmat);
		clump->getFrame()->transform(&tmp, rw::COMBINEREPLACE);
		if(isbiped)
			clump->getFrame()->transform(&invbipmat, rw::COMBINEPRECONCAT);
	}
	if(isbiped)
		rootFrame->transform(&bipmat, rw::COMBINEREPLACE);
	else
		rootFrame->transform(&rwidentmat, rw::COMBINEREPLACE);

	// Create skinned geometries after frame hierarchy is done.
	if(this->exportSkin){
		findSkinnedGeometry(this->ifc->GetRootNode());
//		Frame *f = DFFExport::version < 0x35000 ? clump->getFrame() : rootFrame;
		for(int i = 0; i < this->numSkins; i++){
			Frame *f = DFFExport::version < 0x35000 ? clump->getFrame() : findFrameOfNode(findSkinRootBone(this->skinNodes[i]));
			assert(f != nil);
			convertAtomic(f, rootFrame, clump, this->skinNodes[i]);
		}
	}


	delete[] this->nodearray;

	//dumpFrameHier(clump->getFrame());
	StreamFile stream;

#ifdef _UNICODE
	char path[MAX_PATH];
	wcstombs(path, filename, MAX_PATH);
	if(stream.open(path, "wb") == NULL){
#else
	if(stream.open(filename, "wb") == NULL){
#endif
		lprintf(_T("error: couldn't open file\n"));
		return 0;
	}
	hAnimDoStream = DFFExport::exportHAnim;
	rw::version = DFFExport::version;
	clump->streamWrite(&stream);
	hAnimDoStream = 1;
	stream.close();
	clump->destroy();

	rw::platform = PLATFORM_D3D8;
	return 1;
}

static struct {
	TCHAR *text;
	int version;
} gameversions[] = {
	{ _T("GTA III PS2 (3.1)"), 0x31000 },
	{ _T("GTA III PC (3.3)"),  0x33002 },
	{ _T("GTA VC PS2 (3.3)"),  0x33002 },
	{ _T("GTA VC PC (3.4)"),   0x34003 },
	{ _T("GTA III/VC Mobile (3.4)"),   0x34005 },
	{ _T("GTA III/VC XBOX (3.5)"),   0x35000 },
	{ _T("GTA SA (3.6)"),   0x36003 },
	{ NULL, 0 }
};

static void
setVersionFields(HWND hWnd)
{
	ISpinnerControl  *spin;
	spin = GetISpinner(GetDlgItem(hWnd, IDC_V1SPIN));
	spin->LinkToEdit(GetDlgItem(hWnd, IDC_V1), EDITTYPE_INT); 
	spin->SetLimits(0, 7, TRUE); 
	spin->SetScale(1.0f);
	// only go up to 7, not 15
	spin->SetValue((DFFExport::version >> 12)&0x7, FALSE);
	ReleaseISpinner(spin);

	spin = GetISpinner(GetDlgItem(hWnd, IDC_V2SPIN));
	spin->LinkToEdit(GetDlgItem(hWnd, IDC_V2), EDITTYPE_INT); 
	spin->SetLimits(0, 15, TRUE); 
	spin->SetScale(1.0f);
	spin->SetValue((DFFExport::version >> 8)&0xF, FALSE);
	ReleaseISpinner(spin);

	spin = GetISpinner(GetDlgItem(hWnd, IDC_V3SPIN));
	spin->LinkToEdit(GetDlgItem(hWnd, IDC_V3), EDITTYPE_INT); 
	spin->SetLimits(0, 63, TRUE); 
	spin->SetScale(1.0f);
	spin->SetValue(DFFExport::version&0x3F, FALSE);
	ReleaseISpinner(spin);
}

static INT_PTR CALLBACK
ExportDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	ISpinnerControl  *spin;
	HWND cb;
	DFFExport *exp = DLGetWindowLongPtr<DFFExport*>(hWnd); 
	switch(msg){
	case WM_INITDIALOG:
		exp = (DFFExport*)lParam;
		DLSetWindowLongPtr(hWnd, lParam); 
		CenterWindow(hWnd, GetParent(hWnd)); 
		CheckDlgButton(hWnd, IDC_LIGHTING, DFFExport::exportLit);
		CheckDlgButton(hWnd, IDC_NORMALS, DFFExport::exportNormals);
		CheckDlgButton(hWnd, IDC_PRELIT, DFFExport::exportPrelit);
		CheckDlgButton(hWnd, IDC_TRISTRIP, DFFExport::exportTristrip);

		CheckDlgButton(hWnd, IDC_WORLDSPACE, DFFExport::worldSpace);

		CheckDlgButton(hWnd, IDC_HANIM, DFFExport::exportHAnim);
		CheckDlgButton(hWnd, IDC_SKINNING, DFFExport::exportSkin);

		CheckDlgButton(hWnd, IDC_RS_NODENAME, DFFExport::exportNames);
		CheckDlgButton(hWnd, IDC_RS_EXTRACOLORS, DFFExport::exportExtraColors);

		setVersionFields(hWnd);

		cb = GetDlgItem(hWnd, IDC_GAME);
		for(int i = 0; gameversions[i].text; i++)
			ComboBox_AddString(cb, gameversions[i].text);
		break;
	case WM_COMMAND:
		switch(HIWORD(wParam)){
		case CBN_SELCHANGE:
			int sel = ComboBox_GetCurSel((HWND)lParam);
			DFFExport::version = gameversions[sel].version;
			setVersionFields(hWnd);
			break;
		}
		switch(LOWORD(wParam)){
		case IDOK:
			DFFExport::exportLit = IsDlgButtonChecked(hWnd, IDC_LIGHTING);
			DFFExport::exportNormals = IsDlgButtonChecked(hWnd, IDC_NORMALS);
			DFFExport::exportPrelit = IsDlgButtonChecked(hWnd, IDC_PRELIT);
			DFFExport::exportTristrip = IsDlgButtonChecked(hWnd, IDC_TRISTRIP);

			DFFExport::worldSpace = IsDlgButtonChecked(hWnd, IDC_WORLDSPACE);

			DFFExport::exportHAnim = IsDlgButtonChecked(hWnd, IDC_HANIM);
			DFFExport::exportSkin = IsDlgButtonChecked(hWnd, IDC_SKINNING);
 
			DFFExport::exportNames = IsDlgButtonChecked(hWnd, IDC_RS_NODENAME);
			DFFExport::exportExtraColors = IsDlgButtonChecked(hWnd, IDC_RS_EXTRACOLORS);

			DFFExport::version = 0x30000;
			spin = GetISpinner(GetDlgItem(hWnd, IDC_V1SPIN));
			DFFExport::version |= spin->GetIVal() << 12;
			ReleaseISpinner(spin);

			spin = GetISpinner(GetDlgItem(hWnd, IDC_V2SPIN));
			DFFExport::version |= spin->GetIVal() << 8;
			ReleaseISpinner(spin);

			spin = GetISpinner(GetDlgItem(hWnd, IDC_V3SPIN));
			DFFExport::version |= spin->GetIVal();
			ReleaseISpinner(spin);

			EndDialog(hWnd, 1);
			break;
		case IDCANCEL:
			EndDialog(hWnd, 0);
			break;
		}
		break;
		default:
			return FALSE;
	}
	return TRUE;
}

DFFExport::DFFExport(void)
{
}

DFFExport::~DFFExport(void)
{
}

int
DFFExport::ExtCount(void)
{
	return 1;
}

const TCHAR*
DFFExport::Ext(int n)
{
	switch(n){
	case 0:
		return _T("DFF");
	}
	return _T("");
}

const TCHAR*
DFFExport::LongDesc(void)
{
	return _T(STR_DFFFILE); //GetStringT(IDS_DFFFILE);
}
	
const TCHAR*
DFFExport::ShortDesc(void)
{
	return _T(STR_CLASSNAME); //GetStringT(IDS_CLASSNAME);
}

const TCHAR*
DFFExport::AuthorName(void)
{
	return _T(STR_AUTHOR); //GetStringT(IDS_AUTHOR);
}

const TCHAR*
DFFExport::CopyrightMessage(void)
{
	return _T(STR_COPYRIGHT); //GetStringT(IDS_COPYRIGHT);
}

const TCHAR *
DFFExport::OtherMessage1(void)
{
	return _T("");
}

const TCHAR *
DFFExport::OtherMessage2(void)
{
	return _T("");	
}

unsigned int
DFFExport::Version(void)
{				// Version number * 100 (i.e. v3.01 = 301)
	return VERSION;
}

void
DFFExport::ShowAbout(HWND hWnd)
{
}

int
DFFExport::DoExport(const TCHAR *filename, ExpInterface *ei, Interface *i, BOOL suppressPrompts, DWORD options)
{
	initRW();
	this->ifc = i;
	this->expifc = ei;

	if(!suppressPrompts)
		if(!DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_DFFEXPORT),
			i->GetMAXHWnd(), ExportDlgProc, (LPARAM)this))
			return 1;

	if(DFFExport::exportSkin)
		DFFExport::exportHAnim = 1;
	return writeDFF(filename);
}

BOOL
DFFExport::SupportsOptions(int ext, DWORD options)
{
	return(options == SCENE_EXPORT_SELECTED) ? TRUE : FALSE;
}
