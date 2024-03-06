import time
import asyncio
import logging
import carb
from pxr import Sdf
import omni.usd
import omni.replicator.core as rep

logger = logging.getLogger(__name__)
logger.info("Logging with 'logging'")
carb.log_info("Logging with 'carb'")
rep.settings.carb_settings("/omni/replicator/RTSubframes", 1) #If randomizing materials leads to problems, try value 3

def make_asset_attribute_accessible(prim, attribute_name, attr_value):
	if not prim.GetAttribute(attribute_name).IsValid():
		prim.CreateAttribute(attribute_name, Sdf.ValueTypeNames.Asset, custom=True).Set(attr_value)
		carb.log_info(f'Set prim for {attribute_name}')
		print(f'Set prim for {attribute_name}')

def change_attribute(prim, obj, attribute, value):
	carb.log_info(f'{attribute} try value: {str(value)}')
	carb.log_info(f'{attribute} old value: {str(prim.GetAttribute(attribute).Get())}')
	with obj:
		rep.modify.attribute(attribute, value)  # input_prims=prim
	carb.log_info(f'{attribute} new value: {str(prim.GetAttribute(attribute).Get())}')

with rep.new_layer():
	def randomize_camera():
		with camera:
			rep.randomizer.scatter_2d(surface_prims=cameraPlane)
			#rep.modify.pose(look_at=icicles)
			rep.modify.attribute("focalLength", rep.distribution.uniform(10.0, 40.0))
		return camera.node
	

	def increment_time(shader_prim, shader_obj):
		# carb.log_info(f'Increment_time')
		change_attribute(shader_prim, shader_obj, "inputs:SHA", 134.6565399169922)
		change_attribute(shader_prim, shader_obj, "inputs:Azimuth", 45.35400390625)
		change_attribute(shader_prim, shader_obj, "inputs:Elevation", -8.07358169555664)
		change_attribute(shader_prim, shader_obj, "inputs:SunColor", (0.6525097, 0.41065302, 0.047867507))
		#with environment:
		#rep.modify.attribute("focalLength", rep.distribution.uniform(10.0, 40.0))
		#return environment.node
		return shader_obj.node


	def scatter_ice(icicles):
		with icicles:
			carb.log_info(f'Scatter icicle {icicles}')
			# flip.. rep.modify.pose(rotation=rep.distribution.uniform((0, 0, 0), (0, 360, 0)))
			rep.randomizer.scatter_2d(surface_prims=icePlane, check_for_collisions=True)
		return icicles.node

	stage = omni.usd.get_context().get_stage()
	#rep.settings.set_render_pathtraced(samples_per_pixel=64)
	cameraPlane = rep.get.prims(path_pattern='/World/CameraPlane')
	icePlane = rep.get.prims(path_pattern='/World/IcePlane')
	# environment = rep.get.prims(path_pattern='/Environment')
	# skyMaterial = rep.get.prims(path_pattern='/Environment/sky/Looks/SkyMaterial')


	# 14.02.2024 Bodø 51.42599868774414 -0.9850000143051147
	#/Environment/sky/Looks/SkyMaterial/Shader.inputs:SHA = -87.8794937133789  
	#/Environment/sky/Looks/SkyMaterial/Shader.inputs:Azimuth = -97.15502166748047
	#/Environment/sky/Looks/SkyMaterial/Shader.inputs:Elevation = -11.439241409301758
	#/Environment/sky/Looks/SkyMaterial/Shader.inputs:SunColor = (0.25, 0.013, 0)

	#timeofday= 6.0
	#/Environment/sky/Looks/SkyMaterial/Shader.inputs:SHA = -90.34346771240234  
	#/Environment/sky/Looks/SkyMaterial/Shader.inputs:Azimuth = -86.57725524902344
	#/Environment/sky/Looks/SkyMaterial/Shader.inputs:Elevation = 7.2430572509765625
	#/Environment/sky/Looks/SkyMaterial/Shader.inputs:SunColor = (0.43885505, 0.22980562, 0.075542025)

	#timeofday= 12.0
	#/Environment/sky/Looks/SkyMaterial/Shader.inputs:SHA = -0.3434655964374542 
	#/Environment/sky/Looks/SkyMaterial/Shader.inputs:Azimuth = -179.60435485839844
	#/Environment/sky/Looks/SkyMaterial/Shader.inputs:Elevation = 30.718976974487305
	#/Environment/sky/Looks/SkyMaterial/Shader.inputs:SunColor = (1, 0.98, 0.95)

	#timeofday= 21.0
	#/Environment/sky/Looks/SkyMaterial/Shader.inputs:SHA = 134.6565399169922
	#/Environment/sky/Looks/SkyMaterial/Shader.inputs:Azimuth = 45.35400390625
	#/Environment/sky/Looks/SkyMaterial/Shader.inputs:Elevation = -8.07358169555664
	#/Environment/sky/Looks/SkyMaterial/Shader.inputs:SunColor = (0.25, 0.013, 0)

	carb.log_info(f'Get assets')
	camera = rep.create.camera(position=(0, 0, 0))
	icicles = rep.get.prims(semantics=[("class", "ice")])
	#firstIcicle = rep.get.prims(path_pattern='/World/icicles')
	#shader_prim = rep.GetPrimAtPath("/Environment/sky/Looks/SkyMaterial/Shader")
	
	#sun_shader_prim = stage.GetPrimAtPath("/Environment/sky/Looks/SkyMaterial/Shader")
	#shader = rep.get.prims("/Environment/sky/Looks/SkyMaterial/Shader")
	
	#shader_prim = stage.GetPrimAtPath("/Environment/sky/Looks/SkyMaterial/Shader")
	

	render_product = rep.create.render_product(camera, (1024, 1024))
	
	carb.log_info(f'Register randomizers')
	rep.randomizer.register(randomize_camera)
	rep.randomizer.register(scatter_ice)
	rep.randomizer.register(increment_time)

	sky = rep.create.from_usd("https://omniverse-content-production.s3.us-west-2.amazonaws.com/Environments/2023_1/DomeLights/Dynamic/CumulusLight.usd")
	
	sun_shader_prim = stage.GetPrimAtPath("/Replicator/Ref_Xform/Ref/Looks/SkyMaterial/Shader")
	sun_shader = rep.get.prims(path_pattern="/Replicator/Ref_Xform/Ref/Looks/SkyMaterial/Shader")

	carb.log_info(f'Make attributes accessible')
	make_asset_attribute_accessible(sun_shader_prim, "inputs:SHA", "")
	make_asset_attribute_accessible(sun_shader_prim, "inputs:Azimuth", "")
	make_asset_attribute_accessible(sun_shader_prim, "inputs:Elevation", "")
	make_asset_attribute_accessible(sun_shader_prim, "inputs:SunColor", "")	
    
	carb.log_info(f'rep.trigger.on_frame')
	with rep.trigger.on_frame(num_frames=100):  # rt_subframes=50
		rep.randomizer.scatter_ice(icicles)
		rep.randomizer.randomize_camera()
		rep.randomizer.increment_time(sun_shader_prim, rep.get.prims(path_pattern="/Replicator/Ref_Xform/Ref/Looks/SkyMaterial/Shader"))

	writer = rep.WriterRegistry.get("BasicWriter")
	writer.initialize(
		output_dir="C:/Users/eivho/source/repos/icicle-monitor/omniverse-replicator/out",
		rgb=True,
		bounding_box_2d_loose=True)

	writer.attach([render_product])
	rep.orchestrator.preview()
	#asyncio.ensure_future(rep.orchestrator.step_async())

