#include "dffimp.h"

int exportTristrips = 0;	// TODO

// stolen from RW
static void
GetFileName(char *outFileName, char *fullPathName)
{
	int strLen;
	char *start, *end;
	BOOL strippedext = FALSE;

	start = strrchr(fullPathName, '\\');
	if(start)
	    start++;
	else
	    start = fullPathName;
	end = strrchr(start, '.');
	if(end){
	    *end = '\0';
	    strippedext = TRUE;
	}else
	    end = start + strlen(start);
	strLen = end - start;
	strncpy(outFileName, start, strLen + 1);
	outFileName[strLen] = '\0';
	if(strippedext) *end = '.';
}

static rw::Texture*
convertTexture(Texmap *tex, Texmap *mask)
{
	using namespace rw;

	char buf[MAX_PATH];
	BitmapTex *bmtex = NULL;
	TCHAR *name = NULL;
	TCHAR *maskname = NULL;
	if(tex == NULL || tex->ClassID() != Class_ID(BMTEX_CLASS_ID,0))
		return NULL;
	bmtex = (BitmapTex*)tex;
	name = (TCHAR*)bmtex->GetMapName();
	if(mask && mask->ClassID() == Class_ID(BMTEX_CLASS_ID,0)){
		BitmapTex *bmmask = (BitmapTex*)mask;
		maskname = (TCHAR*)bmmask->GetMapName();
	}

	Texture *rwt = Texture::create(NULL);
#ifdef _UNICODE
	char fuckmax[MAX_PATH];
	wcstombs(fuckmax, name, MAX_PATH);
	GetFileName(buf, fuckmax);
#else
	GetFileName(buf, name);
#endif
	strncpy(rwt->name, buf, 32);
	if(maskname){
#ifdef _UNICODE
		char fuckmax[MAX_PATH];
		wcstombs(fuckmax, maskname, MAX_PATH);
		GetFileName(buf, fuckmax);
#else
		GetFileName(buf, maskname);
#endif
		strncpy(rwt->mask, buf, 32);
	}

	int filter = Texture::NEAREST;
	int wrapu = Texture::CLAMP;
	int wrapv = Texture::CLAMP;
	switch(bmtex->GetFilterType()){
	case FILTER_SAT:
		filter = Texture::LINEAR;
		break;
	case FILTER_PYR:
		filter = Texture::LINEARMIPLINEAR;
		break;
	}
	int tiling = bmtex->GetTextureTiling();
	if(tiling & U_WRAP)
		wrapu = Texture::WRAP;
	if(tiling & U_MIRROR)
		wrapu = Texture::MIRROR;
	if(tiling & V_WRAP)
		wrapv = Texture::WRAP;
	if(tiling & V_MIRROR)
		wrapv = Texture::MIRROR;
	rwt->filterAddressing = (wrapv << 12) | (wrapu << 8) | filter;
	return rwt;
}

