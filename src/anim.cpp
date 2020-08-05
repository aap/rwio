#include "dffimp.h"

void
traverseHierarchy(std::vector<INode*> &hierarchy, INode *node)
{
	int id, hasid;
	hasid = getID(node, &id);
	if(hasid && id < 0)
		return;	// stop traversal at negative ID

	hierarchy.push_back(node);

	// Sort child nodes by ID
	int numChildren = node->NumberOfChildren();
	SortNode *children = new SortNode[numChildren];
	for(int i = 0; i < numChildren; i++){
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
		traverseHierarchy(hierarchy, children[i].node);
	}
	delete[] children;

}

/*
 *
 *
 * Import
 *
 *
 */

static void
applyNodeAnimation(INode *node, rw::Animation *anim, int n)
{
	Control *control = node->GetTMController();
	Control *posControl = control->GetPositionController();
	Control *rotControl = control->GetRotationController();
	assert(posControl);
	assert(rotControl);

	anim->getAnimFrame(n)->prev = nil;	// make sure first frame doesn't point to junk
	rw::HAnimKeyFrame *prev = nil;
//	lprintf(_T("node %d of %d (%d frames)\n"), n, anim->getNumNodes(), anim->numFrames);
	AnimateOn();
	for(int i = n; i < anim->numFrames; i++){
		rw::HAnimKeyFrame *frame = (rw::HAnimKeyFrame*)anim->getAnimFrame(i);
		// only interested in keyframes for this node
		if(frame->prev != prev)
			continue;
		prev = frame;
		TimeValue t = SecToTicks(frame->time);

//		lprintf(_T("%d %d - %f %d\n"), n, i, frame->time, t);
		Point3 trans(frame->t.x, frame->t.y, frame->t.z);
		posControl->SetValue(t, &trans);
		Quat rot(-frame->q.x, -frame->q.y, -frame->q.z, frame->q.w);
		rotControl->SetValue(t, &rot);
	}
	AnimateOff();
}

static rw::Animation*
readAnim(const TCHAR *filename)
{
	rw::StreamFile in;
	if(in.open(getAsciiStr(filename), "rb") == nil)
		return nil;
	if(rw::findChunk(&in, rw::ID_ANIMANIMATION, nil, nil))
		return rw::Animation::streamRead(&in);
	return nil;
}

BOOL
ANMImport::anmFileRead(const TCHAR *filename)
{
	INode *rootnode = getRootOfSelection(this->ifc);
	if(rootnode == nil){
		lprintf(_T("error: nothing selected\n"));
		return 0;
	}

	rw::Animation *anim = readAnim(filename);
	if(anim == nil)
		return 0;
	if(anim->interpInfo->id != 1){
		lprintf(_T("error: Not HAnim animation\n"));
		anim->destroy();
		return 0;
	}

	lprintf(_T("got anim: %d frames, length %f, %d nodes\n"),
		anim->numFrames, anim->duration,
		anim->getNumNodes());

	// Now create canonical hierarchy as if we were exporting a DFF
	std::vector<INode*> hierarchy;
	traverseHierarchy(hierarchy, rootnode);

	for(int i = 0; i < hierarchy.size(); i++)
		lprintf(_T("%d: %s\n"), i, hierarchy[i]->GetName());

	int numNodes = anim->getNumNodes();

	if(hierarchy.size() != numNodes){
		lprintf(_T("error: Node mismatch: Anim %d, Hierarchy %d\n"),
			numNodes, hierarchy.size());
		anim->destroy();
		return 0;
	}

	extendAnimRange(anim->duration);

	for(int i = 0; i < numNodes; i++)
		applyNodeAnimation(hierarchy[i], anim, i);

	anim->destroy();

	return 1;
}


ANMImport::ANMImport(void)
{
}

ANMImport::~ANMImport(void)
{
}

int
ANMImport::ExtCount(void)
{
	return 1;
}

const TCHAR*
ANMImport::Ext(int n)
{
	switch(n){
	case 0:
		return _T("ANM");
	}
	return _T("");
}

const TCHAR*
ANMImport::LongDesc(void)
{
	return _T(STR_ANMFILE); //GetStringT(IDS_DFFFILE);
}
	
const TCHAR*
ANMImport::ShortDesc(void)
{
	return _T(STR_ANMCLASSNAME); //GetStringT(IDS_CLASSNAME);
}

const TCHAR*
ANMImport::AuthorName(void)
{
	return _T(STR_AUTHOR); //GetStringT(IDS_AUTHOR);
}

