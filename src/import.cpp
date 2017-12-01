#include "dffimp.h"

int DFFImport::convertHierarchy = 1;
int DFFImport::autoSmooth = 1;
int DFFImport::prepend;
float DFFImport::smoothingAngle = 45.0f;
int DFFImport::explicitNormals = 0;

std::vector<INode*> DFFImport::lastImported;

static int
findFirstVertex(rw::V3d *verts, int n, rw::V3d *v)
{
	// compare as int because NaN != NaN (not seen with vertices yet, but be safe)
	rw::uint32 *iverts, *iv;
	iv = (rw::uint32*)v;
	iverts = (rw::uint32*)verts;
	for(int i = 0; i < n; i++) {
		if(iverts[0] == iv[0] && iverts[1] == iv[1] && iverts[2] == iv[2])
			return i;
		iverts += 3;
	}
	/* shouldn't happen */
	return -1;
}

static int
findFirstTexCoord(rw::TexCoords *verts, int n, rw::TexCoords *v)
{
	// compare as int because NaN != NaN and some files have that
	rw::uint32 *iverts, *iv;
	iv = (rw::uint32*)v;
	iverts = (rw::uint32*)verts;
	for(int i = 0; i < n; i++) {
		if(iverts[0] == iv[0] && iverts[1] == iv[1])
			return i;
		iverts += 2;
	}
	/* shouldn't happen */
	return -1;
}

static int
findFirstColor(rw::RGBA *verts, int n, rw::RGBA *v)
{
	for(int i = 0; i < n; i++) {
		if(rw::equal(*verts, *v))
			return i;
		verts++;
	}
	/* shouldn't happen */
	return -1;
}

static void
animateTexture(rw::Animation *anim, BitmapTex *tex)
{
	using namespace rw;
	if(tex == NULL)
		return;
	UVAnimCustomData *cust = (UVAnimCustomData*)anim->customData;
	StdUVGen *uvgen = tex->GetUVGen();
	int channel = uvgen->GetMapChannel();
	if(cust->nodeToUVChannel[0] != channel-1)
		return;
	if(anim->interpInfo->id != 0x1C1){
		lprintf(_T("warning: cannot import animations of type %x\n"),
		        anim->interpInfo->id);
		return;
	}
	UVAnimKeyFrame *kf = (UVAnimKeyFrame*)anim->keyframes;
	AnimateOn();
	for(int32 i = 0; i < anim->numFrames; i++){
		TimeValue t = SecToTicks(kf->time);
		uvgen->SetWAng(kf->uv[0], t);
		uvgen->SetUScl(kf->uv[1], t);
		uvgen->SetVScl(kf->uv[2], t);
		// no skew :/
		uvgen->SetUOffs(-kf->uv[4], t);
		uvgen->SetVOffs(kf->uv[5], t);
		kf++;
	}
	AnimateOff();
}

void
makeTexturePath(MCHAR *in, MCHAR *out)
{
	MCHAR test[MAX_PATH];
	MCHAR *exsts[] = { _M(".tga"), _M(".bmp"), _M(".png"), _M(".jpg"), _M(".dds"), NULL };
	for(int i = 0; exsts[i]; i++){
		_tcscpy(test, in);
		_tcscat(test, exsts[i]);
		if(BMMGetFullFilename(test, out))
			return;
	}
	_tcscpy(out, in);
	_tcscat(out, exsts[0]);
}

static BitmapTex*
makeTexture(rw::Texture *tex)
{
	using namespace rw;
	static MCHAR bmpname[MAX_PATH];
	static MCHAR fullname[MAX_PATH];
	static WCHAR fuckyou[MAX_PATH];
	static char texname[MAX_PATH];

	BitmapTex *bmtex;
	if(tex == NULL)
		return NULL;
	bmtex = NewDefaultBitmapTex();

#ifdef _UNICODE
	mbstowcs(bmpname, tex->name, MAX_PATH);
#else
	strncpy(bmpname, tex->name, MAX_PATH);
#endif
	makeTexturePath(bmpname, fullname);
	bmtex->SetMapName(fullname);
/*
	strncpy(texname, tex->name, 32);
	strcat(texname, ".tga");
#ifdef _UNICODE
	mbstowcs(fuckyou, texname, MAX_PATH);
	bmtex->SetMapName(fuckyou);
#else
	bmtex->SetMapName(texname);
#endif
*/
	switch(tex->filterAddressing & 0xFF){
	case Texture::NEAREST:
		bmtex->SetFilterType(FILTER_NADA);
		break;
	case Texture::LINEAR:
		bmtex->SetFilterType(FILTER_SAT);
		break;
	case Texture::LINEARMIPLINEAR:
		bmtex->SetFilterType(FILTER_PYR);
		break;
	}
	switch(tex->filterAddressing>>8 & 0xF){
	case Texture::WRAP:
		bmtex->GetUVGen()->SetFlag(U_WRAP, 1);
		break;
	case Texture::MIRROR:
		bmtex->GetUVGen()->SetFlag(U_MIRROR, 1);
		break;
	}
	switch(tex->filterAddressing>>12 & 0xF){
	case Texture::WRAP:
		bmtex->GetUVGen()->SetFlag(V_WRAP, 1);
		break;
	case Texture::MIRROR:
		bmtex->GetUVGen()->SetFlag(V_MIRROR, 1);
		break;
	}
	return bmtex;
}