static rw::Material*
convertMaterial(Mtl *mtl)
{
	using namespace rw;
	Color c;
	rw::Material *rwm = rw::Material::create();
	Interval valid = FOREVER;

	Texmap *difftex = NULL;
	Texmap *masktex = NULL;
	Texmap *envtex = NULL;
	Texmap *bumptex = NULL;
	Texmap *dualtex = NULL;
	float bumpCoef = 0.0f, envCoef = 0.0f;
	int srcblend = 0, dstblend = 0;
	int effects = MatFX::NOTHING;
	// standard material
	if(mtl->ClassID() == Class_ID(DMTL_CLASS_ID, 0)){
		StdMat* std = (StdMat*)mtl;
		c = std->GetDiffuse(0);
		rwm->color.red   = c.r*255.0f;
		rwm->color.green = c.g*255.0f;
		rwm->color.blue  = c.b*255.0f;
		rwm->color.alpha = std->GetOpacity(0)*255.0f;
		rwm->surfaceProps.specular = std->GetShinStr(0);
		if(std->MapEnabled(ID_DI))
			difftex = std->GetSubTexmap(ID_DI);
		if(std->MapEnabled(ID_OP))
			masktex = std->GetSubTexmap(ID_OP);
		if(std->MapEnabled(ID_BU))
			bumptex = std->GetSubTexmap(ID_BU);
		if(std->MapEnabled(ID_RL))
			envtex = std->GetSubTexmap(ID_RL);
		bumpCoef = std->GetTexmapAmt(ID_BU, 0);
		envCoef = std->GetTexmapAmt(ID_RL, 0);
		if(bumptex)
			effects |= MatFX::BUMPMAP;
		if(envtex)
			effects |= MatFX::ENVMAP;
	}else if(mtl->ClassID() == Class_ID(0x29b71842, 0x52508b70)){	// my very own RW Material
		IParamBlock2 *pb = mtl->GetParamBlock(0);
		float alpha;
		int hasEnv, hasSpec;
		pb->GetValue(mat_color, 0, c, valid);
		pb->GetValue(mat_coloralpha, 0, alpha, valid);
		rwm->color.red   = c.r*255.0f;
		rwm->color.green = c.g*255.0f;
		rwm->color.blue  = c.b*255.0f;
		rwm->color.alpha = alpha*255.0f;
		pb->GetValue(mat_sp_ambient, 0, rwm->surfaceProps.ambient, valid);
		pb->GetValue(mat_sp_diffuse, 0, rwm->surfaceProps.diffuse, valid);
		pb->GetValue(mat_sp_specular, 0, rwm->surfaceProps.specular, valid);
		pb->GetValue(mat_texmap_texture, 0, difftex, valid);

		pb->GetValue(mat_matfxeffect, 0, effects, valid);
		effects -= 1;
		pb->GetValue(mat_texmap_envmap, 0, envtex, valid);
		pb->GetValue(mat_envmap_amount, 0, envCoef, valid);
		pb->GetValue(mat_texmap_bumpmap, 0, bumptex, valid);
		pb->GetValue(mat_bumpmap_amount, 0, bumpCoef, valid);
		pb->GetValue(mat_texmap_pass2, 0, dualtex, valid);
		pb->GetValue(mat_pass2_srcblend, 0, srcblend, valid);
		pb->GetValue(mat_pass2_destblend, 0, dstblend, valid);

		// R* extensions
		IParamBlock2 *pbr = mtl->GetParamBlock(1);
		pbr->GetValue(mat_enEnv, 0, hasEnv, valid);
		if(hasEnv){
			float f;
			gta::EnvMat *env = new gta::EnvMat;
			*PLUGINOFFSET(gta::EnvMat*, rwm, gta::envMatOffset) = env;
			pbr->GetValue(mat_scaleX, 0, f, valid);
			env->scaleX = f*8.0f;
			pbr->GetValue(mat_scaleY, 0, f, valid);
			env->scaleY = f*8.0f;
			pbr->GetValue(mat_transScaleX, 0, f, valid);
			env->transScaleX = f*8.0f;
			pbr->GetValue(mat_transScaleY, 0, f, valid);
			env->transScaleY = f*8.0f;
			pbr->GetValue(mat_shininess, 0, f, valid);
			env->shininess = f*255.0f;
		}

		pbr->GetValue(mat_enSpec, 0, hasSpec, valid);
		if(hasSpec){
			Texmap *specmap;
			gta::SpecMat *spec = new gta::SpecMat;
			*PLUGINOFFSET(gta::SpecMat*, rwm, gta::specMatOffset) = spec;
			pbr->GetValue(mat_specularity, 0, spec->specularity, valid);
			pbr->GetValue(mat_specMap, 0, specmap, valid);
			spec->texture = convertTexture(specmap, NULL);
		}
	}
	rwm->texture = convertTexture(difftex, masktex);

	if(effects != MatFX::NOTHING){
		MatFX::setEffects(rwm, effects);
		MatFX *matfx = MatFX::get(rwm);
		if(effects == MatFX::BUMPMAP || effects == MatFX::BUMPENVMAP){
			matfx->setBumpTexture(convertTexture(bumptex, NULL));
			matfx->setBumpCoefficient(bumpCoef);
		}
		if(effects == MatFX::ENVMAP || effects == MatFX::BUMPENVMAP){
			matfx->setEnvTexture(convertTexture(envtex, NULL));
			matfx->setEnvCoefficient(envCoef);
		}
		if(effects == MatFX::DUAL || effects == MatFX::DUALUVTRANSFORM){
			matfx->setDualTexture(convertTexture(dualtex, NULL));
			matfx->setDualSrcBlend(srcblend);
			matfx->setDualDestBlend(dstblend);
		}
	}
	return rwm;
}

