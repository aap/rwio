-- assumes Vice City rig

-- GTA name, biped node, biped link id
pedHierarchy = #(
	#("Root", #horizontal, 1),
	#("Pelvis", #pelvis, 1),
	#("Spine", #spine, 1),
	#("Spine1", #spine, 2),
	#("Neck", #neck, 1),
	#("Head", #head, 1),
	#("Bip01 L Clavicle", #larm, 1),
	#("L UpperArm", #larm, 2),
	#("L Forearm", #larm, 3),
	#("L Hand", #larm, 4),
	#("L Finger", #lfingers, 1),
	#("Bip01 R Clavicle", #rarm, 1),
	#("R UpperArm", #rarm, 2),
	#("R Forearm", #rarm, 3),
	#("R Hand", #rarm, 4),
	#("R Finger", #rfingers, 1),
	#("L Thigh", #lleg, 1),
	#("L Calf", #lleg, 2),
	#("L Foot", #lleg, 3),
	#("L Toe0", #ltoes, 1),
	#("R Thigh", #rleg, 1),
	#("R Calf", #rleg, 2),
	#("R Foot", #rleg, 3),
	#("R Toe0", #rtoes, 1)
)

fn getChildByName obj name = (
	if obj.name == name do
		return obj
	for c in obj.children do (
		child = getChildByName c name
		if child != undefined do
			return child
	)
	return undefined
)

fn getRoot obj = (
	while obj.parent != undefined do obj = obj.parent
	obj
)

fn bipedScaleX bip scx = (
	scale = biped.getTransform bip #scale
	scale.x = scx
	biped.setTransform bip #scale scale false
)

fn bipedScaleY bip scy = (
	scale = biped.getTransform bip #scale
	scale.y = scy
	biped.setTransform bip #scale scale false
)

fn bipedScaleZ bip scz = (
	scale = biped.getTransform bip #scale
	scale.z = scz
	biped.setTransform bip #scale scale false
)

fn bipRotation rot = (
	-- not quite sure about this
	(quat rot.x rot.z rot.y rot.w)
)