void
DFFImport::makeMaterials(rw::Atomic *a, INode *inode)
{
	using namespace rw;
	using namespace gta;

	Geometry *g = a->geometry;
	MultiMtl *multi = NULL;

	if(g->matList.numMaterials == 0)
		return;	// can this happen?
	if(g->matList.numMaterials > 1){
		multi = NewDefaultMultiMtl();
		multi->SetNumSubMtls(g->matList.numMaterials);
	}

	for(int32 i = 0; i < g->matList.numMaterials; i++){
		rw::Material *m = g->matList.materials[i];
		Mtl *mat = (Mtl*)ifc->CreateInstance(SClass_ID(MATERIAL_CLASS_ID), Class_ID(0x29b71842, 0x52508b70));
		IParamBlock2 *pb = mat->GetParamBlock(0);

		pb->SetValue(mat_color, 0, Color(m->color.red/255.0f, m->color.green/255.0f, m->color.blue/255.0f));
		pb->SetValue(mat_coloralpha, 0, m->color.alpha/255.0f);
		pb->SetValue(mat_sp_ambient, 0, m->surfaceProps.ambient);
		pb->SetValue(mat_sp_diffuse, 0, m->surfaceProps.diffuse);
		pb->SetValue(mat_sp_specular, 0, m->surfaceProps.specular);

		BitmapTex *diffmap = NULL;
		if(m->texture)
			mat->GetParamBlock(0)->SetValue(mat_texmap_texture, 0, diffmap = makeTexture(m->texture));

		MatFX *matfx = *PLUGINOFFSET(MatFX*, m, matFXGlobals.materialOffset);
		int32 idx;
		BitmapTex *bmtex;
		BitmapTex *dualmap = NULL;
		if(matfx){
			pb->SetValue(mat_matfxeffect, 0, (int)matfx->type+1);
			switch(matfx->type){
			case MatFX::BUMPMAP:
				idx = matfx->getEffectIndex(MatFX::BUMPMAP);
				assert(idx >= 0);
				bmtex = makeTexture(matfx->fx[idx].bump.bumpedTex);
				pb->SetValue(mat_texmap_bumpmap, 0, bmtex);
				pb->SetValue(mat_bumpmap_amount, 0, matfx->fx[idx].bump.coefficient);
				break;
	
			case MatFX::ENVMAP:
				idx = matfx->getEffectIndex(MatFX::ENVMAP);
				assert(idx >= 0);
				bmtex = makeTexture(matfx->fx[idx].env.tex);
				pb->SetValue(mat_texmap_envmap, 0, bmtex);
				pb->SetValue(mat_envmap_amount, 0, matfx->fx[idx].env.coefficient);
				break;
	
			case MatFX::DUAL:
				idx = matfx->getEffectIndex(MatFX::DUAL);
				assert(idx >= 0);
				dualmap = makeTexture(matfx->fx[idx].dual.tex);
				if(dualmap)
					dualmap->GetUVGen()->SetMapChannel(2);
				pb->SetValue(mat_texmap_pass2, 0, dualmap);
				pb->SetValue(mat_pass2_srcblend, 0, matfx->fx[idx].dual.srcBlend);
				pb->SetValue(mat_pass2_destblend, 0, matfx->fx[idx].dual.dstBlend);
				break;
	
			case MatFX::BUMPENVMAP:
				idx = matfx->getEffectIndex(MatFX::ENVMAP);
				assert(idx >= 0);
				bmtex = makeTexture(matfx->fx[idx].env.tex);
				pb->SetValue(mat_texmap_envmap, 0, bmtex);
				pb->SetValue(mat_envmap_amount, 0, matfx->fx[idx].env.coefficient);
				idx = matfx->getEffectIndex(MatFX::BUMPMAP);
				assert(idx >= 0);
				bmtex = makeTexture(matfx->fx[idx].bump.bumpedTex);
				pb->SetValue(mat_texmap_bumpmap, 0, bmtex);
				pb->SetValue(mat_bumpmap_amount, 0, matfx->fx[idx].bump.coefficient);
				break;
			}
		}

		// R* extensions
		IParamBlock2 *pbr = mat->GetParamBlock(1);
		EnvMat *env = *PLUGINOFFSET(EnvMat*, m, envMatOffset);
		if(env){
			pbr->SetValue(mat_enEnv, 0, TRUE, 0);
			pbr->SetValue(mat_scaleX, 0, env->scaleX/8.0f);
			pbr->SetValue(mat_scaleY, 0, env->scaleY/8.0f);
			pbr->SetValue(mat_transScaleX, 0, env->transScaleX/8.0f);
			pbr->SetValue(mat_transScaleY, 0, env->transScaleY/8.0f);
			pbr->SetValue(mat_shininess, 0, env->shininess/255.0f);
		}

		SpecMat *spec = *PLUGINOFFSET(SpecMat*, m, specMatOffset);
		if(spec){
			pbr->SetValue(mat_enSpec, 0, TRUE, 0);
			pbr->SetValue(mat_specularity, 0, spec->specularity);
			pbr->SetValue(mat_specMap, 0, makeTexture(spec->texture));
		}

		UVAnim *uvanim = PLUGINOFFSET(UVAnim, m, uvAnimOffset);
		for(int j = 0; j < 8; j++)
			if(uvanim->interp[j]){
				animateTexture(uvanim->interp[j]->currentAnim, diffmap);
				animateTexture(uvanim->interp[j]->currentAnim, dualmap);
			}

		if(g->matList.numMaterials == 1){
			inode->SetMtl(mat);
			return;
		}
		MSTR str = _T("");
		multi->SetSubMtlAndName(i, mat, str);
	}
	inode->SetMtl(multi);
}

