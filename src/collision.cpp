#include "dffimp.h"

#define COLL 0x4C4C4F43

#define COLATTRIB_CLASSID Class_ID(0x19cb2748, 0x358e9f50)

IParamBlock2*
getColAttribs(INode *node)
{
	BaseObject *obj = getBaseObject(node);
	ICustAttribContainer *cc = obj->GetCustAttribContainer();
	if(cc){
		int nca = cc->GetNumCustAttribs();
		for(int j = 0; j < nca; j++){
			CustAttrib *a = cc->GetCustAttrib(j);
			if(a->ClassID() == COLATTRIB_CLASSID)
				return a->GetParamBlock(0);
		}
	}
	return NULL;
}

int
getSurface(IParamBlock2 *pb)
{
	Interval valid = FOREVER;
	int surf = 0;
	for(int i = 0; i < pb->NumParams(); i++){
		if(pb->GetLocalName(i) == MSTR(TEXT("surf"))){
			pb->GetValue(i, 0, surf, valid);
			break;
		}
	}
	return surf;
}

int
getPieceType(IParamBlock2 *pb)
{
	Interval valid = FOREVER;
	int piece = 0;
	for(int i = 0; i < pb->NumParams(); i++)
		if(pb->GetLocalName(i) == MSTR(TEXT("piece"))){
			pb->GetValue(i, 0, piece, valid);
			break;
		}
	return piece;
}

static void
convertSphere(CColSphere *colsphere, INode *spherenode)
{
	Interval valid = FOREVER;
	IParamBlock2 *pb = getColAttribs(spherenode);
	GeomObject *obj = (GeomObject*)spherenode->GetObjectRef();
	IParamArray *param = obj->GetParamBlock();
	Matrix3 objectTM = getObjectToLocalMatrix(spherenode);
	float radius;
	param->GetValue(SPHERE_RADIUS, 0, radius, valid);
	Matrix3 trans = getLocalMatrix(spherenode);
	Point3 pos = objectTM * trans.GetTrans();
	colsphere->center.set(pos.x, pos.y, pos.z);
	colsphere->radius = radius;
	colsphere->surface = getSurface(pb);
	colsphere->piece = getPieceType(pb);
}

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

static void
convertBox(CColBox *colbox, INode *boxnode)
{
	Interval valid = FOREVER;
	IParamBlock2 *pb = getColAttribs(boxnode);
	GeomObject *obj = (GeomObject*)boxnode->GetObjectRef();
	IParamArray *param = obj->GetParamBlock();
	Matrix3 objectTM = getObjectToLocalMatrix(boxnode);

	Point3 pos = getLocalMatrix(boxnode).GetTrans();
	float length, width, height;
	param->GetValue(BOXOBJ_LENGTH, 0, length, valid);
	param->GetValue(BOXOBJ_WIDTH, 0, width, valid);
	param->GetValue(BOXOBJ_HEIGHT, 0, height, valid);
	Point3 min = pos - Point3(width/2.0f, length/2.0f, 0.0f);
	Point3 max = pos + Point3(width/2.0f, length/2.0f, height);
	min = objectTM * min;
	max = objectTM * max;

	colbox->min.set(MIN(min.x, max.x), MIN(min.y, max.y), MIN(min.z, max.z));
	colbox->max.set(MAX(min.x, max.x), MAX(min.y, max.y), MAX(min.z, max.z));
	colbox->surface = getSurface(pb);
	colbox->piece = getPieceType(pb);
}

