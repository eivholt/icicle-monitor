import time
import asyncio
import random
import logging
import carb
from pxr import Sdf
import omni.usd
import omni.replicator.core as rep

logger = logging.getLogger(__name__)
logger.info("Logging with 'logging'")
carb.log_info("Logging with 'carb'")
rep.settings.carb_settings("/omni/replicator/RTSubframes", 1) #If randomizing materials leads to problems, try value 3

with rep.new_layer():
	def randomize_camera(targets):
		with camera:
			rep.randomizer.scatter_2d(surface_prims=cameraPlane)
			rep.modify.pose(look_at=targets)
			rep.modify.attribute("focalLength", rep.distribution.uniform(10.0, 40.0))
		return camera.node
	

	def scatter_ice(icicles):
		with icicles:
			carb.log_info(f'Scatter icicle {icicles}')

			ice_rotation = random.choice(
				[
					(-90, 90, 0),
					(-90, -90, 0),
				]
			)
			rep.modify.pose(rotation=ice_rotation)
			rep.randomizer.scatter_2d(surface_prims=icePlane, check_for_collisions=True)
		return icicles.node

	#rep.settings.set_render_pathtraced(samples_per_pixel=64)
	cameraPlane = rep.get.prims(path_pattern='/World/CameraPlane')
	icePlane = rep.get.prims(path_pattern='/World/IcePlane')

	carb.log_info(f'Get assets')
	camera = rep.create.camera(position=(0, 0, 0))
	icicles = rep.get.prims(semantics=[("class", "ice")])

	render_product = rep.create.render_product(camera, (1024, 1024))
	
	carb.log_info(f'Register randomizers')
	rep.randomizer.register(randomize_camera)
	rep.randomizer.register(scatter_ice)
	
	carb.log_info(f'rep.trigger.on_frame')
	with rep.trigger.on_frame(num_frames=100):  # rt_subframes=50
		rep.randomizer.scatter_ice(icicles)
		rep.randomizer.randomize_camera(icicles)

	writer = rep.WriterRegistry.get("BasicWriter")
	writer.initialize(
		output_dir="C:/Users/eivho/source/repos/icicle-monitor/omniverse-replicator/out",
		rgb=True,
		bounding_box_2d_loose=True)

	writer.attach([render_product])
	rep.orchestrator.preview()
	#asyncio.ensure_future(rep.orchestrator.step_async())