void
DFFImport::makeMesh(rw::Atomic *a, Mesh *maxmesh)
{
	using namespace rw;

	Geometry *g = a->geometry;

	// not all GTA tools generate face info correctly :(
// currently broken
//	if(g->meshHeader){
//		delete[] g->triangles;
//		g->triangles = NULL;
//		g->numTriangles = 0;
//		g->generateTriangles();
//	}

	V3d *gverts = g->morphTargets[0].vertices;
	V3d *gnorms = g->morphTargets[0].normals;
	int *newind = new int[g->numVertices];
	for(int32 i = 0; i < g->numVertices; i++){
		int j = findFirstVertex(gverts, g->numVertices, &gverts[i]);
		assert(j >= 0);
		newind[i] = j;
	}
	maxmesh->setNumVerts(g->numVertices);
	Point3 maxvert;
	for(int32 i = 0; i < g->numVertices; i++){
		int j = newind[i];
		maxvert[0] = gverts[j].x;
		maxvert[1] = gverts[j].y;
		maxvert[2] = gverts[j].z;
		maxmesh->setVert(i, maxvert);
	}
	maxmesh->setNumFaces(g->numTriangles);
	for(int32 i = 0; i < g->numTriangles; i++){
		maxmesh->faces[i].setVerts(newind[g->triangles[i].v[0]],
		                           newind[g->triangles[i].v[1]],
		                           newind[g->triangles[i].v[2]]);
		maxmesh->faces[i].setSmGroup(0);
		maxmesh->faces[i].setMatID(g->triangles[i].matId);
		maxmesh->faces[i].setEdgeVisFlags(1, 1, 1);
	}

	if(explicitNormals && g->flags & rw::Geometry::NORMALS){
		for(int32 i = 0; i < g->numVertices; i++){
			int j = findFirstVertex(gnorms, g->numVertices, &gnorms[i]);
			assert(j >= 0);
			newind[i] = j;
		}
		maxmesh->SpecifyNormals();
		MeshNormalSpec *normalSpec = maxmesh->GetSpecifiedNormals();
		normalSpec->ClearNormals();
		normalSpec->SetNumNormals(g->numVertices);
		for(int32 i = 0; i < g->numTriangles; i++){
			int j = newind[i];
			maxvert[0] = gnorms[j].x;
			maxvert[1] = gnorms[j].y;
			maxvert[2] = gnorms[j].z;
			normalSpec->Normal(i) = maxvert;
			normalSpec->SetNormalExplicit(i, true);
		}
		normalSpec->SetNumFaces(g->numTriangles);
		MeshNormalFace *faces = normalSpec->GetFaceArray();
		for(int32 i = 0; i < g->numTriangles; i++){
			faces[i].SpecifyAll();
			faces[i].SetNormalID(0, newind[g->triangles[i].v[0]]);
			faces[i].SetNormalID(1, newind[g->triangles[i].v[1]]);
			faces[i].SetNormalID(2, newind[g->triangles[i].v[2]]);
		}
	}

	for(int k = 0; k < g->numTexCoordSets; k++){
		maxmesh->setMapSupport(MAP_TEXCOORD0+k, TRUE);
		TexCoords *uvs = g->texCoords[k];
		for(int32 i = 0; i < g->numVertices; i++){
			int j = findFirstTexCoord(uvs, g->numVertices, &uvs[i]);
			assert(j >= 0);
			newind[i] = j;
		}
		maxmesh->setNumMapVerts(MAP_TEXCOORD0+k, g->numVertices);
		Point3 maxvert;
		for(int32 i = 0; i < g->numVertices; i++){
			int j = newind[i];
			maxvert[0] = uvs[j].u;
			maxvert[1] = 1-uvs[j].v;
			maxvert[2] = 0.0f;
			maxmesh->setMapVert(MAP_TEXCOORD0+k, i, maxvert);
		}
		maxmesh->setNumMapFaces(MAP_TEXCOORD0+k, g->numTriangles);
		TVFace *tvf = maxmesh->mapFaces(MAP_TEXCOORD0+k);
		for(int32 i = 0; i < g->numTriangles; i++){
			tvf[i].setTVerts(newind[g->triangles[i].v[0]],
			                 newind[g->triangles[i].v[1]],
			                 newind[g->triangles[i].v[2]]);
		}
	}

	if(g->flags & rw::Geometry::PRELIT){
		if(!maxmesh->mapSupport(MAP_ALPHA))
			maxmesh->setMapSupport(MAP_ALPHA, TRUE);
		rw::RGBA *prelit = g->colors;
		for(int32 i = 0; i < g->numVertices; i++){
			int j = findFirstColor(prelit, g->numVertices, &prelit[i]);
			assert(j >= 0);
			newind[i] = j;
		}
		maxmesh->setNumVertCol(g->numVertices);
		maxmesh->setNumMapVerts(MAP_ALPHA, g->numVertices);
		Point3 maxvert;
		for(int32 i = 0; i < g->numVertices; i++){
			int j = newind[i];
			maxvert[0] = prelit[j].red/255.0f;
			maxvert[1] = prelit[j].green/255.0f;
			maxvert[2] = prelit[j].blue/255.0f;
			maxmesh->vertCol[i] = maxvert;
			maxvert[0] = maxvert[1] = maxvert[2] = prelit[j].alpha/255.0f;
			maxmesh->setMapVert(MAP_ALPHA, i, maxvert);
		}
		maxmesh->setNumVCFaces(g->numTriangles);
		maxmesh->setNumMapFaces(MAP_ALPHA, g->numTriangles);
		TVFace *af = maxmesh->mapFaces(MAP_ALPHA);
		for(int32 i = 0; i < g->numTriangles; i++){
			maxmesh->vcFace[i].setTVerts(newind[g->triangles[i].v[0]],
			                             newind[g->triangles[i].v[1]],
			                             newind[g->triangles[i].v[2]]);
			af[i].setTVerts(maxmesh->vcFace[i].getAllTVerts());
		}
	}

	gta::ExtraVertColors *colordata = PLUGINOFFSET(gta::ExtraVertColors, g, gta::extraVertColorOffset);
	if(colordata->nightColors){
		maxmesh->setMapSupport(MAP_EXTRACOLORS, TRUE);
		maxmesh->setMapSupport(MAP_EXTRAALPHA, TRUE);
		rw::RGBA *prelit = colordata->nightColors;
		for(int32 i = 0; i < g->numVertices; i++){
			int j = findFirstColor(prelit, g->numVertices, &prelit[i]);
			assert(j >= 0);
			newind[i] = j;
		}
		maxmesh->setNumMapVerts(MAP_EXTRACOLORS, g->numVertices);
		maxmesh->setNumMapVerts(MAP_EXTRAALPHA, g->numVertices);
		Point3 maxvert;
		for(int32 i = 0; i < g->numVertices; i++){
			int j = newind[i];
			maxvert[0] = prelit[j].red/255.0f;
			maxvert[1] = prelit[j].green/255.0f;
			maxvert[2] = prelit[j].blue/255.0f;
			maxmesh->setMapVert(MAP_EXTRACOLORS, i, maxvert);
			maxvert[0] = maxvert[1] = maxvert[2] = prelit[j].alpha/255.0f;
			maxmesh->setMapVert(MAP_EXTRAALPHA, i, maxvert);
		}
		maxmesh->setNumMapFaces(MAP_EXTRACOLORS, g->numTriangles);
		maxmesh->setNumMapFaces(MAP_EXTRAALPHA, g->numTriangles);
		TVFace *cf = maxmesh->mapFaces(MAP_EXTRACOLORS);
		TVFace *af = maxmesh->mapFaces(MAP_EXTRAALPHA);
		for(int32 i = 0; i < g->numTriangles; i++){
			cf[i].setTVerts(newind[g->triangles[i].v[0]],
			                newind[g->triangles[i].v[1]],
			                newind[g->triangles[i].v[2]]);
			af[i].setTVerts(cf[i].getAllTVerts());
		}
	}

	maxmesh->DeleteIsoVerts();
	maxmesh->DeleteIsoMapVerts();

	// cargo cult
	maxmesh->buildNormals();
	maxmesh->buildBoundingBox();
	maxmesh->InvalidateEdgeList();
	maxmesh->InvalidateGeomCache();

	if(DFFImport::autoSmooth)
		maxmesh->AutoSmooth(DegToRad(DFFImport::smoothingAngle), 0, 1);

	delete[] newind;
}