// Calculate bounding volumes from bounding box of selected node
// and collision volumes inside model
void
calculateBounds(CColModel *colmodel, INode *root)
{
	Box3 selbox;
	Interface *ifc = GetCOREInterface();
	Matrix3 trans = root->GetNodeTM(0);
	root->SetNodeTM(0, IdentityTM());
	ifc->SelectNode(root);
	ifc->GetSelectionWorldBox(0, selbox);
	root->SetNodeTM(0, trans);
	Point3 selmax = selbox.Max();
	Point3 selmin = selbox.Min();

/*
	Point3 center = box.Center();
	colmodel->boundingBox.min.set(min.x, min.y, min.z);
	colmodel->boundingBox.max.set(max.x, max.y, max.z);
	Point3 dim = (max - min)/2;
	colmodel->boundingSphere.radius = dim.Length();
	colmodel->boundingSphere.center.set(center.x, center.y, center.z);
*/
	int i;
	rw::BBox box;
	rw::V3d v;

	// initialize with selection box, because for some objects this is everything we have
	rw::V3d points[2];
	points[0].set(selmin.x, selmin.y, selmin.z);
	points[1].set(selmax.x, selmax.y, selmax.z);
	box.calculate(points, 2);

	// Now the collision volumes
	for(i = 0; i < colmodel->numSpheres; i++){
		v.x = colmodel->spheres[i].center.x + colmodel->spheres[i].radius;
		v.y = colmodel->spheres[i].center.y + colmodel->spheres[i].radius;
		v.z = colmodel->spheres[i].center.z + colmodel->spheres[i].radius;
		box.addPoint(&v);
		v.x = colmodel->spheres[i].center.x - colmodel->spheres[i].radius;
		v.y = colmodel->spheres[i].center.y - colmodel->spheres[i].radius;
		v.z = colmodel->spheres[i].center.z - colmodel->spheres[i].radius;
		box.addPoint(&v);
	}
	for(i = 0; i < colmodel->numBoxes; i++){
		box.addPoint(&colmodel->boxes[i].min);
		box.addPoint(&colmodel->boxes[i].max);
	}
	for(i = 0; i < colmodel->numLines; i++){
		box.addPoint(&colmodel->lines[i].p0);
		box.addPoint(&colmodel->lines[i].p1);
	}
	for(i = 0; i < colmodel->numTriangles; i++){
		v = colmodel->vertices[colmodel->triangles[i].a];
		box.addPoint(&v);
		v = colmodel->vertices[colmodel->triangles[i].b];
		box.addPoint(&v);
		v = colmodel->vertices[colmodel->triangles[i].c];
		box.addPoint(&v);
	}

	colmodel->boundingBox.min = box.inf;
	colmodel->boundingBox.max = box.sup;

	colmodel->boundingSphere.radius = rw::length(rw::scale(rw::sub(box.sup, box.inf), 0.5f));
	colmodel->boundingSphere.center = rw::scale(rw::add(box.sup, box.inf), 0.5f);

}

void
convertMesh(CColModel *colmodel, int vertOffset, INode *meshnode, Mesh *mesh)
{
	Interval valid = FOREVER;
	IParamBlock2 *pb = getColAttribs(meshnode);
	Matrix3 trans = getObjectToLocalMatrix(meshnode) * getLocalMatrix(meshnode);

	int faceOffset = colmodel->numTriangles;
	int numFaces = mesh->getNumFaces();
	int numVerts = mesh->getNumVerts();
	int surf = getSurface(pb);
	for(int i = 0; i < numFaces; i++){
		int f = i+faceOffset;
		colmodel->triangles[f].a = mesh->faces[i].v[0]+vertOffset;
		colmodel->triangles[f].b = mesh->faces[i].v[2]+vertOffset;
		colmodel->triangles[f].c = mesh->faces[i].v[1]+vertOffset;
		colmodel->triangles[f].surface = surf;
	}
	for(int i = 0; i < numVerts; i++){
		Point3 v = trans * mesh->getVert(i);
		colmodel->vertices[i+vertOffset].set(v.x, v.y, v.z);
	}
}

// strip /_L[0-2]$/ if found (actually strip /_.?.?$/ because i'm lazy)
char*
modelName(char *name)
{
	char *colname, *s;
	int l = strlen(name);
	colname = new char[l+1];
	strcpy(colname, name);
	s = strrchr(colname, '_');
	if(s == NULL)
		return colname;
	l = strlen(s);
	if(l <= 3 && l > 1)
		*s = '\0';
	return colname;
}