const TCHAR*
ANMImport::CopyrightMessage(void)
{
	return _T(STR_COPYRIGHT); //GetStringT(IDS_COPYRIGHT);
}

const TCHAR *
ANMImport::OtherMessage1(void)
{
	return _T("");
}

const TCHAR *
ANMImport::OtherMessage2(void)
{
	return _T("");	
}

unsigned int
ANMImport::Version(void)
{				// Version number * 100 (i.e. v3.01 = 301)
	return VERSION;
}

void
ANMImport::ShowAbout(HWND hWnd)
{
}

int
ANMImport::DoImport(const TCHAR *filename, ImpInterface *ii, Interface *i, BOOL suppressPrompts)
{
	initRW();
	this->ifc = i;

	if(anmFileRead(filename))
		return 1;
	return 0;
}


/*
 *
 *
 * Export
 *
 *
 */

int
sortTime(const void *a, const void *b)
{
	TimeValue *ta = (TimeValue*)a;
	TimeValue *tb = (TimeValue*)b;
	return *ta - *tb;
}

struct NodeFrames
{
	rw::HAnimKeyFrame *frames;
	int numFrames;
};

void
getKeyFrame(rw::HAnimKeyFrame *frame, Control *control, TimeValue time)
{
	Interval valid = FOREVER;
	Control *posControl = control->GetPositionController();
	Control *rotControl = control->GetRotationController();
	Point3 trans;
	Quat rot;
	posControl->GetValue(time, &trans, valid);
	rotControl->GetValue(time, &rot, valid);
	frame->time = TicksToSec(time);
	frame->t.x = trans.x;
	frame->t.y = trans.y;
	frame->t.z = trans.z;
	frame->q.x = -rot.x;
	frame->q.y = -rot.y;
	frame->q.z = -rot.z;
	frame->q.w = rot.w;
}

void
getNodeKeyFrames(INode *node, NodeFrames *nodeframes)
{
	int i;
	Control *control = node->GetTMController();
	Control *posControl = control->GetPositionController();
	Control *rotControl = control->GetRotationController();

	int npos = posControl->NumKeys();
	int nrot = rotControl->NumKeys();
	if(npos + nrot == 0){
		// No keyframes but we still need data
		nodeframes->frames = rwNewT(rw::HAnimKeyFrame, 2, 0);
		nodeframes->numFrames = 1;
		getKeyFrame(&nodeframes->frames[0], control, 0);
		return;
	}

	// Get all the keyframe times, include 0.0
	TimeValue *times = new TimeValue[1 + npos + nrot];
	times[0] = 0;
	for(i = 0; i < npos; i++)
		times[1+i] = posControl->GetKeyTime(i);
	for(i = 0; i < nrot; i++)
		times[1+npos+i] = rotControl->GetKeyTime(i);
	qsort(times, 1+npos+nrot, sizeof(TimeValue), sortTime);
	// remove duplicates
	int n = 1;
	for(i = 1; i < npos+nrot; i++)
		if(times[i] > times[n-1])
			times[n++] = times[i];
//	lprintf(_T("num KF: %d (%f %f)\n"), n, TicksToSec(times[0]), TicksToSec(times[n-1]));

	// Now get all the data into hanim keyframes
	// allocate one more if we don't have an end frame
	nodeframes->frames = rwNewT(rw::HAnimKeyFrame, n+1, 0);
	nodeframes->numFrames = n;
	for(i = 0; i < n; i++)
		getKeyFrame(&nodeframes->frames[i], control, times[i]);
	delete[] times;
}

void
storeKeyframes(rw::Animation *anim, NodeFrames *nodeframes, int numNodes)
{
	struct Iter {
		rw::KeyFrameHeader *prev;	// in dst
		rw::KeyFrameHeader *next;	// in src
		int nleft;
	} *it;

	int i, j;
	it = new Iter[numNodes];
	for(i = 0; i < numNodes; i++){
		it[i].prev = nil;
		it[i].next = (rw::KeyFrameHeader*)nodeframes[i].frames;
		it[i].nleft = nodeframes[i].numFrames;
	}

	rw::KeyFrameHeader *dst = (rw::KeyFrameHeader*)anim->keyframes;
	int kfsz = anim->interpInfo->animKeyFrameSize;
	int framesLeft = anim->numFrames;

	// Add first two key frames for each node
	for(i = 0; i < 2; i++)
		for(j = 0; j < numNodes; j++){
			memcpy(dst, it[j].next, kfsz);
			dst->prev = it[j].prev;
			it[j].prev = dst;
			dst = dst->next(kfsz);
			it[j].next = it[j].next->next(kfsz);
			it[j].nleft--;
			framesLeft--;
		}
	// Now add all the rest in sorted order
	while(framesLeft--){
		assert(framesLeft >= 0);
		float mintime = 1.0e20f;
		j = -1;
		for(i = 0; i < numNodes; i++){
			if(it[i].nleft > 0 && it[i].next->time < mintime){
				mintime = it[i].next->time;
				j = i;
			}
		}
		assert(j >= 0);
		memcpy(dst, it[j].next, kfsz);
		dst->prev = it[j].prev;
		it[j].prev = dst;
		dst = dst->next(kfsz);
		it[j].next = it[j].next->next(kfsz);
		it[j].nleft--;
	}

	delete[] it;
}