// TODO: delete
static void
getBonePositions(rw::Skin *skin, rw::HAnimHierarchy *hier, Point3 *positions)
{
	using namespace rw;

	::Matrix3 matrix;
	for(int32 i = 0; i < skin->numBones; i++){
		float *m = &skin->inverseMatrices[i*16];
		matrix.SetRow(0, Point3(m[0],  m[1],  m[2]));
		matrix.SetRow(1, Point3(m[4],  m[5],  m[6]));
		matrix.SetRow(2, Point3(m[8],  m[9],  m[10]));
		matrix.SetRow(3, Point3(m[12], m[13], m[14]));
		matrix.Invert();
		Point3 pos = matrix.GetRow(3);
		*positions++ = pos;
	}
}

static int
findVertex(rw::Geometry *g, Point3 *v)
{
	using namespace rw;
	V3d *verts = g->morphTargets[0].vertices;
	for(int32 i = 0; i < g->numVertices; i++){
		if(abs(verts[i].x-v->x) < 0.0001f &&
		   abs(verts[i].y-v->y) < 0.0001f &&
		   abs(verts[i].z-v->z) < 0.0001f)
			return i;
	}
	return -1;
}

static ::Mesh*
getMesh(INode *node, TimeValue t)
{
	::Object *o = node->EvalWorldState(t).obj;
	if(o->CanConvertToType(Class_ID(TRIOBJ_CLASS_ID, 0))){
		TriObject *tri = (TriObject*)o->ConvertToType(t, Class_ID(TRIOBJ_CLASS_ID, 0));
		// TODO: if tri != o delete!
		return &tri->GetMesh();
	}
	return nil;
}

