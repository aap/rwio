plugin Material RW_Mtl
    name:"RW Material"
    classID:#(0x29b71842, 0x52508b70)
	extends:Standard replaceUI:true version:1
(
	parameters main rollout:params (
		color 				type:#color default:white 	ui:color
		coloralpha				type:#float default:1.0     ui:alpha
		sp_ambient					type:#float default:1.0 	ui:amb
		sp_diffuse				type:#float default:1.0 	ui:dif
		sp_specular                type:#float default:1.0 	ui:spec
		texmap_texture 			type:#TextureMap  			ui:texture

		matfxeffect   type:#integer default:1 ui:fxtype
		texmap_envmap   type:#TextureMap ui:envmap
		envmap_amount  type:#float default:0.5 ui:envcoef
		texmap_bumpmap  type:#TextureMap ui:bumpmap
		bumpmap_amount type:#float default:0.5 ui:bumpcoef
		texmap_pass2    type:#TextureMap ui:dpmap
		pass2_srcblend type:#integer default:2 ui:srcblend
		pass2_destblend type:#integer default:2 ui:destblend

		on color set val do (
			if delegate.diffuseMap != undefined do
				delegate.diffuseMap.color1 = val*sp_diffuse
			delegate.diffuse = val*sp_diffuse
		)
		on sp_diffuse set val do (
			if delegate.diffuseMap != undefined do
				delegate.diffuseMap.color1 = color*val
			delegate.diffuse = color*val
		)
		on coloralpha set val do delegate.opacity = val*100
		on texmap_texture set tex do (
			if delegate.diffuseMap != undefined do
				delegate.diffuseMap.map2 = tex
			if classof tex == Bitmaptexture then (
				alphamap = copy tex
				alphamap.monoOutput = 1
				delegate.opacityMap = alphamap
			) else
				delegate.opacityMap = undefined
		)
	)
	parameters rsExt rollout:rsExt (
		enEnv type:#boolean default:false ui:enEnv
		enSpec type:#boolean default:false ui:enSpec

		scaleX      type:#float default:1.0 ui:sclX
		scaleY      type:#float default:1.0 ui:sclY
		transScaleX type:#float default:1.0 ui:trSclX
		transScaleY type:#float default:1.0 ui:trSclY
		shininess   type:#float default:0.25 ui:shininess

		specularity  type:#float default:0.5 ui:specularity
		specMap     type:#TextureMap ui:specmap
	)
	--parameters render rollout:render (
	--	pluginID      type:#integer default:0 ui:pluginID
	--	pluginData    type:#integer default:0 ui:pluginData
	--)
	rollout params  "RenderWare Material" (
		group "RpMaterial Parameters" (
			label           colorlbl    "Color" pos:[57, 30]
 			colorpicker		color 		"" width:51 height:16 pos:[89, 28]
			label           alphalbl    "Alpha" pos:[224, 30]
			spinner			alpha		""	width:49 height:16 type:#float range:[0,1.0,1.0] pos:[263, 28]

			label           surfp1lbl   "Surface" pos:[44, 62]
			label           surfp2lbl   "Properties" pos:[32, 75]
			label           amblbl      "Ambient" pos:[96, 50]
			spinner 		amb         ""	range:[0,1,0] width:49 height:16 pos:[92, 67]
			label           diflbl      "Diffuse" pos:[182, 50]
			spinner 		dif         ""	range:[0,1,0] width:49 height:16 pos:[177, 67]
			label           speclbl     "Specular" pos:[267, 50]
			spinner 		spec        ""	range:[0,1,0] width:49 height:16 pos:[263, 67]

			label           texlbl      "Texture" pos:[42, 98]
			mapbutton		texture     "None" pos:[92, 93] width:219 height:21
		)
		group "MatFX Options" (
			label        fxlbl "Effect Type" pos:[25,147]
			dropdownlist fxtype "" pos:[92, 145] width:219 height:21 items:#("rpMATFXEFFECTNULL","rpMATFXEFFECTBUMPMAP","rpMATFXEFFECTENVMAP","rpMATFXEFFECTBUMPENVMAP","rpMATFXEFFECTDUAL","rpMATFXEFFECTUVTRANSFORM","rpMATFXEFFECTDUALUVTRANSFORM")
			label        envmaplbl "Env Map" pos:[39,173]
			mapbutton    envmap    "None" pos:[92, 171] width:168 height:21
			spinner      envcoef   ""	range:[0,1,0.5] width:49 height:16 pos:[263, 174]
			label        bumpmaplbl "Bump Map" pos:[32,199]
			mapbutton    bumpmap   "None" pos:[92, 197] width:168 height:21
			spinner      bumpcoef  ""	range:[0,1,0.5] width:49 height:16 pos:[263, 200]
			label        dpmaplbl "Dual Pass" pos:[35,225]
			mapbutton    dpmap     "None" pos:[92, 223] width:219 height:21
			label        srclbl "Src Blend" pos:[37,251]
			dropdownlist srcblend "" pos:[92, 249] width:219 height:21 items:#("rpBLENDZERO", "rpBLENDONE", "rpBLENDSRCCOLOR", "rpBLENDINVSRCCOLOR", "rpBLENDSRCALPHA", "rpBLENDINVSRCALPHA", "rpBLENDDESTALPHA", "rpBLENDINVDESTALPHA", "rpBLENDDESTCOLOR", "rpBLENDINVDESTCOLOR", "rpBLENDSRCALPHASAT")
			label        destlbl "Dest Blend" pos:[30,274]
			dropdownlist destblend "" pos:[92, 271] width:219 height:21 items:#("rpBLENDZERO", "rpBLENDONE", "rpBLENDSRCCOLOR", "rpBLENDINVSRCCOLOR", "rpBLENDSRCALPHA", "rpBLENDINVSRCALPHA", "rpBLENDDESTALPHA", "rpBLENDINVDESTALPHA", "rpBLENDDESTCOLOR", "rpBLENDINVDESTCOLOR", "rpBLENDSRCALPHASAT")
		)
 	)
	rollout rsExt "Rockstar Extensions" (
		group "Env mat Parameters" (
			checkbox  enEnv "Enable" pos:[20, 30]
			label     shinlbl "Shininess" pos:[208,30]
			spinner   shininess ""	range:[0,1,0.25] width:49 height:16 pos:[263, 30]
			label     sclXlbl "ScaleX" pos:[28,53]
			spinner   sclX ""	range:[-1000,1000,1.0] width:49 height:16 pos:[17, 70]
			label     sclYlbl "ScaleY" pos:[110,53]
			spinner   sclY ""	range:[-1000,1000,1.0] width:49 height:16 pos:[99, 70]
			label     tsclXlbl "TransSclX" pos:[183,53]
			spinner   trSclX ""	range:[-1000,1000,1.0] width:49 height:16 pos:[181, 70]
			label     tsclYlbl "TransSclY" pos:[265,53]
			spinner   trSclY ""	range:[-1000,1000,1.0] width:49 height:16 pos:[263, 70]
		)
		group "Spec mat Parameters" (
			checkbox  enSpec "Enable" pos:[20, 122]
			label     speclbl "Specularity" pos:[200,122]
			spinner   specularity ""	range:[0,1,0.5] width:49 height:16 pos:[263, 122]
			label     specmaplbl "Spec Map" pos:[30,151]
			mapbutton specmap "None" pos:[92, 148] width:219 height:21
		)
	)
	--rollout render "Right to Render" (
	--		label idlbl "Plugin" pos:[10, 12]
	--		editText pluginID "" pos:[50, 10] width:100
	--		label datalbl "Data" pos:[185, 12]
	--		editText pluginData "" pos:[220, 10] width:100
	--)

	on create do (
		this.name = delegate.name
		--delegate.adLock = false
		delegate.diffuseMap = RGB_Multiply()
		delegate.diffuseMapEnable = true
	)
	on load do (
		this.name = delegate.name         
	)
)