static void
convertMaterials(rw::Geometry *geo, INode *node)
{
	using namespace rw;

	Mtl *mtl = node->GetMtl();
	rw::Material *mat;
	if(mtl == NULL){
		mat = rw::Material::create();
		geo->matList.appendMaterial(mat);
		DWORD wire = node->GetWireColor();
		mat->color.red = GetRValue(wire);
		mat->color.green = GetGValue(wire);
		mat->color.blue = GetBValue(wire);
	}else if(mtl->ClassID() == Class_ID(MULTI_CLASS_ID, 0)){
		for(int i = 0; i < mtl->NumSubMtls(); i++)
			geo->matList.appendMaterial(convertMaterial(mtl->GetSubMtl(i)));
	}else
		geo->matList.appendMaterial(convertMaterial(mtl));
}

// Gets the first n consecutive UV channels
static int
getNumUVSets(Mesh *mesh)
{
	int n = 0;
	for(int i = 0; i < 8; i++){
		if(mesh->mapFaces(MAP_TEXCOORD0+i) &&
		   mesh->getNumMapVerts(MAP_TEXCOORD0+i))
			n++;
		else
			break;
	}
	return n;
}

rw::Geometry*
DFFExport::convertGeometry(INode *node, int **vertexmap)
{
	using namespace rw;

	::Object *obj = node->EvalWorldState(0).obj;
	TriObject *tri = (TriObject*)obj->ConvertToType(0, triObjectClassID);
	::Mesh *mesh = &tri->mesh;

	//lprintf("geometry: %s\n", obj->GetObjectName());

	int mask = 1;
	int numTris = mesh->numFaces;
	int numVertices = 0;
	int32 flags = Geometry::POSITIONS;
	if(exportTristrips)
		flags |= Geometry::TRISTRIP;
	if(exportLit)
		flags |= Geometry::LIGHT;
	if(exportNormals){
		mask |= 0x10;
		flags |= Geometry::NORMALS;
	}
	if(exportPrelit){
		mask |= 0x100;
		flags |= Geometry::PRELIT;
	}
	if(exportExtraColors)
		mask |= 0x200;
	int numUV = getNumUVSets(mesh);
	// TODO: allow more!
	if(numUV > 2)	numUV = 2;
	if(numUV == 1)
		flags |= Geometry::TEXTURED;
	if(numUV > 1)
		flags |= Geometry::TEXTURED2;
	for(int i = 0; i < numUV; i++)
		mask |= 0x1000 << i;

	// if skin
	//	mask |= 0x10000;

	//lprintf(_T(" %x %d %d\n"), flags, numTris*3, numTris);
	flags |= numUV << 16;
	Geometry *geo = Geometry::create(numTris*3, numTris, flags);
	if(mask & 0x200)
		gta::allocateExtraVertColors(geo);
	convertMaterials(geo, node);
	if(geo->hasColoredMaterial())
		geo->flags |= Geometry::MODULATE;
	geo->numVertices = 0;

	int *remap = NULL;
	if(vertexmap)
		*vertexmap = remap = new int[numTris*3];

	mesh->checkNormals(TRUE);
	gta::SaVert v;
	for(int i = 0; i < numTris; i++){
		for(int j = 0; j < 3; j++){
			// vertex
			int idx = mesh->faces[i].v[j];
			v.p.x = mesh->verts[idx].x;
			v.p.y = mesh->verts[idx].y;
			v.p.z = mesh->verts[idx].z;

			// normal
			if(mask & 0x10){
				RVertex *rv = &mesh->getRVert(idx);
				int numN = rv->rFlags & NORCT_MASK;
				Point3 p3n;
				if(numN == 1)
					p3n = rv->rn.getNormal();
				else
					for(int k = 0; k < numN; k++)
						if(rv->ern[k].getSmGroup() &
						   mesh->faces[i].getSmGroup()){
							p3n = rv->ern[k].getNormal();
							break;
						}
				v.n.x = p3n.x;
				v.n.y = p3n.y;
				v.n.z = p3n.z;
			}

			// prelight
			if(mask & 0x100){
				if(mesh->vcFace){
					idx = mesh->vcFace[i].t[j];
					v.c.red   = mesh->vertCol[idx].x*255;
					v.c.green = mesh->vertCol[idx].y*255;
					v.c.blue  = mesh->vertCol[idx].z*255;
				}else
					v.c.red = v.c.green = v.c.blue = 255;
				if(mesh->mapFaces(MAP_ALPHA)){
					idx = mesh->mapFaces(MAP_ALPHA)[i].t[j];
					v.c.alpha = mesh->mapVerts(MAP_ALPHA)[idx].x*255;
				}else
					v.c.alpha = 255;
			}

			// SA extra colors
			if(mask & 0x200){
				if(mesh->mapFaces(MAP_EXTRACOLORS)){
					idx = mesh->mapFaces(MAP_EXTRACOLORS)[i].t[j];
					UVVert *col = mesh->mapVerts(MAP_EXTRACOLORS);
					v.c1.red   = col[idx].x*255;
					v.c1.green = col[idx].y*255;
					v.c1.blue  = col[idx].z*255;
				}else
					v.c1.red = v.c1.green = v.c1.blue = 255;
				if(mesh->mapFaces(MAP_EXTRAALPHA)){
					idx = mesh->mapFaces(MAP_EXTRAALPHA)[i].t[j];
					v.c1.alpha = mesh->mapVerts(MAP_EXTRAALPHA)[idx].x*255;
				}else
					v.c1.alpha = 255;
			}

			// tex coords
			for(int k = 0; k < numUV; k++){
				idx = mesh->mapFaces(MAP_TEXCOORD0+k)[i].t[j];
				UVVert *uv = mesh->mapVerts(MAP_TEXCOORD0+k);
				// ugly and wrong
				if(k == 0){
					v.t.u = uv[idx].x;
					v.t.v = 1.0f-uv[idx].y;
				}
				if(k == 1){
					v.t1.u = uv[idx].x;
					v.t1.v = 1.0f-uv[idx].y;
				}
			}
			idx = gta::findSAVertex(geo, NULL, mask, &v);
			if(idx < 0){
				idx = geo->numVertices++;
				insertSAVertex(geo, idx, mask, &v);
			}
			geo->triangles[i].v[j] = idx;
			if(remap)
				remap[idx] = mesh->faces[i].v[j];
		}
		MtlID id = mesh->getFaceMtlIndex(i);
		geo->triangles[i].matId = id % geo->matList.numMaterials;
	}

	geo->buildMeshes();
	geo->removeUnusedMaterials();

	if(tri != obj)
		tri->DeleteMe();
	return geo;
}