static rw::Skin*
getSkin(rw::Atomic *a)
{
	return rw::Skin::get(a->geometry);
}

void
DFFImport::axesHeuristics(rw::Frame *f)
{
	static rw::Matrix bipmat = {
		{0.0f, 0.0f, 1.0f}, rw::Matrix::TYPEORTHONORMAL,
		{1.0f, 0.0f, 0.0f}, 0,
		{0.0f, 1.0f, 0.0f}, 0,
		{0.0f, 0.0f, 0.0f}, 0
	};

	// If the root transform is not identity we're in
	// RW world coordinates.
	// But we have to be careful with the flags, IDENTITY is not set by frame stream read.
	rw::Matrix root = f->matrix;
	root.optimize();
	this->isWorldSpace = !(root.flags & rw::Matrix::IDENTITY);

	// This heuristic is wrong if we aren't in world space
	// and the root's only child happens to have a biped matrix.
	// AND this with the existance of a hierarchy after the call
	// to improve the guess (who uses bipeds without a hierarchy?).
	rw::Frame *child = f->child;
	this->isBiped = 0;
	if(child && child->next == NULL)
		this->isBiped = rw::equal(child->matrix.right, bipmat.right) &&
			rw::equal(child->matrix.at, bipmat.at) &&
			rw::equal(child->matrix.up, bipmat.up) &&
			rw::equal(child->matrix.pos, bipmat.pos);
}

void
printHier(INode *node, int indent)
{
	for(int i = 0; i < indent; i++)
		lprintf(_T("   "));
	lprintf(_T("%s\n"), node->NodeName());
	for(int i = 0; i < node->NumberOfChildren(); i++)
		printHier(node->GetChildNode(i), indent+1);
}

void
DFFImport::saveNodes(INode *node)
{
	lastImported.push_back(node);
	for(int i = 0; i < node->NumberOfChildren(); i++)
		saveNodes(node->GetChildNode(i));
}

static int
getNumChildren(rw::Frame *f)
{
	int n = 0;
	rw::Frame *c;
	for(c = f->child; c; c = c->next)
		n++;
	return n;
}

static int
getChildIndex(rw::Frame *child)
{
	int n = 0;
	rw::Frame *c;
	if(child->getParent() == nil)
		return 0;
	for(c = child->getParent()->child; c; c = c->next){
		if(c == child)
			return n;
		n++;
	}
	// cannot happen
	return -1;
}

//#define USE_IMPNODES 0