BOOL
ANMExport::anmFileWrite(const TCHAR *filename)
{
	INode *rootnode = getRootOfSelection(this->ifc);
	if(rootnode == nil){
		lprintf(_T("error: nothing selected\n"));
		return 0;
	}

	// Now create canonical hierarchy as if we were exporting a DFF
	std::vector<INode*> hierarchy;
	traverseHierarchy(hierarchy, rootnode);

	int numNodes = (int)hierarchy.size();
	NodeFrames *nodes = new NodeFrames[numNodes];

	float duration = 0.0f;
	for(int i = 0; i < numNodes; i++){
//		lprintf(_T("%d: %s\n"), i, hierarchy[i]->GetName());
		getNodeKeyFrames(hierarchy[i], &nodes[i]);
		if(nodes[i].frames[nodes[i].numFrames-1].time > duration)
			duration = nodes[i].frames[nodes[i].numFrames-1].time;
	}
	// Add end frames where needed
//	lprintf(_T("duration: %f\n"), duration);
	int totalNumFrames = 0;
	for(int i = 0; i < numNodes; i++){
		NodeFrames *n = &nodes[i];
		if(n->frames[n->numFrames-1].time < duration){
			// copy last frame
			n->frames[n->numFrames] = n->frames[n->numFrames-1];
			n->frames[n->numFrames].time = duration;
			n->numFrames++;
		}
		totalNumFrames += n->numFrames;
	}

	rw::AnimInterpolatorInfo *interpInfo = rw::AnimInterpolatorInfo::find(1);
	// TODO: flags?
	rw::Animation *anim = rw::Animation::create(interpInfo, totalNumFrames, 0, duration);
	storeKeyframes(anim, nodes, numNodes);
	for(int i = 0; i < numNodes; i++)
		rwFree(nodes[i].frames);
	delete[] nodes;

	// All done, write the file
	rw::StreamFile out;
	if(out.open(getAsciiStr(filename), "wb") == NULL){
		lprintf(_T("error: couldn't open file\n"));
		out.close();
		anim->destroy();
		return 0;
	}
	anim->streamWrite(&out);
	out.close();
	anim->destroy();

	return 1;
}

ANMExport::ANMExport(void)
{
}

ANMExport::~ANMExport(void)
{
}

int
ANMExport::ExtCount(void)
{
	return 1;
}

const TCHAR*
ANMExport::Ext(int n)
{
	switch(n){
	case 0:
		return _T("ANM");
	}
	return _T("");
}

const TCHAR*
ANMExport::LongDesc(void)
{
	return _T(STR_ANMFILE);
}
	
const TCHAR*
ANMExport::ShortDesc(void)
{
	return _T(STR_ANMCLASSNAME);
}

const TCHAR*
ANMExport::AuthorName(void)
{
	return _T(STR_AUTHOR); //GetStringT(IDS_AUTHOR);
}

const TCHAR*
ANMExport::CopyrightMessage(void)
{
	return _T(STR_COPYRIGHT); //GetStringT(IDS_COPYRIGHT);
}

const TCHAR *
ANMExport::OtherMessage1(void)
{
	return _T("");
}

const TCHAR *
ANMExport::OtherMessage2(void)
{
	return _T("");	
}

unsigned int
ANMExport::Version(void)
{				// Version number * 100 (i.e. v3.01 = 301)
	return VERSION;
}

void
ANMExport::ShowAbout(HWND hWnd)
{
}

int
ANMExport::DoExport(const TCHAR *filename, ExpInterface *ei, Interface *i, BOOL suppressPrompts, DWORD options)
{
	initRW();
	this->ifc = i;
	this->expifc = ei;

	if(!anmFileWrite(filename))
		return 0;
	return 1;
}

BOOL
ANMExport::SupportsOptions(int ext, DWORD options)
{
	return(options == SCENE_EXPORT_SELECTED) ? TRUE : FALSE;
}