void
DFFExport::convertSkin(rw::Geometry *geo, rw::Frame *frame, INode *node, Modifier *mod, int *map)
{
	using namespace rw;

	ISkin *skin = (ISkin*)mod->GetInterface(I_SKIN);
	int numBones = skin->GetNumBones();
	if(numBones != this->numNodes){
		lprintf(_T("Error: unequal number of bones and nodes\n"));
		return;
	}
	rw::Skin *rwskin = new rw::Skin;
	rw::Skin::set(geo, rwskin);
	//lprintf("BONES: %d %d\n", skin->GetNumBones(), skin->GetNumBonesFlat());
	rwskin->init(numBones, numBones, geo->numVertices);
	ISkinContextData *skindata = skin->GetContextInterface(node);
	float *weights = rwskin->weights;
	uint8 *indices = rwskin->indices;
	memset(weights, 0, geo->numVertices*4*sizeof(float));
	memset(indices, 0, geo->numVertices*4*sizeof(uint8));

	::Object *obj = node->EvalWorldState(0).obj;
	TriObject *tri = (TriObject*)obj->ConvertToType(0, triObjectClassID);
	::Mesh *mesh = &tri->mesh;

	int numWeights;
	int idx;
	int *bonemap = new int[numBones];
	for(int i = 0; i < numBones; i++){
		INode *bone = skin->GetBone(i);
		for(int j = 0; j < numBones; j++)
			if(this->nodearray[j].node == bone){
				bonemap[i] = j;
				break;
			}
		//lprintf("bone: %d %s\n", bonemap[i], bone->GetName());
	}
	for(int i = 0; i < geo->numVertices; i++){
		idx = map[i];
		numWeights = skindata->GetNumAssignedBones(idx);
		if(numWeights > 4) numWeights = 4;
		for(int j = 0; j < numWeights; j++){
			indices[i*4+j] = bonemap[skindata->GetAssignedBone(idx, j)];
			weights[i*4+j] = skindata->GetBoneWeight(idx, j);
		}
	}
	delete[] bonemap;

	if(tri != obj)
		tri->DeleteMe();

	/* calculate inverse bone matrices */
	rw::Matrix rootinv, tmp;
	RWNode *n = &this->nodearray[0];
	rw::Matrix::invert(&rootinv, frame->getLTM());
	for(int i = 0; i < numBones; i++){
		n = &this->nodearray[i];
		rw::Matrix::mult(&tmp, &rootinv, n->frame->getLTM());
		rw::Matrix::invert((rw::Matrix*)&rwskin->inverseMatrices[i*16], &tmp);
	}

	rwskin->findNumWeights(geo->numVertices);
	rwskin->findUsedBones(geo->numVertices);
}