BOOL
DFFImport::dffFileRead(const TCHAR *filename)
{
	using namespace rw;

	StreamFile in;
	Clump *c;

#ifdef _UNICODE
	char path[MAX_PATH];
	wcstombs(path, filename, MAX_PATH);
	if(in.open(path, "rb") == NULL)
#else
	if(in.open(filename, "rb") == NULL)
#endif
		return 0;
	currentUVAnimDictionary = NULL;
	TexDictionary::setCurrent(TexDictionary::create());
	debugFile = (char*)filename;
	ChunkHeaderInfo header;
	readChunkHeaderInfo(&in, &header);
	if(header.type == ID_UVANIMDICT){
		UVAnimDictionary *dict = UVAnimDictionary::streamRead(&in);
		currentUVAnimDictionary = dict;
		readChunkHeaderInfo(&in, &header);
	}
	if(header.type != ID_CLUMP){
		in.close();
		return 0;
	}
	c = Clump::streamRead(&in);
	in.close();
	if(c == NULL)
		return 0;

	int32 platform = findPlatform(c);
	if(platform){
		rw::platform = platform;
		switchPipes(c, platform);
	}

	FORLIST(lnk, c->atomics){
		Atomic *a = Atomic::fromClump(lnk);
		a->uninstance();
		ps2::unconvertADC(a->geometry);
	}


	//
	// Import
	//

	TimeValue t = ifc->GetTime();
	int32 numFrames = c->getFrame()->count();
	Frame **flist = new Frame*[numFrames];
	makeFrameList(c->getFrame(), flist);

	HAnimHierarchy *hier = HAnimHierarchy::find(c->getFrame());
	if(hier)
		hier->attach();
	int numAtomics = c->countAtomics();
	INode **skinNodes = nil;
	Atomic **skinAtomics = nil;
	if(numAtomics > 0){
		skinNodes = new INode*[numAtomics];
		skinAtomics = new Atomic*[numAtomics];
	}
	int numSkinned = 0;

	INode *bones[256];
	memset(bones, 0, sizeof(bones));

	axesHeuristics(c->getFrame());
	this->isBiped = this->isBiped && hier != nil;
	//lprintf("%s %s\n",
	//	this->isWorldSpace ? "world space" : "",
	//	this->isBiped ? "biped" : "");

	INode **nodelist = new INode*[numFrames];
#ifdef USE_IMPNODES
	ImpNode **impnodelist = new ImpNode*[numFrames];
#endif
	for(int j = 0; j < numFrames; j++){
		Frame *f = flist[j];

		INode *node;
#ifdef USE_IMPNODES
		ImpNode *impnode = impifc->CreateNode();
		impifc->AddNodeToScene(impnode);
		node = impnode->GetINode();
		impnodelist[j] = impnode;
#else
		int hasObject = 0;
		// Dummy as placeholder
		::Object *obj = (::Object*)ifc->CreateInstance(HELPER_CLASS_ID, Class_ID(DUMMY_CLASS_ID, 0));
		node = ifc->CreateObjectNode(obj);
#endif
		nodelist[j] = node;
		::Matrix3 tm;
		MRow *m = tm.GetAddr();
		rw::Matrix *rwm = f->getLTM();
		m[0][0] = rwm->right.x;
		m[0][1] = rwm->right.y;
		m[0][2] = rwm->right.z;
		m[1][0] = rwm->up.x;
		m[1][1] = rwm->up.y;
		m[1][2] = rwm->up.z;
		m[2][0] = rwm->at.x;
		m[2][1] = rwm->at.y;
		m[2][2] = rwm->at.z;
		m[3][0] = rwm->pos.x;
		m[3][1] = rwm->pos.y;
		m[3][2] = rwm->pos.z;
#ifdef USE_IMPNODES
		impnode->SetTransform(t, tm);
#else
		node->SetNodeTM(t, tm);
#endif
		int isbone = 0;

		// HAnim
		if(hier){
			HAnimData *hanim = HAnimData::get(f);
			if(hanim->id >= 0){
				isbone = 1;
				node->SetUserPropInt(_T("tag"), hanim->id);
				bones[hier->getIndex(hanim->id)] = node;
			}
		}

		if(f->getParent() && getNumChildren(f->getParent()) > 1)
			node->SetUserPropInt(_T("childNum"), getChildIndex(f));

		// GTA Node name
		char *name = gta::getNodeName(f);
		char maxname[32];
		if(name[0]){
			if(DFFImport::prepend){
				maxname[0] = '!';
				maxname[1] = '\0';
				strcat(maxname, name);
			}else
				strncpy(maxname, name, 32);
#ifdef _UNICODE
			WCHAR fuckyou[50];
			mbstowcs(fuckyou, maxname, 50);
#ifdef USE_IMPNODES
			impnode->SetName(fuckyou);
#else
			node->SetName(fuckyou);
#endif
#else
#ifdef USE_IMPNODES
			impnode->SetName(maxname);
#else
			node->SetName(maxname);
#endif
#endif
		}

		// Objects
		int flipz = 0;
		FORLIST(lnk, f->objectList){
			ObjectWithFrame *obj = ObjectWithFrame::fromFrame(lnk);
			if(obj->object.type == Atomic::ID){
				Atomic *a = (Atomic*)obj;
				TriObject *tri = CreateNewTriObject();
				if(!tri) return 0;
				::Mesh *msh = &tri->GetMesh();
				makeMesh(a, msh);
				// make a new node outside the hierarchy for skinned objects
				if(getSkin(a)){
					INode *skinNode;
#ifdef USE_IMPNODES
					ImpNode *impnode = impifc->CreateNode();
					impifc->AddNodeToScene(impnode);
					impnode->Reference(tri);
					skinNode = impnode->GetINode();
					impnode->SetTransform(t, node->GetNodeTM(t));
#else
					skinNode = ifc->CreateObjectNode(tri);
					skinNode->SetNodeTM(t, node->GetNodeTM(t));
#endif
					makeMaterials(a, skinNode);
					skinAtomics[numSkinned] = a;
					skinNodes[numSkinned] = skinNode;
					numSkinned++;
				}else{
#ifdef USE_IMPNODES
					impnode->Reference(tri);
#else
					node->SetObjectRef(tri);
					hasObject = 1;
#endif
					makeMaterials(a, node);
					break;
				}
			}else if(obj->object.type == rw::Light::ID){
				rw::Light *l = (rw::Light*)obj;
				int maxtype;
				int type = l->getType();
				switch(type){
				case rw::Light::SOFTSPOT:
				case rw::Light::SPOT:
					maxtype = FSPOT_LIGHT;
					break;
				case rw::Light::DIRECTIONAL:
					maxtype = DIR_LIGHT;
					break;
				case rw::Light::POINT:
				case rw::Light::AMBIENT:
				default:
					maxtype = OMNI_LIGHT;
					break;
				}
				GenLight *gl = impifc->CreateLightObject(maxtype);
				gl->SetRGBColor(t, Point3(l->color.red, l->color.green, l->color.blue));
				if(type >= rw::Light::POINT)
					gl->SetAtten(t, ATTEN_END, l->radius);
				if(maxtype == FSPOT_LIGHT){
					float32 a = l->getAngle()*2*180.0f/pi;
					gl->SetFallsize(t, a);
					a -= type == rw::Light::SPOT ? 2.0f : 10.0f;
					gl->SetHotspot(t, a);
				}
				gl->SetAmbientOnly(type == rw::Light::AMBIENT);
#ifdef USE_IMPNODES
				impnode->Reference(gl);
#else
				node->SetObjectRef(gl);
				hasObject = 1;
#endif
				node->SetWireColor(0xFF00E5FF);
				ifc->AddLightToScene(node);
				gl->Enable(TRUE);
				flipz = 1;
				break;
			}else if(obj->object.type == rw::Camera::ID){
				rw::Camera *c = (rw::Camera*)obj;
				GenCamera *gc = impifc->CreateCameraObject(c->projection == 1 ? FREE_CAMERA : PARALLEL_CAMERA);
#ifdef USE_IMPNODES
				impnode->Reference(gc);
#else
				node->SetObjectRef(gc);
				hasObject = 1;
#endif
				gc->SetFOV(t, atan(c->viewWindow.x)*2);
				gc->SetManualClip(1);
				gc->SetClipDist(t, CAM_HITHER_CLIP, c->nearPlane);
				gc->SetClipDist(t, CAM_YON_CLIP, c->farPlane);
				gc->Enable(TRUE);
				flipz = 1;
				break;
			}
		}
#ifdef USE_IMPNODES
		if(node->GetObjectRef() == nil)
			impnode->Reference((::Object*)ifc->CreateInstance(HELPER_CLASS_ID, Class_ID(DUMMY_CLASS_ID, 0)));
#else
	// is it deleted when the reference changes?
//		if(hasObject){
//			obj->DeleteMe();
//			obj = nil;
//		}
#endif

		if(flipz){
			AngAxis aa(Point3(0,1,0), pi);
			node->Rotate(t, tm, aa);	// ImpNode crash
		}
	}

#if 0
	Point3 positions[256];
	if(skinAtomics && hier){
		rw::Skin *rwskin = rw::Skin::get(skinAtomics[0]->geometry);
		Matrix3 matrix;
		rw::Matrix *m = skinAtomics[0]->getFrame()->getLTM();
		matrix.SetRow(0, Point3(m->right.x,  m->right.y,  m->right.z));
		matrix.SetRow(1, Point3(m->up.x,  m->up.y,  m->up.z));
		matrix.SetRow(2, Point3(m->at.x,  m->at.y,  m->at.z));
		matrix.SetRow(3, Point3(m->pos.x,  m->pos.y,  m->pos.z));
		getBonePositions(rwskin, hier, positions);
		matrix.TransformPoints(positions, hier->numNodes);
		for(int32 i = 0; i < hier->numNodes; i++){
			INode *n = ifc->CreateObjectNode((::Object*)ifc->CreateInstance(HELPER_CLASS_ID, Class_ID(DUMMY_CLASS_ID, 0)));
			n->SetNodeTM(t, TransMatrix(positions[i]));
		}
	}
#endif

	/* build hierarchy */
	INode *rootnode = NULL;
	for(int j = 0; j < numFrames; j++){
		if(flist[j]->getParent() == NULL){
			rootnode = nodelist[j];
			continue;
		}
		int frm = findPointer(flist[j]->getParent(), (void**)flist, numFrames);
		// keepTM is broken for ImpNodes so do it manually
		::Matrix3 tm = nodelist[j]->GetNodeTM(0);
		nodelist[frm]->AttachChild(nodelist[j], 0);
		nodelist[j]->SetNodeTM(0, tm);
	}

	// Convert RW to Max world space.
	// Rotate Bipeds too because it looks nicer.
	if(this->convertHierarchy &&
	   (this->isWorldSpace || this->isBiped)){
		AngAxis aa(Point3(1,0,0), -pi/2);
		::Matrix3 tm;
		tm.IdentityMatrix();
		rootnode->Rotate(t, tm, aa);		// ImpNode crash
		for(int i = 0; i < numSkinned; i++)
			skinNodes[i]->Rotate(t, tm, aa);	// ImpNode crash
	}
	// Remember whether this was a biped for exporting
	if(this->isBiped && rootnode->NumberOfChildren() == 1)
		rootnode->GetChildNode(0)->SetUserPropBool(_T("fakeBiped"), TRUE);

	//
	// Skin
	//

	for(int i = 0; i < numSkinned && hier; i++){
		INode *skinNode = skinNodes[i];
		Skin *rwskin = getSkin(skinAtomics[i]);
		if(rwskin->numBones != hier->numNodes){
			lprintf(_T("number of bones doesn't match hierarchy\n"));
			goto noskin;
		}
		for(int j = 0; j < hier->numNodes; j++){
			if(bones[j] == NULL){
				lprintf(_T("bone missing\n"));
				goto noskin;
			}
			bones[j]->ShowBone(2);
			bones[j]->SetWireColor(0xFF00FFFF);
		}
		Modifier *skin = (Modifier*)ifc->CreateInstance(SClass_ID(OSM_CLASS_ID), SKIN_CLASSID);
		ISkinImportData *skinImp = (ISkinImportData*)skin->GetInterface(I_SKINIMPORTDATA);
		GetCOREInterface7()->AddModifier(*skinNode, *skin);
		for(int j = 0; j < hier->numNodes; j++)
			skinImp->AddBoneEx(bones[j], true);
		// Assigning weights becomes a bit complicated because
		// we deleted isolated vertices.
		Tab<::INode*> bt;
		Tab<float> wt;
		bt.SetCount(4, 1);
		wt.SetCount(4, 1);
		skinNode->EvalWorldState(t);
		float *w;
		uint8 *ix;
		::Mesh *maxmesh = getMesh(skinNode, t);
		assert(maxmesh != NULL);
		int numVerts = maxmesh->getNumVerts();
		for(int j = 0; j < numVerts; j++){
			Point3 pos = maxmesh->getVert(j);
			int idx = findVertex(skinAtomics[i]->geometry, &pos);
			assert(idx >= 0);
			w = &rwskin->weights[idx*4];
			ix = &rwskin->indices[idx*4];
			for(int k = 0; k < 4; k++){
				bt[k] = bones[ix[k]];
				wt[k] = w[k];
			}
			skinImp->AddWeights(skinNode, j, bt, wt);
		}
	}

noskin:
	INode *realroot;
	// Remove dummy node if one was inserted on export
	if(this->convertHierarchy &&
	   (this->isWorldSpace || hier) &&
	   rootnode->GetChildNode(0)){
		realroot = rootnode->GetChildNode(0);
		rootnode->Delete(t, TRUE);	// ImpNode crash
	}else
		realroot = rootnode;

//	printHier(realroot, 0);
//	for(int i = 0; i < numSkinned; i++)
//		printHier(skinNodes[i], 0);

	// save nodes for selection
	saveNodes(realroot);
	for(int i = 0; i < numSkinned; i++)
		saveNodes(skinNodes[i]);

	impifc->RedrawViews();
	delete[] skinAtomics;
	delete[] skinNodes;
	delete[] nodelist;
#ifdef USE_IMPNODES
	delete[] impnodelist;
#endif
	delete[] flist;

	c->destroy();
	if(currentUVAnimDictionary)
		currentUVAnimDictionary->destroy();
	TexDictionary::getCurrent()->destroy();

	ifc->SelectNode(realroot);

	rw::platform = rw::PLATFORM_D3D8;
	return TRUE;
}