bool
COLExport::writeCOL1(const TCHAR *filename)
{
	char *colname;
	int numSelected = this->ifc->GetSelNodeCount();
	INode *rootnode = NULL;
	for(int i = 0; i < numSelected; i++){
		rootnode = this->ifc->GetSelNode(i);
		break;
	}
	if(rootnode == NULL){
		lprintf(_T("error: nothing selected\n"));
		return 0;
	}
	TCHAR *name = (TCHAR*)rootnode->GetName();
#ifdef _UNICODE
	char cname[MAX_PATH];
	wcstombs(cname, name, MAX_PATH);
	colname = modelName(cname);
#else
	colname = modelName(name);
#endif	
	lprintf(_T("exporting col %s\n"), colname);

	std::vector<INode*> spheres;
	std::vector<INode*> boxes;
	std::vector<INode*> meshes;
	int numChildren = rootnode->NumberOfChildren();
	for(int i = 0; i < numChildren; i++){
		INode *child = rootnode->GetChildNode(i);
		IParamBlock2 *pb = getColAttribs(child);
		BaseObject *obj = child->GetObjectRef();
		Class_ID id = obj->ClassID();
		if(pb){
			if(id == Class_ID(BOXOBJ_CLASS_ID, 0))
				boxes.push_back(child);
			else if(id == Class_ID(SPHERE_CLASS_ID, 0))
				spheres.push_back(child);
			else if(obj->SuperClassID() == GEOMOBJECT_CLASS_ID)
				meshes.push_back(child);
		}
	}
	CColModel *colmodel = new CColModel;
	colmodel->numSpheres = spheres.size();
	if(colmodel->numSpheres){
		colmodel->spheres = new CColSphere[colmodel->numSpheres];
		for(int i = 0; i < colmodel->numSpheres; i++)
			convertSphere(&colmodel->spheres[i], spheres[i]);
	}

	colmodel->numBoxes = boxes.size();
	if(colmodel->numBoxes){
		colmodel->boxes = new CColBox[colmodel->numBoxes];
		for(int i = 0; i < colmodel->numBoxes; i++)
			convertBox(&colmodel->boxes[i], boxes[i]);
	}

	colmodel->numLines = 0;
	colmodel->numTriangles = 0;
	int numVertices = 0;
	for(int i = 0; i < meshes.size(); i++){
		Mesh *m = NULL;
		::Object *o = meshes[i]->EvalWorldState(0).obj;
		assert(o->CanConvertToType(Class_ID(TRIOBJ_CLASS_ID, 0)));
		TriObject *tri = (TriObject*)o->ConvertToType(0, Class_ID(TRIOBJ_CLASS_ID, 0));
		m = &tri->GetMesh();
		colmodel->numTriangles += m->getNumFaces();
		numVertices += m->getNumVerts();
		if(tri != o)
			tri->DeleteMe();
	}
	if(numVertices && colmodel->numTriangles){
		colmodel->triangles = new CColTriangle[colmodel->numTriangles];
		colmodel->vertices = new rw::V3d[numVertices];
	}
	colmodel->numTriangles = 0;
	numVertices = 0;
	for(int i = 0; i < meshes.size(); i++){
		Mesh *m = NULL;
		::Object *o = meshes[i]->EvalWorldState(0).obj;
		assert(o->CanConvertToType(Class_ID(TRIOBJ_CLASS_ID, 0)));
		TriObject *tri = (TriObject*)o->ConvertToType(0, Class_ID(TRIOBJ_CLASS_ID, 0));
		m = &tri->GetMesh();
		convertMesh(colmodel, numVertices, meshes[i], m);
		colmodel->numTriangles += m->getNumFaces();
		numVertices += m->getNumVerts();
		if(tri != o)
			tri->DeleteMe();
	}
	calculateBounds(colmodel, rootnode);

	rw::StreamFile stream;
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

	char outname[24];
	struct {
		rw::uint32 ident;
		rw::uint32 size;
	} header;
	rw::uint8 *buf;
	header.ident = COLL;
	header.size = writeColModel(colmodel, &buf)+24;
	stream.write8(&header, 8);
	memset(outname, 0, 24);
	strncpy(outname, colname, 24);
	delete[] colname;
	stream.write8(outname, 24);
	stream.write8(buf, header.size-24);
	delete[] buf;
	stream.close();
	delete colmodel;
	return 1;
}

COLExport::COLExport(void)
{
}

COLExport::~COLExport(void)
{
}

int
COLExport::ExtCount(void)
{
	return 1;
}

const TCHAR*
COLExport::Ext(int n)
{
	switch(n){
	case 0:
		return _T("COL");
	}
	return _T("");
}

const TCHAR*
COLExport::LongDesc(void)
{
	return _T("Rockstar COL file");
}
	
const TCHAR*
COLExport::ShortDesc(void)
{
	return _T("Rockstar Collision");
}

const TCHAR*
COLExport::AuthorName(void)
{
	return _T(STR_AUTHOR); //GetStringT(IDS_AUTHOR);
}

const TCHAR*
COLExport::CopyrightMessage(void)
{
	return _T(STR_COPYRIGHT); //GetStringT(IDS_COPYRIGHT);
}

const TCHAR *
COLExport::OtherMessage1(void)
{
	return _T("");
}

const TCHAR *
COLExport::OtherMessage2(void)
{
	return _T("");	
}

unsigned int
COLExport::Version(void)
{				// Version number * 100 (i.e. v3.01 = 301)
	return VERSION;
}

void
COLExport::ShowAbout(HWND hWnd)
{
}

int
COLExport::DoExport(const TCHAR *filename, ExpInterface *ei, Interface *i, BOOL suppressPrompts, DWORD options)
{
	this->ifc = i;
	this->expifc = ei;

	//if(!suppressPrompts)
	//	if(!DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_DFFEXPORT),
	//		i->GetMAXHWnd(), ExportDlgProc, (LPARAM)this))
	//		return 1;

	if(!writeCOL1(filename))
		return 0;
	return 1;
}

BOOL
COLExport::SupportsOptions(int ext, DWORD options)
{
	return(options == SCENE_EXPORT_SELECTED) ? TRUE : FALSE;
}