fn createBip target = (
	-- find base position of the bones
	lthigh = getChildByName target "L Thigh"
	rthigh = getChildByName target "R Thigh"
	pelvpos = (lthigh.pos + rthigh.pos)/2
	-- since the spine is moved down, use the toe position for the basic scale
	ltoe = getChildByName target "L Toe0"
	rtoe = getChildByName target "R Toe0"
	toez = (ltoe.pos.z + rtoe.pos.z)/2
	-- the height of the pelvis is 0.5388*height
	height = abs(toez - pelvpos.z)/0.5388

	bip = biped.createNew height -90 [0, 0, 0]
	bipctl = bip.controller
	bipctl.bodyType = 3
	bipctl.displayBones = true
	bipctl.displayFootsteps = false
	bipctl.figureMode = true
	bipctl.spineLinks = 2
	bipctl.fingers = 1
	bipctl.fingerLinks = 1
	bipctl.toes = 1
	bipctl.toeLinks = 1
	bipctl.height = height	-- have to set again because it changes due to the above stuff
	biped.setTransform bip #pos pelvpos false
	bipctl.rubberBandMode = true
	biped.setTransform bip #pos (getChildByName target "Root").pos false
	bipctl.rubberBandMode = false

	-- set pelvis width as midpoint of thighs
	pelvwidth = length(lthigh.pos - rthigh.pos)
	bipedScaleZ (biped.getNode bip #pelvis) pelvwidth

	spine = getChildByName target "Spine"
	spine1 = getChildByName target "Spine1"
	spineheight = length (spine.pos - spine1.pos)
	biped.setTransform (biped.getNode bip #spine) #pos spine.pos false
	bipedScaleX (biped.getNode bip #spine) spineheight

	neck = getChildByName target "Neck"
	spine1height = length (spine1.pos - neck.pos)
	bipedScaleX (biped.getNode bip #spine link:2) spine1height

	head = getChildByName target "Head"
	bip_neck = biped.getNode bip #neck
	neckheight = length (neck.pos - head.pos)
	bipedScaleX bip_neck neckheight
	biped.setTransform bip_neck #rotation (conjugate neck.rotation) false
	biped.setTransform (biped.getNode bip #head) #rotation (conjugate head.rotation) false

	lclav = getChildByName target "Bip01 L Clavicle"
	bip_lclav = biped.getNode bip #larm
	biped.setTransform bip_lclav #pos lclav.pos false

	rclav = getChildByName target "Bip01 R Clavicle"
	bip_rclav = biped.getNode bip #rarm
	biped.setTransform bip_rclav #pos rclav.pos false

	lupper = getChildByName target "L UpperArm"
	lclavlen = length (lupper.pos - lclav.pos)
	bipedScaleX bip_lclav lclavlen
	rupper = getChildByName target "R UpperArm"
	rclavlen = length (rupper.pos - rclav.pos)
	bipedScaleX bip_rclav rclavlen

	-- clavicle rotation is utterly fucked in figure mode
	-- set rotation outside of figure mode and copy-paste posture to figure mode
	biped.createCopyCollection bipctl "tmptmptmp"

	bipctl.figureMode = false
	biped.setTransform bip_lclav #rotation (conjugate lclav.rotation) false
	select bip_lclav
	print $
	tmpname = biped.copyPosture bip.controller #posture true true true
	bipctl.figureMode = true
	biped.pastePosture bip.controller #posture false tmpname

	bipctl.figureMode = false
	biped.setTransform bip_rclav #rotation (conjugate rclav.rotation) false
	select bip_rclav
	tmpname = biped.copyPosture bip.controller #posture true true true
	bipctl.figureMode = true
	biped.pastePosture bip.controller #posture false tmpname

	biped.deleteAllCopyCollections bipctl

	-- upper arms

	lfore = getChildByName target "L Forearm"
	lupperlen = length (lfore.pos - lupper.pos)
	bip_lupper = biped.getNode bip #larm link:2
	bipedScaleX bip_lupper lupperlen
	biped.setTransform bip_lupper #rotation (conjugate lupper.rotation) false

	rfore = getChildByName target "R Forearm"
	rupperlen = length (rfore.pos - rupper.pos)
	bip_rupper = biped.getNode bip #rarm link:2
	bipedScaleX bip_rupper rupperlen
	biped.setTransform bip_rupper #rotation (conjugate rupper.rotation) false

	-- forearms

	lhand = getChildByName target "L Hand"
	lforelen = length (lhand.pos - lfore.pos)
	bip_lfore = biped.getNode bip #larm link:3
	bipedScaleX bip_lfore lforelen
	biped.setTransform bip_lfore #rotation (conjugate lfore.rotation) false

	rhand = getChildByName target "R Hand"
	rforelen = length (rhand.pos - rfore.pos)
	bip_rfore = biped.getNode bip #rarm link:3
	bipedScaleX bip_rfore rforelen
	biped.setTransform bip_rfore #rotation (conjugate rfore.rotation) false

	-- hands

	lfinger = getChildByName target "L Finger"
	lhandlen = length (lfinger.pos - lhand.pos)
	bip_lhand = biped.getNode bip #larm link:4
	bipedScaleX bip_lhand lhandlen
	biped.setTransform bip_lhand #rotation (conjugate lhand.rotation) false

	rfinger = getChildByName target "R Finger"
	rhandlen = length (rfinger.pos - rhand.pos)
	bip_rhand = biped.getNode bip #rarm link:4
	bipedScaleX bip_rhand rhandlen
	biped.setTransform bip_rhand #rotation (conjugate rhand.rotation) false

	-- thighs

	lcalf = getChildByName target "L Calf"
	bip_lthigh = biped.getNode bip #lleg
	lthighlen = length (lcalf.pos - lthigh.pos)
	bipedScaleX bip_lthigh lthighlen
	biped.setTransform bip_lthigh #rotation (conjugate lthigh.rotation) false

	rcalf = getChildByName target "R Calf"
	bip_rthigh = biped.getNode bip #rleg
	rthighlen = length (rcalf.pos - rthigh.pos)
	bipedScaleX bip_rthigh rthighlen
	biped.setTransform bip_rthigh #rotation (conjugate rthigh.rotation) false

	-- calves

	lfoot = getChildByName target "L Foot"
	bip_lcalf = biped.getNode bip #lleg link:2
	lcalflen = length (lfoot.pos - lcalf.pos)
	bipedScaleX bip_lcalf lcalflen
	biped.setTransform bip_lcalf #rotation (conjugate lcalf.rotation) false

	rfoot = getChildByName target "R Foot"
	bip_rcalf = biped.getNode bip #rleg link:2
	rcalflen = length (rfoot.pos - rcalf.pos)
	bipedScaleX bip_rcalf rcalflen
	biped.setTransform bip_rcalf #rotation (conjugate rcalf.rotation) false

	-- feet

	ltoe = getChildByName target "L Toe0"
	bip_lfoot = biped.getNode bip #lleg link:3
	bipedScaleX bip_lfoot (abs (ltoe.pos.z - lfoot.pos.z))
	bipedScaleY bip_lfoot (abs (ltoe.pos.y - lfoot.pos.y)*1.25)

	rtoe = getChildByName target "R Toe0"
	bip_rfoot = biped.getNode bip #rleg link:3
	bipedScaleX bip_rfoot (abs (rtoe.pos.z - rfoot.pos.z))
	bipedScaleY bip_rfoot (abs (rtoe.pos.y - rfoot.pos.y)*1.25)

	bip
)

-- unused
fn dummiesFromBiped bipnode ind = (
	format "% %\n" ind bipnode.name
	pos = biped.getTransform bipnode #pos
	rot = biped.getTransform bipnode #rotation
	bonedummy = bone scale:[0.01, 0.01, 0.01] rotation:rot
	bonedummy.pos = pos
	bonedummy.name = bipnode.name + "_dummy"
	for child in bipnode.children do (
		childdummy = dummiesFromBiped child (ind + "  ")
		attachObjects bonedummy childdummy move:false
	)
	bonedummy
)

utility GtaHelper "Biped match" (
	button convMat "Match Biped to bones" width:140 align:#center
	on convMat pressed do (
		if $ != undefined do
			createBip (getRoot $)
	)
)