static INT_PTR CALLBACK
ImportDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	ISpinnerControl  *spin;
	DFFImport *imp = DLGetWindowLongPtr<DFFImport*>(hWnd); 
	switch(msg){
	case WM_INITDIALOG:
		imp = (DFFImport*)lParam;
		DLSetWindowLongPtr(hWnd, lParam); 
		CenterWindow(hWnd, GetParent(hWnd)); 
		CheckDlgButton(hWnd, IDC_CONVAXIS, DFFImport::convertHierarchy);
		CheckDlgButton(hWnd, IDC_EXPLNORM, DFFImport::explicitNormals);
		CheckDlgButton(hWnd, IDC_AUTOSMOOTH, DFFImport::autoSmooth);

		spin = GetISpinner(GetDlgItem(hWnd, IDC_ANGLESPIN));
		spin->LinkToEdit(GetDlgItem(hWnd, IDC_ANGLE), EDITTYPE_INT); 
		spin->SetLimits(0, 180, TRUE); 
		spin->SetScale(1.0f);
		spin->SetValue(DFFImport::smoothingAngle, FALSE);
		ReleaseISpinner(spin);
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam)){
		case IDOK:
			DFFImport::convertHierarchy = IsDlgButtonChecked(hWnd, IDC_CONVAXIS);
			DFFImport::explicitNormals = IsDlgButtonChecked(hWnd, IDC_EXPLNORM);
			DFFImport::autoSmooth = IsDlgButtonChecked(hWnd, IDC_AUTOSMOOTH);

			spin = GetISpinner(GetDlgItem(hWnd, IDC_ANGLESPIN));
			DFFImport::smoothingAngle = spin->GetIVal();
			ReleaseISpinner(spin);

			EndDialog(hWnd, 1);
			break;
		case IDCANCEL:
			EndDialog(hWnd, 0);
			break;
		default:
			//lprintf("WM_COMMAND: %d\n", LOWORD(wParam));
			break;
		}
		break;
		default:
			//lprintf("MESG: %d\n", LOWORD(msg));
			return FALSE;
	}
	return TRUE;
}

