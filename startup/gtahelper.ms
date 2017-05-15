try (fileIn "RW_Material.ms") catch ()
try (fileIn "GTA_Material.ms") catch ()

fn RwToKam m = (
	if (classof m) == Multimaterial do (
		for i = 1 to m.materialList.count do
			m.materialList[i] = RwToKam m.materialList[i]
		return m
	)
	if (classof m) != RW_Mtl do
		return m
	kam = GTA_Mtl()
	kam.amb = m.sp_ambient
	kam.spc = m.sp_specular
	kam.dif = m.sp_diffuse
	kam.color = m.color
	kam.colormap = m.texmap_texture
	kam.use_colormap = kam.colormap != undefined
	kam.alpha = m.coloralpha*255
-- 	kam.alphamap 
-- 	kam.use_alphamap
	kam.use_RF = m.matfxeffect == 3
	kam.Reflection = m.envmap_amount*100
	kam.reflectionmap = m.texmap_envmap
	kam.use_reflectionmap = kam.reflectionmap != undefined

	kam.use_SAS = m.enSpec
-- 	kam.specular
	kam.specularmap = m.specMap
	kam.use_specularmap = kam.specularmap != undefined

	kam.use_SI = m.enEnv
-- 	kam.spec_alpha
 	kam.spec_power = m.specularity*100
	kam.blend = m.shininess*100
	kam
)

utility GtaHelper "GTA helper" (
	button convMat "RW to GTA materials" width:140 align:#center
	on convMat pressed do ( 
		for o in objects do (
			o.material = RwToKam o.material
		)
	)
)