DFFImport::DFFImport(void)
{
}

DFFImport::~DFFImport(void)
{
}

int
DFFImport::ExtCount(void)
{
	return 1;
}

const TCHAR*
DFFImport::Ext(int n)
{
	switch(n){
	case 0:
		return _T("DFF");
	}
	return _T("");
}

const TCHAR*
DFFImport::LongDesc(void)
{
	return _T(STR_DFFFILE); //GetStringT(IDS_DFFFILE);
}
	
const TCHAR*
DFFImport::ShortDesc(void)
{
	return _T(STR_CLASSNAME); //GetStringT(IDS_CLASSNAME);
}

const TCHAR*
DFFImport::AuthorName(void)
{
	return _T(STR_AUTHOR); //GetStringT(IDS_AUTHOR);
}

const TCHAR*
DFFImport::CopyrightMessage(void)
{
	return _T(STR_COPYRIGHT); //GetStringT(IDS_COPYRIGHT);
}

const TCHAR *
DFFImport::OtherMessage1(void)
{
	return _T("");
}

const TCHAR *
DFFImport::OtherMessage2(void)
{
	return _T("");	
}

unsigned int
DFFImport::Version(void)
{				// Version number * 100 (i.e. v3.01 = 301)
	return VERSION;
}

void
DFFImport::ShowAbout(HWND hWnd)
{
}

int
DFFImport::DoImport(const TCHAR *filename, ImpInterface *ii, Interface *i, BOOL suppressPrompts)
{
	initRW();
	this->ifc = i;
	this->impifc = ii;

	lastImported.clear();
	if(!suppressPrompts){
		DFFImport::prepend = 0;
		if(!DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_DFFIMPORT),
			i->GetMAXHWnd(), ImportDlgProc, (LPARAM)this))
			return 1;
	}

	if(dffFileRead(filename))
		return 1;
	return 0;
}
